# ADR 0001: Source Override Architecture

Status: Accepted

Date: 2026-08-12

## Context

The fork must customize Godot Engine — rebrand it, fork GDScript, and override individual core files — without modifying any upstream Godot source file. Modifying upstream files directly would create an unmanageable merge surface when rebasing on stable releases.

Three mechanisms were evaluated:

1. A source-file interception layer patching `env.add_source_files()` with an override map (proposed in the original fork plan).
2. Whole-module path swap via `env.module_list` redirection.
3. Single-file swap via an `add_library()` hook.

## Decision

Goblin uses three override mechanisms, all living inside `modules/goblin/`:

1. **Module Directory Override** — `GOBLIN_MODULE_OVERRIDES` in `modules/goblin/SCsub` replaces an entry in `env.module_list` with a goblin-owned mirror directory. Currently maps `gdscript` → `modules/goblin/modules/gdscript/`. Used when forking many files of one module.

2. **Core File Override** — `goblin_add_library()` in `modules/goblin/config.py` intercepts `env.add_library("core", ...)` and swaps a single source `Object` node before the library captures its sources. Currently swaps `variant_construct.cpp`. Used when overriding one or two core files surgically.

3. **Builder Monkey-Patching** — `configure()` in `config.py` replaces build-time generator functions (version header, splash, icons, authors/license) and renames binaries `godot` → `goblin`. Used when replacing a build-time generator.

The source-file interception layer (option 1) was rejected in favor of mechanisms 2 and 3, which are simpler and leverage Godot's existing `env.module_list` and `add_library` infrastructure directly.

## Consequences

Positive:
- Upstream files are never modified; rebase surface stays minimal.
- Each mechanism is narrowly scoped and independently testable.
- The override surface is auditable by reading one dict and one hook function.

Negative:
- A whole-module override (the GDScript fork) is a large divergence surface that must be diffed against upstream on every rebase.
- The `add_library` hook is name-matching based (`"variant_construct" in str(_s)`); it should be generalized to a `{basename: path}` dict before adding a second file override.
- Mechanisms are documented in `.kilo/rules/rules.md` so both agents follow the same injection points.

## Related Documents

- [ROADMAP.md](../ROADMAP.md)
- [.kilo/rules/rules.md](../../../.kilo/rules/rules.md)
