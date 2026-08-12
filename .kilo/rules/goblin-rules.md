# Goblin Engine — Agent Rules

## NEVER delete, remove, or clean any file in `bin/`

Do not run `scons -c`, `Remove-Item bin/`, `rm -rf bin/`, or any equivalent. Do not delete `.sconsign*` or `.scons_env.json`. The build cache must never be cleared.

## NEVER modify files outside `modules/goblin/` without explicit permission

Upstream Godot source files in `core/`, `modules/gdscript/`, `platform/`, `thirdparty/`, etc. must not be touched. If a change requires modifying an upstream file, ask first and propose alternatives.

## NEVER disable modules or change build flags without explicit permission

The module trim list in `modules/goblin/config.py:DISABLE_MODULES` and the build command flags are fixed. Do not add or remove modules or flags.

## Core file overrides go through `goblin_add_library()` in `config.py`

The only approved mechanism for replacing a core source file is the `add_library` hook in `modules/goblin/config.py:goblin_add_library()`. No other monkey-patches, env wrappers, or SCons hacks.
