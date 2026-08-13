---
description: Generate project status report - backlog state, recent changes, build state, blockers. Read-only; no file writes, no builds.
agent: architect
---
# Project Report

Generate a terse status report for Goblin Engine.

## Steps

1. Read `modules/goblin/docs/backlog.md`. Summarize per section: counts per status (`done` / `doing` / `todo` / `rejected`). List all `doing` items + all `P0`/`P1` `todo` items.
2. Recent changes: `git log --oneline -20 -- modules/goblin/ .kilo/` (or since `$1` if a date/range is given). Summarize what landed.
3. Build state: compare `bin/goblin.windows.editor.x86_64.exe` LastWriteTime against newest source mtime under `modules/goblin/`. Binary older than source edits? Say "stale build".
4. Blockers / risks: from backlog notes, `blocked` items, upstream issues referenced.

## Output

- Sections: Doing / Recently done / Up next / Build / Risks. <=40 lines. Terse, no filler.
- Do NOT modify any file. Do NOT build. Do NOT clean.

## Rules

- Hard rules in `.kilo/rules/rules.md` apply.
