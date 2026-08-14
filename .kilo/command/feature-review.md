---
description: Implementation review of a feature or code area - correctness, spec compliance, edge cases, integration with other fork features. Review only; no code changes, no builds.
agent: architect
---
# Feature Review

Review the CURRENT implementation of a feature/area for potential issues: correctness, spec compliance, edge cases, integration. Distinct from `/tech-debt-review` (recent-changes debt scan) and `/report` (status).

## Scope

- `$1` = feature or area (e.g. `shaped dictionaries`, `then/elthen`, `unions`), a path (e.g. `modules/goblin/modules/gdscript/gdscript_analyzer.cpp`), or empty = the highest-priority `doing`/`todo` item in `backlog.md` that has implementation.

## Steps

1. Find the spec: `modules/goblin/docs/plans/` (if the feature has one), `docs/backlog.md` ticket, `docs/adr/` (structural), `docs/gdscript_features.md` (language features).
2. Identify implementation files: `docs/CODE_MAP.md` fast-lookup; diff vs upstream for language features (`git diff --no-index modules/gdscript modules/goblin/modules/gdscript`).
3. Review against checks:
   - **Spec compliance**: locked semantics implemented exactly as documented?
   - **Correctness**: edge cases - empty/null/nested values, constant folding, error messages, and interactions with OTHER fork features (unions x shaped dicts x @private x then/elthen). Non-regression of upstream behavior.
   - **Integration**: DB corpus + test gate plausible? New tests present and meaningful? `.out` files verified (`--gdscript-generate-tests`)?
   - **VM/runtime**: opcode safety (stack discipline, bounds), hot-path impact, ABI constraints (`gdscript.h` layout must stay identical to upstream).
   - **Upstream drift**: divergence surface growing unnecessarily? Rebase risk?
4. Classify each finding: **CRITICAL** (breaks correctness/compat) / **MAJOR** (bug or spec deviation) / **MINOR** (edge case, hardening) / **NIT** (style). Label verified (in code) or inferred (needs runtime test) - never present inference as fact.
5. Classify origin: **FORK** (ours - fix) / **UPSTREAM** (Godot behavior - report to user, do not fix).

## Output

- Verdict: PASS / PASS-WITH-FIXES / FAIL (+ reason).
- Findings table: issue | file:line | severity | class | verified/inferred | recommendation.
- Critical/major fork findings -> new row in the matching `backlog.md` section (§1/§2/§3) with an appropriate ID. Upstream findings -> report only, no backlog.

## Rules

- Review only: no code changes, no builds, no cleaning.
- Hard rules in `.kilo/rules/rules.md` apply. Never touch upstream files.
