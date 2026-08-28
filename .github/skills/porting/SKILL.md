---
name: porting
description: Procedure for porting upstream Godot commits into the Goblin Engine fork. Load when pulling upstream changes, rebasing onto newer Godot, updating fork.
---
# Porting Upstream Godot Changes

Fork tracks upstream stable. Upstream changes -> goblin copies absorb delta, keep fork features.

## Procedure

1. Identify upstream change (commit / version range).
2. Measure divergence. Mirrors:
   - `modules/goblin/modules/gdscript/` <-> `modules/gdscript/`
   - `modules/goblin/core/` <-> `core/`

   Diff mirrors vs upstream:
   ```
   git diff upstream/4.7 -- modules/goblin/modules/gdscript modules/gdscript
   ```
3. Apply upstream change to goblin copy, NOT upstream. Preserve fork features.
4. Re-check: step-2 diff -> only fork changes remain.
5. Build + verify (`build` skill) -> run test project.

## Rules

- Upstream stays untouched.
- Keep divergence surface small. Prefer upstream structure.
- Heavy fork changes + upstream conflict -> escalate to architect.
