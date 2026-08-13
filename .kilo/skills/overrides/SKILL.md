---
name: overrides
description: The three override mechanisms of Goblin Engine + procedures for adding each kind. Load when making or planning any engine change.
---

# Override Mechanisms

All fork changes injected at build time via 3 mechanisms. Upstream files: never modified.

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

Add one: copy upstream file -> `modules/goblin/core/<path>/<file>`, extend hook (prefer dict `{basename: goblin_path}` over if-chains).

## 3. Builder patch (function-level)

Replace build-time generator (version header, splash, icons, authors/license) + rename godot -> goblin binaries. Done in `configure()`: import upstream builders, save originals, replace:

```python
core_builders.version_info_builder = goblin_builders.goblin_version_info_builder
main_builders.make_splash = goblin_builders.goblin_splash_builder
...
```

Add one: implement in `goblin_builders.py`, assign in `configure()`.

## Choose mechanism

- Many files of one module -> 1.
- 1-2 core files -> 2.
- Build-time generator -> 3.
- No fit -> extend existing mechanism. New mechanism? Document in `.kilo/rules/rules.md`.
