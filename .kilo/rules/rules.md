# Goblin Engine - Rules

Fork of Godot 4.7.x. Overrides and branding live inside `modules/goblin/`, injected at build time. Additive feature modules (zero overrides) live at the repo root in `modules/<name>/` as standalone modules (ADR 0008). Upstream files: never modified.

## Hard Rules

1. NEVER clean `bin/`: no `scons -c`, no `Remove-Item bin/`, keep `.sconsign*` + `.scons_env.json`. Build cache never cleared.
2. NEVER modify outside `modules/goblin/` without explicit permission. Seem to need upstream edits? STOP, propose override. Exception: new additive feature modules in `modules/<name>/` (ADR 0008) — self-contained, no upstream file touched.
3. NEVER disable modules / change build flags without permission. `DISABLE_MODULES` + build command fixed.
4. Core file overrides only via `goblin_add_library()` in `modules/goblin/config.py`. No other hacks.

## Thinking

- Duty: project quality, correctness, maintainability. Not agreement.
- User statements = hypotheses. Test against code/docs.
- Weak/underspecified/risky request -> say plainly + why.
- Endorse after strongest counter-argument, not before.
- No praise without concrete reasoning.
- Evidence insufficient? Say so. Name what would change conclusion.
- Objection overridden? Keep on record: "Accepted Risk: concern -> consequence -> why proceeding".
- Label claims: verified (in code/build) / inferred / assumed. Never present inference as fact.
- Never claim success without build. Report unverified gaps.

## Naming

- Fork code: upstream Godot conventions. No `Goblin*`/`goblin_*` prefixes on classes, methods, files.
- Wrong: `GoblinVariant`, `GoblinArray`, `goblin_validate()`.
- Right: upstream names (`GDScriptDataType::validate()`, `append_datatype()`).
- `Goblin` prefix allowed only: build/branding artifacts (`bin/goblin.*.exe`, `modules/goblin/`, hooks `goblin_add_library`, `GOBLIN_MODULE_OVERRIDES`). No `Goblin*` housekeeping singletons anymore (ADR 0007 deleted `GoblinBranding`/`GoblinExportTweaks`).

## Overrides (summary)

1. Module override: `GOBLIN_MODULE_OVERRIDES` in `modules/goblin/SCsub`. Whole module -> goblin copy. Current: `gdscript`.
2. Core file override: `goblin_add_library()` hook in `modules/goblin/config.py`. Single .cpp swap. Current: `variant_construct.cpp`.
3. Builder patch: `configure()` in `modules/goblin/config.py`. Build-time generators + binary rename.
4. Build-time option injection: module-level ARGUMENTS mutation in `modules/goblin/config.py` (import time, first module loop, before the first `opts.Update`) — sets `module_*_enabled=no` for `DISABLE_MODULES`. Used for: the module trim gate (ADR 0012). User CLI wins; custom.py / profile files are beaten (args layer precedence). Regression canary: configure() print.

Procedures: load `overrides` skill.

## Checklist (before done)

- `Goblin*`/`goblin_*` naming on code? -> rename to upstream style.
- Upstream file touched? -> revert, use override.
- `scons -c` / cleaned bin/? -> violation. Stop.
- Wrong mechanism (module copy for 1 file)? -> use core swap.
- Mirrored file drifted from upstream without port note? -> sync (`porting` skill).
- Architecture/feature changed but `modules/goblin/docs/` stale? -> update docs (`CODE_MAP.md`, `gdscript_features.md`).
- Locked decision changes a principle/hierarchy/non-negotiable but `.kilo/rules/master_prompt.md` not updated? -> update it (living charter, architect maintains).
- Work not reflected in `backlog.md` (new/done/rejected)? -> update backlog.

## Orientation

- Vision + decision hierarchy: `.kilo/rules/master_prompt.md`.
- Tickets: `modules/goblin/docs/backlog.md` - check before starting work, update after (new/done/rejected). Detailed specs in `modules/goblin/docs/plans/`.
- Code map: `modules/goblin/docs/CODE_MAP.md` - read before implementing, update after.
- Build + verify: `build` skill.
- Port upstream: `porting` skill.
- Workflows: `/report` (status), `/feature-review` (implementation review), `/tech-debt-review` (debt scan).
- Docs: `modules/goblin/docs/`, `modules/goblin/INDEX.md`.
- Goblin copy = source of truth. Read it, not this file.
