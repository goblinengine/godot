# Plan Index

Implementation breakdowns for Goblin Engine (the Godot fork). These are the execution specs the developer follows: locked semantics, mechanism/placement, phases with files and effort, test gates, risks.

## When To Use A Plan vs An RFC vs An ADR

Use a **plan** when:
- the design direction is settled enough to break into implementation phases
- the engineer needs files, effort estimates, and test gates to execute

Use an **RFC** (see [../rfc/](../rfc/)) when:
- the design is still exploratory and multiple credible options need evaluation

Use an **ADR** (see [../adr/](../adr/)) when:
- a decision is stable, expensive to reverse, and guides multiple implementations

Lifecycle: RFC (explore) → plan (execute) → ADR (lock). All three are kept permanently; outdated plans get a superseded marker inside the file, never deleted. Backlog rows track status and link the plan.

## Active Plans

| Plan | Area | Status |
|------|------|--------|
| [lightmapper-cpu-plan.md](lightmapper-cpu-plan.md) | Core rendering | 2026-08-14. Stage-by-stage port map of `lightmapper_rd` → CPU, data structures, threading, perf model, risks. Companion to the [lightmapper-cpu-rfc](../rfc/lightmapper-cpu-rfc.md) |
| [cut-upscalers-plan.md](cut-upscalers-plan.md) | Core rendering | 2026-08-14. Locked spec: CUT1/2/3 GLES3 3D-scaling modes, clean-room, mirror+swap set + header edit; phases/gates/config keys |
| [fast-scene-tree-plan.md](fast-scene-tree-plan.md) | Core — SceneTree | 2026-08-16. FULL re-implementation: `FastSceneTree : public MainLoop` (additive module `modules/fast_scene_tree/`, also hosts the EntityNode/EntityComponent phase-2 layer) via `application/run/main_loop_type`; P0 evidence+audit gate → P1 contract matrix (spec) → P2 seam (node.h additive field + node.cpp swap) → P3–P4 contract impl → P6 optimizations (flat lists, intrusive groups) → P8 full validation. Companion to the [fast-scene-tree-rfc](../rfc/fast-scene-tree-rfc.md) |
| [entity-node-plan.md](entity-node-plan.md) | Core — hybrid tree+ECS | 2026-08-20. Locked spec (D-20 runtime core): `EntityComponent : Node` (S1 override), EntityRegistry + per-type ComponentPools owned by SceneTree (forward-declared pointer member, scene_tree.h +5), compile-on-enter, dirty-flag batched flush in mirror scene_tree.cpp, `_GOBLIN_FILE_ADDITIONS` source injection, registration at SCENE level. Batch 1: Transform3D/MeshInstance/Visibility. Phases P1–P6, doctest + GDScript smoke gates. Companion to the [entity-node-rfc](../rfc/entity-node-rfc.md) |
| [module-trim-fix-plan.md](module-trim-fix-plan.md) | Build system | 2026-08-16. Locked spec (ADR 0012): import-time ARGUMENTS injection makes the module trim actually engage; trim list 30→28 (tinyexr + godot_physics_3d re-enabled); dead `disabled_modules` code removed; build/suite/import gates. Re-opens B-01 (was falsely `done`) |
