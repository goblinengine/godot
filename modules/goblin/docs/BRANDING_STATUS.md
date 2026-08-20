# Goblin Branding - What Gets Renamed

> Status (2026-08-13): compile-time overrides are now the primary branding mechanism (ADR 0007). About dialog, export dialog, Project Manager donate button, and editor help menu are overridden at build time via `goblin_add_library()` mirrors in `modules/goblin/editor/overrides/`; remaining strings are rebranded at runtime by `branding_translations.cpp` translation overrides. This file documents what is renamed where.

## ✅ Renamed via Compile-Time Overrides (ADR 0007)

Editor files mirrored under `modules/goblin/editor/overrides/` and swapped at build time:

| File | What is renamed/removed |
|------|------------------------|
| `gui/editor_about.cpp` | "Thanks from the Goblin community!", "© 2007-present Goblin & Godot contributors.", "Goblin Engine relies on..."; Donors tab removed |
| `export/project_export.cpp` | "Goblin Project Pack" filters; "Goblin executable" tooltips; "Export With Debug" option only when a debug template exists; missing-debug-template warnings filtered |
| `project_manager/project_manager.cpp` | Donate button removed |
| `editor_node.cpp` | "Support Godot Development" menu item/shortcut removed |
| `editor/icons/Logo.svg` | About dialog + credits roll: face + "GOBLIN" + "Engine" as **white path letters** (187×76). `<text>` elements forbidden — ThorVG has no font loader (B-14) |
| `editor/icons/Godot.svg`, `TitleBarLogo.svg` | Help-menu About icon (face, 16×16) + PM title bar (face + "GOBLIN", 100×24) |
| `platform/windows/goblin.rc` + `goblin_res_wrap.rc` | Windows exe icon (build-time `goblin.ico` from `app_icon.png`) + version info ("Goblin Engine", goblin-engine.org) — compiled instead of upstream `godot_res.rc` (B-16) |

## ✅ Renamed via Runtime Translation Overrides (fallback)

`editor/branding_translations.cpp` rebrands remaining editor strings (e.g. "About Godot..." shortcut name, Project Manager logo tooltip, generic "Godot" → "Goblin" in translated UI text).

## ⚠️ Known Gaps (backlog B-11)

Composed strings the exact-key translation overrides never matched stay Godot-branded: `"%s - Godot Engine"` window titles, `"Godot Version"`, `"Godot Feature Profile"`. Decide later whether to override those files.

## ✅ Automatically Renamed (via GODOT_VERSION_NAME define)

These use the `GODOT_VERSION_NAME` define which is set to "Goblin Engine":

1. **Window Titles**
   - Project Manager: "Goblin Engine - Project Manager"
   - Editor: "Goblin Engine - [Scene Name]"

2. **Console Output**
   - Startup banner: "Goblin Engine v4.x.x"
   - All version printing

3. **File Dialogs**
   - Import filters: "Goblin Engine Project"

4. **Resource Files (.rc)**
   - File description: "Goblin Engine"
   - Product name: "Goblin Engine"

5. **Generated Shader Comments**
   - "// NOTE: Shader automatically converted from Goblin Engine..."

6. **OpenGL Profile**
   - Application profile name: "Goblin Engine"

## ✅ Renamed via Generated Headers

These are replaced by the goblin module's generated files:

1. **AUTHORS.md** → `core/authors.gen.h`
   - Author credits and contributor lists

2. **DONORS.md** → `core/donors.gen.h`
   - Sponsor and patron lists

3. **COPYRIGHT.txt + LICENSE.txt** → `core/license.gen.h`
   - Copyright notices and license text

4. **Splash Screens**
   - Runtime splash: `main/splash.gen.h` (regenerates on goblin `main/splash.png` change via Depends edges, B-15)
   - Editor splash: `main/splash_editor.gen.h` — upstream 4.7 removed the editor splash; the fork re-enables it (B-15): config.py strips `NO_EDITOR_SPLASH`, goblin SCsub generates the header from `modules/goblin/main/splash_editor.png`

5. **App Icon**
   - Window icon: `main/app_icon.gen.h`

6. **Editor Icons**
   - Logo in about screen (if you put Logo.svg in icons/ folder)
   - Any other SVG icons you add
