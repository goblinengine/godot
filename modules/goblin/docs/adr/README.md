# ADR Index

Accepted and proposed Architecture Decision Records for Goblin Engine (the Godot fork).

## When To Use An ADR vs An RFC

Use an **ADR** when:
- a decision is stable enough to guide multiple implementations
- the decision is expensive to reverse later
- the decision is no longer mainly exploratory

Use an **RFC** (see [../rfc/](../rfc/)) when:
- the design is still exploratory
- the implementation shape is not yet proven enough to freeze
- multiple credible options still need evaluation

Promote an RFC to an ADR when direction and boundary are both stable enough to freeze.

## Accepted ADRs

1. [0001-source-override-architecture.md](0001-source-override-architecture.md)
   Locks the three override mechanisms: whole-module replacement, single-file core swap, and builder monkey-patching — plus build-time option injection (0012). All changes live in `modules/goblin/`.

2. [0002-stable-release-tracking.md](0002-stable-release-tracking.md)
   Locks tracking of official Godot stable release tags instead of `master`, with rebase only at release boundaries.

3. [0003-module-trim-evidence-standard.md](0003-module-trim-evidence-standard.md)
   Locks the evidence standard required before any module is disabled, and the fixed trim list.

## Proposed ADRs (not yet accepted)

4. `0004-gdscript-union-types` — union types in the GDScript fork.
5. `0005-gdscript-safe-navigation-null-coalescing` — `then` / `elthen` operators (keywords locked; `?.`/`??` rejected).
6. `0006-runtime-lightmap-api-surface` — superseded in direction (2026-08-14) by [lightmapper-cpu-rfc](../rfc/lightmapper-cpu-rfc.md): engine CPU `Lightmapper` via `Lightmapper::create_cpu` instead of promoting the extension `LightmapBaker`.

## Accepted ADRs (continued)

7. [0007-compile-time-ui-overrides.md](0007-compile-time-ui-overrides.md)
   Replaces the runtime UI-patching singletons (120-attempt retry loops, `node_added` tree scans) with compile-time file overrides of four editor files; extends `goblin_add_library()` to a library-scoped dict.

8. [0008-standalone-additive-modules.md](0008-standalone-additive-modules.md)
   Additive feature modules (zero overrides) live at the repo root in `modules/<name>/` as standalone Godot modules with standard module anatomy and full lifecycle (`MODULE_<NAME>_ENABLED`, `DISABLE_MODULES`, own registration/docs/icons/tests); `modules/goblin/` stays override/branding-only. MIDI lives at `modules/midi/`.

9. [0009-clean-room-cut-upscalers.md](0009-clean-room-cut-upscalers.md)
   Clean-room GPL boundary for the CUT upscalers + first header direct-edit precedent (`rendering_server_enums.h`, unreachable by swap).

10. [0012-build-time-option-injection.md](0012-build-time-option-injection.md)
   4th override mechanism: module-level ARGUMENTS mutation in `modules/goblin/config.py` (import time, before `opts.Update`) sets `module_*_enabled=no` for the module trim gate — fixes B-01 (the trim was a silent no-op). User CLI wins; configure() canary print.

## Not Yet ADRs

The following remain at RFC level intentionally:
- exact struct memory layout and copy semantics
- typed dictionary syntax and runtime representation
- light-probe grid data format and sampling model
- which GDScript performance optimizations (opcode fusing, inline caching, PIC) to port and in what order
