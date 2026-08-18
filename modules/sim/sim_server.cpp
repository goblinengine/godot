/**************************************************************************/
/*  sim_server.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "sim_server.h"

#include "core/config/engine.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"

SimServer *SimServer::singleton = nullptr;

// --- Constructor / Destructor ---

SimServer::SimServer() {
	singleton = this;
}

SimServer::~SimServer() {
	_stimulus_log.clear();
	_schedule.clear();
	singleton = nullptr;
}

SimServer *SimServer::get_singleton() {
	return singleton;
}

// =========================================================================
// S-01: Clock & Cadence
// =========================================================================

int64_t SimServer::get_tick() const {
	return _tick;
}

double SimServer::now_seconds() const {
	return _tick * _tick_duration;
}

Dictionary SimServer::schedule_at_tick(int64_t due_tick, const StringName &kind, const Variant &payload,
		const StringName &tag, int64_t repeat_ticks) {
	ScheduleEntry entry;
	entry.due_tick = due_tick;
	entry.kind = kind;
	entry.payload = payload;
	entry.tag = tag;
	entry.repeat_ticks = repeat_ticks;
	_schedule.push_back(entry);

	Dictionary result;
	result["due_tick"] = due_tick;
	result["kind"] = kind;
	result["tag"] = tag;
	result["repeat_ticks"] = repeat_ticks;
	result["scheduled"] = true;
	return result;
}

void SimServer::schedule_in_seconds(double secs, const StringName &kind, const Variant &payload,
		const StringName &tag, double repeat_secs) {
	int64_t tick_offset = (int64_t)Math::ceil(secs / _tick_duration);
	int64_t due_tick = _tick + tick_offset;
	int64_t repeat_ticks = repeat_secs > 0.0 ? (int64_t)Math::ceil(repeat_secs / _tick_duration) : 0;
	schedule_at_tick(due_tick, kind, payload, tag, repeat_ticks);
}

int SimServer::cancel_by_tag(const StringName &tag) {
	int cancelled = 0;
	List<ScheduleEntry>::Element *it = _schedule.front();
	while (it != nullptr) {
		List<ScheduleEntry>::Element *next = it->next();
		if (it->get().tag == tag) {
			it->erase();
			cancelled++;
		}
		it = next;
	}
	return cancelled;
}

bool SimServer::is_deadline_active(int64_t tick) {
	for (const ScheduleEntry &entry : _schedule) {
		if (entry.due_tick == tick) {
			return true;
		}
	}
	return false;
}

int64_t SimServer::deadline_tick_after(double secs) {
	return _tick + (int64_t)Math::ceil(secs / _tick_duration);
}

double SimServer::deadline_remaining_seconds(int64_t tick) {
	return (double)(tick - _tick) * _tick_duration;
}

void SimServer::register_cadence(const StringName &name, double rate_hz, const Callable &callable) {
	CadenceGroup group;
	group.rate_hz = rate_hz;
	if (rate_hz > 0.0) {
		group.ticks_per_call = (int64_t)Math::floor(1.0 / (rate_hz * _tick_duration));
		if (group.ticks_per_call < 1) {
			group.ticks_per_call = 1;
		}
	} else {
		group.ticks_per_call = 1;
	}
	group.callable = callable;
	_cadences[name] = group;
}

void SimServer::unregister_cadence(const StringName &name) {
	_cadences.erase(name);
}

void SimServer::advance_ticks(int64_t n) {
	_tick += n;

	// Phase 1: pre_tick / sim_tick — dispatch schedule entries due at or before current tick.
	_process_schedule();

	// Phase 2: sim_tick — run cadence callbacks at their registered rate.
	_run_cadences();

	// Phase 3: post_tick — deliver stimuli to registered listeners.
	_process_stimuli();

	// Cleanup: prune stimulus log entries outside the retention window.
	_prune_stimulus_log();
}

Dictionary SimServer::get_time_state() const {
	Dictionary state;
	state["tick"] = _tick;
	state["tick_duration"] = _tick_duration;
	return state;
}

void SimServer::restore_time_state(const Dictionary &state) {
	if (state.has("tick")) {
		_tick = (int64_t)state["tick"];
	} else {
		// Full reset: clear transient event state.
		_tick = 0;
		_stimulus_log.clear();
	}
	_tick_duration = state.has("tick_duration") ? (double)state["tick_duration"] : (1.0 / 60.0);
}

Dictionary SimServer::get_schedule_state() const {
	Dictionary state;
	Array entries;
	for (const ScheduleEntry &entry : _schedule) {
		Dictionary e;
		e["due_tick"] = entry.due_tick;
		e["kind"] = entry.kind;
		e["payload"] = entry.payload;
		e["tag"] = entry.tag;
		e["repeat_ticks"] = entry.repeat_ticks;
		entries.append(e);
	}
	state["entries"] = entries;
	return state;
}

void SimServer::restore_schedule_state(const Dictionary &state) {
	_schedule.clear();
	if (!state.has("entries")) {
		return;
	}
	Array entries = state["entries"];
	for (int i = 0; i < entries.size(); i++) {
		Dictionary e = entries[i];
		ScheduleEntry entry;
		entry.due_tick = e.has("due_tick") ? (int64_t)e["due_tick"] : 0;
		entry.kind = e.has("kind") ? (StringName)e["kind"] : StringName();
		entry.payload = e.has("payload") ? e["payload"] : Variant();
		entry.tag = e.has("tag") ? (StringName)e["tag"] : StringName();
		entry.repeat_ticks = e.has("repeat_ticks") ? (int64_t)e["repeat_ticks"] : 0;
		_schedule.push_back(entry);
	}
}

// --- Internal: schedule processing (pre_tick + sim_tick) ---

void SimServer::_process_schedule() {
	List<ScheduleEntry>::Element *it = _schedule.front();
	while (it != nullptr) {
		List<ScheduleEntry>::Element *next = it->next();
		ScheduleEntry &entry = it->get();

		if (entry.due_tick <= _tick) {
			// Dispatch: if the payload carries a Callable, invoke it.
			// The Callable carries its own context (closure / bound method).
			if (entry.payload.get_type() == Variant::CALLABLE) {
				Callable cb = entry.payload;
				if (!cb.is_null()) {
					cb.call(entry.kind);
				}
			}

			// Handle repeat: reschedule if repeat_ticks > 0.
			if (entry.repeat_ticks > 0) {
				ScheduleEntry repeated;
				repeated.due_tick = entry.due_tick + entry.repeat_ticks;
				repeated.kind = entry.kind;
				repeated.payload = entry.payload;
				repeated.tag = entry.tag;
				repeated.repeat_ticks = entry.repeat_ticks;
				_schedule.push_back(repeated);
			}

			// Remove the original entry (one-shot or repeated-with-new-entry).
			it->erase();
		}

		it = next;
	}
}

void SimServer::_run_cadences() {
	for (KeyValue<StringName, CadenceGroup> &kv : _cadences) {
		CadenceGroup &group = kv.value;
		if (group.ticks_per_call > 0 && (_tick - group.last_run_tick) >= group.ticks_per_call) {
			if (!group.callable.is_null()) {
				group.callable.call();
			}
			group.last_run_tick = _tick;
		}
	}
}

// =========================================================================
// S-01: Stimulus Bus
// =========================================================================

RID SimServer::emit_stimulus(const StringName &type, const Vector3 &position, float radius,
		const Dictionary &payload, const Dictionary &opts) {
	StimulusEvent event;
	event.rid = RID::from_uint64(_next_stimulus_id++);
	event.type = type;
	event.position = position;
	event.radius = radius;
	event.payload = payload;
	event.opts = opts;
	event.emit_tick = _tick;
	_stimulus_log.push_back(event);
	return event.rid;
}

RID SimServer::register_stimulus_listener(const AABB &area_of_interest, const Callable &filter,
		const Callable &callable) {
	StimulusListener listener;
	listener.area_of_interest = area_of_interest;
	listener.filter = filter;
	listener.callable = callable;
	return _rid_owner.make_rid(listener);
}

void SimServer::unregister_stimulus_listener(RID rid) {
	_rid_owner.free(rid);
}

void SimServer::_process_stimuli() {
	if (_stimulus_log.is_empty() || _rid_owner.get_rid_count() == 0) {
		return;
	}

	LocalVector<RID> rids = _rid_owner.get_owned_list();

	// Iterate events outer, listeners inner — each event is delivered once
	// (push delivery at post_tick), then stays in the log for pull queries.
	for (StimulusEvent &event : _stimulus_log) {
		if (event.emit_tick > _tick || event.delivered) {
			continue;
		}

		for (const RID &rid : rids) {
			StimulusListener *listener = _rid_owner.get_or_null(rid);
			if (listener == nullptr) {
				continue;
			}

			// Spatial filter: stimulus sphere intersects listener AABB.
			const AABB &aabb = listener->area_of_interest;
			bool intersects;
			if (event.radius <= 0.0f) {
				intersects = aabb.has_point(event.position);
			} else {
				Vector3 closest;
				closest.x = CLAMP(event.position.x, aabb.position.x, aabb.position.x + aabb.size.x);
				closest.y = CLAMP(event.position.y, aabb.position.y, aabb.position.y + aabb.size.y);
				closest.z = CLAMP(event.position.z, aabb.position.z, aabb.position.z + aabb.size.z);
				intersects = event.position.distance_squared_to(closest) <= event.radius * event.radius;
			}
			if (!intersects) {
				continue;
			}

			// Faction / game-specific filter.
			if (!listener->filter.is_null()) {
				Variant result = listener->filter.call(event.rid, event.type, event.position, event.payload);
				if (!result.booleanize()) {
					continue;
				}
			}

			// Build delivery record.
			Dictionary delivery;
			delivery["rid"] = event.rid;
			delivery["type"] = event.type;
			delivery["position"] = event.position;
			delivery["radius"] = event.radius;
			delivery["payload"] = event.payload;
			delivery["emit_tick"] = event.emit_tick;

			if (!listener->callable.is_null()) {
				listener->callable.call(delivery);
			}
		}

		event.delivered = true;
	}
}

void SimServer::_prune_stimulus_log() {
	int64_t cutoff = _tick - _stimulus_log_retention;
	List<StimulusEvent>::Element *it = _stimulus_log.front();
	while (it != nullptr) {
		List<StimulusEvent>::Element *next = it->next();
		if (it->get().emit_tick < cutoff) {
			it->erase();
		}
		it = next;
	}
}

Array SimServer::query_stimulus(const Vector3 &position, float radius, const Array &types,
		int64_t since_tick) {
	Array results;
	for (const StimulusEvent &event : _stimulus_log) {
		if (event.emit_tick < since_tick) {
			continue;
		}

		// Type filter.
		if (!types.is_empty()) {
			bool type_match = false;
			for (int i = 0; i < types.size(); i++) {
				if ((StringName)types[i] == event.type) {
					type_match = true;
					break;
				}
			}
			if (!type_match) {
				continue;
			}
		}

		// Spatial filter: stimulus sphere intersects query sphere.
		float combined_radius = event.radius + radius;
		float dist_sq = event.position.distance_squared_to(position);
		if (combined_radius > 0.0f && dist_sq > combined_radius * combined_radius) {
			continue;
		}

		Dictionary result;
		result["rid"] = event.rid;
		result["type"] = event.type;
		result["position"] = event.position;
		result["radius"] = event.radius;
		result["payload"] = event.payload;
		result["emit_tick"] = event.emit_tick;
		results.append(result);
	}
	return results;
}

// =========================================================================
// S-02: Surface registry & query (stub — implemented in S-02 phase)
// =========================================================================

void SimServer::set_surface_properties(Object *target, const Ref<Resource> &surface_properties) {
	// S-02: store surface properties mapping for the target. No-op until S-02 ships.
}

Dictionary SimServer::query_surface(const Vector3 &from, const Vector3 &to, const Dictionary &opts) {
	// S-02: wraps PhysicsServer3D::intersect_ray, decorates with surface properties + UV.
	Dictionary result;
	result["hit"] = false;
	return result;
}

// =========================================================================
// S-03: Ambient field (stub — implemented in S-03 phase)
// =========================================================================

RID SimServer::field_create(const AABB &aabb, float cell_size, int channels) {
	// S-03: allocate a grid-based ambient field. Stub for v1.
	return RID();
}

void SimServer::field_bake(RID rid, int budget_per_frame) {
	// S-03: async bake via WorkerThreadPool. Stub for v1.
}

Variant SimServer::get_field_sample(RID rid, const Vector3 &position, int channel) {
	// S-03: trilinear sample. Stub for v1.
	return Variant();
}

void SimServer::invalidate_region(RID rid, const AABB &aabb) {
	// S-03: enqueue region for budgeted rebake. Stub for v1.
}

void SimServer::field_set_dynamic_source(RID rid, RID source_rid, float energy) {
	// S-03: torch on/off etc. Stub for v1.
}

float SimServer::get_stealth_value(const Vector3 &position, Object *subject) {
	// S-03: gameplay readout over the light channel. Stub for v1.
	return 0.0f;
}

// =========================================================================
// S-04: Interaction substrate (stub — implemented in S-04 phase)
// =========================================================================

Array SimServer::query_interaction_focus(const Vector3 &camera_origin, const Vector3 &camera_dir,
		int max_results) {
	// S-04: scored candidates via intersect_ray / shape query.
	return Array();
}

// =========================================================================
// Binding
// =========================================================================

void SimServer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_tick"), &SimServer::get_tick);
	ClassDB::bind_method(D_METHOD("now_seconds"), &SimServer::now_seconds);
	ClassDB::bind_method(D_METHOD("schedule_at_tick", "due_tick", "kind", "payload", "tag", "repeat_ticks"),
			&SimServer::schedule_at_tick,
			StringName(), 0);
	ClassDB::bind_method(D_METHOD("schedule_in_seconds", "secs", "kind", "payload", "tag", "repeat_secs"),
			&SimServer::schedule_in_seconds,
			StringName(), 0.0);
	ClassDB::bind_method(D_METHOD("cancel_by_tag", "tag"), &SimServer::cancel_by_tag);
	ClassDB::bind_method(D_METHOD("is_deadline_active", "tick"), &SimServer::is_deadline_active);
	ClassDB::bind_method(D_METHOD("deadline_tick_after", "secs"), &SimServer::deadline_tick_after);
	ClassDB::bind_method(D_METHOD("deadline_remaining_seconds", "tick"), &SimServer::deadline_remaining_seconds);
	ClassDB::bind_method(D_METHOD("register_cadence", "name", "rate_hz", "callable"), &SimServer::register_cadence);
	ClassDB::bind_method(D_METHOD("unregister_cadence", "name"), &SimServer::unregister_cadence);
	ClassDB::bind_method(D_METHOD("advance_ticks", "n"), &SimServer::advance_ticks);
	ClassDB::bind_method(D_METHOD("get_time_state"), &SimServer::get_time_state);
	ClassDB::bind_method(D_METHOD("restore_time_state", "state"), &SimServer::restore_time_state);
	ClassDB::bind_method(D_METHOD("get_schedule_state"), &SimServer::get_schedule_state);
	ClassDB::bind_method(D_METHOD("restore_schedule_state", "state"), &SimServer::restore_schedule_state);

	ClassDB::bind_method(D_METHOD("emit_stimulus", "type", "position", "radius", "payload", "opts"),
			&SimServer::emit_stimulus,
			Dictionary(), Dictionary());
	ClassDB::bind_method(D_METHOD("register_stimulus_listener", "area_of_interest", "filter", "callable"),
			&SimServer::register_stimulus_listener,
			Callable(), Callable());
	ClassDB::bind_method(D_METHOD("unregister_stimulus_listener", "rid"), &SimServer::unregister_stimulus_listener);
	ClassDB::bind_method(D_METHOD("query_stimulus", "position", "radius", "types", "since_tick"),
			&SimServer::query_stimulus,
			Array(), 0);

	// S-02 stub — bound so GDScript can call even before implementation ships.
	ClassDB::bind_method(D_METHOD("set_surface_properties", "target", "surface_properties"), &SimServer::set_surface_properties);
	ClassDB::bind_method(D_METHOD("query_surface", "from", "to", "opts"), &SimServer::query_surface, Dictionary());

	// S-03 stub
	ClassDB::bind_method(D_METHOD("field_create", "aabb", "cell_size", "channels"), &SimServer::field_create);
	ClassDB::bind_method(D_METHOD("field_bake", "rid", "budget_per_frame"), &SimServer::field_bake);
	ClassDB::bind_method(D_METHOD("get_field_sample", "rid", "position", "channel"), &SimServer::get_field_sample);
	ClassDB::bind_method(D_METHOD("invalidate_region", "rid", "aabb"), &SimServer::invalidate_region);
	ClassDB::bind_method(D_METHOD("field_set_dynamic_source", "rid", "source_rid", "energy"), &SimServer::field_set_dynamic_source);
	ClassDB::bind_method(D_METHOD("get_stealth_value", "position", "subject"), &SimServer::get_stealth_value);

	// S-04 stub
	ClassDB::bind_method(D_METHOD("query_interaction_focus", "camera_origin", "camera_dir", "max_results"),
			&SimServer::query_interaction_focus, 32);
}
