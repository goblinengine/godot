---
name: build
description: Build + verify Goblin Engine fork with SCons on Windows. Load when compiling, verifying a change, or build problems.
---

# Build

## Command

`debug_symbols=yes` is REQUIRED for all test/diagnostic builds (a crash dump or live debugger is only usable when the PDB exists in `bin/`). The PDB is written next to the exe (`bin/goblin.windows.editor.x86_64.pdb`).

```
scons platform=windows target=editor module_mono_enabled=no accesskit=no angle=no debug_symbols=yes -j4
```

Small changes, faster:

```
scons platform=windows target=editor module_mono_enabled=no accesskit=no angle=no debug_symbols=yes -j4 --max-drift=1 --implicit-deps-unchanged
```

Output: `bin/goblin.windows.editor.x86_64.exe` (+ `.console.exe`).

## Never clean

- No `scons -c`. No deleting in `bin/`, `.sconsign*`, `.scons_env.json`, stale `.obj`.
- Incremental: re-run scons. Recompiles only changed.

## Verify

1. Build editor -> binary exists (+ PDB: `bin/goblin.windows.editor.x86_64.pdb`).
2. `bin/goblin.windows.editor.x86_64.exe --path <project>`
3. GDScript changes: editor output -> parser/analyzer errors? Exercise feature in project.

## Diagnose

- Crash dump: WER writes `%LOCALAPPDATA%\CrashDumps\goblin.windows.editor.x86_64.exe.*.dmp`; symbolize with `WinDbgX -z <dump> -c '.ecxr; kn; q'` and `_NT_SYMBOL_PATH` pointing at `bin/`.
- Heap corruption (0xC0000374 / 0xC0000005 in ntdll): enable `appverif -enable Heaps -for goblin.windows.editor.x86_64.exe` (elevated, once) — the corrupting write faults with a stack instead of failing later at a free.
- Mirrored class layout: a goblin mirror header must NOT change a class's size if an upstream TU instantiates it (`memnew` sizes by the TU's header) — keep added state in file-scope statics (B-14).

## Troubleshoot

- Swap not compiled? Check `goblin_add_library()` basename match. Never delete .obj to force rebuild.
- Rebuild reason: `scons --debug=explain`.
- Link errors: swap missing -> check hook + goblin file path.
