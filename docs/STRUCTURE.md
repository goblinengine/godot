# Goblin Engine — Module Structure

This module mirrors Godot's source tree so that each override is immediately obvious: a file at `modules/goblin/<path>/<file>` overrides the upstream file at `<path>/<file>`.

## Directory Layout

```
modules/goblin/
├── config.py                    # configure() hook — runs before any SCsub
│   │                            #   · monkey-patches builders (branding)
│   │                            #   · goblin_add_library() — library-scoped file swaps
│   │                            #   · DISABLE_MODULES — module trim (import-time ARGUMENTS injection, ADR 0012; canary print in configure)
│   └── (registers the add_program/add_library/add_shared_library hooks)
│
├── SCsub                        # Main build script
│   │                            #   · builds goblin-owned sources
│   │                            #   · GOBLIN_MODULE_OVERRIDES — whole-module swaps
│   └── (chains core/SCsub, editor/SCsub)
│
├── goblin_builders.py           # Branding builders (delegate to originals where possible)
├── goblin_manager.py            # Utility for inspecting/verifying the module
├── register_types.cpp/h         # Module registration (register_branding_translations)
│
├── core/                        # Mirrors upstream core/ — single-file overrides
│   ├── SCsub
│   ├── AUTHORS.md               # → core/authors.gen.h
│   ├── DONORS.md                # → core/donors.gen.h
│   ├── COPYRIGHT.txt + LICENSE.txt  # → core/license.gen.h
│   ├── version_override.py      # → core/version_generated.gen.h
│   └── variant/
│       ├── variant_construct.cpp  # overrides core/variant/variant_construct.cpp
│       └── variant_construct.h
│
├── modules/                     # Mirrors upstream modules/ — whole-module overrides
│   └── gdscript/                # The GDScript fork (compiled instead of modules/gdscript/)
│       ├── config.py, SCsub, register_types.*
│       ├── gdscript_parser/analyzer/compiler/vm/... (forked + modified)
│       └── tests/
│
├── editor/                      # Editor branding
│   ├── branding_translations.cpp/h  # Runtime translation fallback (ADR 0007)
│   ├── icons/                   # Logo.svg, Godot.svg, TitleBarLogo.svg overrides
│   └── overrides/               # Editor file mirrors — swapped by goblin_add_library()
│       ├── gui/editor_about.cpp        # About dialog (Goblin literals, no Donors tab)
│       ├── export/project_export.{cpp,h}  # Export dialog (debug-template-aware option)
│       ├── project_manager/project_manager.cpp  # Project Manager (no Donate button)
│       └── editor_node.cpp              # EditorNode (no Support Godot Development item)
│
├── main/                        # Splash / app icon overrides
│   ├── splash.png, splash_editor.png, app_icon.png
│
├── platform/windows/            # Platform-specific overrides (goblin.rc)
├── tools/                       # sync_godot_icons.py and other utilities
└── docs/                        # All documentation (adr/, rfc/, plans/, backlog, vision, ROADMAP)
```

## The Three Override Mechanisms

### 1. Module Directory Override (whole module)

In `SCsub`:

```python
GOBLIN_MODULE_OVERRIDES = {
    "gdscript": os.path.join(goblin_module_path, "modules", "gdscript"),
}
for _mod_name, _goblin_path in GOBLIN_MODULE_OVERRIDES.items():
    if os.path.isdir(_goblin_path) and _mod_name in env.module_list:
        env.module_list[_mod_name] = os.path.abspath(_goblin_path).replace("\\", "/")
```

SCons compiles each enabled module from the path in `env.module_list`. Replacing the `gdscript` entry redirects the ENTIRE module to the goblin copy. Upstream `modules/gdscript/` still exists for reference but is never compiled.

**Use when:** forking many files of one module (the GDScript language fork).

### 2. Core/Editor File Override (single .cpp swap)

In `config.py` → `goblin_add_library()`:

```python
_GOBLIN_FILE_OVERRIDES = {
    "core":   {"variant_construct": os.path.join(_goblin_dir, "core", "variant", "variant_construct.cpp")},
    "editor": {"editor_about":    os.path.join(_goblin_dir, "editor", "overrides", "gui", "editor_about.cpp"),
               "project_export":  os.path.join(_goblin_dir, "editor", "overrides", "export", "project_export.cpp"),
               "project_manager": os.path.join(_goblin_dir, "editor", "overrides", "project_manager", "project_manager.cpp"),
               "editor_node":     os.path.join(_goblin_dir, "editor", "overrides", "editor_node.cpp")},
}
# hook: lib_name = str(program).replace("#bin/obj/", "").split(".", 1)[0]
# for each source: stem = basename(source) minus extension; if stem in overrides[lib_name],
# replace with self_env.Object(goblin_path) before the library captures its sources.
```

`configure()` runs before any SCsub and monkey-patches `env.add_library`. When `core/SCsub` creates the core static library (or `editor/SCsub` the editor library), the hook swaps the matching `Object` nodes before the library captures its source list. The goblin `.obj` lands in the library; the original is never compiled.

**Placement rules for editor mirrors** (ADR 0007):
- Mirrors live under `modules/goblin/editor/overrides/` — a subtree that `modules/goblin/editor/SCsub`'s non-recursive `*.cpp` glob never sees, so swapped sources are not double-compiled.
- Unmodified headers stay upstream; rewrite the bare own-header include in the mirror `.cpp` to a root-relative path (`#include "editor/gui/editor_about.h"`). `project_export.h` is the one exception: it is copied because it gains a fork-only private method.

**Use when:** overriding one or two files surgically in `core` or `editor`. Add entries to the dict (ADR 0007 resolved B-09: the hook is library-scoped, no more if-chains).

### 3. Builder Monkey-Patching (build-time generator)

In `config.py` → `configure()`:

```python
core_builders.version_info_builder = goblin_builders.goblin_version_info_builder
main_builders.make_splash = goblin_builders.goblin_splash_builder
# ... authors, donors, license, app icon, editor splash
```

Also wraps `add_program` / `add_library` / `add_shared_library` to rename `godot.*` binaries to `goblin.*`.

**Use when:** replacing a build-time generator function (branding).

## Adding A New Override

1. Decide which mechanism fits (whole module → `GOBLIN_MODULE_OVERRIDES`; single file → `goblin_add_library()`; generator → `configure()`).
2. Mirror the upstream path under `modules/goblin/` (`core/`, `modules/`, `editor/overrides/`, `main/`, `platform/`, `scene/`, `servers/`, ...).
3. Wire it into the appropriate hook.
4. Document the mechanism in `.kilo/rules/rules.md` so the coder agent follows it.
5. Update [gdscript_features.md](gdscript_features.md) (if a GDScript change) or the relevant ADR.

## Building

```
scons platform=windows target=editor module_mono_enabled=no accesskit=no angle=no -j4
```

Output: `bin/goblin.windows.editor.x86_64.exe`. Never delete or clean `bin/`.

## Porting Across Engine Versions

When rebasing on a new stable release, diff:
- `modules/goblin/modules/gdscript/` against `modules/gdscript/`
- `modules/goblin/core/` against `core/`
- `modules/goblin/editor/overrides/` against `editor/` (mirrors: `gui/editor_about.cpp`, `export/project_export.{cpp,h}`, `project_manager/project_manager.cpp`, `editor_node.cpp`)

Track the divergence surface explicitly. See ADR 0002.

## Naming Conventions

- Engine features registered in ClassDB use **clean, descriptive Godot names**, not a `Goblin` prefix: `PortalSurface3D`, `PalettePostProcess`, `LightField`. This is a fork — the goblin module *is* the engine, so classes are named for what they do (like upstream `Light3D`, not `GodotLight3D`).
- Prefix only where a real ClassDB collision exists with an upstream class (e.g. `PortalSurface3D` vs upstream `Portal`/`Room` occlusion nodes).
- The `Goblin` prefix is allowed only for build/branding artifacts (`bin/goblin.*.exe`, `modules/goblin/`, hooks `goblin_add_library`, `GOBLIN_MODULE_OVERRIDES`). Housekeeping singletons are gone (ADR 0007 deleted `GoblinBranding`/`GoblinExportTweaks`).
- Follow Godot's own suffix conventions (`*3D` for spatial nodes, `*PostProcess` for post-effects).
