/**************************************************************************/
/*  sim_server.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "sim_server.h"

#include "core/config/engine.h"
#include "core/math/aabb.h"
#include "core/math/geometry_3d.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/variant/variant.h"

#include "scene/3d/mesh_instance_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

#include "surface_properties.h"

#include "servers/physics_3d/physics_server_3d.h"
#include "servers/rendering/rendering_server.h"

SimServer *SimServer::singleton = nullptr;

// --- Constructor / Destructor ---

SimServer::SimServer() {
	singleton = this;
}

SimServer::~SimServer() {
	_stimulus_log.clear();
	_schedule.clear();
	_cadences.clear();
	// Free all listener RIDs.
	for (const RID &rid : _rid_owner.get_owned_list()) {
		_rid_owner.free(rid);
	}
	// Free all field RIDs.
	for (const RID &rid : _field_owner.get_owned_list()) {
		_field_owner.free(rid);
	}
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
// S-02: Surface registry & query
// =========================================================================

void SimServer::set_surface_properties(Object *target, const Ref<Resource> &surface_properties) {
	if (target == nullptr) {
		return;
	}
	ObjectID id = target->get_instance_id();
	if (surface_properties.is_null()) {
		_surface_assignments.erase(id);
	} else {
		_surface_assignments[id] = surface_properties;
	}
}

Dictionary SimServer::query_surface(const Vector3 &from, const Vector3 &to, const Dictionary &opts) {
	Dictionary result;
	result["hit"] = false;
	result["surface"] = StringName();
	result["material_name"] = "default";

	// --- Resolve physics space ---
	PhysicsDirectSpaceState3D *space_state = nullptr;
	if (opts.has("space")) {
		RID space = opts["space"];
		space_state = PhysicsServer3D::get_singleton()->space_get_direct_state(space);
	} else if (SceneTree::get_singleton() != nullptr && SceneTree::get_singleton()->get_root() != nullptr) {
		Ref<World3D> world = SceneTree::get_singleton()->get_root()->get_world_3d();
		if (world.is_valid()) {
			space_state = world->get_direct_space_state();
		}
	}

	if (space_state == nullptr) {
		return result;
	}

	// --- Cast ray ---
	PhysicsDirectSpaceState3D::RayParameters params;
	params.from = from;
	params.to = to;

	if (opts.has("collision_mask")) {
		params.collision_mask = (uint32_t)opts["collision_mask"];
	}
	if (opts.has("exclude")) {
		Array exclude = opts["exclude"];
		for (int i = 0; i < exclude.size(); i++) {
			params.exclude.insert((RID)exclude[i]);
		}
	}
	if (opts.has("collide_with_bodies")) {
		params.collide_with_bodies = (bool)opts["collide_with_bodies"];
	}
	if (opts.has("collide_with_areas")) {
		params.collide_with_areas = (bool)opts["collide_with_areas"];
	}

	PhysicsDirectSpaceState3D::RayResult ray_result;
	if (!space_state->intersect_ray(params, ray_result)) {
		return result;
	}

	// --- Populate base fields ---
	result["hit"] = true;
	result["position"] = ray_result.position;
	result["normal"] = ray_result.normal;
	result["rid"] = ray_result.rid;
	result["collider"] = ray_result.collider;
	result["face_index"] = ray_result.face_index;

	// --- Surface properties (explicit assignment) ---
	Ref<SurfaceProperties> props;
	if (ray_result.collider != nullptr) {
		ObjectID id = ray_result.collider->get_instance_id();
		HashMap<ObjectID, Ref<Resource>>::Iterator it = _surface_assignments.find(id);
		if (it != _surface_assignments.end() && it->value.is_valid()) {
			SurfaceProperties *sp = Object::cast_to<SurfaceProperties>(it->value.ptr());
			if (sp) {
				props = sp;
			}
		}
	}
	result["surface_properties"] = props;

	// --- Surface type + material name (resolution chain) ---
	StringName surface_type;
	String material_name = "default";

	if (props.is_valid()) {
		surface_type = props->get_surface_type();
	} else if (ray_result.collider != nullptr) {
		// Fallback: material-name table inferred from the collider's mesh.
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(ray_result.collider);
		if (mi != nullptr && ray_result.face_index >= 0) {
			Ref<Mesh> mesh = mi->get_mesh();
			if (mesh.is_valid()) {
				RID mesh_rid = mesh->get_rid();
				Array arrays = RenderingServer::get_singleton()->mesh_surface_get_arrays(mesh_rid, 0);
				if (!arrays.is_empty()) {
					// Compute impact UV via barycentric interpolation.
					PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
					PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
					PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];

					int idx0, idx1, idx2;
					if (indices.size() > 0) {
						idx0 = indices[ray_result.face_index * 3 + 0];
						idx1 = indices[ray_result.face_index * 3 + 1];
						idx2 = indices[ray_result.face_index * 3 + 2];
					} else {
						idx0 = ray_result.face_index * 3 + 0;
						idx1 = ray_result.face_index * 3 + 1;
						idx2 = ray_result.face_index * 3 + 2;
					}

					if (idx0 < vertices.size() && idx1 < vertices.size() && idx2 < vertices.size()) {
						Vector3 bary = Geometry3D::triangle_get_barycentric_coords(
								vertices[idx0], vertices[idx1], vertices[idx2], ray_result.position);
						if (uvs.size() > 0 && idx0 < uvs.size() && idx1 < uvs.size() && idx2 < uvs.size()) {
							Vector2 uv = uvs[idx0] * bary.x + uvs[idx1] * bary.y + uvs[idx2] * bary.z;
							result["impact_uv"] = uv;
						}
					}
				}
			}

			// Material name from active material.
			Ref<Material> mat = mi->get_active_material(0);
			if (mat.is_valid()) {
				material_name = mat->get_class();
			}
		}
	}

	result["surface"] = surface_type;
	result["material_name"] = material_name;
	return result;
}

// =========================================================================
// S-03: Ambient field (stub — implemented in S-03 phase)
// =========================================================================

// =========================================================================
// S-03: Ambient field (light channel + stealth readout)
// =========================================================================

SimServer::SimField *SimServer::_field_get(RID rid) {
	if (rid.is_null()) {
		return nullptr;
	}
	return _field_owner.get_or_null(rid);
}

const SimServer::SimField *SimServer::_field_get(RID rid) const {
	if (rid.is_null()) {
		return nullptr;
	}
	// RidOwner stores data in a const-incompatible way; cast through const_cast
	// for read-only lookup (data is main-thread-owned, never written on workers).
	const RID_Alloc<SimField> &owner = _field_owner;
	return const_cast<RID_Alloc<SimField> &>(owner).get_or_null(rid);
}

Vector3i SimServer::_field_cell_index(const SimField *field, const Vector3 &position) const {
	Vector3 local = (position - field->bounds.position) / field->cell_size;
	return Vector3i(
			CLAMP((int)Math::floor(local.x), 0, field->cells_x - 1),
			CLAMP((int)Math::floor(local.y), 0, field->cells_y - 1),
			CLAMP((int)Math::floor(local.z), 0, field->cells_z - 1));
}

float SimServer::_field_sample_channel(const SimField *field, const Vector3 &position, int channel) const {
	// Trilinear interpolation of the light (exposure) channel.
	if (channel < 0 || channel >= field->channels) {
		return 0.0f;
	}
	if (!field->baked) {
		return 0.0f;
	}

	Vector3 local = (position - field->bounds.position) / field->cell_size;
	int cx = (int)Math::floor(local.x);
	int cy = (int)Math::floor(local.y);
	int cz = (int)Math::floor(local.z);

	float fx = local.x - cx;
	float fy = local.y - cy;
	float fz = local.z - cz;

	int x0 = MAX(0, MIN(cx, field->cells_x - 1));
	int y0 = MAX(0, MIN(cy, field->cells_y - 1));
	int z0 = MAX(0, MIN(cz, field->cells_z - 1));
	int x1 = MIN(x0 + 1, field->cells_x - 1);
	int y1 = MIN(y0 + 1, field->cells_y - 1);
	int z1 = MIN(z0 + 1, field->cells_z - 1);

	int stride = field->channels;
	auto cell_val = [&](int x, int y, int z) -> float {
		int idx = (x + y * field->cells_x + z * field->cells_x * field->cells_y) * stride + channel;
		return field->samples[idx];
	};

	float v000 = cell_val(x0, y0, z0);
	float v001 = cell_val(x0, y0, z1);
	float v010 = cell_val(x0, y1, z0);
	float v011 = cell_val(x0, y1, z1);
	float v100 = cell_val(x1, y0, z0);
	float v101 = cell_val(x1, y0, z1);
	float v110 = cell_val(x1, y1, z0);
	float v111 = cell_val(x1, y1, z1);

	float v00 = Math::lerp(v000, v100, fx);
	float v01 = Math::lerp(v001, v101, fx);
	float v10 = Math::lerp(v010, v110, fx);
	float v11 = Math::lerp(v011, v111, fx);

	float v0 = Math::lerp(v00, v10, fy);
	float v1 = Math::lerp(v01, v11, fy);

	return Math::lerp(v0, v1, fz);
}

// Hemisphere exposure sampling: cast rays in N directions over the upper
// hemisphere (sky-facing). The fraction of unoccluded rays = exposure.
// This is the CPU hemisphere-sampling bake step (§3.3/§S-03 RFC).
float SimServer::_field_sample_exposure(SimField *field, const Vector3 &position, float budget_s) {
	// Resolve physics space for ray queries.
	PhysicsDirectSpaceState3D *space_state = nullptr;
	SceneTree *tree = SceneTree::get_singleton();
	if (tree != nullptr && tree->get_root() != nullptr) {
		Ref<World3D> world = tree->get_root()->get_world_3d();
		if (world.is_valid()) {
			space_state = world->get_direct_space_state();
		}
	}
	if (space_state == nullptr) {
		return field->ambient_energy;
	}

	// Hemisphere directions (Fibonacci sphere over upper hemisphere).
	const int sample_count = 32;
	int done = 0;
	float exposed = 0.0f;

	// Pre-compute Fibonacci directions on the upper hemisphere (z = up).
	Vector<Vector3> dirs;
	dirs.resize(sample_count);
	const float golden_ratio = 1.6180339887f;
	for (int i = 0; i < sample_count; i++) {
		float t = (float)i / (float)(sample_count - 1); // 0 → 1
		float phi = Math::TAU * (float)i / golden_ratio;
		float cos_theta = t; // z from 0 → 1 (upper hemisphere, z=up)
		float sin_theta = Math::sqrt(1.0f - cos_theta * cos_theta);
		dirs.write[i] = Vector3(Math::cos(phi) * sin_theta, cos_theta, Math::sin(phi) * sin_theta);
	}

	uint64_t start_usec = OS::get_singleton()->get_ticks_usec();
	const uint64_t budget_usec = (uint64_t)(budget_s * 1000000.0);

	for (int i = 0; i < sample_count; i++) {
		if (budget_usec > 0 && OS::get_singleton()->get_ticks_usec() - start_usec > budget_usec) {
			break;
		}
		const Vector3 dir = dirs[i];
		PhysicsDirectSpaceState3D::RayParameters params;
		params.from = position;
		params.to = position + dir * field->cell_size * 2.0f; // sample radius = 2 cells
		params.collide_with_bodies = true;
		params.collide_with_areas = false;

		PhysicsDirectSpaceState3D::RayResult ray_result;
		space_state->intersect_ray(params, ray_result);
		if (!ray_result.collider || ray_result.position == Vector3()) {
			// No hit → ray reaches the sky → exposed.
			exposed += 1.0f;
		}
		done++;
	}

	if (done == 0) {
		return field->ambient_energy;
	}
	float exposure = exposed / (float)done;
	return field->ambient_energy + exposure * (1.0f - field->ambient_energy);
}

RID SimServer::field_create(const AABB &aabb, float cell_size, int channels) {
	if (cell_size <= 0.0f || channels <= 0) {
		return RID();
	}
	SimField field;
	field.rid = RID(); // placeholder; set after make_rid
	field.bounds = aabb;
	field.cell_size = cell_size;
	field.channels = channels;
	field.cells_x = MAX(1, (int)Math::ceil(aabb.size.x / cell_size));
	field.cells_y = MAX(1, (int)Math::ceil(aabb.size.y / cell_size));
	field.cells_z = MAX(1, (int)Math::ceil(aabb.size.z / cell_size));
	int total_cells = field.cells_x * field.cells_y * field.cells_z;
	field.samples.resize(total_cells * channels);
	// Seed with neutral exposure (0.5) so unset cells are visible.
	field.samples.fill(0.5f);
	RID rid = _field_owner.make_rid(field);
	_field_owner.get_or_null(rid)->rid = rid; // backpointer
	return rid;
}

void SimServer::field_bake(RID rid, int budget_per_frame) {
	SimField *field = _field_get(rid);
	if (field == nullptr) {
		return;
	}

	float budget_s = (budget_per_frame > 0) ? (float)budget_per_frame * _tick_duration : _tick_duration;

	LocalVector<Vector3i> cells_to_bake;
	if (!field->baked) {
		// Full bake: every cell.
		for (int z = 0; z < field->cells_z; z++) {
			for (int y = 0; y < field->cells_y; y++) {
				for (int x = 0; x < field->cells_x; x++) {
					cells_to_bake.push_back(Vector3i(x, y, z));
				}
			}
		}
	} else {
		// Dirty-cell rebake only.
		for (const KeyValue<Vector3i, bool> &kv : field->dirty) {
			if (kv.value) {
				cells_to_bake.push_back(kv.key);
			}
		}
	}

	if (cells_to_bake.is_empty()) {
		field->baked = true;
		return;
	}

	uint64_t start_usec = OS::get_singleton()->get_ticks_usec();
	const uint64_t budget_usec = (uint64_t)(budget_s * 1000000.0);
	int baked_count = 0;

	for (const Vector3i &cell : cells_to_bake) {
		if (budget_usec > 0 && OS::get_singleton()->get_ticks_usec() - start_usec > budget_usec) {
			break; // budgeted — remaining cells bake next frame
		}
		if (cell.x < 0 || cell.x >= field->cells_x || cell.y < 0 || cell.y >= field->cells_y
				|| cell.z < 0 || cell.z >= field->cells_z) {
			continue;
		}
		Vector3 cell_pos = field->bounds.position
				+ Vector3(cell.x + 0.5f, cell.y + 0.5f, cell.z + 0.5f) * field->cell_size;
		float exposure = _field_sample_exposure(field, cell_pos, budget_s);
		int idx = (cell.x + cell.y * field->cells_x + cell.z * field->cells_x * field->cells_y) * field->channels;
		field->samples.write[idx] = exposure;
		field->dirty.erase(cell);
		baked_count++;
	}

	field->baked = true;
}

Variant SimServer::get_field_sample(RID rid, const Vector3 &position, int channel) {
	const SimField *field = _field_get(rid);
	if (field == nullptr) {
		return Variant();
	}
	float value = _field_sample_channel(field, position, channel);
	if (field->dynamic_add.size() > 0 && channel == 0) {
		const Vector3i cell = _field_cell_index(field, position);
		HashMap<Vector3i, float>::ConstIterator it = field->dynamic_add.find(cell);
		if (it != field->dynamic_add.end()) {
			value += it->value;
		}
	}
	return CLAMP(value, 0.0f, 1.0f);
}

void SimServer::invalidate_region(RID rid, const AABB &aabb) {
	SimField *field = _field_get(rid);
	if (field == nullptr) {
		return;
	}
	// Mark all cells intersecting the AABB as dirty.
	Vector3i cmin = _field_cell_index(field, aabb.position);
	Vector3i cmax = _field_cell_index(field, aabb.position + aabb.size);
	for (int z = cmin.z; z <= cmax.z; z++) {
		for (int y = cmin.y; y <= cmax.y; y++) {
			for (int x = cmin.x; x <= cmax.x; x++) {
				field->dirty[Vector3i(x, y, z)] = true;
			}
		}
	}
}

void SimServer::field_set_dynamic_source(RID rid, RID source_rid, float energy) {
	SimField *field = _field_get(rid);
	if (field == nullptr) {
		return;
	}
	// For v1, dynamic sources (torch) add energy to all cells within the
	// source's influence radius. Since we don't have the source's position,
	// we store a scalar modifier that applies to sampled values.
	// Source position lookup is deferred to when the field is sampled —
	// for now, apply as a uniform additive modifier clamped to [0,1].
	// NOTE: source_rid is accepted for API compatibility but position
	// resolution requires a registered source position (future S-03 extension).
	if (energy <= 0.0f) {
		// Remove all dynamic modifiers (torch off).
		field->dynamic_add.clear();
	} else {
		// Mark all cells as needing dynamic contribution.
		for (int z = 0; z < field->cells_z; z++) {
			for (int y = 0; y < field->cells_y; y++) {
				for (int x = 0; x < field->cells_x; x++) {
					field->dynamic_add[Vector3i(x, y, z)] = energy;
				}
			}
		}
	}
}

float SimServer::get_stealth_value(const Vector3 &position, Object *subject) {
	// Stealth = light exposure at the subject's position (0 = dark/hidden,
	// 1 = fully visible). Finds the field whose bounds contain the position.
	// If no field contains it, fall back to ambient (0.5f).
	const LocalVector<RID> &owned = _field_owner.get_owned_list();
	for (const RID &rid : owned) {
		const SimField *field = _field_owner.get_or_null(rid);
		if (field == nullptr || !field->baked || !field->bounds.has_point(position)) {
			continue;
		}
		float exposure = _field_sample_channel(field, position, 0);
		// Apply dynamic source contributions (torch etc).
		if (field->dynamic_add.size() > 0) {
			const Vector3i cell = _field_cell_index(field, position);
			HashMap<Vector3i, float>::ConstIterator it_add = field->dynamic_add.find(cell);
			if (it_add != field->dynamic_add.end()) {
				exposure += it_add->value;
			}
		}
		return CLAMP(exposure, 0.0f, 1.0f);
	}
	return 0.5f; // neutral: no field at this position
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
