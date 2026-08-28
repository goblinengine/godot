---
name: overrides
description: The four override mechanisms of Goblin Engine + procedures for adding each kind. Load when making or planning any engine change.
---
# Override Mechanisms

All fork changes injected at build time via 4 mechanisms. Upstream files: never modified.

## 1. Module override (whole module)

Whole module compiled from goblin copy. Used when many files of one module change (current: gdscript).

How: SCons builds modules from `env.module_list`. `modules/goblin/SCsub` rewrites entry -> goblin copy. Upstream stays for reference, never compiled.

```python
GOBLIN_MODULE_OVERRIDES = {
    "gdscript": _os.path.join(goblin_module_path, "modules", "gdscript"),
}
for _mod_name, _goblin_path in GOBLIN_MODULE_OVERRIDES.items():
    if _os.path.isdir(_goblin_path) and _mod_name in env.module_list:
        env.module_list[_mod_name] = _os.path.abspath(_goblin_path).replace("\\", "/")
```

Add one: copy upstream module -> `modules/goblin/modules/<name>/`, add entry.

## 2. Core file override (single .cpp swap)

Replace 1-2 core files without copying core. Hook `goblin_add_library()` intercepts `env.add_library("core", ...)`, swaps Object node BEFORE library captures sources. Goblin .obj lands in `core.lib`; upstream never compiled.

```python
def goblin_add_library(self_env, program, source, **kw):
    ...
    if str(program).replace("#bin/obj/", "").startswith("core"):
        _goblin_src = os.path.join(_goblin_dir, "core", "variant", "variant_construct.cpp")
        if os.path.isfile(_goblin_src):
            _new_source = []
            for _s in source:
                if "variant_construct" in str(_s):
                    _new_source.append(self_env.Object(_goblin_src))  # swap!
                else:
                    _new_source.append(_s)
            source = _new_source
    return godot_methods.add_library(self_env, program, source, **kw)
```

## 3. Builder patch (`configure()` in config.py)

Build-time generators + binary rename. Used for: branding/version/splash/app-icon generation (ADR 0007), RES wrapping for the exe icon (B-16), splash_editor regeneration (B-15). Runs inside `configure()`; `env.AddMethod` shadow + `add_module_version_string` patches.

## 4. Build-time option injection (module trim gate, ADR 0012)

Module-level ARGUMENTS mutation in `modules/goblin/config.py` (import time, first module loop, before the first `opts.Update`) — sets `module_*_enabled=no` for `DISABLE_MODULES`. User CLI wins; custom.py / profile files are beaten (args layer precedence). Regression canary: `configure()` print.

## Picking a mechanism

| Change | Mechanism |
|---|---|
| Many files of one module | Module override |
| 1-2 core files | Core file override (swap) |
| Build-time generation / rename | Builder patch |
| Trim/disable modules | Build-time option injection |

## Rules

- Upstream files: never modified.
- Prefer additive overrides over replacement overrides (smaller merge surface).
- No clean in `bin/`. No `scons -c`. No build flag changes without permission.
