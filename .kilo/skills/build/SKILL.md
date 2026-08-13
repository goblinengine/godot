---
name: build
description: Build + verify Goblin Engine fork with SCons on Windows. Load when compiling, verifying a change, or build problems.
---

# Build

## Command

```
scons platform=windows target=editor module_mono_enabled=no accesskit=no angle=no -j4
```

Small changes, faster:

```
scons platform=windows target=editor module_mono_enabled=no accesskit=no angle=no -j4 --max-drift=1 --implicit-deps-unchanged
```

Output: `bin/goblin.windows.editor.x86_64.exe` (+ `.console.exe`).

## Never clean

- No `scons -c`. No deleting in `bin/`, `.sconsign*`, `.scons_env.json`, stale `.obj`.
- Incremental: re-run scons. Recompiles only changed.

## Verify

1. Build editor -> binary exists.
2. `bin/goblin.windows.editor.x86_64.exe --path <project>`
3. GDScript changes: editor output -> parser/analyzer errors? Exercise feature in project.

## Troubleshoot

- Swap not compiled? Check `goblin_add_library()` basename match. Never delete .obj to force rebuild.
- Rebuild reason: `scons --debug=explain`.
- Link errors: swap missing -> check hook + goblin file path.
