# ADR 0012: Build-Time Option Injection (module trim gate)

Status: Accepted

Date: 2026-08-16

## Context

B-01's module trim never engaged: `env.disabled_modules` is not a compile gate (the real gate
is `module_*_enabled`, SConstruct:1113), and `configure()` (SConstruct:1124) runs after the
one `opts.Update` that decides the module variables — the trim candidates sorting before
"goblin" (astcenc, basis_universal, bmp, camera, csg, dds, etcpak, fbx, glslang, gltf) are
unreachable from `configure()`. All 30 "trimmed" modules compiled; B-01's `done` status was
false. The 30-module trim list and its "verified unused" evidence were validated against a
no-op build (evidence unvalidated).

## Decision

Module trim is applied by mutating `SCons.Script.ARGUMENTS` at module import time in
`modules/goblin/config.py` (first module loop, SConstruct:474, before `opts.Update` at
SConstruct:499): `module_<name>_enabled = "no"` for every `DISABLE_MODULES` entry the user did
not set on the CLI. `DISABLE_MODULES` remains the single source of truth. Precedence:
user CLI > injection > custom.py/profile files. The dead `env.disabled_modules` code
(config.py:244-248, incl. the latent set→list crash) is removed; `configure()` prints a canary
count (28/28 gated off) so a regression or a CLI override is visible in every build log. This
is the 4th override mechanism (rules.md + overrides skill updated).

## Alternatives rejected

- **B: repo-root `custom.py`** — outside `modules/goblin/` (hard rule 2 requires explicit
  permission), a second source of truth that can drift from `DISABLE_MODULES`, and still
  beaten by ARGUMENTS (args layer > files layer in SCons `Variables.Update`).
- **C: duplicate `get_opts()` BoolVariables defaulting False** — covers all candidates, but
  duplicates option keys (Help() pollution, option-system abuse).
- **D: configure()-time env flags only** — insufficient alone (10/28 unreachable).

## Consequences

- Trim gates all modules regardless of alphabetical position; user CLI overrides respected
  (`module_x_enabled=yes` compiles the module — verified with the bmp counter-example).
- `Help()` defaults for trimmed modules are cosmetic-stale (show `yes`, injection wins) —
  accepted; fixing it would require option-system abuse (rejected C).
- A future repo-root `custom.py` re-enabling a trimmed module would be beaten by the
  injection — documented limitation; CLI remains the sanctioned override.
- Trim evidence must be re-validated against the corpus (plan gate 8): the original ADR 0003
  evidence was gathered against the no-op build. Related: ADR 0003 (amended).
- Upstream drift in the options flow (opts.Update/ARGUMENTS path) → injection silently
  no-ops → the configure() canary print (count < 28) catches it.

## Related Documents

- [0003-module-trim-evidence-standard.md](0003-module-trim-evidence-standard.md) (amended)
- [../plans/module-trim-fix-plan.md](../plans/module-trim-fix-plan.md)
- [backlog.md](../backlog.md) — B-01
