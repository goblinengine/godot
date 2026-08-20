# Goblin Engine — Documentation Index

Goblin Engine is a deliberately customized, lightweight fork of Godot Engine. All changes live inside `modules/goblin/`; upstream Godot source files are never modified.

## Start Here

1. **[backlog.md](backlog.md)** — the single source of truth for all work: planned, in-progress, done, rejected.
2. **[ROADMAP.md](ROADMAP.md)** — phased roadmap, pain-point → solution map, risk assessment (§1 carries the vision).
3. **[CODE_MAP.md](CODE_MAP.md)** — navigation map: where files live, what each layer does, where new code goes.

## Architecture Decisions

- **[adr/](adr/)** — accepted and proposed Architecture Decision Records. Locked decisions that guide all implementation.

## RFCs

- **[rfc/](rfc/)** — exploratory design documents (RFCs) before they are promoted to ADRs.

## Implementation Plans

- **[plans/](plans/)** — implementation breakdowns: locked semantics, phases, files, effort, test gates. What the developer follows to build a feature.

## Reference

- **[gdscript_features.md](gdscript_features.md)** — the GDScript fork's language features and divergence from upstream.
- **[genre-coverage.md](genre-coverage.md)** — genre-by-genre needs × Godot overlap × fork plan gap analysis (vision alignment).
- **[rfc/simserver-rfc.md](rfc/simserver-rfc.md)** — SimServer: systemic/immersive-sim server (clock/cadence, stimulus, surface query, ambient field, stealth readout).
- **[rfc/fast-scene-tree-rfc.md](rfc/fast-scene-tree-rfc.md)** — FastSceneTree: additive SceneTree-compatible main loop (new class, cadence fast paths, frame hooks).
- **[rfc/entity-node-rfc.md](rfc/entity-node-rfc.md)** — EntityNode/EntityComponent: hybrid tree+ECS layer (EntityComponent : Node, SoA pools owned by SceneTree, batched server drivers, native editor). Implementation started 2026-08-20 (D-20); plan: [plans/entity-node-plan.md](plans/entity-node-plan.md).
- **[STRUCTURE.md](STRUCTURE.md)** — module layout and the override mechanisms.
- **[BRANDING_STATUS.md](BRANDING_STATUS.md)** — what gets rebranded and what does not.
- **[LIGHTMAP_INVESTIGATION.md](LIGHTMAP_INVESTIGATION.md)** — reference for the lightmap core changes (backlog C-01/C-02).

## Conventions

- ADRs lock stable, expensive-to-reverse decisions. See [adr/README.md](adr/README.md).
- RFCs hold exploratory designs before ADR promotion. See [rfc/README.md](rfc/README.md).
- The backlog is updated whenever work is planned, started, or completed. No task lives only in a prompt or chat.
