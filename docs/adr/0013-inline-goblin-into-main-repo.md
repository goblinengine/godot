# ADR 0013: De-Submodule — Inline the Goblin Override Layer into the Main Repository

Status: Accepted

Date: 2026-08-20

## Context

`modules/goblin` was a git submodule pointing at `https://github.com/goblinengine/goblin`
(~47 commits). It held ALL fork-specific code: the whole-module gdscript override, the
single-file core/editor/drivers/servers/scene mirrors, branding/icons, and the entire fork
documentation tree (backlog, plans, ADRs, RFCs, CODE_MAP, gdscript_features). The main repo
(goblinengine/godot) is the full Godot 4.7.x tree plus standalone additive modules
(`modules/sim/`, `modules/betsy/`, ...) per ADR 0008.

The boundary was failing in ways that cost correctness, not just convenience:

- **Stale-pointer hazard (live).** The recorded gitlink (`cceaa4c`) was 5 commits behind the
  checked-out goblin main (`12bfd67`), which carried the entire dictionary-schema feature.
  Any fresh clone or CI checkout built a goblin missing that work. Inline directories cannot
  desynchronize from their content.
- **Porting measurement was a silent no-op.** The ADR 0002 divergence-measurement command
  (`git diff upstream/4.7 -- modules/goblin/modules/gdscript modules/gdscript`) returns empty
  output because git cannot descend into a gitlink pathspec — the fork's core
  divergence-tracking loop was blind to the override layer.
- **Two-repo overhead, zero used capability.** Every goblin change required a commit in the
  submodule repo plus a pointer-bump commit in the fork (e.g. `6e189a9dd0`, `059527e3bb`).
  Nothing consumed the goblin repo independently: the fork only pinned a pointer, and the
  `ecs` branch work was wanted in `main` anyway.

Every ADR (0001, 0002, 0003, 0007, 0008, 0009, 0012), `rules.md`, `master_prompt.md`, and the
override/porting skills reference `modules/goblin/` as a **directory**, never as a repository
boundary. The build integration is path-relative throughout (`os.path.dirname(__file__)` in
`config.py`, `Dir(".").abspath` in `SCsub`). Upstream has no `modules/goblin/` path, so
rebases onto `upstream/4.7` cannot collide with it.

## Decision

De-submodule: `modules/goblin` becomes a plain tracked directory in the main repo. The goblin
history is grafted into the fork's ancestry with `git subtree add --prefix=modules/goblin`
(merge commit `698bcb1d33`), preserving all commits, messages, authors, and dates. The `ecs`
branch (single commit `4248a68`, EntityNode/Component/Registry first pass — see backlog D-20)
was fast-forwarded into goblin `main` before the graft, so the inlined tree equals goblin
`main` at `4248a68`.

Executed: `.gitmodules` and the gitlink removed; orphaned `submodule.modules/daslang` and
`submodule.modules/gdscript2` config sections dropped; `submodules: recursive` removed from
the 5 checkouts in the fork-owned `.github/workflows/goblin_builds.yml` (upstream workflows
untouched — with `.gitmodules` gone their `submodules: recursive` is a harmless no-op).
`goblinengine/goblin` is archived on GitHub (`ecs` branch preserved); a mirror backup of the
repo remains at the migration time outside the tree.

All override mechanisms are unchanged: `GOBLIN_MODULE_OVERRIDES`, `goblin_add_library()`
library-scoped dict, builder monkey-patching, and the module-trim ARGUMENTS injection
(ADR 0001/0007/0012) now source from the inlined directory.

## Consequences

Positive:

- Single coherent history: `git log -- modules/goblin` shows real goblin commits (previously
  only pointer bumps); the ADR 0002 divergence-measurement command works; no stale-pointer
  class of bugs can exist.
- Consistent with ADR 0008: the fork's entire divergence surface now versions together.
- Reversible: `git subtree split --prefix=modules/goblin -b goblin-restored` regenerates the
  standalone repo; the archived GitHub repo remains as a second copy.
- Reference-title gate: fresh clones and CI build the intended goblin without `--recursive`.

Negative / risks:

- Fork history now mixes goblin commits with upstream merges. Mitigation: path-based
  separation (`git log -- modules/goblin`) — the standard Godot-fork pattern.
- No independent versioning of the override set. Accepted: pointer bumps were ad hoc anyway
  (the stale-pointer incident is the proof).
- The goblin repo's own `README.md`, `.gitignore`, `.github-workflow-example.yml`, and root
  logos merged into the fork as dead weight. Accepted: keeps the graft faithful; optional
  later prune.
- A third inline↔submodule flip is forbidden without a new ADR superseding this one.
