# Goblin Engine - Agent Rules

## NEVER delete, remove, or clean any file in `bin/`

Do not run `scons -c`, `Remove-Item bin/`, `rm -rf bin/`, or any equivalent. Do not delete `.sconsign*` or `.scons_env.json`. The build cache must never be cleared.

## NEVER modify files outside `modules/goblin/` without explicit permission

Upstream Godot source files in `core/`, `modules/gdscript/`, `platform/`, `thirdparty/`, etc. must not be touched. If a change requires modifying an upstream file, ask first and propose alternatives.

## NEVER disable modules or change build flags without explicit permission

The module trim list in `modules/goblin/config.py:DISABLE_MODULES` and the build command flags are fixed. Do not add or remove modules or flags.

## Core file overrides go through `goblin_add_library()` in `config.py`

The only approved mechanism for replacing a core source file is the `add_library` hook in `modules/goblin/config.py:goblin_add_library()`. No other monkey-patches, env wrappers, or SCons hacks.

---

# Project Structure - Goblin Engine

Goblin Engine is a fork of Godot Engine that tracks upstream stable releases. ALL changes live inside `modules/goblin/` - upstream files are never modified. Changes are injected at build time via three mechanisms described below.

```
godot/                              # Upstream Godot repo (NEVER modified)
+-- core/                           # Upstream core (variant/, object/, math/ ...)
+-- modules/                        # Upstream modules
|   +-- gdscript/                   # Upstream GDScript (NOT compiled - overridden)
|   +-- goblin/                     # ? THE GOblin MODULE - all changes live here
|       +-- config.py               # configure() hook: monkey-patches builders,
|       |                           #   add_library hook, DISABLE_MODULES trim
|       +-- SCsub                   # Main goblin build script
|       +-- goblin_builders.py      # Branding builders (version, splash, icons)
|       +-- register_types.cpp/h    # Module registration
|       +-- core/                   # Mirrors upstream core/ for file overrides
|       |   +-- variant/            #   variant_construct.cpp/h (String ctors)
|       +-- modules/                # Mirrors upstream modules/ for module overrides
|       |   +-- gdscript/           #   ? The Goblin GDScript fork (compiled instead
|       |                           #     of upstream modules/gdscript/)
|       +-- editor/                 # Editor branding overrides
|       +-- main/                   # Splash/app icon overrides
|       +-- platform/windows/       # Platform overrides if ever needed
|       +-- docs/                   # Fork plan, structure docs
|       +-- INDEX.md                # Documentation index
+-- bin/                            # ? BUILD OUTPUT - NEVER DELETE OR CLEAN
+-- .kilo/                          # Kilo config (rules, agents, kilo.jsonc)
```

# The Three Override Mechanisms

## 1. Module Directory Override (whole module replacement)

In `modules/goblin/SCsub`:

```python
GOBLIN_MODULE_OVERRIDES = {
    "gdscript": _os.path.join(goblin_module_path, "modules", "gdscript"),
}
for _mod_name, _goblin_path in GOBLIN_MODULE_OVERRIDES.items():
    if _os.path.isdir(_goblin_path) and _mod_name in env.module_list:
        env.module_list[_mod_name] = _os.path.abspath(_goblin_path).replace("\\", "/")
```

**How it works:** SCons builds each enabled module from the path in `env.module_list`. By replacing the `gdscript` entry with `modules/goblin/modules/gdscript/`, the ENTIRE GDScript module is compiled from the goblin copy instead of upstream. The upstream copy still exists for reference but is never compiled.

**Use when:** Modifying many files of one module (e.g. the GDScript language fork: unions, @private annotation, analyzer, compiler, VM).

**To add a new module override:** copy the upstream module dir into `modules/goblin/modules/<name>/`, add an entry to `GOBLIN_MODULE_OVERRIDES`.

## 2. Core File Override (single .cpp swap at library creation)

In `modules/goblin/config.py` ? `goblin_add_library()`:

```python
def goblin_add_library(self_env, program, source, **kw):
    ...
    if str(program).replace("#bin/obj/", "").startswith("core"):
        _goblin_src = os.path.join(_goblin_dir, "core", "variant", "variant_construct.cpp")
        if os.path.isfile(_goblin_src):
            _new_source = []
            for _s in source:
                if "variant_construct" in str(_s):
                    _new_source.append(self_env.Object(_goblin_src))   # swap!
                else:
                    _new_source.append(_s)
            source = _new_source
    return godot_methods.add_library(self_env, program, source, **kw)
```

**How it works:** `configure()` runs before any SCsub. It monkey-patches `env.add_library`. When `core/SCsub` creates the core static library (`env.add_library("core", env.core_sources)`), the hook intercepts and swaps the Object node for `variant_construct.cpp` with one built from the goblin copy. The swap happens BEFORE the library captures its source list, so the goblin .obj lands in `core.lib` and the original source is never compiled.

**Use when:** Overriding one or two core files without replacing the whole core. File copies live in `modules/goblin/core/<mirror path>/`.

**To add a new file override:** copy the upstream file to `modules/goblin/core/<path>/<file>`, then extend the `goblin_add_library()` hook (or generalize it into a dict of `{basename: goblin_path}`).

## 3. Builder Monkey-Patching (function-level replacement)

In `modules/goblin/config.py` ? `configure()`:

The hook imports `core_builders` / `main_builders` BEFORE `goblin_builders`, saves originals, then replaces them:

```python
core_builders.version_info_builder = goblin_builders.goblin_version_info_builder
main_builders.make_splash = goblin_builders.goblin_splash_builder
...
```

Also wraps `add_program` / `add_library` / `add_shared_library` to rename `godot` binaries to `goblin`.

**Use when:** Replacing a specific build-time generator function (branding: version header, splash image, app icon, authors/license files).

---

# Building

The canonical build command (do not change flags without permission):

```
scons platform=windows target=editor module_mono_enabled=no accesskit=no angle=no -j4
```

Optional faster incremental: add `--max-drift=1 --implicit-deps-unchanged`.

Output binaries: `bin/goblin.windows.editor.x86_64.exe` (and `.console.exe`).

**Incremental builds work normally.** After changing goblin sources, just re-run scons - it recompiles only what changed. NEVER clean.

# The GDScript Fork (modules/goblin/modules/gdscript/)

Features added vs upstream:

- **Union types**: `int | float`, `Dictionary | null` - parser (`parse_type`/`parse_type_single`), `DataType::UNION` kind, analyzer (`resolve_datatype`, `check_type_compatibility`, casts), compiler maps UNION ? runtime VARIANT.
- **`@private` annotation**: registered in parser, sets `is_private` on VariableNode/FunctionNode. Analyzer blocks external access (`Cannot access private member "X" of class "Y"`). Autocomplete filters private members from outside the declaring class (`p_recursion_depth > 0` check in `_find_identifiers_in_class`).
- **String constructors in Variant**: `String(int)`, `String(float)`, `String(bool)` registered via `VariantConstructorToString<T>` in the goblin `core/variant/variant_construct.{cpp,h}` (overridden through mechanism #2). Makes `1 as String` ? `"1"` work.

When porting across engine versions, diff `modules/goblin/modules/gdscript/` against `modules/gdscript/` and `modules/goblin/core/` against `core/`.

# Documentation

- `modules/goblin/docs/GOBLIN_FORK_PLAN.md` - vision, ADRs, phased roadmap
- `modules/goblin/docs/STRUCTURE.md` - module layout (branding-focused, older)
- `modules/goblin/INDEX.md` - documentation index