# Goblin Engine — SimServer RFC (Systemic / Immersive Sim Server)

- **Proposal file:** `modules/goblin/docs/rfc/simserver-rfc.md`
- **Date:** 2026-08-16
- **Status:** Proposed — direction agreed 2026-08-16; awaiting architect review before backlog rows are implemented
- **Base:** Godot 4.7.1-stable, fork plan v0.2.0
- **Mechanism:** Additive feature module (ADR 0008), zero overrides, zero upstream edits

---

## 1. Purpose

One server-side home for the systemic / immersive-sim layer of the fork. The charter promises
*"systemic out of the box: interaction, events/stimulus, ambient perception fields (light/acoustics),
cadence scheduling, timers — engine-supported"*. Today those capabilities are scattered or absent:

- cadence scheduling / tick queue — scripted in the reference title (title scheduler script), no engine item
- events/stimulus — no engine item (witness search is O(N) GDScript queries)
- ambient perception fields — C-05/M-07/M-08 at P2, not started
- audio occlusion — C-06 at P2, not started
- surface metadata for hits/impacts — M-09 at P3, scripted in the title (4 static caches + collider walks)
- interaction — scripted in the title, no engine substrate

`SimServer` consolidates these into one additive module with one API surface, following Godot's
own server pattern (`PhysicsServer3D`, `NavigationServer3D`): RID-based data, scene-tree-independent,
consuming other servers for queries. It is the native consolidation of patterns the reference title
already proves in GDScript — not a speculative architecture.

## 2. Context — evidence from the reference title

The title's scripts (read 2026-08-15) implement exactly this layer by hand:

| Title script | Capability | Pain today |
|---|---|---|
| title scheduler script (357 lines) | authoritative tick, tagged deadline queue, repeat, cadence groups, time-skip fast path, save/restore | `sort_custom` per schedule; cadences via `call_group` string names |
| title physics surface resolution | surface type from material/texture, absorption, level-hit decoration | 4 static caches, up to 4-hop collider walks, per-hit, in combat hot path |
| title witness search (physics helper) | stimulus delivery: short-range OR LOS-within-sight, faction filter | O(N) shape query + per-witness raycast, GDScript |
| title interaction script | verb dispatch (USE/HIT), rule gates, lock/key | generic focus/target resolution mixed with game policy |
| viewport-based light sensor | light readout for stealth | explicitly "not scalable" (ROADMAP §9) |

`SimServer` makes these engine-native and cross-feeding: a stimulus sound resolves surface
properties, which feeds the acoustic field, which selects the effect, which reads the light field —
all inside one server with a defined data-flow order.

## 3. Architecture

### 3.1 Shape

- Additive module `modules/sim/` (ADR 0008 anatomy, mirrors `modules/midi/`):
  `SCsub`, `config.py` (`can_build()`/`get_doc_classes()`/`get_icons_path()`), `register_types.{h,cpp}`,
  `doc_classes/`, `tests/`, `editor/icons/`. Zero overrides; nothing outside `modules/sim/` touched.
- `SimServer` singleton registered like `JoltPhysicsServer3D` (module-provided server precedent):
  `Object` singleton, own `RID_Alloc` owner, own data maps.
- RID-space only: subsystems hold `RID → data`; nodes/scripts opt in by registering RIDs.
  SimServer never walks the SceneTree and never requires it — headless-capable.
- Consumes other servers: `PhysicsServer3D` (ray/shape/space queries), `RenderingServer` (as needed
  for visuals), `AudioServer` (as needed for occlusion data consumption — mixer stays upstream).
- **SceneTree replacement is explicitly out of scope and orthogonal** (see §10). SimServer is
  RID-space data + services; the tree remains the presentation/lifecycle layer.

### 3.2 The cadence pipeline (determinism rule)

All cross-subsystem data flow runs through a fixed per-tick order. No subsystem reads another
outside its phase — this is what makes "everything feeds everything" deterministic:

```
pre_tick   emissions enter: emit_stimulus, surface queries, interaction verbs, hits
sim_tick   resolution: stimulus → surface resolve → field sampling → acoustics
post_tick  propagation: subscriber delivery, effects, stealth readout update
```

- Authoritative tick owned by SimServer (`get_tick()`); `advance_ticks(n)` fast path for
  time-skip/offline simulation (skips nothing server-side — the fast path exists for visual cadences).
- Cadence groups register with a rate; handlers run at their cadence inside the tick order.
- Tick order is fixed (phase + registration order), so simulation is reproducible for a given seed.

### 3.3 Threading

- API calls: synchronous on the main thread (like physics *queries*). No locks on the API path.
- Heavy work (ambient field baking): `WorkerThreadPool` with a per-frame ray budget.
  No `force_immediate` escape hatch; region invalidation enqueues cells, not synchronous work.
- Internal state is main-thread-owned; worker threads only compute samples into pre-allocated buffers.

## 4. Subsystems (phases)

### S-01 — Clock & cadence + stimulus bus

**Clock/cadence API (replaces the standalone `SimClock` idea — no separate class):**

```
SimServer.get_tick() -> int
SimServer.now_seconds() -> float
SimServer.schedule_at_tick(due_tick, kind, payload, tag, repeat_ticks) -> Dictionary
SimServer.schedule_in_seconds(secs, kind, payload, tag, repeat_secs)
SimServer.cancel_by_tag(tag) -> int
SimServer.deadline_tick_after(secs) / is_deadline_active(tick) / deadline_remaining_*(...)
SimServer.register_cadence(name, rate_hz, callable) / unregister_cadence(name)
SimServer.advance_ticks(n)          # authoritative time skip
SimServer.get_time_state() / restore_time_state(dict)      # save/restore, matches title pattern
SimServer.get_schedule_state() / restore_schedule_state(dict)
```

API maps 1:1 onto the title's scheduler script (verified signatures above). Event dispatch is generic
(`kind` + `payload`); game-specific kinds (effect/rule/shot/emit_signal) are game-registered handlers.

**Stimulus bus:**

```
SimServer.emit_stimulus(type, position, radius, payload, opts) -> RID   # pre_tick entry
SimServer.register_stimulus_listener(area_of_interest, filter, callable) -> RID
SimServer.unregister_stimulus_listener(rid)
SimServer.query_stimulus(position, radius, types, since_tick) -> Array  # pull mode
```

- Spatial index (uniform grid, sized to level scale) — delivery is O(nearby), not O(world).
- Push delivery happens at post_tick on the listener's cadence; pull queries read the tick-tagged log.
- witness-search semantics (short-range OR LOS-within-sight, faction filter) become query options.

**Gates:** title scheduler-script semantics reproduced: same tick math, same save/restore payload shape,
same tag/cancel/repeat behavior. Corpus + 342 tests + level load unchanged.

### S-02 — Surface registry & query

```
SurfaceProperties : Resource            # module class, editor-assignable
  surface_type: StringName              # "ice", "metal", "stone", "flesh", ...
  impact_sound / footstep_sound: StringName
  penetration: float
  absorption: float
  decal: StringName
  physics_material: PhysicsMaterial     # OPTIONAL reference — physics response stays core
SimServer.set_surface_properties(rid_or_node, res)     # explicit assignment
SimServer.query_surface(from, to, opts) -> Dictionary  # wraps intersect_ray, decorates result
```

Query result = existing ray dict + `surface`, `surface_properties`, `impact_uv`, `material_name`.
Resolution chain (verified against title code): level-hit metadata → explicit RID assignment →
material-name table fallback (the title's current model) → default.

- **Impact UV**: Godot's `intersect_ray` returns `face_index` but no UV; module-side barycentric
  interpolation over the hit triangle (~50 lines C++, pure math over mesh arrays — no core change).
- **PhysicsMaterial stays core and untouched.** PhysicsServer owns friction/bounce as floats;
  `SurfaceProperties.physics_material` is a read reference merged into the query record. Two layers,
  one merged read — resolves the "PhysicsMaterial vs SurfaceProperties" conflict without core edits.
- Kills the title's 4 static caches + collider-hierarchy walks (`_get_material_from_ray_hit` etc.).

**Gates:** query returns correct UV/material/surface for: mesh-instance hits, level-geometry hits,
bodies with explicit assignment, bodies with no assignment (fallback chain). Same dict keys the
title reads today (`surface_type`, `absorption`, `position`, `normal`, ...) so call sites keep working.

### S-03 — Ambient field (light + acoustics) + stealth readout

```
SimServer.field_create(aabb, cell_size, channels) -> RID          # light channel v1
SimServer.field_bake(rid, budget_per_frame)                       # async, WorkerThreadPool
SimServer.get_field_sample(rid, pos, channel) -> Variant          # trilinear, cheap
SimServer.invalidate_region(rid, aabb)                            # dynamic patch → budgeted rebake
SimServer.field_set_dynamic_source(rid, source_rid, energy, ...)  # torch on/off etc.
SimServer.get_stealth_value(pos, subject) -> float                # M-08 consumer API
```

- **Field bake is independent of lightmap baking.** Lightmaps = surface lighting (C-02);
  the field = ambient data at points (exposure, occlusion) via CPU hemisphere sampling against
  physics geometry + the light list. Works in scenes with zero lightmaps. Static bake per level/region.
- **Dynamic updates** = region invalidation → budgeted async rebake (torch-out scenario).
  No force-immediate path.
- **Acoustics channel later**: environment/reverb zones from the field; per-source audio occlusion
  (C-06) becomes: runtime raycasts cached at cadence (Hz rate, not per-frame) + field acoustic
  channel. C-06 folds here — node-layer bus routing stays script-side.
- Channels are typed grids on the same mechanism (v1 = light only; no speculative channels).
- Stealth readout = consumer API over the light channel (direct + occlusion + ambient) —
  the "gameplay readout, not HUD" contract (M-08).

**Gates:** synthetic room: light exposure analytic match, occlusion boundary, torch-out → rebake
within N frames under budget, sample determinism for a seed.

### S-04 — Interaction substrate (post-field, optional in v1)

```
Interactable3D : Node3D   # verbs, prompt data, interact(data) contract
SimServer.query_interaction_focus(camera_ray, opts) -> Array      # scored candidates
```

Engine ships the generic layer only (target resolution, candidate priority, occlusion-aware focus).
Rule gates / lock-key / difficulty / HUD stay game script (policy). Formalizes the title's
duck-typed `has_method("interact")` contract.

### S-05 — Combat integration

Combat subsystem (C-14, co-located in `modules/sim/`, moved 2026-08-17) gains optional SimServer hooks: `Hitbox3D` hits
emit `stimulus` (impact events), `Projectile3D` resolves `query_surface` for surface-specific
impact effects/sounds. Co-located in `modules/sim/` — hooks are internal C++ calls, no cross-module coupling.

## 5. In / Out guardrails

**In (systemic/immersive, cross-cutting, world-data):**
clock/cadence · stimulus bus · surface registry + query · ambient field (light/acoustic channels)
· stealth readout · interaction focus query (S-04).

**Out (honest boundaries):**
physics simulation (PhysicsServer owns; SimServer only queries) · audio mixing/positioning
(AudioServer owns; SimServer supplies occlusion *data*) · rendering · input · UI prompts ·
save-file format · game rules/policy · scene lifecycle (SceneTree stays).

**Rule of thumb:** if it is presentation or policy, it stays outside; if it is world-data that two
or more subsystems read, it lives inside.

## 6. Naming

- Server: `SimServer` (Godot `*Server` convention; module-owned singleton per Jolt precedent).
- No separate clock class — clock is SimServer API (`get_tick`, `schedule_at_tick`,
  `register_cadence`). `SimClock`/`GameClock`/`TickScheduler`/`CadenceScheduler` all rejected.
- `SurfaceProperties` (not `MaterialProperties` — "Material" collides with the Godot Material class
  family in docs/autocomplete). `query_surface` (dict-compatible result; typed `SurfaceHit3D`
  accessor deferred unless autocomplete needs it).
- Field API uses `field_*` prefix (`field_create`, `field_bake`, `get_field_sample`).
  `FieldServer` rejected — the field is a SimServer subsystem, not a second server.

## 7. Effort & phases

| Phase | Ships | Effort | Depends |
|---|---|---|---|
| S-01 | Clock/cadence + stimulus bus | 1–2 w | — |
| S-02 | Surface registry + query (incl. impact UV) | 3–5 d | S-01 (tick timing) |
| S-03 | Ambient field v1 (light channel) + stealth readout | 2–3 w | S-01 |
| S-04 | Interaction substrate | 3–5 d | S-02 (focus ray resolution) |
| S-05 | Combat integration hooks | ~1 w | S-01/S-02 + C-14 |

**Total ~5–8 w.** S-01 + S-02 deliver the two highest-pain migrations first (scheduler script,
surface-resolution path). Suggested order: S-01 → S-02 → S-03 (light channel) → S-04 → S-05.

## 8. Test gates

- Per-phase: module `tests/` doctest cases (S-01: tick math, tag/cancel/repeat, save/restore
  round-trip; S-02: UV/material/surface resolution incl. fallback chain; S-03: analytic exposure,
  occlusion boundary, budgeted rebake, determinism).
- Always: full GDScript suite green (`--test --test-case "[Modules][GDScript]*"` on `tests=yes`),
  reference corpus compile, 342 tests, level load unchanged. SimServer must not change behavior of
  anything that does not opt in.

## 9. Risks

| Risk | Mitigation |
|---|---|
| Scope creep ("everything in SimServer") | §5 guardrails + rule of thumb; phase review at each milestone |
| Cross-subsystem ordering bugs | Fixed cadence pipeline (§3.2); tests assert phase order |
| RID lifecycle leaks (dangling refs) | `RID_Alloc` owner + explicit `free()` on unregister; tests for unregister paths |
| Worker-thread bake races | Main-thread-owned state; workers only fill pre-allocated buffers |
| Title migration breaks existing scripts | Dict-compatible results (S-02), API 1:1 with the scheduler script (S-01); migration is opt-in per call site |

## 10. Out of scope — SceneTree replacement (separate track)

SceneTree replacement is a **separate track, deliberately not part of this RFC**: SimServer is
RID-space and orthogonal to the tree. Direction locked 2026-08-16: `FastSceneTree : public MainLoop`
— a full re-implementation (spec in `fast-scene-tree-rfc.md` + `plans/fast-scene-tree-plan.md`,
backlog M-14). The EntityNode/EntityComponent flat-data stage is a further, deferred RFC
(`entity-node-rfc.md`) riding on the tree's stable-iteration hooks. SimServer and the tree track
stay independent; the cadence pipeline (S-01) may later consume the tree's `register_cadence`
(T5) when both exist.

## 11. Open questions (decide at plan time)

1. Grid cell size / index policy for stimulus + field — fixed per level or adaptive?
2. Does S-03 light channel sample dynamic lights (baked static + per-tick dynamic reads) in v1?
3. Save/restore scope: which SimServer state persists in the title's delta save/load (tick yes,
   schedule yes, field samples no)?
4. `query_surface` return: plain dict (title-compatible) vs typed object — dict in v1, revisit
   when structs (G-07) land.

## 12. Backlog mapping

Supersedes/folds: C-05, C-06, M-07, M-08, M-09 → S-01/S-02/S-03 (statuses updated in backlog.md).
C-14 (combat subsystem) co-located in `modules/sim/` (moved 2026-08-17); S-05 adds integration hooks. Genre-coverage cluster 2
(perception fields) now maps to S-01/S-03.
