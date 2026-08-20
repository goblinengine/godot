# Goblin Engine — Fork Roadmap

**Version:** 0.2.0 (draft)
**Date:** 2026-08-11
**Author:** Filip Anton (filipworksdev)
**Status:** Active — Phase 0, based on Godot 4.7.1-stable (commit a13da4feb8d)

---

## 1. Vision & Philosophy

### What This Fork Is

A deliberately customized, lightweight fork of Godot Engine tailored specifically for immersive sim / systems-heavy FPS RPG development. It preserves full compatibility with existing Godot projects and the genre set's reference title while removing unused engine components and extending the engine where it limits the genre.

### What This Fork Is NOT

- Not a general-purpose engine fork for broad consumption
- Not a "GDScript replacement" or "GDScript vs daScript" project
- Not a full rewrite — it preserves Godot's editor, renderer, and asset pipeline
- Not a rebranding project (that already exists in `modules/goblin/`)

### Core Principles

1. **Source Override, Not Source Modification.** All changes live inside `modules/goblin/`. Godot core files are never modified. The build system redirects compilation to goblin-owned replacement files.

2. **Surgical, Not Sweeping. Genre-Specific Only.** Every core change must justify its existence against a concrete genre requirement. No speculative over-engineering. This fork serves a narrow genre set — immersive sim / systems-heavy FPS RPG — not the general Godot community. Features that benefit "most users" but don't serve the genre set have zero priority. Core engine edits are a LAST RESORT, only when a module override or GDScript extension is impossible.

3. **Trim Aggressively, Keep Compatibility.** Strip unused modules when evidence confirms they are unused. The trimmed binary still runs reference-title projects unmodified.

4. **Single-Branch Progressive Stable Tracking.** `master` is the one and only branch. It tracks official Godot stable releases, never the upstream `master` development branch. At each new stable release (e.g., 4.7.2, 4.8.0), `master` is rebased onto that release tag and goblin changes are re-applied. No branch-per-release archaeology — this fork serves a narrow genre set and complexity is waste. Current base: `4.7.1-stable` (commit a13da4feb8d).

5. **ADR-Governed Architecture.** All structural decisions — GDScript language extensions, core engine modifications, module architecture, renderer changes — go through Architecture Decision Records (ADRs) modeled on the Goblin Custom Engine's governance. No feature lands without a locked ADR. The ADR must specify: what changes, why it's justified against a concrete genre requirement, what files are touched, what the merge conflict surface is, and what tests gate acceptance.

6. **Cherry-Pick GDScript Features, Don't Fork Wholesale.** The existing `gdscript2` module has partial breakage and untracked complexity across multiple unmerged branches. Instead of merging it wholesale, start from clean upstream GDScript and port individual features one at a time: union types first, then `then`/`elthen`, then performance optimizations. Each feature lands as its own tested, reviewed change.

7. **Minimal Override Surface.** Every overridden source file is a merge-conflict liability. Prefer additive overrides (new files that ADD functionality to a module) over replacement overrides (files that completely replace upstream). When replacement is necessary, document the exact divergence and track which upstream releases touch the overridden file.

---

## 2. Architecture Governance

This fork follows the same ADR-driven governance as the Goblin Custom Engine. No structural decision lands without a locked Architecture Decision Record.

### ADR Template

Each ADR must address:
- **Status:** Proposed → Accepted → Superseded
- **Context:** What concrete genre requirement or engine limitation justifies this change?
- **Decision:** What exactly changes? What files are touched? What is the override surface?
- **Alternatives Considered:** What other approaches were rejected and why?
- **Consequences:** What becomes easier? What becomes harder? What is the merge-conflict surface?
- **Test Gates:** What tests must pass for this ADR to be accepted?
- **Stable Release Target:** Which Godot stable release tag is this based on?

### ADR Categories

| Category | Example Decisions | Governing Principle |
|----------|------------------|---------------------|
| **Language** | Union types, structs, typed dicts | Every syntax addition must solve a concrete genre requirement. No speculative language features. |
| **Engine Core** | LightmapGI fix, MIDI support, AStar3D keys | Surgical changes only. Minimum files touched. Must be upstream-acceptable where possible. |
| **Module** | Which modules to trim, which to keep | Evidence-based: verified against the reference title's project config, script corpus, and content files. |
| **Renderer** | GL Compat vs Forward+, custom render passes | Reference-title compatibility is non-negotiable. Renderer changes are post-first-title only. |
| **Build System** | Source override mechanism, SCons hooks | Must not modify Godot build files. All changes in `modules/goblin/`. |

### ADR Lifecycle

1. **Proposal:** Draft ADR in `modules/goblin/docs/adr/`. Must include concrete evidence (reference-title code references, benchmark data, module dependency analysis).
2. **Review:** Cross-check against existing ADRs for conflicts. Verify test gates are specific and measurable.
3. **Acceptance:** Lock the ADR. Implementation begins only after acceptance.
4. **Implementation:** Code changes live in `modules/goblin/overrides/`. Test suite gates the merge.
5. **Supersedure:** If a later ADR replaces this decision, the original is marked Superseded with a pointer to the replacement.

### Current ADRs (to be created)

| ADR # | Topic | Status |
|-------|-------|--------|
| 0001 | Source override system architecture | Accepted |
| 0002 | Stable-release tracking strategy | Accepted |
| 0003 | Module trim methodology and evidence standard | Accepted |
| 0007 | Goblin branding — replace retry loops with compile-time overrides | Accepted |
| 0004 | GDScript union types — port from gdscript2 | Proposed |
| 0005 | GDScript safe navigation / null coalescing | Proposed |
| 0006 | Runtime lightmap API surface | Proposed |

---

## 3. Current State Analysis

### Existing Goblin Module (`modules/goblin/`)

**What it does:**
- SCons builder monkey-patching: replaces version info, authors, donors, license, splash, app icon, binary name
- Compile-time editor UI overrides (ADR 0007): About dialog, export dialog, Project Manager, editor help menu
- Runtime translation injection as fallback for strings in files not overridden
- GDScript language fork (whole-module override)

**Limitations (as of ADR 0007):**
- **Fragile coupling.** The `config.py` → `goblin_builders.py` handoff relies on module-level dictionary injection before any builder runs. If import order changes, this breaks silently.
- **Override rebase tax.** The four editor file mirrors (`editor_about.cpp`, `project_export.cpp`, `project_manager.cpp`, `editor_node.cpp`) must be re-diffed against upstream on each rebase; `editor_node.cpp` is the highest-churn file.
- **Composed-string gaps.** Exact-key translation overrides cannot rebrand composed strings (`"%s - Godot Engine"` window titles, "Godot Version") — tracked as backlog B-11.

The runtime retry loops and `node_added` tree scanning (formerly in `goblin_about.cpp` / `goblin_export.cpp`) were eliminated by ADR 0007.

### Existing GDScript2 Module (`modules/gdscript2/`)

**Implemented on HEAD (7 commits):**
- Union types (`int | String`, `Dictionary | null`)
- `swap(a, b)` built-in function
- Safe navigation (keyword `then`) 
- Null coalescing (keyword `elthen`)
- Inline caching for property access (monomorphic, 4 opcodes)
- Fast String cast optimization

**Implemented on branches (not merged):**
- `pic` — 4-way polymorphic inline cache with negative caching
- `opcode_fusing` — 40+ fused opcodes for array/dict/iterate fast paths
- `jit` — Experimental native JIT via opcode segment fusion
- `native-inlining` — `@private` annotation, native call inlining
- `ideas` — Design docs for named args, destructuring, structs, tuples, comprehension, etc.

**Known Issues:**
- User reports partial breakage (certain scripting patterns cause issues) — needs diagnosis and fix before merging into goblin

### Reference Title Engine Dependencies

**Full dependency map documented separately.** Key findings:
- **Renderer:** GL Compatibility only. No Forward+ or Mobile.
- **Physics:** Jolt only. GodotPhysics3D unused.
- **Scripting:** GDScript only. No C#, no GDExtension (except MidiStream).
- **Navigation:** Custom A* on AStar3D. NavigationServer3D unused.
- **Networking:** Complete stub. No networking code.
- **Animation:** AnimationPlayer + Tween. No Skeleton3D, no AnimationTree.
- **3D Nodes:** Core types only (CharacterBody, RigidBody, StaticBody, AnimatableBody, Area3D, RayCast3D, MeshInstance3D, Camera3D). No CSG, GridMap, Decal, VoxelGI, ReflectionProbe, Path3D.
- **Audio:** WAV + MIDI (via MidiStream GDExtension). OGG supported but unused. MP3 unused.
- **Textures:** PNG only. No WebP, JPEG, BMP, TGA, SVG.
- **Fonts:** TTF with MSDF. No bitmap fonts.
- **Platforms:** Windows only. No Linux, macOS, Android, iOS, Web.
- **Input:** Full keyboard+mouse+joypad. No touch, no VR controllers.

---

## 4. Source Override System

### Implemented Mechanisms

Three build-time injection mechanisms replace upstream files without modifying Godot's source. Full details and code in ADR 0001 and [STRUCTURE.md](STRUCTURE.md).

**1. Module Directory Override** (`SCsub` → `GOBLIN_MODULE_OVERRIDES`)
Swaps an entry in `env.module_list` for a goblin-owned mirror directory. Currently maps `gdscript` → `modules/goblin/modules/gdscript/`. Used for whole-module forks (the GDScript language fork).

**2. Core/Editor File Override** (`config.py` → `goblin_add_library()`)
Intercepts `env.add_library(...)` and swaps selected source `Object` nodes before the library captures its sources. A library-scoped dict `_GOBLIN_FILE_OVERRIDES = {lib: {stem: goblin_path}}` covers `core` (currently `variant_construct.cpp`) and `editor` (currently `editor_about.cpp`, `project_export.cpp`, `project_manager.cpp`, `editor_node.cpp`). Used for surgical single-file overrides. Editor mirrors live in `modules/goblin/editor/overrides/` — a subtree never globbed by SCsub (the module's `*.cpp` glob is non-recursive), so swapped sources are not double-compiled.

**3. Builder Monkey-Patching** (`config.py` → `configure()`)
Replaces build-time generator functions (version header, splash, icons, authors/license) and renames binaries `godot` → `goblin`. Used for branding.

### The Chosen Architecture

The original fork plan proposed an `add_source_files()` interception layer with a declarative `OVERRIDE_MAP`. This was **rejected** in favor of the simpler mechanisms above, which leverage Godot's existing `env.module_list` redirection and `add_library` infrastructure directly. No `goblin_source_overrides.py` or interception layer exists; the mirror directories are `modules/goblin/modules/`, `modules/goblin/core/`, and `modules/goblin/editor/overrides/` (the last is the home of the editor file mirrors from ADR 0007 — the directory name is shared with the rejected layer, but the mechanism is the accepted `goblin_add_library()` swap).

### What This Enables

| Current Limitation | Solved By |
|---|---|
| Fork GDScript (union types, @private, String ctors) | Module directory override — done |
| Override a single core file (variant_construct.cpp) | Core file override — done |
| Hardcoded "Godot" strings in About dialog | Compile-time override of `editor_about.cpp` (backlog B-04) — done |
| Add renderer hooks later | Module or core override, chosen when needed |
| Modify physics behavior | Single-file core override |
| Awkward retry loops for SceneTree hooking | Compile-time changes instead of runtime polling (backlog B-04) — done |

### Eliminating the Retry Loops

Done (ADR 0007, 2026-08-13): the runtime singletons (`GoblinBranding` in `goblin_about.cpp`, `GoblinExportTweaks` in `goblin_export.cpp`) and their 120-attempt SceneTree polling loops and `node_added` tree scans are **deleted**. Their behavior is now compile-time: four editor files are overridden via the library-scoped dict in `goblin_add_library()` — `editor_about.cpp` (Goblin literals, Donors tab removed), `project_export.cpp` (debug-template-aware "Export With Debug" option + warning filter), `project_manager.cpp` (Donate button removed), `editor_node.cpp` (Support Godot Development item removed). The runtime translation injection is kept as a fallback in `branding_translations.cpp` for strings in files not overridden — zero runtime overhead, zero polling.

---

## 5. Module Trim Plan

### Methodology

Each module was assessed against:
1. Does the reference title directly use the classes or APIs it provides?
2. Is it a dependency of another module the reference title uses?
3. Would removing it break editor functionality needed for reference-title development?

### Keep List (~20 modules)

| Module | Reason | Can Runtime-Trim? |
|--------|--------|-------------------|
| `gdscript` | Entire codebase | No |
| `freetype` | TTF font rasterization | No |
| `msdfgen` | MSDF font generation (enabled in project) | No |
| `text_server_adv` | All text rendering (HarfBuzz) | No |
| `jolt_physics` | Selected physics engine | No |
| `regex` | Level-format parser, dice rolling, string tools | No |
| `goblin` | Fork identity + override system | No |
| `ogg` | OGG container (dep of vorbis) | No |
| `vorbis` | OGG Vorbis audio decoder | No |
| `mp3` | MP3 audio decoder (used in a gameplay script) | No |
| `bcdec` | BC texture decompression | No |
| `meshoptimizer` | Vertex cache optimization for SurfaceTool | No |
| `mbedtls` | TLS, CryptoCore (engine internals) | No |
| `enet` | Planned multiplayer networking | No |
| `websocket` | Planned multiplayer + LSP (dep of gdscript) | No |
| `webrtc` | Planned multiplayer | No |
| `multiplayer` | Planned multiplayer replication | No |
| `jsonrpc` | Planned multiplayer + LSP support | No |
| `upnp` | Planned multiplayer NAT traversal | No |
| `noise` | FastNoiseLite — planned for procedural content | No |

### Editor-Only (keep for editor, trim from export templates)

| Module | Reason |
|--------|--------|
| `cvtt` | Texture compression (editor import) |
| `betsy` | GPU texture compression (editor import) |
| `lightmapper_rd` | GPU lightmap baker (editor tool) |
| `xatlas_unwrap` | Lightmap UV unwrapping (editor tool) |
| `svg` | Editor icon rendering (not used in game content) |

### Trim List (~42 modules)

**Image/Texture Formats (10 modules):**
`bmp`, `tga`, `dds`, `hdr`, `tinyexr`, `basis_universal` (runtime), `ktx`, `astcenc`, `etcpak`, `svg` (runtime)

*The reference title uses only PNG. All other image formats are dead code.*
*EXCEPTION (B-21, 2026-08-18): `webp` + `jpg` are KEPT. Vanilla Godot 4.7 writes lossless/lossy texture imports as WebP-embedded .ctex, so trimming `webp` breaks every pre-existing vanilla-imported project cache (`compressed_texture.cpp:343`). `jpg` kept for `.jpg` source compat. Godot compatibility trumps trim size.*

**Audio/Video (2 modules):**
`theora` (video playback), `interactive_music` (AudioStreamPlaylist/Interactive/Synchronized)

*No video playback. No interactive music.*

**Networking KEPT (6 modules) — user has plans for multiplayer:**
All networking modules preserved: `enet`, `websocket`, `webrtc`, `upnp`, `multiplayer`, `jsonrpc`.

**VR/XR (3 modules):**
`webxr`, `openxr`, `mobile_vr`

*No VR usage. Trivially re-addable later if needed.*

**3D Nodes/Scene (4 modules):**
`csg`, `gridmap`, `gltf`, `fbx`

*No CSG, GridMap, GLTF/FBX import used.*

**Navigation (2 modules):**
`navigation_3d`, `navigation_2d`

*Verified: the reference title's navigation script (1648 lines) uses only `AStar3D` — a core math class in `core/math/`, NOT part of the `navigation_3d` module. Zero references to `NavigationServer3D`, `NavigationAgent3D`, `NavigationRegion3D`, or `NavigationMesh`. The `navigation_3d` module provides these unused classes. `AStar3D` is preserved as core.*

**Physics (2 modules):**
`godot_physics_3d`, `godot_physics_2d`

*Jolt Physics is the selected engine. GodotPhysics is dead code.*

**Shader/Compiler (1 module):**
`glslang`

*GL Compatibility uses GLSL directly. No SPIR-V compilation needed.*

**Utility (2 modules):**
`camera` (CameraServer/webcam), `zip` (ZIPReader/ZIPPacker)

*Not used. .pck loading uses internal minizip, not the zip module.*

**Rendering (1 module):**
`raycast` (Embree occlusion culling)

*Only used by Forward+/Mobile renderers. GL Compatibility uses BVH.*

**Debug (1 module, release only):**
`objectdb_profiler`

**Physics Tools (1 module, editor only):**
`vhacd` (convex decomposition)

### Platform Trim

**Platforms: ALL KEPT.** User wants cross-platform support (Windows, Linux, macOS, Android, iOS, Web). No platform trimming.

**Drivers to remove (platform-specific, not needed for target platforms):**
`xaudio2/` (older Windows audio — wasapi supersedes), `apple/`, `apple_embedded/`, `coreaudio/`, `coremidi/` (macOS/iOS — keep platforms but audio drivers unused since we target WASAPI on Windows primarily), `sdl/` (SDL platform abstraction — not needed with native platform drivers)

**Drivers to keep:**
`gles3/`, `gl_context/`, `png/`, `wasapi/`, `winmidi/`, `windows/`, `vulkan/`, `d3d12/`, `metal/`, `egl/`, `pulseaudio/`, `alsa/`, `alsamidi/`, `unix/`, `accesskit/`, `backtrace/`

### Impact

- **Compile time:** ~55% reduction in modules compiled (~22 from 56 enabled, up from 14 due to keeping networking + noise + platforms)
- **Binary size:** Estimated 30-45% reduction in export template
- **Linker time:** Significant reduction from fewer static libraries
- **Risk:** Re-validation pending — the original "verified unused" evidence (ADR 0003) was gathered while the trim was a no-op (B-01 was falsely `done` until 2026-08-16). Trim now actually engages; B-01 plan gates re-validate against the reference corpus.

### Trimming Mechanism

Module trim is a build-time option injection (ADR 0012), not `env.disabled_modules` (that was a no-op). `module_*_enabled` is decided at `SConstruct:499` `opts.Update` from `ARGUMENTS` (args layer beats defaults and custom.py/profile files). `configure()` runs too late for the trim candidates sorting before "goblin", so `modules/goblin/config.py` mutates `SCons.Script.ARGUMENTS` at IMPORT time (first module loop, `SConstruct:474`, before the Update):

```python
from SCons.Script import ARGUMENTS
for _mod in DISABLE_MODULES:
    _key = f"module_{_mod}_enabled"
    if _key not in ARGUMENTS:   # user CLI wins
        ARGUMENTS[_key] = "no"
```

The gate at `SConstruct:1113` (`if not env[f"module_{name}_enabled"]: continue`) then skips all trimmed modules regardless of alphabetical position. `configure()` prints a canary (`Goblin: Module trim active (28/28 modules gated off)`) so a regression or CLI override is visible in every build log. Trim list: 28 modules (tinyexr kept for editor `.exr` lightmap save, C-11; godot_physics_3d kept as the default physics server — jolt registers no default, ADR 0003). GL Compatibility-only fork: glslang trimmed by design (Forward+/Mobile require re-enabling it).

---

## 6. GDScript Feature Roadmap

### Tier 0 — Cherry-Pick Core Features (Phase 0, from clean GDScript base)

**Strategy:** Start from the upstream GDScript at the tracked stable release tag. Do NOT merge the existing `gdscript2` module wholesale — it has partial breakage and unmerged branches with unknown interactions. Instead, port individual features one at a time into `modules/goblin/overrides/gdscript/`. Each feature lands as its own tested, ADR-governed change.

**Feature 0a — Union Types (`int | String`, `Dictionary | null`)**
- Highest impact / lowest risk of the gdscript2 features
- Solves ~30 `typeof()` + `as` variant-checking patterns in the navigation builder alone
- Replaces `Variant` branching in physics shape-size code
- Port approach: extract the union type changes from gdscript2's parser/analyzer/compiler diffs. Add union type test suite (50+ patterns). Run against the reference corpus.

**Feature 0b — Safe Navigation `then` and Null Coalescing `elthen`**
- Replace hundreds of `if x != null:` guards across the codebase
- Port approach: extract the THEN/ELTHEN tokenizer changes + binary op node changes from gdscript2
- Keywords are `then`/`elthen` (locked decision, 2026-08-13; `?.`/`??` NOT planned). Semantics locked 2026-08-13: `then` is null-only (`a != null ? b : a`), `elthen` is deliberately truthy (`a ? a : b`) — the earlier null-only port note is superseded; do not change the code. See `gdscript_features.md` §then/elthen.

**Feature 0c — Inline Caching for Property Access**
- Monomorphic property cache (4 opcodes: SET/GET named/member)
- Port approach: extract bytecode gen slot reservation + VM fast-path changes
- Validate: profile hot paths in the reference title's physics (`_physics_process`) and AI detection

**Feature 0d — Fast String Cast**
- Trivial optimization: `as String` fast path in VM
- Port approach: single change in `gdscript_vm.cpp`
- Validate: benchmark string-heavy code paths (navigation keys, level-format parsing)

**ADR Required:** Each feature gets its own ADR documenting: what changes, exact files touched in `overrides/gdscript/`, what test suite gates acceptance, what upstream GDScript behavior is preserved vs modified.

### Tier 1 — Performance Optimizations (Phase 1, post-Tier-0 stabilization)

**Opcode Fusing (40+ fused opcodes):**
Extract from gdscript2's `opcode_fusing` branch. Fused opcodes for typed array/dict access, integer arithmetic, fast iteration, fused condition+dispatch. Each fused opcode is an independent change — port the highest-impact ones first (array access, dict iteration, integer arithmetic).

**PIC (Polymorphic Inline Cache):**
Extract from gdscript2's `pic` branch. Upgrade monomorphic cache to 4-way PIC with negative caching. Extend to method calls.

**Impact on the reference title:** Eliminates per-frame lambda allocation for `sort_custom` (33+ call sites). Dictionary hot paths in combat/stealth/conditions see 2-5x speedup.

### Tier 2 — Language Features (Post-first-title 1.0, high impact)

**Structs / Value Types:**
The single biggest language gap. The reference title's entire entity model is `Dictionary` because GDScript lacks value types:
- 60+ `duplicate(true)` call sites to avoid reference-sharing bugs
- Physics hot paths take individual scalar parameters instead of data dicts for performance
- Enables stack-allocated, copy-by-value data with known compile-time layout

Implementation approach (daslang-inspired):
- `struct` keyword in parser → new AST node
- Compiles to a Variant-compatible packed struct type with known byte offsets
- No inheritance, no methods, no virtual dispatch — pure data
- Zero-cost copy (memcpy), no refcounting

**Typed Dictionaries:**
`var items: Dictionary[StringName, Dictionary]` or similar syntax. Makes `Dictionary.get()` return typed values instead of `Variant`, eliminating the ~30 `typeof()` + `as` checks in the navigation builder.

**Built-in PriorityQueue / Binary Heap:**
GDScript has no stdlib data structures. The navigation system's Dijkstra is O(N²) because it uses linear array scan for the unvisited set. A built-in `PriorityQueue` class replaces that with O(log N) extract-min.

### Tier 3 — Language Deepening (Post-first-title 1.0, medium impact)

**Blocks / Stack-Bound Callables:**
Zero-allocation callbacks that capture by reference and cannot escape scope. Replaces `sort_custom` lambda allocations (33+ sites) and AI filter lambdas with zero-cost alternatives.

**`yield` Generators:**
Generator-based lazy iteration (`generator func` → compiled to finite state machine). Eliminates intermediate array allocations for query/filter/map chains.

**Comprehensions:**
`[for x in data: transform(x) if condition]` — syntactic sugar that compiles to efficient typed loops. Replaces `filter()` + `map()` patterns with readable, allocation-free code.

**`var inscope` / RAII:**
Compiler inserts `finally` block for cleanup on scope exit. Useful for resource handles, lock guards, temporary allocations.

### Tier 4 — Transformative (Long-term, very high impact)

**Generics with `typeinfo` (LOW priority — add "for fun", speculative):**
daslang-style compile-time generics with type reflection. Enables typed containers (`Array[MyStruct]` with full type safety), generic algorithms (sort, filter, map over typed collections without boxing), and template specialization. Not required by the reference title today (unions + typed containers cover the data-layer gaps; the "consistency across positions" case is `Array[MyStruct]` after G-07 structs). Added 2026-08-19 at user request as a low-priority / experimental item.

**Zero-Marshaling C++ Interop:**
Script data laid out at C++-compatible offsets. `ManagedStructureAnnotation` allows scripts to read/write engine data directly at C++ addresses. This is the hardest feature but the most transformative — it eliminates the single biggest performance cost in Godot's scripting model.

---

## 7. Core Engine Changes

These are surgical modifications to Godot core files that cannot be achieved through module overrides or GDScript extensions. Ordered by justification clarity.

### 6.1 LightmapGI Frustum Culling Fix (#71585)

**Pain point:** LightmapGI node frustum culling breaks lightmap injection when the node is out of view. The entire runtime lightmap pipeline (~17 functions in the level script) partially exists to work around this.

**Change:** Fix the frustum culling logic in `LightmapGI` so lightmap injection is independent of node visibility. This is a known upstream Godot bug (issue #71585).

**Files touched:** ~1-2 in `scene/3d/lightmap_gi.cpp` or `servers/rendering/renderer_scene_cull.cpp`

**Justification:** Fixes a shipped engine bug that directly impacts the genre set's runtime-lightmap workflow. Upstream would accept this fix.

### 6.2 Runtime LightmapBaker API Surface

**Pain point:** Runtime lightmap baking requires `ClassDB.class_exists("LightmapBaker")` guard — the baker class is a custom module, not in standard Godot builds. `lightmap_unwrap()` and runtime `bake()` are gated behind custom additions.

**Change:** Expose `lightmap_unwrap()` and `bake()` on `LightmapGI` as public API, or promote the `LightmapBaker` class to a first-class engine feature. This eliminates the `ClassDB` guard and makes runtime baking a standard capability.

**Files touched:** ~1-2 in `scene/3d/lightmap_gi.cpp/.h`

**Justification:** Level authoring needs runtime lightmap baking for procedural geometry. Making this a standard API reduces the custom module surface.

### 6.3 MIDI Support in AudioStreamPlayer3D

**Pain point:** 3D spatialized MIDI playback requires manual `AudioStreamPlayer3D` construction + `MidiStream` cloning + soundfont copying. The current code creates players dynamically per NPC body and manually copies soundfont.

**Change:** Add `midi_stream` property to `AudioStreamPlayer3D` (or `AudioStreamPlayer`) so MIDI playback is a first-class feature with automatic spatialization.

**Files touched:** ~1-2 in `scene/audio/audio_stream_player_3d.cpp/.h`

**Justification:** The genre set includes instrument-playing NPCs and MIDI-based adaptive music. This is a small change with large gameplay impact.

### 6.4 Vector3i Keys for AStar3D

**Pain point:** Navigation edge tracking uses string-format keys (`"%d|%d|%d"` in hot paths). String allocation and hashing in inner navigation loops.

**Change:** Expose `Vector3i` as a key type in AStar3D's internal point lookup, or add an overload that accepts integer triples. `Vector3i` already exists as a Variant type — just needs to be used as a hash key internally.

**Files touched:** ~1-2 in `core/math/a_star.cpp/.h` or `core/math/a_star_3d.cpp/.h`

**Justification:** Small core change, eliminates string allocation in a hot path, benefits any project using AStar3D with coordinate-based keys.

---

## 8. Renderer Strategy

### Current: GL Compatibility (OpenGL ES 3.0)

**Pros:**
- The reference title is built and tested on it
- Low-fi pixel art aesthetic works perfectly
- All 25 shaders are GLSL — fully compatible
- No SPIR-V compilation needed

**Cons:**
- 16 lights per object limit (worked around via lightmap baking)
- No post-opaque compositor callback
- No resolved-depth callback
- No compute shaders
- No `RenderingDevice` API
- No custom render pass injection

### Option: Switch to Forward+ (Vulkan)

**Pros:**
- 256+ dynamic lights (eliminates 16-light limit entirely)
- Full `RenderingDevice` API (compute shaders, custom passes)
- Post-processing hooks
- Better performance on modern GPUs
- Light probe grid for stealth would be trivial

**Cons:**
- All 25 shaders need `.gdshader` header changes (`render_mode` may need adjustment)
- The reference title's GL Compatibility-specific workarounds need audit
- Build includes `vulkan/` driver and `glslang/` module
- Testing burden: full visual regression pass needed

**Recommendation:** Stay on GL Compatibility through the first title's release. Re-evaluate Forward+ for the next title. The lightmap baking system already works around the 16-light limit acceptably. The shader work for Forward+ migration is straightforward (mostly render_mode header changes) but carries regression risk that's not worth taking before 1.0.

### GLES3 3D Scaling: CUT Upscalers

CUT1/2/3 fragment-only upscalers for `scaling_3d_mode` on the Compatibility renderer (clean-room, ADR 0009; backlog C-12/C-13). Closes the "no compute upscaler" gap for GLES3; pixel-art-optimal. See [cut-upscalers.md](cut-upscalers.md).

---

## 9. Goblin Custom Engine Ideas to Port

The custom engine at `D:\DEV\Goblin` (raylib + daslang, v0.37.0) has several architectural ideas that are directly applicable to the Godot fork, even though the engine itself is not viable as the host engine.

### Already Adopted by the Reference Title (via Design Convergence)

| Goblin Custom Engine Idea | Reference-Title Implementation |
|---|---|
| Lego-block entity composition | Type templates + runtime dicts with fallback chains |
| Scene-first authoring → compiled world | Text-level-format sectors → level builder → ArrayMesh + collision |
| Partition streaming | Sector-based level loading (per-sector) |
| Minimal kernel, extension-first | Static system functions over node types |
| Binary world packages | Delta save/load with curated runtime state |
| Cadence-based scheduler | Cadence scheduler (tick/anim/map/low/decay cadences) |

### Ideas to Port to the Godot Fork

1. **Component Family Contracts (from ADR-0004):** Goblin Custom's 12 durable component families with stable type IDs and archive-vs-runtime separation. This could inform a GDScript-native ECS layer on top of Godot's node system for the next game.

2. **Lego-Block Entity System (daScript API):** The `goblin_ecs` module's compile-time component type registration with builder pattern could inspire a GDScript API using structs + typed containers.

3. **Scheduler Cadence Model:** Goblin Custom's `scheduler.hpp` has `FrameScheduler` with explicit cadence groups, warmup/cooldown, and budget enforcement. The reference title's scheduler already does much of this. For the fork, consider exposing cadence configuration as an engine-level feature rather than a GDScript autoload.

4. **Script Module Tier System (StableGameplay / ToolingData / ExpertExtension):** Goblin Custom's three-tier API stability model for daScript bindings. This could inform how enhanced GDScript exposes engine internals — stable gameplay APIs in tier 1, editor/data APIs in tier 2, expert/meshing APIs in tier 3.

5. **Light Probe Grid for Stealth:** Goblin Custom's retro-native RFC proposes a coarse ambient light probe grid with CPU-readable values. The reference title's viewport-based light sensor is not scalable. A first-class light probe grid as an engine feature would solve stealth detection read from world, not HUD.

6. **Basis-Frame + Position Transform Authority:** Goblin Custom avoids Euler angles and quaternions, using basis-frame transforms instead. This is a deep change but eliminates gimbal lock and axis-order ambiguity. In Godot the `Basis` type already exists — the convention could be enforced via the scripting layer.

7. **Portals & Mirrors as Custom Nodes (not first-class core):** Provided by the goblin module as custom nodes — e.g. `PortalSurface3D` / `MirrorSurface3D` extending `Node3D`/`Area3D`, registered in ClassDB. They approximate the spatial-weirdness of Ultima Underworld / System Shock / Thief via `SubViewport` + teleportation plus portal-aware query helpers, without deep core surgery. The standalone engine made portals first-class because its renderer/physics are custom; the fork does not need that — a node is the right granularity. (`PortalSurface3D` is named to avoid the upstream `Portal`/`Room` occlusion-culling nodes in Godot 4.4+.)

8. **Retro-Native Rendering (editor-provided):** Palette quantization, indexed-color looks, dithering, color cycling, and posterization as editor-provided tools — custom editor nodes/plugins (`PalettePostProcess`, `DitherPostProcess`, `ColorCycle`) — not core renderer changes. The genre set already uses pixel-art upscalers (scale2x, hq2x, eagle2x) as shaders; these can be consolidated into goblin-owned nodes exposed in the editor.

9. **Generic Spatial Field System:** The standalone engine's "ambient probe / ambient field" concept, generalized. A coarse spatial field (scalar or vector samples over a grid) that multiple systems sample from — light exposure for stealth, audio environment/reverb zones, dynamic music behavior, and effect intensity. The retro-native RFC's post-1.0 direction explicitly expands the ambient probe beyond light into "environment channels that can drive audio environment response and dynamic music behavior." This is the Thief "light field" generalized: one field infrastructure, many consumers.

10. **Native Stealth Shadow Value (Thief Light Gem):** A gameplay-facing shadow readout combining direct light, shadow occlusion between actor and lights, and ambient light — inspectable and queryable at runtime for AI, HUD, and scripting. It is the stable, temporally-smoothed gameplay readout on top of the field system (idea 9).

11. **Concrete retro-native features worth porting (from the retro-native RFC):**
    - **Per-view palette selection and blending** — palette overrides that portal views inherit (Feature 1).
    - **Texture-space animation families** — UV scroll and frame cycling driven by a global simulation clock, evaluated in-shader (Feature 3). This is the "color cycling" mechanism.
    - **Hitscan surface metadata** — raycast results that return surface class (wall/floor/ceiling), object ID, and impact UV, for weapon/trigger/material logic (Feature 9). Reference-title code already resolves surface class via slope thresholds; a native contract would formalize it.
    - **Kinematic brush movers** — doors, lifts, crushers with deterministic hull traces and blocking policy (Feature 7). Reference-title code already has kinematic movers via `AnimatableBody3D`; the mover contract is the native generalization.
    - **Lightstyle channels and surface-class lighting** — style-channel modulation of baked light plus per-surface retro class flags (Feature 8).
    - **Visibility-set override** — precomputed visibility sets that reject offscreen partitions early (Feature 6).

*Note: the scheduler/cadence idea (idea 3) and the "off-screen simulation" idea are already realized — a cadence scheduler script implements custom process groups (`tick`/`anim`/`map`/`low`/`effect`/`condition`) and a tick-based event queue with save/restore. There is nothing further to port there.*

---

## 10. Implementation Phases

### Phase 0 — Foundation (Now, during first-title development)

**Goal:** Prove the source override system works, fix the retry loops, and land the first GDScript feature (union types) from a clean base.

| Task | Effort | Risk | ADR |
|------|--------|------|-----|
| Implement source override system in `goblin/config.py` | 2-3 days | Low — additive change to config.py | 0001 |
| **Replace retry loops with compile-time overrides** (`editor_about.cpp`) | 1-2 days | Low — proves source override end-to-end | 0007 |
| Cherry-pick union types from gdscript2 into clean GDScript base | 3-5 days | Medium — extract parser/analyzer/compiler diffs, write 50-pattern test suite | 0004 |
| Run the reference title against the goblin build with union types | 1 day | Gate: all reference scripts compile and all 342 tests pass | 0004 |

### Phase 1 — Trim & More Features (During first-title development)

**Goal:** Strip unused modules, land remaining Tier 0 features, and add performance optimizations.

| Task | Effort | Risk | ADR |
|------|--------|------|-----|
| Implement module disable list in config.py | 1 day | Low — uses existing Godot mechanism | 0003 |
| Verify trimmed build runs the reference title without errors | 1-2 days | Low — all trims verified against the reference title | 0003 |
| Cherry-pick safe navigation `then` and null coalescing `elthen` | 2-3 days | Medium — tokenizer/parser changes | 0005 |
| Cherry-pick inline caching for property access | 2-3 days | Medium — VM changes, validate with profiling | — |
| Cherry-pick fast String cast | 1 day | Low — single VM change | — |
| Cherry-pick opcode fusing (high-impact fused opcodes first) | 3-5 days | Medium — integration testing | — |
| Cherry-pick PIC (polymorphic inline cache) | 2-3 days | Medium — builds on inline caching | — |
| Run the reference title against the optimized build, measure perf delta | 1 day | Gate: no regressions, measurable speedup | — |

### Phase 2 — Core Changes (Post-first-title 1.0, 2028+)

**Goal:** Surgical engine changes that directly improve the genre set's next title.

| Task | Effort | Risk |
|------|--------|------|
| Fix LightmapGI frustum culling (#71585) | 1-2 days | Low — known bug, well-scoped |
| Expose runtime LightmapBaker API | 2-3 days | Medium — API design needed |
| Add MIDI support to AudioStreamPlayer3D | 1-2 days | Low — small feature |
| Add Vector3i keys to AStar3D | 1-2 days | Low — small math change |

### Phase 3 — GDScript Deepening (Post-first-title 1.0, 2028-2029)

**Goal:** Language features the genre set needs beyond the first title.

| Task | Effort | Risk |
|------|--------|------|
| Structs / value types | 2-3 weeks | High — touches parser, analyzer, compiler, VM |
| Typed dictionaries | 1-2 weeks | Medium — builds on union type infrastructure |
| Built-in PriorityQueue | 2-3 days | Low — standalone class |
| Blocks / stack-bound callables | 2-3 weeks | Medium — new AST node + VM support |
| `yield` generators | 3-4 weeks | High — FSM compilation |

### Phase 4 — Transformative (2029+)

**Goal:** Features that fundamentally change the engine's capabilities.

| Task | Effort | Risk |
|------|--------|------|
| Generics + typeinfo | 4-6 weeks | Very high — complex across all layers |
| Zero-marshaling C++ interop | 8-12 weeks | Very high — Variant system redesign |
| Light probe grid for stealth | 2-3 weeks | Medium — new renderer feature |
| Custom renderer pass hooks (if still on GL Compat) | 3-4 weeks | High — depends on renderer architecture |

---

## 11. Testing Strategy

Every phase has concrete test gates. No feature merges without passing its gate.

### Test Layers

| Layer | What it covers | When it runs | Tool |
|-------|---------------|-------------|------|
| **Reference-Title Unit Tests** | 342 existing tests in `tests/` — combat, inventory, interaction, rule eval, quest, chargen, etc. | Phase 0-4: every change | The reference title's test runner |
| **Reference Script Corpus Compile** | All ~100+ `.gd` scripts, 25 `.gdshader` files, dynamic `GDScript.new()` compilation in the runtime value helper | Every GDScript feature change | Manual: load the reference title in the goblin editor, verify zero parse errors |
| **Reference Level Load** | The reference level loads all 30 sectors, 39 objects, 9 NPCs, builds nav graph, bakes lightmaps | Every core engine change | Level-load tooling (MCP) |
| **GDScript Feature Test Suites** | 50+ targeted tests per feature (union types, structs, etc.) written against the GDScript test framework | At feature implementation time | `modules/goblin/modules/gdscript/tests/` (fork's copy; run via `--test --test-case "[Modules][GDScript]*"` on a `tests=yes` build) |
| **Navigation Regression** | Nav graph build produces identical point counts, connections, and path results as unmodified Godot | Every change touching AStar3D, navigation, or collision | Manual: sparse-graph build comparison |
| **Combat Regression** | Identical damage values, hit/miss results, and charge/block behavior | Every change touching Variant dispatch or math | Reference unit test suite |
| **Health Scan** | Architecture conformance, missing types, anti-patterns | Every code change | The reference title's health scan |

### Per-Phase Gates

**Phase 0 Go/No-Go:**
- Source override system: `editor_about.cpp` override compiles and goblin-branded About dialog appears
- Union types: reference title loads, all 342 unit tests pass, no parse errors in any script
- Module trim: disabled module list produces a compilable build

**Phase 1 Go/No-Go:**
- `then` / `elthen`: all reference scripts compile, null-guard patterns verify correctly
- Inline caching: profile shows measurable speedup in physics/AI hot paths without regressions
- Opcode fusing: no behavioral change in combat/stealth/navigation
- Trimmed build: reference title editor launches, level loads, navigation builds, all unit tests pass

**Phase 2+ Go/No-Go:**
- LightmapGI fix: runtime lightmap baking produces identical visual results
- MIDI AudioStreamPlayer3D: instrument playback produces identical audio output
- Vector3i AStar3D keys: navigation produces identical paths (verified by path comparison harness)

### Structs De-Risking Protocol

Before implementing structs in the compiler/VM, validate the design with 50+ parser-only test cases:

1. Basic struct declaration and instantiation
2. Field access (read and write)
3. Nested structs
4. Arrays of structs
5. Dictionaries with struct values
6. Structs as function parameters and return values
7. Struct equality comparison
8. Struct copy semantics (value copy, not reference)
9. Struct in Variant (round-trip through Dictionary, save/load)
10. Struct default values
11. Struct with typed fields (including union types)
12. Array of structs sorted with sort_custom
13. Struct in dictionary key (via hash)
14. Struct in signal emission
15. Struct in @export var

All 50+ test cases must pass in the parser and analyzer before VM/compiler work begins. This de-risks the largest unknown — whether the GDScript type system can cleanly support value types.

### Regression Test Harness for Navigation

Since navigation correctness is critical for gameplay, maintain a regression harness:

```gdscript
# In tests/world/test_navigation.gd (new)
func test_graph_build_produces_identical_results():
    var level_data = load_test_level("level1")
    Navigation.build_sparse_graph(level_data, [], mock_nav_context())
    var point_count: int = Navigation.get_point_count()
    var connection_count: int = Navigation.get_connections().size()
    # These values are frozen at Godot 4.7.1 stable baseline
    assert_eq(point_count, BASELINE_POINT_COUNT, "Nav point count changed")
    assert_eq(connection_count, BASELINE_CONNECTION_COUNT, "Nav connection count changed")
```

---

## 12. Risk Assessment

### High Risk

| Risk | Mitigation |
|------|-----------|
| Cherry-picked GDScript features introduce subtle regressions in the reference corpus | Each feature lands independently with its own test suite. Reference corpus compile is the gate. Features are isolated — if union types breaks something, only union types is reverted, not all GDScript changes. |
| Struct implementation introduces subtle Variant compatibility bugs | De-risk via 50+ parser-only test cases before VM work begins (see Testing Strategy). Implement as a new Variant type with exhaustive test coverage. Ensure round-trip through Variant, Dictionary, and save/load. |
| Aggressive module trimming breaks editor functionality | Trim conservatively for editor builds (keep svg, lightmapper_rd, etc. in editor). Trim aggressively only for export templates. |
| Upstream Godot stable release rebase creates merge conflicts in overridden source files | Keep override surface small. Track which upstream releases touch overridden files. Prefer additive overrides. Rebase only at stable release boundaries, not continuously. |

### Medium Risk

| Risk | Mitigation |
|------|-----------|
| GL Compatibility deprecation by upstream Godot | Monitor upstream. GL Compatibility is kept for mobile/Web export — unlikely to be dropped. If dropped, Forward+ migration is straightforward. |
| MidiStream GDExtension breaks or needs porting | Replace with built-in MIDI support (Phase 2, item 6.3). Eliminates external dependency. |
| Additions addon (`CompoundMeshInstance3D`, `AreaLight3D`) breaks | These are used by the reference title's emitter system. `CompoundMeshInstance3D` could be promoted to an engine feature. `AreaLight3D` can be replaced with OmniLight + custom culling. |

### Low Risk

| Risk | Mitigation |
|------|-----------|
| Module trim breaks unexpected engine internals | Test suite catches this. Trim list derived from systematic analysis of every module against the reference title's project config, scripts, and content. |
| Source override system has subtle build-ordering bugs | SCons `configure()` runs before any `SCsub` — guaranteed. Fallback to original file if goblin override doesn't exist on disk. |
| The reference title doesn't run on the trimmed fork | Full verification pass (Phase 1). Trim list is conservative — only modules verified as unused are trimmed. |

---

## Appendix A: Files to Create/Modify in `modules/goblin/`

```
modules/goblin/
├── config.py                      # [MODIFIED] configure() hooks: builders, goblin_add_library(), module trim
├── SCsub                          # [MODIFIED] GOBLIN_MODULE_OVERRIDES (whole-module swap)
├── goblin_builders.py             # [MODIFIED] Branding builders
├── register_types.cpp/h           # [MODIFIED] Module registration
├── core/                          # Mirror for core file overrides
│   ├── variant/variant_construct.{cpp,h}   # [NEW] String ctors
│   └── version_override.py + AUTHORS/DONORS/COPYRIGHT/LICENSE  # [NEW] Branding
├── modules/gdscript/              # [NEW] GDScript fork (whole-module override)
├── editor/                        # branding_translations.{cpp,h} (runtime fallback), icons/, README
│   └── overrides/                 # [NEW] Editor file mirrors (never globbed by SCsub)
│       ├── gui/editor_about.cpp              # About dialog (Goblin literals, no Donors tab)
│       ├── export/project_export.{cpp,h}     # Export dialog (debug-template-aware option)
│       ├── project_manager/project_manager.cpp  # PM (Donate button removed)
│       └── editor_node.cpp                   # EditorNode (Support item removed)
├── main/                          # splash, splash_editor, app icon
├── platform/windows/              # goblin.rc
├── tools/                         # sync_godot_icons.py
└── docs/                          # This document, backlog, ADRs, CODE_MAP
```

Editor file mirrors (backlog B-04, ADR 0007): the four files above are swapped in at build time by `goblin_add_library()`. The originally proposed `goblin_source_overrides.py` / interception layer was rejected (see §4); the `overrides/` directory name is reused for the accepted mirror home.

## Appendix B: Genre Requirements → Fork Solution Map

| Genre Requirement | Severity | Solution | Phase |
|---|---|---|---|
| No structs — 60+ `duplicate(true)` calls | High | GDScript structs | Phase 3 |
| O(N²) Dijkstra — no priority queue | High | Built-in PriorityQueue | Phase 3 |
| Dict overhead in physics hot paths | High | Structs (physics data is ideal struct candidate) | Phase 3 |
| LightmapGI frustum culling bug | Critical | Fix #71585 in core | Phase 2 |
| Runtime LightmapBaker requires custom module | High | Promote to public API | Phase 2 |
| Callable overhead in procedural meshing | High | Blocks + opcode fusing (Phase 1) | Phase 1 |
| AI time-slicing (can't do per-frame N-agent updates) | High | GDScript perf (opcode fusing, PIC, then structs) | Phases 1-3 |
| ~30 Variant checks in navigation builder | Medium | Union types (already in gdscript2) + typed dicts | Phase 0 + Phase 3 |
| 33+ `sort_custom` lambda allocations | Medium | Opcode fusing (fused iterator opcodes) | Phase 1 |
| Variant shape-size branching in physics | Medium | Union types (already in gdscript2) | Phase 0 |
| 3D MIDI player requires manual construction | Medium | Built-in AudioStreamPlayer3D MIDI | Phase 2 |
| String-format keys in hot navigation path | Medium | Vector3i keys for AStar3D | Phase 2 |
| LUT generation bound by GDScript (135ms for 48³) | Low | GDScript perf + structs | Phases 1-3 |
| Editor retry loops (120-attempt polling for SceneTree) | Low | Compile-time file overrides (ADR 0007) — done | Phase 1 |
| `Array[StringName].sort()` unreliable | Low | Opcode fusing (fused sort paths) | Phase 1 |

## Appendix C: Decision Log

| Decision | Rationale | Date |
|----------|-----------|------|
| Rebase fork on Godot 4.7.1-stable (a13da4feb8d) | Move from tracking upstream `master` to tracking stable releases on a single `master` branch. Progressive rebase at each stable release boundary. | 2026-08-11 |
| Single-branch over branch-per-release | Branch-per-release adds archaeology complexity with zero value for a single-project fork. `master` is the one branch; rebase it at each stable release. | 2026-08-11 |
| Fork approach over custom engine | Custom engine (Goblin raylib+daslang) is 2-3 years from viable. Fork preserves all reference-title work, editor, asset pipeline. | 2026-08-11 |
| Source override over core patching | Allows all changes to live in `modules/goblin/`. Clean rebase surface. | 2026-08-11 |
| ADR governance for all structural decisions | Production-quality fork requires rigid architectural constraints, not experimental features. Modeled on Goblin Custom Engine governance. | 2026-08-11 |
| Stable-release tracking over master | Reduces merge churn, avoids half-baked upstream features, ensures production-tested base. Rebase only at tagged stable releases. | 2026-08-11 |
| Cherry-pick GDScript features from clean base | gdscript2 has partial breakage and unmerged branches. Port individual features one at a time, each with its own ADR and test gate. | 2026-08-11 |
| Stay on GL Compatibility through the first title's release | Avoids regression risk from Forward+ migration. Lightmap system already works around GL limits. | 2026-08-11 |
| Trim modules rather than stub them | Simpler build, fewer dependencies, faster compile. Stubs create maintenance burden and silent failure modes. | 2026-08-11 |
| Keep all networking modules | User has multiplayer plans. Trim would be premature. | 2026-08-11 |
| Keep all target platforms | User wants cross-platform support. Platform removal is easy later, hard to restore. | 2026-08-11 |
| Trim navigation_3d module | Verified: the reference title uses AStar3D (core math, not a module). NavigationServer3D/Agent3D/Region3D completely unused. | 2026-08-11 |
| Keep noise module | User wants FastNoiseLite for potential procedural content. | 2026-08-11 |
| Trim VR/XR modules | Not needed. Trivially re-addable via module enable flag. | 2026-08-11 |
| Retry-loop fix in Phase 0 | Most embarrassing code in the fork. Proves source override system end-to-end. | 2026-08-11 |
