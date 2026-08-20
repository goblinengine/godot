# ADR 0014: Fork Documentation at the Repo Root (`docs/`)

Status: Accepted

Date: 2026-08-20

## Context

Before ADR 0013, `modules/goblin` was a standalone submodule repo; its `docs/` tree
(backlog, ROADMAP, CODE_MAP, gdscript_features, ADRs, RFCs, plans) lived inside the module so
it versioned with the goblin repo. ADR 0013 inlined the override layer: the entire fork is now
one repository, and the docs-versioning rationale is gone.

Two properties make `modules/goblin/` the wrong home for the docs in the new model:

- `modules/goblin/` is the pseudo-engine-root mirror of upstream paths (`core/`, `scene/`,
  `editor/`, ...) for override placement. The docs mirror nothing upstream — they are fork
  documentation and do not follow that rule.
- ADR 0008 already constrains `modules/goblin/` to "override machinery, branding, and fork
  documentation only" — mixing documentation with the override mirror keeps the module
  cluttered and hides the docs one level down.

Every agent rule, skill, and workflow (`rules.md`, `developer.md`, `architect.md`,
`/report`, `/feature-review`, `/tech-debt-review`) referenced the docs path.

## Decision

Move the fork documentation from `modules/goblin/docs/` to the repo root `docs/`. All fork
docs live there: `backlog.md`, `ROADMAP.md`, `CODE_MAP.md`, `gdscript_features.md`,
`STRUCTURE.md`, `genre-coverage.md`, `cut-upscalers.md`, `LIGHTMAP_INVESTIGATION.md`,
`BRANDING_STATUS.md`, `adr/`, `rfc/`, `plans/`, `README.md`.

`modules/goblin/` keeps only the override layer and branding (plus its own `INDEX.md`,
which now links to `../docs/`). All path references were updated in `.kilo/rules/rules.md`,
`.kilo/agents/*`, `.kilo/command/*`, the docs' internal cross-links, and `INDEX.md`.
`rules.md` hard rule 2 gains an exception for editing root `docs/` (fork documentation).

## Consequences

Positive:

- Top-level `docs/` is discoverable and conventional (sibling to upstream `doc/`, which stays
  the class-reference tree).
- `modules/goblin/` is purely the override mirror + branding — clean for porting and
  `goblin_add_library` mirror-drift discipline.
- Single-repo: `git log -- docs/` gives the docs history; no more path ambiguity.

Negative / risks:

- A future `git subtree split --prefix=modules/goblin` would no longer capture the docs.
  Accepted: the goblin repo is archived (ADR 0013); re-extraction would not need the docs.
- One-time reference churn across `.kilo/` and doc-internal links. Done once, verified by
  grep for stale `modules/goblin/docs` references (only the historical de-submodule plan file
  keeps them, as a record).
