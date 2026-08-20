# ADR 0007: Branding — Replace Retry Loops with Compile-Time Overrides

Status: Accepted

Date: 2026-08-13

## Context

The fork's editor branding used two runtime singletons (`GoblinBranding`, `GoblinExportTweaks`) that polled for `SceneTree` availability with hardcoded 120-attempt retry loops, then patched the UI by scanning the live node tree on every `node_added` signal. This had three problems (fork plan §3):

- **Ugly and inefficient.** Each deferred retry burns an idle frame; tree scans run on every node addition and walk the entire UI subtree.
- **Fragile.** Behavior depended on runtime timing (dialog must exist, be inside the tree, and be visible when the scan runs).
- **Partial.** String changes were done via `TranslationServer` overrides with exact-key lookups, which never matched composed strings like `"%s - Godot Engine"` window titles or `"Godot Project Pack"` filters.

Meanwhile the source-override mechanism (ADR 0001, mechanism 2: `goblin_add_library()` in `modules/goblin/config.py`) already proved it can swap a single upstream `.cpp` at build time (`variant_construct.cpp`).

## Decision

Replace the runtime UI patching with **compile-time file overrides**, extending mechanism 2 from a core-only if-chain to a **library-scoped dict** `{lib: {stem: goblin_path}}` covering `core` and `editor`.

Four upstream editor files get goblin copies (under `modules/goblin/editor/overrides/`, a non-globbed subtree):

| File | Edits |
|------|-------|
| `editor/gui/editor_about.cpp` | Goblin title/contributor/third-party literals; Donors tab block deleted |
| `editor/export/project_export.cpp` (+ `.h`) | "Export With Debug" option shown/hidden per debug-template availability; missing-debug-template warning lines filtered; `Godot Project Pack`/`Godot executable` literals fixed |
| `editor/project_manager/project_manager.cpp` | Donate button creation + its theme-icon line removed |
| `editor/editor_node.cpp` | "Support Godot Development" menu item, shortcut registration, fund-page case, and both icon-refresh lines removed |

Supporting changes:

- The two singletons are **deleted**; `GOBLIN_BRANDING_RUNTIME_ENABLED` define removed (`GOBLIN_BRANDING_ENABLED` survives via the generated version header).
- `TranslationServer` overrides are **kept** as a fallback for strings in files we do not override (e.g. "About Godot..." shortcut name, Project Manager logo tooltip), relocated to `modules/goblin/editor/branding_translations.cpp`, trimmed of keys that are now compile-time literals.
- Icon assets `Godot.svg` and `TitleBarLogo.svg` (copies of the goblin `Logo.svg`) are registered via the existing module-icons mechanism (last-wins registration, zero C++ change).

Alternatives considered:

- **Keep runtime hooks only for menu/button removal** — rejected: preserves the mechanism the user requires dead.
- **Translation-blanking only** ("Support Godot Development" → "") — rejected: leaves clickable empty menu items pointing at Godot donation pages.
- **Accept the Godot items back** — rejected: regresses existing fork behavior.
- **Not overriding `editor_node.cpp`** — rejected: it is the only mechanism that removes the menu item; the diff is 6 small documented edits.

## Consequences

Positive:

- Zero runtime overhead and zero polling for branding; the About/export/PM behavior is decided at compile time.
- The override hook is now generic (dict-based), resolving backlog B-09.
- Compile-time literals fix strings the exact-key translation overrides never covered.

Negative / accepted risks:

- **Rebase tax**: `editor_node.cpp` (9664 lines) and `project_manager.cpp` are high-churn upstream files; each sync must re-apply 5-6 small documented edits. Mitigation: porting skill procedure, drift check (backlog B-10), and the diffs are tiny and mechanical.
- `project_export.h` is mirrored (+1 private method) — a new drift point.
- **macOS single-zip + custom-template debug detection quirks** are preserved verbatim from the runtime singleton (a `macos.zip` bundles debug+release yet reports no debug template; custom-template presets are ignored). Same behavior as before the change; the reference title targets Windows.
- `_open_donate_page()` and the `donate_btn` member stay as dead code in the `project_manager.cpp` mirror (header consistency without copying the 293-line header).
- `"%s - Godot Engine"` window titles and "Godot Version" strings remain Godot-branded (the exact-key overrides never matched them; no regression). Tracked as backlog B-11.
- The Donors tab is deleted rather than showing goblin donors — matches the previous runtime strip; `donors.gen.h` generation is retained for other consumers.
- SCU builds would silently skip the swap (stems wouldn't match) — not enabled; build gate proves the swap fires.

## Related Documents

- [ROADMAP.md](../ROADMAP.md) — §3 limitations, §4 architecture, "Eliminating the Retry Loops"
- [backlog.md](../backlog.md) — B-04, B-09, B-10, B-11
- [CODE_MAP.md](../CODE_MAP.md) — override hook, editor overrides
