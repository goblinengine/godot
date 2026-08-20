# RFC Index

Exploratory design documents for Goblin Engine (the Godot fork). These hold designs that are directionally approved but not yet frozen into ADRs.

## When To Use An RFC

Use an RFC when:
- the design is still exploratory
- the implementation shape is not proven enough to freeze
- multiple credible options still need evaluation

Promote an RFC to an ADR (in [../adr/](../adr/)) when direction and boundary are both stable enough to freeze.

## Active RFCs

| RFC | Area | Status |
|-----|------|--------|
| [lightmapper-cpu-rfc.md](lightmapper-cpu-rfc.md) | Core rendering | Proposed 2026-08-14; awaiting architect review. Runtime CPU lightmapper via `Lightmapper::create_cpu`; supersedes ADR 0006 direction |
| [cut-upscalers-rfc.md](cut-upscalers-rfc.md) | Core rendering | Proposed 2026-08-14; clean-room CUT1/2/3 for GLES3 3D scaling (license path + enum strategy + mechanism options; locked in plan) |
| [native-game-features-rfc.md](native-game-features-rfc.md) | GDScript language | Proposed 2026-08-13; consolidated. §3 (`then`/`elthen`, recovered from gdscript2) = **shipped (G-04/G-05, locked semantics)**; §2.1 typed entries = **shipped (G-17)**; §2 template dictionaries = **rejected 2026-08-19** (templates stay GDScript-side, see `data.gd`; §2 superseded-marker in-file); §2.5 callable shorthand = **G-19 (todo)**. Secondary candidates + rejected ideas retained. Not yet approved as an implementation plan |
| [simserver-rfc.md](simserver-rfc.md) | Systemic / immersive sim | Proposed 2026-08-16; direction agreed (user 2026-08-16). Additive module `modules/sim/`: clock/cadence + stimulus bus (S-01), surface registry + query (S-02), ambient field + stealth readout (S-03), interaction substrate (S-04), combat hooks (S-05). Folds C-05/C-06/M-07/M-08/M-09. Cadence pipeline (pre_tick → sim_tick → post_tick) for determinism; RID-space, orthogonal to SceneTree |
| [fast-scene-tree-rfc.md](fast-scene-tree-rfc.md) | Core — SceneTree replacement | Proposed 2026-08-16; direction locked (user 2026-08-16): **`FastSceneTree : public MainLoop` — full re-implementation** (extending SceneTree rejected — inherits unoptimizable private machinery). Additive module `modules/fast_scene_tree/` (ADR 0008; also hosts the EntityNode/EntityComponent phase-2 layer), selected via `application/run/main_loop_type`; **seam = generic `BaseSceneTree : MainLoop`** (public virtual API; narrow sanctioned upstream edits: scene_tree.h base change + node.h retype; no dual-path, no file swaps). P0 = A/B benchmark (SceneTree vs FastSceneTree), not a gate. T1–T6 new internals; type-identity divergence documented. Companion plan: `plans/fast-scene-tree-plan.md` |

Implementation breakdowns for approved directions live in [../plans/](../plans/) (e.g. `lightmapper-cpu-plan.md` — stage-by-stage port map of `lightmapper_rd` → CPU, data structures, threading, perf model, risks; companion to the RFC).

## Candidate RFCs

| Candidate | Area | Notes |
|-----------|------|-------|
| gdscript-structs-rfc | Language | Struct memory layout, copy semantics, Variant round-trip |
| gdscript-typed-dictionaries-rfc | Language | `Dictionary[K, V]` syntax and runtime representation |
| gdscript-performance-rfc | Language | Opcode fusing, inline caching, PIC — what to port and order |
| portals-as-nodes-rfc | Module | `PortalSurface3D`/`MirrorSurface3D` custom nodes; `SubViewport` + teleport + portal-aware queries |
| retro-native-editor-rfc | Editor | Palette, dither, color cycling, posterization as editor-provided nodes/plugins |
| brush-mover-contract-rfc | Core | Kinematic doors/lifts/crushers; deterministic hull traces |
| lightstyle-surface-class-rfc | Core | Style-channel modulation of baked light + retro surface classes |
| [entity-node-rfc.md](entity-node-rfc.md) | Core — hybrid tree+ECS | **Active, design locked** — implementation started 2026-08-20 (D-20; deferral gate MET, M-14 T1/T6 landed). EntityNode : Node (entity_id + type mask, scripts) + **EntityComponent : Node** (revised 2026-08-20: was Object sibling — native editor piggyback requires tree children). Data in SceneTree-owned per-type SoA pools; batched server flush at frame boundary. Batch 1: Transform3D/MeshInstance/Visibility; physics bodies + CollisionShape + camera deferred. Editor: native (no custom work). Not a full ECS — no archetypes/systems. Plan: `../plans/entity-node-plan.md` |

Each RFC must state: context, the problem, options considered, a recommended direction, and open questions. RFCs are strong directional guidance unless explicitly marked superseded.
