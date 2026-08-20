---
description: Tech-debt review of recent fork changes - scan modules/goblin for debt patterns, classify fork-debt vs upstream issues, update backlog. Review only, no code changes, no builds.
agent: architect
---
# Tech Debt Review

Review recent changes to `modules/goblin/` for accumulating tech debt. Focus on OUR changes. Upstream Godot bugs are reported to the user, not fixed here.

## Steps

1. Scope: last 15 commits touching `modules/goblin/` by default (`git log -15 --oneline -- modules/goblin/`). Override with `$1` (commit count or date range).
2. Diff the scope (`git diff <base> -- modules/goblin/`) and scan changed files for debt signals:
   - `// Goblin:` / `TODO` / `FIXME` / `HACK` / `XXX` comments
   - Naming violations: `Goblin*` / `goblin_*` on classes, methods, files (policy: `.kilo/rules/rules.md` Naming)
   - Workarounds for upstream bugs (upstream issue references, `workaround`, `hack`)
   - Polling / retry loops (attempt counters, SceneTree polling)
   - Features without tests (no new cases under the mirror `tests/`)
   - Docs not updated (`CODE_MAP.md`, `gdscript_features.md`, `backlog.md`)
   - Parallel systems / reinvented infrastructure (new tracking dicts, registries, loops where infrastructure exists)
   - Spaghetti: long functions, deep nesting, duplicated logic, pass-through wrappers
3. Classify each finding:
   - **FORK-DEBT** -> ours to fix. Add a row to `docs/backlog.md` §6 (Tech Debt), ID `TD-NN`, following the table format.
   - **UPSTREAM-ISSUE** -> Godot bug/limitation worked around. Do NOT fix, do NOT backlog. List in the report with the upstream reference (issue number / commit) so the user can decide whether it is critical.

## Output

- Findings table: item | file | class (FORK-DEBT / UPSTREAM-ISSUE) | severity (high/med/low) | action.
- Fork-debt severity high -> recommend fixing now; med/low -> backlog ticket.
- Update `backlog.md` §6 (create the section if missing) with fork-debt rows.

## Rules

- Review only: no code changes, no builds, no cleaning, no upstream file edits.
- Hard rules in `.kilo/rules/rules.md` apply.
