/**************************************************************************/
/*  sim_server.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "core/config/engine.h"
#include "core/math/aabb.h"
#include "core/math/vector3.h"
#include "core/math/vector3i.h"
#include "core/math/transform_3d.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/io/resource.h"
#include "core/string/string_name.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/templates/rid.h"
#include "core/templates/rid_owner.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"
#include "core/variant/variant.h"

// SimServer — systemic / immersive-sim server (S-01: cadence, S-02: surfaces,
// S-03: ambient field, S-04: interaction, S-05: combat hooks).
//
// Registered as an Engine singleton via modules/sim/register_types.cpp (SCENE level).
// RID-space: subsystems hold RID → data; never walks the SceneTree. Headless-capable.
// Consumes PhysicsServer3D / RenderingServer for queries.
//
// Cadence pipeline (determinism rule, §3.2 RFC):
//   pre_tick  — emissions enter: emit_stimulus, surface queries, interaction verbs, hits
//   sim_tick  — resolution: stimulus → surface resolve → field sampling → acoustics
//   post_tick — propagation: subscriber delivery, effects, stealth readout update
//
// Full API spec: docs/rfc/simserver-rfc.md §4.

class SimServer : public Object {
	GDCLASS(SimServer, Object)

	// --- Internal structures (private) ---

	struct ScheduleEntry {
		int64_t due_tick = 0;
		StringName kind;
		Variant payload;
		StringName tag;
		int64_t repeat_ticks = 0; // 0 = one-shot
	};

	struct CadenceGroup {
		double rate_hz = 1.0;
		int64_t ticks_per_call = 1;
		int64_t last_run_tick = 0;
		Callable callable;
	};

  	struct StimulusEvent {
  		RID rid;
  		StringName type;
  		Vector3 position;
  		float radius = 0.0f;
  		Dictionary payload;
  		Dictionary opts;
  		int64_t emit_tick = 0;
  		bool delivered = false; // set true after push delivery at post_tick
  	};

	struct StimulusListener {
		AABB area_of_interest;
		Callable filter;
		Callable callable;
	};

	static SimServer *singleton;

	// --- S-01: Tick state ---
	int64_t _tick = 0;
	double _tick_seconds = 0.0;
	double _tick_duration = (1.0 / 60.0); // 60 Hz default

	// --- S-01: Schedule (deadline queue) ---
	List<ScheduleEntry> _schedule;

	// --- S-01: Cadence groups ---
	HashMap<StringName, CadenceGroup> _cadences;

	// --- S-01: Stimulus ---
	List<StimulusEvent> _stimulus_log;       // tick-tagged event history
	int64_t _stimulus_log_retention = 10;     // prune events older than N ticks

  	// Listener RIDs: generate via _rid_owner (RID_Alloc<StimulusListener>), data stored inline.
  	RID_Alloc<StimulusListener> _rid_owner;
  	uint64_t _next_stimulus_id = 1; // stimulus event RID source (events live in _stimulus_log)

	// --- S-02: Surface registry ---
	HashMap<ObjectID, Ref<Resource>> _surface_assignments; // ObjectID -> SurfaceProperties

	// --- S-03: Ambient field ---
	// Per-field grid: uniform cell grid over an AABB. The light channel (index 0)
	// stores exposure in [0,1] (1 = fully exposed to sky/ambient, 0 = fully occluded).
	struct SimField {
		RID rid;
		AABB bounds;
		float cell_size = 1.0f;
		int channels = 1;        // v1 = light only (index 0)
		int cells_x = 0, cells_y = 0, cells_z = 0;
		Vector<float> samples;   // cells_x * cells_y * cells_z * channels
		HashMap<Vector3i, bool> dirty; // dirty cell tracking for rebake
		float ambient_energy = 0.2f;  // base ambient illumination
		// Dynamic light sources: cell-key → additive energy modifier (torch etc).
		HashMap<Vector3i, float> dynamic_add;
		bool baked = false;
	};
	RID_Alloc<SimField> _field_owner; // RID allocator + inline data

	// --- Internal helpers ---
	void _process_schedule();     // pre_tick: dispatch due schedule entries
	void _process_stimuli();      // post_tick: deliver stimuli to listeners
	void _run_cadences();         // run cadence callbacks at tick boundaries
	void _prune_stimulus_log();   // drop events older than retention window

	// --- S-03 helpers ---
	SimField *_field_get(RID rid);
	const SimField *_field_get(RID rid) const;
	Vector3i _field_cell_index(const SimField *field, const Vector3 &position) const;
	float _field_sample_channel(const SimField *field, const Vector3 &position, int channel) const;
	float _field_sample_exposure(SimField *field, const Vector3 &position, float budget_s);

protected:
	static void _bind_methods();

public:
	// --- Singleton ---
	static SimServer *get_singleton();

	// --- S-01: Clock & cadence ---
	int64_t get_tick() const;
	double now_seconds() const;
	Dictionary schedule_at_tick(int64_t due_tick, const StringName &kind, const Variant &payload,
			const StringName &tag, int64_t repeat_ticks);
	void schedule_in_seconds(double secs, const StringName &kind, const Variant &payload,
			const StringName &tag, double repeat_secs);
	int cancel_by_tag(const StringName &tag);
	bool is_deadline_active(int64_t tick);
	int64_t deadline_tick_after(double secs);
	double deadline_remaining_seconds(int64_t tick);
	void register_cadence(const StringName &name, double rate_hz, const Callable &callable);
	void unregister_cadence(const StringName &name);
	void advance_ticks(int64_t n);
	Dictionary get_time_state() const;
	void restore_time_state(const Dictionary &state);
	Dictionary get_schedule_state() const;
	void restore_schedule_state(const Dictionary &state);

	// --- S-01: Stimulus bus ---
	RID emit_stimulus(const StringName &type, const Vector3 &position, float radius,
			const Dictionary &payload, const Dictionary &opts);
	RID register_stimulus_listener(const AABB &area_of_interest, const Callable &filter,
			const Callable &callable);
	void unregister_stimulus_listener(RID rid);
	Array query_stimulus(const Vector3 &position, float radius, const Array &types,
			int64_t since_tick);

	// --- S-02: Surface registry + query ---
	void set_surface_properties(Object *target, const Ref<Resource> &surface_properties);
	Dictionary query_surface(const Vector3 &from, const Vector3 &to, const Dictionary &opts);

	// --- S-03: Ambient field ---
	RID field_create(const AABB &aabb, float cell_size, int channels);
	void field_bake(RID rid, int budget_per_frame);
	Variant get_field_sample(RID rid, const Vector3 &position, int channel);
	void invalidate_region(RID rid, const AABB &aabb);
	void field_set_dynamic_source(RID rid, RID source_rid, float energy);
	float get_stealth_value(const Vector3 &position, Object *subject);

	// --- S-04: Interaction substrate ---
	Array query_interaction_focus(const Vector3 &camera_origin, const Vector3 &camera_dir, int max_results);

	SimServer();
	~SimServer();
};
