# Goblin Engine - Editor Branding

This folder holds the editor-side branding of the fork.

## What lives here

| Path | Role |
|------|------|
| `branding_translations.cpp/h` | Runtime fallback: `TranslationServer` overrides that rebrand remaining "Godot" strings in editor files that are not overridden at compile time (e.g. "About Godot..." shortcut name, Project Manager logo tooltip). Registered at module init (EDITOR + SCENE levels), `TOOLS_ENABLED`-gated. |
| `icons/` | Editor icon overrides: `Logo.svg` = goblin banner at 187×76 (About dialog + credits roll), `Godot.svg` = goblin face at 16×16 (help-menu About item), `TitleBarLogo.svg` = face + "GOBLIN" wordmark at 100×24 (Project Manager title bar). **CRITICAL — no `<text>` elements**: ThorVG (Godot's SVG rasterizer) has no font loader, so `<text>` renders nothing — every letter must be path data (the wordmark paths come from `logo_outlined.svg`'s text group). **CRITICAL — balanced extraction**: when copying face/letter `<g>` groups out of the source SVGs, extract with a depth-counting parser, NOT a non-greedy regex (`<g id="goblin".*?</g>` stops at the first inner `</g>` and emits unbalanced tags — broke TitleBarLogo once, B-14). **Fixed-size requirement**: the source SVGs use `width="100%"`; the editor icon generator rasterizes at the SVG's intrinsic size, so an un-resized banner renders at 1024px and fills the dialog. Registered via `config.get_icons_path()` at configure time — do NOT move registration into an SCsub: SConstruct builds `editor/icons/SCsub` before `modules/SCsub`, so a late append never applies (B-12). |
| `platform/windows/` | `goblin.rc` + `goblin_res_wrap.rc` (exe icon + version info, compiled instead of upstream `godot_res.rc` via the RES wrapper in config.py) + `goblin.ico` (generated at build time from `main/app_icon.png`, B-16). |
| `overrides/` | **Editor file mirrors** swapped in at build time by `goblin_add_library()` (ADR 0007): `gui/editor_about.cpp`, `export/project_export.{cpp,h}`, `project_manager/project_manager.cpp`, `editor_node.cpp`. This subtree must NEVER be globbed by SCsub (the module's `*.cpp` glob is non-recursive — keep mirrors in subdirectories). |

## What the overrides change

- **About dialog** (`overrides/gui/editor_about.cpp`): Goblin literals ("Thanks from the Goblin community!", "© 2007-present Goblin & Godot contributors.", "Goblin Engine relies on..."); Donors tab removed.
- **Export dialog** (`overrides/export/project_export.{cpp,h}`): "Export With Debug" file-dialog option shown only when a debug export template exists for the current platform; missing-debug-template warning lines filtered; "Goblin Project Pack" / "Goblin executable" literals.
- **Project Manager** (`overrides/project_manager/project_manager.cpp`): Donate button removed (`_open_donate_page` + member stay as dead code for header consistency).
- **EditorNode** (`overrides/editor_node.cpp`): "Support Godot Development" menu item, shortcut registration, fund-page case, and both icon-refresh lines removed.

## Notes

- Upstream files are never modified; the overrides are the only fork edits.
- On rebase, re-diff each mirror against upstream (`git diff --no-index editor/<file> modules/goblin/editor/overrides/<mirror>`); `project_export.h` is the only copied header (it carries the fork-only `_update_export_debug_option()`).
- Known branding gaps (composed strings the exact-key translation overrides never matched) are tracked as backlog B-11.
