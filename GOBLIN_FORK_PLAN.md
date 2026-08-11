# Goblin Engine — Lightweight Godot Fork Plan

**Version:** 0.2.0 (draft)
**Date:** 2026-08-11
**Author:** Filip Anton (filipworksdev)
**Status:** Active — Phase 0, based on Godot 4.7.1-stable (commit a13da4feb8d)

---

## 1. Vision & Philosophy

### What This Fork Is

A deliberately customized, lightweight fork of Godot Engine tailored specifically for immersive sim / systems-heavy FPS RPG development. It preserves full compatibility with the DB project's existing code and assets while removing unused engine components and extending the engine where it limits the game.

### What This Fork Is NOT

- Not a general-purpose engine fork for broad consumption
- Not a "GDScript replacement" or "GDScript vs daScript" project
- Not a full rewrite — it preserves Godot's editor, renderer, and asset pipeline
- Not a rebranding project (that already exists in `modules/goblin/`)

### Core Principles

1. **Source Override, Not Source Modification.** All changes live inside `modules/goblin/`. Godot core files are never modified. The build system redirects compilation to goblin-owned replacement files.

2. **Surgical, Not Sweeping. Project-Specific Only.** Every core change must justify its existence against a concrete DB project pain point. No speculative over-engineering. This fork serves ONE project — not the general Godot community. Features that benefit "most users" but don't serve DB have zero priority. Core engine edits are a LAST RESORT, only when a module override or GDScript extension is impossible.

3. **Trim Aggressively, Keep Compatibility.** Strip unused modules when evidence confirms they are unused. The trimmed binary still runs DB unmodified.

4. **Single-Branch Progressive Stable Tracking.** `master` is the one and only branch. It tracks official Godot stable releases, never the upstream `master` development branch. At each new stable release (e.g., 4.7.2, 4.8.0), `master` is rebased onto that release tag and goblin changes are re-applied. No branch-per-release archaeology — this fork serves one project and complexity is waste. Current base: `4.7.1-stable` (commit a13da4feb8d).

5. **ADR-Governed Architecture.** All structural decisions — GDScript language extensions, core engine modifications, module architecture, renderer changes — go through Architecture Decision Records (ADRs) modeled on the Goblin Custom Engine's governance. No feature lands without a locked ADR. The ADR must specify: what changes, why it's justified against a concrete DB pain point, what files are touched, what the merge conflict surface is, and what tests gate acceptance.

6. **Cherry-Pick GDScript Features, Don't Fork Wholesale.** The existing `gdscript2` module has partial breakage and untracked complexity across multiple unmerged branches. Instead of merging it wholesale, start from clean upstream GDScript and port individual features one at a time: union types first, then `?.`/`??`, then performance optimizations. Each feature lands as its own tested, reviewed change.

7. **Minimal Override Surface.** Every overridden source file is a merge-conflict liability. Prefer additive overrides (new files that ADD functionality to a module) over replacement overrides (files that completely replace upstream). When replacement is necessary, document the exact divergence and track which upstream releases touch the overridden file.

---

## 2. Architecture Governance

This fork follows the same ADR-driven governance as the Goblin Custom Engine. No structural decision lands without a locked Architecture Decision Record.

### ADR Template

Each ADR must address:
- **Status:** Proposed → Accepted → Superseded
- **Context:** What concrete DB pain point or engine limitation justifies this change?
- **Decision:** What exactly changes? What files are touched? What is the override surface?
- **Alternatives Considered:** What other approaches were rejected and why?
- **Consequences:** What becomes easier? What becomes harder? What is the merge-conflict surface?
- **Test Gates:** What tests must pass for this ADR to be accepted?
- **Stable Release Target:** Which Godot stable release tag is this based on?

### ADR Categories

| Category | Example Decisions | Governing Principle |
|----------|------------------|---------------------|
| **Language** | Union types, structs, typed dicts | Every syntax addition must solve a concrete DB pain point. No speculative language features. |
| **Engine Core** | LightmapGI fix, MIDI support, AStar3D keys | Surgical changes only. Minimum files touched. Must be upstream-acceptable where possible. |
| **Module** | Which modules to trim, which to keep | Evidence-based: verified against DB project config, script corpus, and content files. |
| **Renderer** | GL Compat vs Forward+, custom render passes | DB 1.0 compatibility is non-negotiable. Renderer changes are post-1.0 only. |
| **Build System** | Source override mechanism, SCons hooks | Must not modify Godot build files. All changes in `modules/goblin/`. |

### ADR Lifecycle

1. **Proposal:** Draft ADR in `modules/goblin/docs/adr/`. Must include concrete evidence (DB code references, benchmark data, module dependency analysis).
2. **Review:** Cross-check against existing ADRs for conflicts. Verify test gates are specific and measurable.
3. **Acceptance:** Lock the ADR. Implementation begins only after acceptance.
4. **Implementation:** Code changes live in `modules/goblin/overrides/`. Test suite gates the merge.
5. **Supersedure:** If a later ADR replaces this decision, the original is marked Superseded with a pointer to the replacement.

### Current ADRs (to be created)

| ADR # | Topic | Status |
|-------|-------|--------|
| 0001 | Source override system architecture | Proposed |
| 0002 | Stable-release tracking strategy | Proposed |
| 0003 | Module trim methodology and evidence standard | Proposed |
| 0004 | GDScript union types — port from gdscript2 | Proposed |
| 0005 | GDScript safe navigation / null coalescing | Proposed |
| 0006 | Runtime lightmap API surface | Proposed |
| 0007 | Goblin branding — replace retry loops with compile-time overrides | Proposed |

---

## 3. Current State Analysis

### Existing Goblin Module (`modules/goblin/`)

**What it does:**
- SCons builder monkey-patching: replaces version info, authors, donors, license, splash, app icon, binary name
- Runtime UI patching: translation injection for "Godot" → "Goblin" strings, donate button removal
- Export dialog tweaks: architecture filtering, template detection

**Limitations:**
- **Cannot replace C++ source files.** The monkey-patching operates at the Python/SConf function level — it can swap build targets (splash, icons, version info) but cannot tell SCons "compile my .cpp instead of the original."
- **Inefficient retry loops.** `goblin_about.cpp` and `goblin_export.cpp` both poll `SceneTree` availability with hardcoded 120-attempt bounds and no backoff. Each deferred retry burns an idle frame.
- **Fragile coupling.** The `config.py` → `goblin_builders.py` handoff relies on module-level dictionary injection before any builder runs. If import order changes, this breaks silently.
- **No source-level integration.** All runtime changes are done via translation strings and scene-tree scanning. Compile-time string replacement is impossible.

### Existing GDScript2 Module (`modules/gdscript2/`)

**Implemented on HEAD (7 commits):**
- Union types (`int | String`, `Dictionary | null`)
- `swap(a, b)` built-in function
- Safe navigation `?.` (keyword `then`) 
- Null coalescing `??` (keyword `elthen`)
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

### DB Project Engine Dependencies

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

### The Problem

The current `modules/goblin/` can only ADD source files to the build. To truly customize Godot, we need the ability to REPLACE specific `.cpp`/`.h` files from any module with goblin-owned versions — all without modifying Godot's build files.

### The Mechanism

Godot collects module sources through a clean pipeline:
```
SConstruct → detect_modules() → config.py:configure() → modules/SCsub
                                                           │
                                           for each module:
                                             env.modules_sources = []
                                             SConscript(module + "/SCsub")
                                               → add_source_files(sources, "*.cpp")
                                             add_library("module_X", sources)
```

The interception point is `env.add_source_files()`. By monkey-patching it in `config.py:configure()` (which runs before any SCsub), we can check `env.current_module` (set by SConstruct line 1102) and substitute goblin-owned files for originals during the glob expansion.

### Implementation

**New files:**
```
modules/goblin/
├── goblin_source_overrides.py    # Declarative override map
└── overrides/                    # Replacement source tree
    ├── gdscript/                 # Forked GDScript files
    │   ├── gdscript_compiler.cpp
    │   └── ...
    ├── editor/                   # Editor UI overrides
    │   └── editor_about.cpp      # Replaces hardcoded strings at compile time
    └── scene/                    # Scene node overrides
        └── ...
```

**`goblin_source_overrides.py`:**
```python
OVERRIDE_MAP = {
    "gdscript": {
        # filename → relative path to goblin override
        "gdscript_compiler.cpp": "overrides/gdscript/gdscript_compiler.cpp",
    },
    "editor": {
        "editor_about.cpp": "overrides/editor/editor_about.cpp",
    },
}
```

**`config.py` addition:** Intercept `env.add_source_files()`. When `env.current_module` matches an override map entry, substitute matching filenames with goblin paths. Files that don't exist on disk fall back gracefully to originals.

### What This Enables

| Current Limitation | Solved By |
|---|---|
| Can't modify GDScript compiler | Override `gdscript_compiler.cpp` in goblin |
| Hardcoded "Godot" strings in About dialog | Override `editor_about.cpp` — no more retry loops |
| Can't add renderer hooks | Override `renderer_scene_render_rd.cpp` to inject callbacks |
| Can't modify physics behavior | Override specific physics body files |
| Awkward retry loops for SceneTree hooking | Replace with compile-time changes in the overridden source |

### Eliminating the Retry Loops

With source overrides, `goblin_about.cpp`'s SceneTree polling loop (120 attempts at one-per-idle-frame) can be replaced by directly modifying `editor_about.cpp` to use goblin branding strings at compile time. The runtime translation injection can be kept as a fallback, but the primary mechanism becomes compile-time — zero runtime overhead, zero polling.

---

## 5. Module Trim Plan

### Methodology

Each module was assessed against:
1. Does DB directly use the classes or APIs it provides?
2. Is it a dependency of another module DB uses?
3. Would removing it break editor functionality needed for DB development?

### Keep List (~20 modules)

| Module | Reason | Can Runtime-Trim? |
|--------|--------|-------------------|
| `gdscript` | Entire codebase | No |
| `freetype` | TTF font rasterization | No |
| `msdfgen` | MSDF font generation (enabled in project) | No |
| `text_server_adv` | All text rendering (HarfBuzz) | No |
| `jolt_physics` | Selected physics engine | No |
| `regex` | CaveIni parser, dice rolling, string tools | No |
| `goblin` | Fork identity + override system | No |
| `ogg` | OGG container (dep of vorbis) | No |
| `vorbis` | OGG Vorbis audio decoder | No |
| `mp3` | MP3 audio decoder (used in shot.gd) | No |
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

**Image/Texture Formats (12 modules):**
`bmp`, `tga`, `dds`, `hdr`, `jpg`, `webp`, `tinyexr`, `basis_universal` (runtime), `ktx`, `astcenc`, `etcpak`, `svg` (runtime)

*DB uses only PNG. All other image formats are dead code.*

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

*Verified: DB's `navigation.gd` (1648 lines) uses only `AStar3D` — a core math class in `core/math/`, NOT part of the `navigation_3d` module. Zero references to `NavigationServer3D`, `NavigationAgent3D`, `NavigationRegion3D`, or `NavigationMesh`. The `navigation_3d` module provides these unused classes. `AStar3D` is preserved as core.*

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
- **Risk:** Low — all trimmed modules verified unused by DB project

### Trimming Mechanism

Module trimming uses Godot's existing `module_check_dependencies()` disabling mechanism. In `goblin/config.py:configure()`:

```python
# After the source override hook, disable modules DB doesn't use
DISABLE_MODULES = {
    "bmp", "tga", "dds", "hdr", "jpg", "webp", "tinyexr",
    "basis_universal", "ktx", "astcenc", "etcpak",
    "theora", "interactive_music",
    "webxr", "openxr", "mobile_vr",
    "csg", "gridmap", "gltf", "fbx",
    "navigation_3d", "navigation_2d",
    "godot_physics_3d", "godot_physics_2d",
    "glslang", "camera", "zip", "raycast",
}

env.disabled_modules = list(getattr(env, 'disabled_modules', []))
for mod in DISABLE_MODULES:
    if mod not in env.disabled_modules:
        env.disabled_modules.append(mod)
```

This leverages Godot's existing `module_check_dependencies()` in `methods.py` — modules in `disabled_modules` are skipped during discovery and their dependencies cascade.

---

## 6. GDScript Feature Roadmap

### Tier 0 — Cherry-Pick Core Features (Phase 0, from clean GDScript base)

**Strategy:** Start from the upstream GDScript at the tracked stable release tag. Do NOT merge the existing `gdscript2` module wholesale — it has partial breakage and unmerged branches with unknown interactions. Instead, port individual features one at a time into `modules/goblin/overrides/gdscript/`. Each feature lands as its own tested, ADR-governed change.

**Feature 0a — Union Types (`int | String`, `Dictionary | null`)**
- Highest impact / lowest risk of the gdscript2 features
- Solves ~30 `typeof()` + `as` variant-checking patterns in navigation builder alone
- Replaces `Variant` branching in physics shape-size code
- Port approach: extract the union type changes from gdscript2's parser/analyzer/compiler diffs. Add union type test suite (50+ patterns). Run against DB script corpus.

**Feature 0b — Safe Navigation `?.` and Null Coalescing `??`**
- Replace hundreds of `if x != null:` guards across the codebase
- Port approach: extract the THAN/ELTHEN tokenizer changes + binary op node changes from gdscript2
- Rename keywords from `then`/`elthen` to `?.`/`??` syntax for readability

**Feature 0c — Inline Caching for Property Access**
- Monomorphic property cache (4 opcodes: SET/GET named/member)
- Port approach: extract bytecode gen slot reservation + VM fast-path changes
- Validate: profile hot paths in DB's physics (`_physics_process`) and AI detection

**Feature 0d — Fast String Cast**
- Trivial optimization: `as String` fast path in VM
- Port approach: single change in `gdscript_vm.cpp`
- Validate: benchmark string-heavy code paths (navigation keys, CaveIni parsing)

**ADR Required:** Each feature gets its own ADR documenting: what changes, exact files touched in `overrides/gdscript/`, what test suite gates acceptance, what upstream GDScript behavior is preserved vs modified.

### Tier 1 — Performance Optimizations (Phase 1, post-Tier-0 stabilization)

**Opcode Fusing (40+ fused opcodes):**
Extract from gdscript2's `opcode_fusing` branch. Fused opcodes for typed array/dict access, integer arithmetic, fast iteration, fused condition+dispatch. Each fused opcode is an independent change — port the highest-impact ones first (array access, dict iteration, integer arithmetic).

**PIC (Polymorphic Inline Cache):**
Extract from gdscript2's `pic` branch. Upgrade monomorphic cache to 4-way PIC with negative caching. Extend to method calls.

**Impact on DB:** Eliminates per-frame lambda allocation for `sort_custom` (33+ call sites). Dictionary hot paths in combat/stealth/conditions see 2-5x speedup.

### Tier 2 — Language Features (Post-DB 1.0, high impact)

**Structs / Value Types:**
The single biggest language gap. DB's entire entity model is `Dictionary` because GDScript lacks value types:
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

### Tier 3 — Language Deepening (Post-DB 1.0, medium impact)

**Blocks / Stack-Bound Callables:**
Zero-allocation callbacks that capture by reference and cannot escape scope. Replaces `sort_custom` lambda allocations (33+ sites) and AI filter lambdas with zero-cost alternatives.

**`yield` Generators:**
Generator-based lazy iteration (`generator func` → compiled to finite state machine). Eliminates intermediate array allocations for query/filter/map chains.

**Comprehensions:**
`[for x in data: transform(x) if condition]` — syntactic sugar that compiles to efficient typed loops. Replaces `filter()` + `map()` patterns with readable, allocation-free code.

**`var inscope` / RAII:**
Compiler inserts `finally` block for cleanup on scope exit. Useful for resource handles, lock guards, temporary allocations.

### Tier 4 — Transformative (Long-term, very high impact)

**Generics with `typeinfo`:**
daslang-style compile-time generics with type reflection. Enables typed containers (`Array[MyStruct]` with full type safety), generic algorithms (sort, filter, map over typed collections without boxing), and template specialization.

**Zero-Marshaling C++ Interop:**
Script data laid out at C++-compatible offsets. `ManagedStructureAnnotation` allows scripts to read/write engine data directly at C++ addresses. This is the hardest feature but the most transformative — it eliminates the single biggest performance cost in Godot's scripting model.

---

## 7. Core Engine Changes

These are surgical modifications to Godot core files that cannot be achieved through module overrides or GDScript extensions. Ordered by justification clarity.

### 6.1 LightmapGI Frustum Culling Fix (#71585)

**DB Pain Point:** LightmapGI node frustum culling breaks lightmap injection when the node is out of view. The entire runtime lightmap pipeline (~17 functions in `level.gd`) partially exists to work around this.

**Change:** Fix the frustum culling logic in `LightmapGI` so lightmap injection is independent of node visibility. This is a known upstream Godot bug (issue #71585).

**Files touched:** ~1-2 in `scene/3d/lightmap_gi.cpp` or `servers/rendering/renderer_scene_cull.cpp`

**Justification:** Fixes a shipped engine bug that directly impacts DB's lighting quality. Upstream would accept this fix.

### 6.2 Runtime LightmapBaker API Surface

**DB Pain Point:** Runtime lightmap baking requires `ClassDB.class_exists("LightmapBaker")` guard — the baker class is a custom module, not in standard Godot builds. `lightmap_unwrap()` and runtime `bake()` are gated behind custom additions.

**Change:** Expose `lightmap_unwrap()` and `bake()` on `LightmapGI` as public API, or promote the `LightmapBaker` class to a first-class engine feature. This eliminates the `ClassDB` guard and makes runtime baking a standard capability.

**Files touched:** ~1-2 in `scene/3d/lightmap_gi.cpp/.h`

**Justification:** DB's level editor needs runtime lightmap baking for procedural geometry. Making this a standard API reduces the custom module surface.

### 6.3 MIDI Support in AudioStreamPlayer3D

**DB Pain Point:** 3D spatialized MIDI playback requires manual `AudioStreamPlayer3D` construction + `MidiStream` cloning + soundfont copying. The current code creates players dynamically per critter body and manually copies soundfont.

**Change:** Add `midi_stream` property to `AudioStreamPlayer3D` (or `AudioStreamPlayer`) so MIDI playback is a first-class feature with automatic spatialization.

**Files touched:** ~1-2 in `scene/audio/audio_stream_player_3d.cpp/.h`

**Justification:** DB has instrument-playing NPCs (GDR-009b) and MIDI-based adaptive music. This is a small change with large gameplay impact.

### 6.4 Vector3i Keys for AStar3D

**DB Pain Point:** Navigation edge tracking uses string-format keys (`"%d|%d|%d"` in hot paths). String allocation and hashing in inner navigation loops.

**Change:** Expose `Vector3i` as a key type in AStar3D's internal point lookup, or add an overload that accepts integer triples. `Vector3i` already exists as a Variant type — just needs to be used as a hash key internally.

**Files touched:** ~1-2 in `core/math/a_star.cpp/.h` or `core/math/a_star_3d.cpp/.h`

**Justification:** Small core change, eliminates string allocation in a hot path, benefits any project using AStar3D with coordinate-based keys.

---

## 8. Renderer Strategy

### Current: GL Compatibility (OpenGL ES 3.0)

**Pros:**
- DB is built and tested on it
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
- DB's GL Compatibility-specific workarounds need audit
- Build includes `vulkan/` driver and `glslang/` module
- Testing burden: full visual regression pass needed

**Recommendation:** Stay on GL Compatibility through DB 1.0. Re-evaluate Forward+ for the next game. The lightmap baking system already works around the 16-light limit acceptably. The shader work for Forward+ migration is straightforward (mostly render_mode header changes) but carries regression risk that's not worth taking before 1.0.

---

## 9. Goblin Custom Engine Ideas to Port

The custom engine at `D:\DEV\Goblin` (raylib + daslang, v0.37.0) has several architectural ideas that are directly applicable to the Godot fork, even though the engine itself is not viable as a DB host.

### Already Adopted by DB (via Design Convergence)

| Goblin Custom Engine Idea | DB Implementation |
|---|---|
| Lego-block entity composition | DB's type templates + runtime dicts with fallback chains |
| Scene-first authoring → compiled world | DB's CaveIni sectors → Level builder → ArrayMesh + collision |
| Partition streaming | DB's sector-based level loading (per-sector) |
| Minimal kernel, extension-first | DB's static system functions over node types |
| Binary world packages | DB's delta save/load with curated runtime state |
| Cadence-based scheduler | DB's Scheduler (tick/anim/map/low/decay cadences) |

### Ideas to Port to the Godot Fork

1. **Component Family Contracts (from ADR-0004):** Goblin Custom's 12 durable component families with stable type IDs and archive-vs-runtime separation. This could inform a GDScript-native ECS layer on top of Godot's node system for the next game.

2. **Lego-Block Entity System (daScript API):** The `goblin_ecs` module's compile-time component type registration with builder pattern could inspire a GDScript API using structs + typed containers.

3. **Scheduler Cadence Model:** Goblin Custom's `scheduler.hpp` has `FrameScheduler` with explicit cadence groups, warmup/cooldown, and budget enforcement. DB's Scheduler already does much of this. For the fork, consider exposing cadence configuration as an engine-level feature rather than a GDScript autoload.

4. **Script Module Tier System (StableGameplay / ToolingData / ExpertExtension):** Goblin Custom's three-tier API stability model for daScript bindings. This could inform how enhanced GDScript exposes engine internals — stable gameplay APIs in tier 1, editor/data APIs in tier 2, expert/meshing APIs in tier 3.

5. **Light Probe Grid for Stealth:** Goblin Custom's retro-native RFC proposes a coarse ambient light probe grid with CPU-readable values. DB's `LightSensor` (viewport-based sampling) is not scalable. A first-class light probe grid as an engine feature would solve GDR-009a (stealth detection read from world, not HUD).

6. **Basis-Frame + Position Transform Authority:** Goblin Custom avoids Euler angles and quaternions, using basis-frame transforms instead. This is a deep change but eliminates gimbal lock and axis-order ambiguity. For DB, the `Basis` type already exists — the convention could be enforced via the scripting layer.

---

## 10. Implementation Phases

### Phase 0 — Foundation (Now, during DB development)

**Goal:** Prove the source override system works, fix the retry loops, and land the first GDScript feature (union types) from a clean base.

| Task | Effort | Risk | ADR |
|------|--------|------|-----|
| Implement source override system in `goblin/config.py` | 2-3 days | Low — additive change to config.py | 0001 |
| Create `goblin_source_overrides.py` with initial empty map | 1 hour | None | 0001 |
| **Replace retry loops with compile-time overrides** (`editor_about.cpp`) | 1-2 days | Low — proves source override end-to-end | 0007 |
| Cherry-pick union types from gdscript2 into clean GDScript base | 3-5 days | Medium — extract parser/analyzer/compiler diffs, write 50-pattern test suite | 0004 |
| Run DB project against goblin build with union types | 1 day | Gate: all DB scripts compile and all 342 tests pass | 0004 |

### Phase 1 — Trim & More Features (During DB development)

**Goal:** Strip unused modules, land remaining Tier 0 features, and add performance optimizations.

| Task | Effort | Risk | ADR |
|------|--------|------|-----|
| Implement module disable list in config.py | 1 day | Low — uses existing Godot mechanism | 0003 |
| Verify trimmed build runs DB without errors | 1-2 days | Low — all trims verified against DB | 0003 |
| Cherry-pick safe navigation `?.` and null coalescing `??` | 2-3 days | Medium — tokenizer/parser changes | 0005 |
| Cherry-pick inline caching for property access | 2-3 days | Medium — VM changes, validate with profiling | — |
| Cherry-pick fast String cast | 1 day | Low — single VM change | — |
| Cherry-pick opcode fusing (high-impact fused opcodes first) | 3-5 days | Medium — integration testing | — |
| Cherry-pick PIC (polymorphic inline cache) | 2-3 days | Medium — builds on inline caching | — |
| Run DB project against optimized build, measure perf delta | 1 day | Gate: no regressions, measurable speedup | — |

### Phase 2 — Core Changes (Post-DB 1.0, 2028+)

**Goal:** Surgical engine changes that directly improve DB's successor.

| Task | Effort | Risk |
|------|--------|------|
| Fix LightmapGI frustum culling (#71585) | 1-2 days | Low — known bug, well-scoped |
| Expose runtime LightmapBaker API | 2-3 days | Medium — API design needed |
| Add MIDI support to AudioStreamPlayer3D | 1-2 days | Low — small feature |
| Add Vector3i keys to AStar3D | 1-2 days | Low — small math change |

### Phase 3 — GDScript Deepening (Post-DB 1.0, 2028-2029)

**Goal:** Language features that DB didn't have but the next game needs.

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
| **DB Unit Tests** | 342 existing tests in `tests/` — combat, inventory, interaction, rule eval, quest, chargen, etc. | Phase 0-4: every change | `run_test.bat` / `run_test_and_health.bat` |
| **DB Script Corpus Compile** | All ~100+ `.gd` scripts, 25 `.gdshader` files, dynamic `GDScript.new()` compilation in `Var.gd` | Every GDScript feature change | Manual: load DB project in goblin editor, verify zero parse errors |
| **DB Level Load** | `level1.txt` loads all 30 sectors, 39 objects, 9 NPCs, builds nav graph, bakes lightmaps | Every core engine change | `db_level_load("level1")` via MCP tools |
| **GDScript Feature Test Suites** | 50+ targeted tests per feature (union types, structs, etc.) written against the GDScript test framework | At feature implementation time | `modules/gdscript/tests/` (upstream) or `modules/goblin/tests/` |
| **Navigation Regression** | Nav graph build produces identical point counts, connections, and path results as unmodified Godot | Every change touching AStar3D, navigation, or collision | Manual: `_build_sparse_graph` comparison |
| **Combat Regression** | Identical damage values, hit/miss results, and charge/block behavior | Every change touching Variant dispatch or math | DB unit test suite |
| **Health Scan** | `run_health.bat` — architecture conformance, missing types, anti-patterns | Every code change | `run_test_and_health.bat` |

### Per-Phase Gates

**Phase 0 Go/No-Go:**
- Source override system: `editor_about.cpp` override compiles and goblin-branded About dialog appears
- Union types: DB project loads, all 342 unit tests pass, no parse errors in any script
- Module trim: disabled module list produces a compilable build

**Phase 1 Go/No-Go:**
- `?.` / `??`: all DB scripts compile, null-guard patterns verify correctly
- Inline caching: profile shows measurable speedup in physics/AI hot paths without regressions
- Opcode fusing: no behavioral change in combat/stealth/navigation
- Trimmed build: DB editor launches, level loads, navigation builds, all unit tests pass

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
| Cherry-picked GDScript features introduce subtle regressions in DB's script corpus | Each feature lands independently with its own test suite. DB script corpus compile is the gate. Features are isolated — if union types breaks something, only union types is reverted, not all GDScript changes. |
| Struct implementation introduces subtle Variant compatibility bugs | De-risk via 50+ parser-only test cases before VM work begins (see Testing Strategy). Implement as a new Variant type with exhaustive test coverage. Ensure round-trip through Variant, Dictionary, and save/load. |
| Aggressive module trimming breaks editor functionality | Trim conservatively for editor builds (keep svg, lightmapper_rd, etc. in editor). Trim aggressively only for export templates. |
| Upstream Godot stable release rebase creates merge conflicts in overridden source files | Keep override surface small. Track which upstream releases touch overridden files. Prefer additive overrides. Rebase only at stable release boundaries, not continuously. |

### Medium Risk

| Risk | Mitigation |
|------|-----------|
| GL Compatibility deprecation by upstream Godot | Monitor upstream. GL Compatibility is kept for mobile/Web export — unlikely to be dropped. If dropped, Forward+ migration is straightforward. |
| MidiStream GDExtension breaks or needs porting | Replace with built-in MIDI support (Phase 2, item 6.3). Eliminates external dependency. |
| Additions addon (`CompoundMeshInstance3D`, `AreaLight3D`) breaks | These are used by Emitter. `CompoundMeshInstance3D` could be promoted to an engine feature. `AreaLight3D` can be replaced with OmniLight + custom culling. |

### Low Risk

| Risk | Mitigation |
|------|-----------|
| Module trim breaks unexpected engine internals | Test suite catches this. Trim list derived from systematic analysis of every module against DB's project config, scripts, and content. |
| Source override system has subtle build-ordering bugs | SCons `configure()` runs before any `SCsub` — guaranteed. Fallback to original file if goblin override doesn't exist on disk. |
| DB project doesn't run on trimmed fork | Full verification pass (Phase 1). Trim list is conservative — only modules verified as unused are trimmed. |

---

## Appendix A: Files to Create/Modify in `modules/goblin/`

```
modules/goblin/
├── config.py                      # [MODIFY] Add source override hook + module disable list
├── goblin_source_overrides.py     # [NEW] Declarative override map
├── goblin_module_trims.py         # [NEW] Module disable list with documentation
├── overrides/                     # [NEW] Replacement source tree
│   ├── gdscript/                  # Forked GDScript module files (from gdscript2)
│   │   ├── gdscript_compiler.cpp  # With struct/generics changes
│   │   ├── gdscript_parser.cpp    # With struct/generics changes
│   │   ├── gdscript_analyzer.cpp  # With struct/generics changes
│   │   ├── gdscript_vm.cpp        # With performance optimizations
│   │   └── ... (all modified files)
│   ├── editor/
│   │   └── editor_about.cpp       # Goblin-branded About dialog (no retry loops)
│   └── scene/
│       └── 3d/
│           └── lightmap_gi.cpp    # Frustum culling fix + runtime bake API
├── editor/
│   ├── goblin_about.cpp           # [SIMPLIFY] Remove retry loops (replaced by source override)
│   └── goblin_export.cpp          # [SIMPLIFY] Same
└── docs/
    └── GOBLIN_FORK_PLAN.md        # This document
```

## Appendix B: DB Project Pain Point → Fork Solution Map

| DB Pain Point | Severity | Solution | Phase |
|---|---|---|---|
| No structs — 60+ `duplicate(true)` calls | High | GDScript structs | Phase 3 |
| O(N²) Dijkstra — no priority queue | High | Built-in PriorityQueue | Phase 3 |
| Dict overhead in physics hot paths | High | Structs (physics data is ideal struct candidate) | Phase 3 |
| LightmapGI frustum culling bug | Critical | Fix #71585 in core | Phase 2 |
| Runtime LightmapBaker requires custom module | High | Promote to public API | Phase 2 |
| Callable overhead in voxel mesher | High | Blocks + opcode fusing (Phase 1) | Phase 1 |
| AI time-slicing (can't do per-frame N-agent updates) | High | GDScript perf (opcode fusing, PIC, then structs) | Phases 1-3 |
| ~30 Variant checks in navigation builder | Medium | Union types (already in gdscript2) + typed dicts | Phase 0 + Phase 3 |
| 33+ `sort_custom` lambda allocations | Medium | Opcode fusing (fused iterator opcodes) | Phase 1 |
| Variant shape-size branching in physics | Medium | Union types (already in gdscript2) | Phase 0 |
| 3D MIDI player requires manual construction | Medium | Built-in AudioStreamPlayer3D MIDI | Phase 2 |
| String-format keys in hot navigation path | Medium | Vector3i keys for AStar3D | Phase 2 |
| LUT generation bound by GDScript (135ms for 48³) | Low | GDScript perf + structs | Phases 1-3 |
| Editor retry loops (120-attempt polling for SceneTree) | Low | Source override → compile-time changes | Phase 1 |
| `Array[StringName].sort()` unreliable | Low | Opcode fusing (fused sort paths) | Phase 1 |

## Appendix C: Decision Log

| Decision | Rationale | Date |
|----------|-----------|------|
| Rebase fork on Godot 4.7.1-stable (a13da4feb8d) | Move from tracking upstream `master` to tracking stable releases on a single `master` branch. Progressive rebase at each stable release boundary. | 2026-08-11 |
| Single-branch over branch-per-release | Branch-per-release adds archaeology complexity with zero value for a single-project fork. `master` is the one branch; rebase it at each stable release. | 2026-08-11 |
| Fork approach over custom engine | Custom engine (Goblin raylib+daslang) is 2-3 years from viable. Fork preserves all DB work, editor, asset pipeline. | 2026-08-11 |
| Source override over core patching | Allows all changes to live in `modules/goblin/`. Clean rebase surface. | 2026-08-11 |
| ADR governance for all structural decisions | Production-quality fork requires rigid architectural constraints, not experimental features. Modeled on Goblin Custom Engine governance. | 2026-08-11 |
| Stable-release tracking over master | Reduces merge churn, avoids half-baked upstream features, ensures production-tested base. Rebase only at tagged stable releases. | 2026-08-11 |
| Cherry-pick GDScript features from clean base | gdscript2 has partial breakage and unmerged branches. Port individual features one at a time, each with its own ADR and test gate. | 2026-08-11 |
| Stay on GL Compatibility through DB 1.0 | Avoids regression risk from Forward+ migration. Lightmap system already works around GL limits. | 2026-08-11 |
| Trim modules rather than stub them | Simpler build, fewer dependencies, faster compile. Stubs create maintenance burden and silent failure modes. | 2026-08-11 |
| Keep all networking modules | User has multiplayer plans. Trim would be premature. | 2026-08-11 |
| Keep all target platforms | User wants cross-platform support. Platform removal is easy later, hard to restore. | 2026-08-11 |
| Trim navigation_3d module | Verified: DB uses AStar3D (core math, not a module). NavigationServer3D/Agent3D/Region3D completely unused. | 2026-08-11 |
| Keep noise module | User wants FastNoiseLite for potential procedural content. | 2026-08-11 |
| Trim VR/XR modules | Not needed. Trivially re-addable via module enable flag. | 2026-08-11 |
| Retry-loop fix in Phase 0 | Most embarrassing code in the fork. Proves source override system end-to-end. | 2026-08-11 |
