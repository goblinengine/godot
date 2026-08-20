# Code Map - Goblin Engine Fork

Navigation map for `modules/goblin/`. Read before implementing; update after changes.

Companion docs: `docs/gdscript_features.md` (feature semantics), `docs/ROADMAP.md` (roadmap), `docs/backlog.md` (work tracking), `.kilo/rules/` (constraints + vision).

## Layout

```
modules/goblin/
├── config.py            # Build hooks: configure(), goblin_add_library() + _GOBLIN_FILE_OVERRIDES, module trim
├── SCsub                # GOBLIN_MODULE_OVERRIDES (module swap, line 57)
├── goblin_builders.py   # Branding builders (version/splash/icons/authors/license)
├── register_types.cpp   # Module registration: calls register_branding_translations()
├── core/                # Core mirror - ONLY overridden files
│   ├── variant/variant_construct.{cpp,h}   # String ctors (core file override)
│   └── version_override.py                 # Branding metadata (name, website)
├── modules/gdscript/    # GDScript fork - compiled INSTEAD of upstream modules/gdscript/
├── editor/              # branding_translations.{cpp,h} (runtime translation fallback), icons/, README
│   └── overrides/       # Editor file mirrors - swapped by goblin_add_library(), NEVER globbed by SCsub
│       ├── gui/editor_about.cpp                  # About dialog (Goblin literals, no Donors tab)
│       ├── export/project_export.{cpp,h}         # Export dialog (debug-template-aware option, warning filter)
│       ├── project_manager/project_manager.cpp   # Project Manager (Donate button removed)
│       └── editor_node.cpp                       # EditorNode (Support Godot Development removed)
├── main/                # splash.png, splash_editor.png, app_icon.png
├── platform/windows/    # goblin.rc
├── docs/                # All fork docs (see docs/README.md)
└── goblin_manager.py    # DEV UTILITY - never run its `clean` command (hard rule 1)

modules/midi/            # STANDALONE additive feature module (ADR 0008) - see section below
```

## Build hooks (config.py)

| Symbol | What |
|---|---|
| `configure()` (:5) | Replaces upstream builders with goblin_builders, wraps add_program/add_library/add_shared_library (godot -> goblin rename), prints the module-trim canary (`28/28 modules gated off`, ADR 0012) |
| module-level trim code (import time) | Mutates `SCons.Script.ARGUMENTS` (first module loop, SConstruct:474, before `opts.Update` at 499): `module_<name>_enabled=no` per `DISABLE_MODULES` entry the user did not set on the CLI — the gate (SConstruct:1113) then skips them regardless of alphabetical position (ADR 0012; was a no-op via `env.disabled_modules` until 2026-08-16) |
| `goblin_add_library()` | Library-scoped file override: `_GOBLIN_FILE_OVERRIDES = {lib: {stem: goblin_path}}` for `core` + `editor`; swaps matching basename in the library source list BEFORE capture (ADR 0007 / B-09) |
| `get_icons_path()` | Editor icon overrides (`Logo.svg`, `Godot.svg`, `TitleBarLogo.svg`, ...) registered at configure time — MUST stay here, not in editor/SCsub: SConstruct collects `module_icons_paths` before `editor/icons/SCsub` runs, so a late append never applies |
| `can_build()` (:1) | Module enable check |

## GDScript fork (modules/gdscript/)

Whole-module override (ADR 0001). Mirror of upstream + fork features.
Diff vs upstream: `git diff --no-index --stat modules/gdscript modules/goblin/modules/gdscript` (60 files, +1623/-48).

### Pipeline stages and fork deltas

| Stage | File | Fork work |
|---|---|---|
| Tokenizer | gdscript_tokenizer.{h,cpp} | `then`/`elthen` tokens + keywords (full feature: parser/analyzer/compiler wired — see Features table) |
| Tokenizer buffer | gdscript_tokenizer_buffer.{h,cpp} | Save/restore support (parser lookahead) |
| Parser | gdscript_parser.{h,cpp} | `DataType::UNION` kind, `@private` annotation, shaped dict literals (`key: Type = value`), datatype shape, `@schema` annotation + schema datatype fields (`is_schema`/`schema_name`/`dictionary_shape_defaults`), `is_schema_constant()` helper |
| Analyzer | gdscript_analyzer.cpp | Union resolve/compat, private-access blocking, shape inference + entry-type refinement, schema const finalization + `Dictionary[Name]` resolution (local/member/registry) + literal override-merge (`merge_schema_dictionary`) |
| Compiler | gdscript_compiler.cpp | `OPCODE_CONSTRUCT_SHAPED_DICTIONARY` emit, UNION -> runtime VARIANT, schema metadata copy in `_gdtype_from_datatype`, implicit-initializer default fill for schema members |
| Bytecode gen | gdscript_byte_codegen.{h,cpp} | `append_goblin_datatype()` - recursive datatype as raw instruction words; schema defaults serialized via constant refs; `clear_address` schema branch |
| VM | gdscript_vm.cpp | Shaped-dict opcode dispatch + runtime validation, datatype decode, schema defaults fill (+ container deep-copy), `_normalize_shaped_dict_entry_value` |
| Function | gdscript_function.{h,cpp} | Datatype shape payload + validate/decode helpers; schema fields on `GDScriptDataType` |
| Editor | gdscript_editor.cpp | Autocomplete recursion (shapes), private filter (`p_recursion_depth > 0`) |
| Disassembler | gdscript_disassembler.cpp | Datatype/shape/defaults printing |
| Language | gdscript.{h,cpp} | Global schema registry (`GDScriptLanguage::schemas`) — source-based: editor scan (`_get_global_class_name` body-parse for `@schema` files), reload re-sync (after parse, before analysis), persisted cache (`res://.godot/goblin_schema_cache.cfg`) eager-loaded at init + saved at registration points |

### Features (verified in code)

| Feature | Where | Notes |
|---|---|---|
| Union types | parser + analyzer + compiler | `int | float`, `null` singleton; compiler maps UNION -> VARIANT |
| `@private` | parser + analyzer | Same-script access allowed; no `@export` combo; name still occupies slot in subclasses |
| String ctors | `core/variant/variant_construct.{h,cpp}` | `String(int/float/bool)`, `1 as String` -> `"1"` |
| Shaped dictionaries | parser/analyzer/compiler/vm/editor | Typed entries, recursive shape, `OPCODE_CONSTRUCT_SHAPED_DICTIONARY`, access refinement |
| `@schema` dictionaries | parser/analyzer/compiler/vm + gdscript.{h,cpp} + register_types.cpp | `@schema const` = reusable project-wide schema; `Dictionary[Name]` instantiates with defaults autofilled + typed override-merge + growable. Global registry (name -> script path), source-based (scan + reload-after-parse) + persisted cache eager-loaded at init, shipped in exports via the gdscript export plugin; first-run bootstrap invalidates `filesystem_cache` when the schema cache is missing. `Dictionary[Name]` is type-annotation-only (expression use = analyzer error). Defaults in the datatype (serialized via constant refs), filled by the VM. Details: `docs/gdscript_features.md` |
| `then`/`elthen` | tokenizer + parser + analyzer + compiler | `then` null-only (`a != null ? b : a`), `elthen` truthy (`a ? a : b`) — locked 2026-08-13; no VM changes; tests pending (TD-02) |

## MIDI / SoundFont module (`modules/midi/`)

Standalone additive feature module at the repo root (ADR 0008) — standard Godot module anatomy, auto-discovered by the build with the full module lifecycle: `SCsub`, `config.py` (`can_build`/`get_doc_classes`/`get_icons_path`), `register_types.{h,cpp}` (registered via the generated `register_module_types.gen.cpp`, `MODULE_MIDI_ENABLED`), `doc_classes/`, `tests/`, `editor/icons/`, in-module `thirdparty/`. Not part of the override machinery; no goblin hooks, no upstream file touched. Kills the external MidiStream GDExtension dependency (backlog C-07). Class/property/importer names are identical to the GDExtension so existing projects and `.import` files keep working.

- `MidiStream` is an `AudioStream` -> works in any stream player (`AudioStreamPlayer`, `AudioStreamPlayer3D`, ...) — this delivers backlog C-03 (3D spatialized MIDI) with no extra node.
- The synthesizer is TinySoundFont v0.9 (MIT) + TinyMidiLoader v0.7 (zlib), single-header libs vendored verbatim under `thirdparty/tinysoundfont/`; `TSF_IMPLEMENTATION`/`TML_IMPLEMENTATION` compile into `midi_stream_playback.cpp` only.
- Playback is engine-style: overrides public `start/stop/is_playing/get_loop_count/get_playback_position/seek` + `_mix_internal` + `get_stream_sampling_rate` (AudioStreamPlaybackWAV pattern), NOT the GDVIRTUAL `_start`/`_mix_resampled` hooks (those are for script subclasses).
- Lazy loading (`_ensure_loaded`) parses SF2 + MIDI on first mix (audio thread) — same design as the GDExtension, proven on the reference title. Live notes via `note_on(preset, key, vel)`.
- Importers are `ResourceImporter` subclasses registered in `ResourceFormatImporter` at EDITOR level (WAV-importer pattern). Importer names `midi_stream.mid` / `midi_stream.sf2` match the GDExtension.
- Test note: `--test` mode has no AudioServer; the doctest suite bootstraps `AudioDriverManager::get_driver(0)` (the dummy) via `set_singleton()` + `init()`, and recreates `AudioServer` whenever missing — `GodotTestCaseListener::test_case_end` deletes the singleton after every test case.

## Sim module (`modules/sim/`)

Standalone additive feature module at the repo root (ADR 0008) — standard Godot module anatomy, auto-discovered with the full module lifecycle (`MODULE_SIM_ENABLED`). Same structure as `modules/midi/`. Contains the combat subsystem (C-14, moved 2026-08-17) and the SimServer systemic simulation layer (S-01–S-05). No overrides, no upstream file touched.

**Combat subsystem** (C-14, moved from `modules/combat/`):
- `Hitbox3D` — active damage detector (extends Area3D): monitoring on, monitorable off. Carries attack data (damage, knockback, damage_types, element, source); on Hurtbox3D overlap dedups per activation, forwards via `Hurtbox3D.apply_hit`, emits `hit(hurtbox, hit_data)`.
- `Hurtbox3D` — passive damage receiver (extends Area3D): monitoring off, monitorable on. `apply_hit(attacker, hit_data)` emits `hurt(attacker, hit_data)` when active (virtual, C++ subclasses can intercept damage).
- `Projectile3D` — manual-velocity projectile (extends Area3D, NOT RigidBody3D): owns velocity/gravity/homing, continuous swept collision via an internal `ShapeCast3D` child (no tunneling), bounce/expiry/range behavior, emits `hit(hit_data)` + `expired()`. Forwards hits to Hurtbox3D on the collider.
- Hit-data contract: `CombatUtils` (combat_utils.h) defines the stable Dictionary keys (`damage`, `knockback`, `damage_types`, `element`, `source`, `position`, `normal`, `velocity`, `collider`) shared by all three classes.
- Physics tick: native nodes receive it via `_notification(NOTIFICATION_PHYSICS_PROCESS)` (not a C++ virtual `_physics_process` — GDVIRTUAL only in 4.7).
- **S-05 integration**: `Hitbox3D::register_hit` emits an `"impact"` stimulus via `SimServer::emit_stimulus()` (payload: damage/element/source/collider, radius 5); `Projectile3D::_on_hit` resolves `SimServer::query_surface()` and adds `surface`/`material_name`/`impact_uv` to hit_data. Internal C++ calls, same module — no cross-module coupling. Graceful no-op when SimServer absent.

- **S-03 ambient field**: `field_create` allocates a 3D uniform grid (`RID_Alloc<SimField>`, inline data); `field_bake` samples hemisphere exposure (32 Fibonacci directions over upper hemisphere via `PhysicsDirectSpaceState3D::intersect_ray`, budgeted by `OS::get_ticks_usec`), full bake on first call + dirty-region rebake via `invalidate_region`; `get_field_sample` does trilinear interpolation + merges dynamic source modifiers from `field_set_dynamic_source` (torch on/off); `get_stealth_value` = Thief light gem (M-08) — finds the field containing the query position, returns [0,1] exposure. No SceneTree during test → falls back to ambient 0.2f; no field at position → 0.5f.

**SimServer** (S-01–S-05): server singleton (RID-based, like PhysicsServer3D). Cadence pipeline (`pre_tick → sim_tick → post_tick`), stimulus bus with spatial index, surface registry + query (impact UV via barycentric interpolation), ambient light field + stealth readout, interaction focus query. Consumes PhysicsServer3D (ray/shape queries) + RenderingServer (as needed). Does not walk the SceneTree. Full API in `docs/rfc/simserver-rfc.md`. **S-01 shipped 2026-08-18** (clock/cadence/schedule, stimulus bus with push delivery + pull query, save/restore); **S-02 shipped 2026-08-18** (`SurfaceProperties` Resource + `query_surface` wrapping `PhysicsDirectSpaceState3D::intersect_ray` with barycentric impact UV + resolution chain); **S-03 shipped 2026-08-18** (3D uniform grid ambient light field, hemisphere exposure bake with budgeted dirty-region rebake, trilinear sampling, dynamic source modifiers, `get_stealth_value` Thief light gem readout); **S-05 shipped 2026-08-18** (combat hooks: Hitbox3D stimulus emission + Projectile3D surface resolution); S-04 method stub bound for forward-compatibility.

## Where new code goes

| Change | Location |
|---|---|
| Language feature (parser+analyzer+compiler) | `modules/goblin/modules/gdscript/` |
| Single core .cpp | `modules/goblin/core/<mirror path>/` + dict entry in `goblin_add_library()` |
| Fast scene tree (M-14): SceneTree modified IN PLACE | `modules/goblin/scene/main/scene_tree.cpp` (swap in config.py `"scene"` dict) — content is a faithful upstream copy; optimizations land here directly. Companion core edit: `scene/main/scene_tree.h` (+7 lines: 2 ProcessGroup compaction flags + 3 cached StringName members). No module, no base-class seam: `get_tree()`/`SceneTree::get_singleton()` stay upstream, editor/PM/games all run the one tree. Batches landed: T1 (lazy compaction / copy-free `_process_group`) + T6 (copy-free group calls via `ptr()` / ref-efficient timers / cached signal names).  |
| EntityNode/EntityComponent hybrid tree+ECS (D-20) | New sources in `modules/goblin/scene/main/`: `entity_node`, `entity_component`, `entity_registry`, `transform_3d_component`, `mesh_instance_component`, `visibility_component` — injected into the scene library via the `_GOBLIN_FILE_ADDITIONS` dict in `goblin_add_library()` (additions, NOT swaps — there is no upstream stem to replace). Seams: `scene/main/scene_tree.h` (+5 lines: `class EntityRegistry;` forward decl, `friend class EntityNode/EntityComponent;`, `EntityRegistry *entity_registry = nullptr;` pointer member — pointer only, B-14 ODR rule) + mirror `scene_tree.cpp` (registry ctor/dtor + `flush_dirty()` at the end of both `process()` and `physics_process()`, after `_flush_delete_queue()`). `EntityRegistry` is a plain C++ class (not script-visible); classes register from `modules/goblin/register_types.cpp` at `MODULE_INITIALIZATION_LEVEL_SCENE`. Batch-1 components: Transform3D/MeshInstance/Visibility; bodies + CollisionShape + camera deferred. Icons: `editor/icons/<ClassName>.svg` (class-named, picked up by the editor icon gen). Spec: `docs/rfc/entity-node-rfc.md` + `docs/plans/entity-node-plan.md`. |
| Single editor .cpp | `modules/goblin/editor/overrides/<mirror path>/` + dict entry in `goblin_add_library()` (NEVER a globbed dir — `editor/SCsub` globs `*.cpp` non-recursively; unmodified headers stay upstream, rewrite the bare own-header include to root-relative) |
| New additive feature module (zero overrides) | standalone `modules/<name>/` with standard module anatomy (ADR 0008) — auto-discovered, full lifecycle; never inside `modules/goblin/`. Current: `modules/midi/` (audio synth, thirdparty + importers + optional), `modules/sim/` (combat + SimServer, no thirdparty + genre-essential) |
| New native class (override-adjacent) | .cpp/.h in `modules/goblin/` + `GDREGISTER_CLASS` in register_types.cpp |
| Build-time generator | `goblin_builders.py` + assignment in `configure()` |
| Branding assets | `main/`, `editor/icons/`, `platform/windows/` |

## Tests

- Fork tests: `modules/goblin/modules/gdscript/tests/` (mirror of upstream suite + new cases under `parser/`, `analyzer/`, `runtime/`). The test harness (`gdscript_test_runner_suite.h`, `test_completion.h`, `test_lsp.h`) targets the fork's own tests dir.
- MIDI tests: `modules/midi/tests/test_midi_stream.h` (doctest `TEST_CASE`s, picked up via `modules_tests.gen.h` when `tests=yes`). Generates a minimal SF2 + SMF in memory; covers length, synth render, song-end stop, loop restart, manual notes. Run: `bin/goblin.windows.editor.x86_64.exe --test --test-case="*MidiStream*"`.
Sim module tests: `modules/sim/tests/test_sim.h` — combat subsystem tests (11 doctest cases: Hitbox3D/Hurtbox3D/Projectile3D defaults, hit registration, dedup/reset, inactive states, motion/gravity, bounce math, lifetime expiry, hit-data contract) + S-01 SimServer tests (clock/cadence/stimulus bus: tick math, tag/cancel/repeat, save/restore round-trip, stimulus emit/query/listener delivery/pruning — 11 cases, all green) + S-02 SimServer/SurfaceProperties tests (resource defaults, property round-trip, query_surface hit with explicit assignment, no-hit query — 4 cases; 2 SceneTree-prefixed for physics space) + S-03 ambient field tests (field_create RID+grid, bake+sample, stealth_value reads field, dynamic source adjusts exposure, geometry occlusion sampling, invalidate+rebake — 6 cases) + S-05 combat integration tests (Hitbox3D impact stimulus delivery, Projectile3D surface resolution — 2 cases). Name prefix `[SceneTree]` is required for combat + physics-space tests: physics nodes crash without the per-case physics-server bootstrap that `[SceneTree]`-prefixed cases get in `tests/test_main.cpp`. SimServer tests use `[Modules][SimServer]` prefix (no SceneTree dependency). Run: `bin/goblin.windows.editor.x86_64.exe --test --test-case=SimServer`. Note: 2 combat test failures are pre-existing (Godot 4 Dictionary/Object-Variant copy semantics — null Object storage + non-RefCounted Object copy through emit_signal); identical code at both failure sites; no SimServer test regressions.
- Entity tests: `modules/goblin/tests/test_entity_node.h` (doctest, picked up via `modules_tests.gen.h` when `tests=yes`): registry pool mechanics (create/destroy + id reuse, insert/remove/remap) + SceneTree-prefixed pipeline cases (compile on enter, dirty dedup, churn remap, runtime add/remove component, hybrid child untouched, nested entities, transform-composition anchor, visibility flush). GDScript smoke: `modules/goblin/tests/entity_node_smoke.gd` — run via a scratch project (`--headless --path <scratch>`), expects `ENTITY_SMOKE_OK`. Run: `bin/goblin.windows.editor.x86_64.exe --test --test-case="*Entity*"`.
- Run: build with `tests=yes` (`scons platform=windows target=editor module_mono_enabled=no accesskit=no angle=no tests=yes -j4`), then `bin/goblin.windows.editor.x86_64.exe --headless --test --test-case "[Modules][GDScript]*"`.
- Regenerate expected outputs from current behavior: `bin/goblin.windows.editor.x86_64.exe --headless --gdscript-generate-tests` (writes `.out` files — use with care; it encodes whatever the engine currently does).
- Gotchas:
  - `.out` files must end with a trailing newline: `GDScriptTest::check_output()` compares against `strip_edges(output) + "\n"`.
  - `init_language()` forces `ProjectSettings`' resource path to the test scripts dir so `res://`-relative reads (LSP/completion suites) work even after other suites (e.g. GLTF) leave the singleton's resource path set.
- Gate: reference corpus compile + 342 unit tests + level load. Never claim a change verified without a build.

## Landmines / drift (verified today)

1. `goblin_manager.py` `build` subcommand targets `linuxbsd` - wrong for this project (Windows). Never reintroduce a `clean` command (`scons --clean` violates hard rule 1).
2. `gdscript.h` layout must stay identical to upstream: `main/main.cpp` + `editor/doc/editor_help.cpp` include it outside the module (ABI).
3. `then`/`elthen` semantics: `then` null-only, `elthen` truthy — locked 2026-08-13, do not "fix" to null-only.
4. `editor/overrides/` must stay out of any SCsub glob (non-recursive `*.cpp` glob in `modules/goblin/editor/SCsub` would double-compile swapped sources). Only `goblin_add_library()` references it.
5. Editor mirrors drift from upstream: re-diff on rebase (`git diff --no-index --stat editor/<file> modules/goblin/editor/overrides/<file>`); `project_export.h` mirror is the only copied header (it carries the fork-only `_update_export_debug_option()`).
6. `modules/goblin/core/SCsub` adds `Depends` edges for `authors/donors/license/version_generated.gen.h` AND `main/splash.gen.h`/`splash_editor.gen.h`/`app_icon.gen.h`: the upstream SCsubs declare only upstream files as sources, so without the edges the goblin builders' inputs would never trigger regeneration.
7. Editor splash: upstream 4.7 removed it (commit c283fce698). config.py strips `NO_EDITOR_SPLASH` from CPPDEFINES; `modules/goblin/SCsub` generates `#main/splash_editor.gen.h` itself (main/SCsub skips its command because `no_editor_splash` stays True). Do not flip `no_editor_splash` to False — main/SCsub's command would then target a nonexistent `#main/splash_editor.png` and fail.
8. Windows RES builder is shadowed in config.py (`env.AddMethod` on "RES"): compiles `goblin.rc`/`goblin_res_wrap.rc` instead of upstream `godot_res.rc`. `goblin.ico` is generated at build time from `main/app_icon.png` (PNG-compressed ICO). The `.rc` files must reference the ico with a repo-root-relative path (`modules/goblin/platform/windows/goblin.ico`).
9. Entity headers (`entity_*.h`, `*_component.h`) use **bare same-directory includes** (`#include "entity_registry.h"`) instead of root-relative `scene/main/...`: they are pulled in from multiple environments (scene-library overlay, module registration, `tests/test_main.cpp` via `modules_tests.gen.h`) that do not all carry the goblin tree on CPPPATH, and MSVC's quoted-include search only walks ancestor include-file directories. The mirror `scene_tree.cpp` keeps the root-relative include — the overlay env resolves it.
10. `EntityComponent::_attach()` runs during the parent EntityNode's ENTER_TREE (parent-first order) — the component's `data.tree` is NOT assigned yet. The registry sets the component's `registry` back-pointer before `_attach()`; never call `get_tree()` from `_attach()`/`_detach()`.
11. `NOTIFICATION_MOVED_IN_PARENT` is deprecated in 4.7 (never sent — use `NOTIFICATION_CHILD_ORDER_CHANGED`). World-anchor refresh relies on re-ENTER_TREE after reparent (`reparent()` does remove+add, which recompiles the entity).
12. `RenderingServer` has no `instance_get_transform` readback (dummy renderer discards transforms) — entity tests assert pool state via `EntityRegistry` test-introspection accessors, not server readback.

## Fast lookup

| Task | Start at |
|---|---|
| Fix analyzer error | `gdscript_analyzer.cpp` (reduce_*/resolve_*) |
| Add keyword | tokenizer.h enum + tokenizer.cpp names/keywords, then parser precedence table |
| Add VM opcode | `gdscript_byte_codegen.h` + `gdscript_vm.cpp` (enum + dispatch) |
| Autocomplete behavior | `gdscript_editor.cpp` `_find_identifiers_in_class` |
| Branding string | `core/version_override.py` / `goblin_builders.py` / `editor/branding_translations.cpp` (runtime fallback) / `editor/overrides/` (compile-time) |
| MIDI playback / synth | `modules/midi/midi_stream_playback.cpp` (TSF/TML impl lives there — single TU) |
| MIDI import | `modules/midi/midi_importers.cpp` + registration in `modules/midi/register_types.cpp` |
| Port upstream commit | `porting` skill + diff mirrors vs upstream |

## Update discipline

Code changed -> update this map + `docs/gdscript_features.md`. Stale map is a landmine, not a doc.
