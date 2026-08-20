# Module Trim Fix — Implementation Breakdown & Analysis

Locked mechanism + execution spec for making the module trim actually engage in the Goblin
fork. The trim has been a silent no-op since B-01 shipped: `env.disabled_modules` (the only
thing `configure()` writes) is never a compile gate, so all 30 "trimmed" modules compile and
B-01's `done` status was false. All file/line references verified against the fork
(2026-08-16, SCons 4.10.1, Godot 4.7.x).

> Note on placement: authored under the D-13 convention path
> `modules/goblin/docs/plans/module-trim-fix-plan.md`. Backlog B-01 links here.

Companion ADRs: [0012-build-time-option-injection] (mechanism lock) +
amended [0003-module-trim-evidence-standard].

---

## 1. Purpose

1. Make the 30-module trim actually gate module compilation (today: 0 modules are gated).
2. Replace the dead `env.disabled_modules` code in `config.py` (no-op + latent set→list crash).
3. Re-validate the trim list against evidence, because every "verified unused" claim in ADR 0003
   was gathered while the trim was a no-op — the evidence is **unvalidated**.
4. Prove the trim with build gates (editor green, suite green, trimmed modules provably absent).

Out of scope: the lightmapper_cpu migration (C-02/C-11 plan), platform/driver trimming,
networking decisions. Those only consume this plan's verified baseline.

---

## 2. Root cause (verified facts)

**`env.disabled_modules` is never a compile gate:**
- `SConstruct:121` initializes `env.disabled_modules = set()`.
- `methods.py:352/358` only READ it to suppress the auto-disable warning in
  `module_check_dependencies`.
- The real gate is `SConstruct:1113` `if not env[f"module_{name}_enabled"]: continue` in the
  gate loop; `modules/SCsub:33` compiles only `env.module_list` (== gated modules).
- `modules/goblin/config.py:244-248` appends `DISABLE_MODULES` into `env.disabled_modules` —
  no effect on `module_*_enabled`, so every module passes the gate and compiles.
- Bonus latent crash: `config.py:244` REPLACES the `set()` with a `list`; if
  `module_check_dependencies` ever takes the `self.disabled_modules.add(module)` path
  (`methods.py:358`) on this env, it crashes on `list.add`. Proof the mechanism was never
  exercised.

**Where the fix can hook (SConstruct flow):**
| Point | What happens | Line |
|---|---|---|
| `env.disabled_modules = set()` | Upstream init (keep, untouched) | SConstruct:121 |
| `customs = ["custom.py"]` + `--profile` | Options-file layer for `Variables` | SConstruct:149-158 |
| `opts = Variables(customs, ARGUMENTS)` | Options object | SConstruct:158 |
| `detect_modules` | `files.sort()` → `modules_detected` ALPHABETICAL | methods.py:276 |
| First module loop | per module `import config` → `is_enabled()` → `opts.Add(BoolVariable("module_x_enabled", ...))`; **goblin imports between `godot_physics_3d` and `gridmap`** | SConstruct:471-494 |
| `opts.Update(env, {**ARGUMENTS, **env.Dictionary()})` | The ONE Update that applies `module_*_enabled` (first application; `env.Dictionary()` has no module keys yet → **ARGUMENTS rules**) | SConstruct:499 |
| Gate loop | `if not env[f"module_{name}_enabled"]: continue` (1113); `config.configure(env)` at goblin's alphabetical position (1124); `env.module_list` finalized at 1145 | SConstruct:1112-1145 |

**Ordering caveat (why `configure()` alone can never work):** at `config.configure(env)`
(SConstruct:1124) the gate at 1113 has already passed every module that sorts BEFORE `goblin`.
`configure()` can only affect the 20 trimmed modules that sort after it; the 10 that sort
before (astcenc, basis_universal, bmp, camera, csg, dds, etcpak, fbx, glslang, gltf) are
unreachable. Any fix must act at the **options layer (before 499)**, not the env layer.

---

## 3. Locked mechanism: **A — import-time ARGUMENTS injection** (ADR 0012)

All inside `modules/goblin/config.py`. At goblin's `import config` in the first module loop
(SConstruct:474 — unconditionally, enabled or not), module-level code runs:

```python
from SCons.Script import ARGUMENTS          # plain dict, populated since SConstruct:151
for _mod in DISABLE_MODULES:                # 28 modules (section 5)
    _key = f"module_{_mod}_enabled"
    if _key not in ARGUMENTS:               # user CLI wins
        ARGUMENTS[_key] = "no"
```

Then `opts.Update` at SConstruct:499 merges `ARGUMENTS` (args layer beats defaults and
custom.py/profile files per SCons 4.10.1 `Variables.Update` semantics) → `BoolVariable`
converts `"no"` → `False` → `env["module_x_enabled"] = False` → gate at 1113 skips **all 28
regardless of alphabetical position**.

Precedence rule (locked):
1. **User CLI** (`module_x_enabled=yes/no`) — already in `ARGUMENTS` at goblin import → guard
   skips → user wins. This is how `module_mono_enabled=no` already works in the canonical
   build command.
2. **Injection** (our `"no"`) — applies when the user said nothing.
3. **custom.py / profile files are beaten** by the injection (args layer > files layer).
   Accepted today: no repo-root `custom.py` exists (verified). Documented limitation — if a
   `custom.py` ever needs to re-enable a trimmed module, that module must leave
   `DISABLE_MODULES` or be re-enabled via CLI.

Why not the alternatives (details in ADR 0012):
- **B — repo-root `custom.py`**: new file outside `modules/goblin/` (hard rule 2 requires
  explicit permission), a second source of truth that can drift from `DISABLE_MODULES`,
  and still beaten by ARGUMENTS anyway.
- **C — `get_opts()` duplicate `BoolVariable`s defaulting False**: covers all 28, but
  duplicates option keys (Help() pollution, option-system abuse).
- **D — configure()-time env flags only**: insufficient alone (10/28 unreachable), and
  redundant with A (A already sets every gate before 499).

Cleanup (part of the same change): delete the dead `env.disabled_modules` block
(`config.py:244-248`) — removes the no-op AND the latent set→list crash
(`methods.py:358` `self.disabled_modules.add`).

Regression canary: `configure()` gains a verification print — if injection regressed
(e.g. upstream renames the options path), `env[f"module_{m}_enabled"]` reads `True` and the
count printed is < 28 → visible in every build log.

New mechanism → documented in `.kilo/rules/rules.md` (mechanism 4) + the `overrides` skill
(per overrides-skill rule "New mechanism? Document"). ADR 0012 locks it.

---

## 4. Exact code change shape

File: `modules/goblin/config.py` only (no upstream files touched).

Replace lines 212-248 (the whole `MODULE TRIM` block) with:

```python
# ===================================================================
# MODULE TRIM — 28 modules disabled (~55% faster compile)
# Mechanism: build-time option injection (ADR 0012). module_*_enabled
# is decided at SConstruct:499 opts.Update from ARGUMENTS (args layer
# beats defaults and custom.py/profile files). configure() runs AFTER
# that Update (gate loop, SConstruct:1124) — too late for the 10 trim
# candidates sorting before "goblin". So the injection happens at
# IMPORT time (first module loop, SConstruct:474), before the Update:
# for each trimmed module without a user CLI value, ARGUMENTS gets
# module_<name>_enabled = "no". The gate at SConstruct:1113 then skips
# all of them regardless of alphabetical position. User CLI wins.
# Precedent: methods.py:229, platform/android/detect.py:118.
# ===================================================================
DISABLE_MODULES = {
    # Image/texture formats (PNG only) — tinyexr KEPT (editor .exr
    # lightmap save, Image::save_exr needs modules/tinyexr; C-11)
    "bmp", "tga", "dds", "hdr", "jpg", "webp",
    "basis_universal", "ktx", "astcenc", "etcpak",
    # Audio/video (no video playback, no interactive music)
    "theora", "interactive_music",
    # VR/XR (no VR usage)
    "webxr", "openxr", "mobile_vr",
    # 3D nodes/scene (no CSG, GridMap, GLTF/FBX import)
    "csg", "gridmap", "gltf", "fbx",
    # Navigation (AStar3D from core/math, not these modules)
    "navigation_3d", "navigation_2d",
    # 2D physics (3D game; 2D falls back to PhysicsServer2DDummy).
    # godot_physics_3d KEPT: jolt_physics registers "Jolt Physics"
    # but NOT the default (jolt_physics/register_types.cpp:59);
    # GodotPhysics3D is the registered default (godot_physics_3d/
    # register_types.cpp:56-57) used by main.cpp:362-372 fallback and
    # tests/test_main.cpp:204 new_default_server(). Trimming it would
    # drop the test suite + fresh-project boot to the dummy server.
    "godot_physics_2d",
    # Shader compiler (GL Compatibility uses GLSL directly, no SPIR-V;
    # RenderingDevice shader_compile_spirv_from_source fails without
    # glslang — Forward+/Mobile unsupported by design, ADR 0003)
    "glslang",
    # Utility (no webcam, no runtime zip)
    "camera", "zip",
    # Rendering (Embree occlusion culling — Forward+/Mobile only)
    "raycast",
    # Debug/profiling (release builds only)
    "objectdb_profiler",
    # Physics tools (convex decomposition, editor only)
    "vhacd",
}

# Import-time injection: runs when this module's config.py is imported
# in the first module loop (SConstruct:474), before the options Update
# at SConstruct:499. Idempotent across the second (gate-loop) import.
from SCons.Script import ARGUMENTS
for _mod in DISABLE_MODULES:
    _key = f"module_{_mod}_enabled"
    if _key not in ARGUMENTS:
        ARGUMENTS[_key] = "no"
```

Inside `configure()` (replacing the old `env.disabled_modules` loop + print), add the canary:

```python
# Canary: injection must have gated every trimmed module by now
# (SConstruct:499 already applied ARGUMENTS). Count < len(...) means
# the mechanism regressed or the user re-enabled modules via CLI.
_disabled = [m for m in DISABLE_MODULES if not env[f"module_{m}_enabled"]]
print(f"Goblin: Module trim active ({len(_disabled)}/{len(DISABLE_MODULES)} modules gated off)")
```

No `config.py` changes anywhere else; no `SCsub`/`SConstruct`/`methods.py` edits.

---

## 5. Trim-list adjustments (evidence-driven)

| Module | Change | Evidence |
|---|---|---|
| `tinyexr` | **REMOVE from trim (re-enable)** | `Image::save_exr` is `ERR_UNAVAILABLE` without `save_exr_func` (image.cpp:2824-2829), registered only by `modules/tinyexr` (register_types.cpp:46). Editor lightmap bake save writes `.exr` (lightmap_gi.cpp:894) — C-11, already decided in lightmapper-cpu-plan §9b.3c. Trim never engaged → the break was latent; engaging the trim without this would break editor lightmap saves. |
| `godot_physics_3d` | **REMOVE from trim (re-enable)** | New evidence (2026-08-16): jolt_physics registers "Jolt Physics" but NOT as default (jolt_physics/register_types.cpp:59 — `register_server` only). Default server comes from `godot_physics_3d/register_types.cpp:56-57` (`set_default_server("GodotPhysics3D")`). `main.cpp:362-372`: `new_server(GLOBAL_GET("physics/3d/physics_engine"))` → `new_default_server()` → dummy fallback + warning. `tests/test_main.cpp:204`: `[SceneTree]` tests call `new_default_server()` directly → dummy → C-14 combat tests (11 doctest cases) fail. The editor override pins `physics/3d/physics_engine = "Jolt Physics"` for new projects (goblin editor_node.cpp:8344), so Jolt stays the project-selected engine; GodotPhysics3D stays as the always-registered default (boot + tests). |
| `godot_physics_2d` | **KEEP trimmed** | 2D physics → `PhysicsServer2DDummy` (main.cpp:390-392, graceful). No GDScript-suite 2D physics usage (verified: tests/scripts has no PhysicsBody2D/Area2D); `test_viewport.cpp:1601` explicitly handles the dummy cast. Reference title is 3D. **Gate:** full suite must stay green (P3); if any doctest needs real 2D physics → re-enable with ADR 0003 evidence. |
| `glslang` | **KEEP trimmed** | `RenderingDevice::shader_compile_spirv_from_source` (rendering_device.cpp:230-238) fails without glslang → Forward+/Mobile runtime broken by design. Fork is GL Compatibility-only (ROADMAP, C-12/C-13, lightmap decisions all assume GLES3). `#ifdef MODULE_GLSLANG_ENABLED` guards are the only uses (rendering_device.cpp:48/230, rendering_device_binds.cpp:34/200) — nothing else in the build depends on it. **Gate:** editor boots + renders on GLES3 (P3); document "Forward+/Mobile require re-enabling glslang" in ADR 0003 + ROADMAP. |

Net: **30 → 28 trimmed modules.**

Kept-module dependency check (no cascade expected): `module_check_dependencies`
auto-disable warnings are printed at build; the log must contain none (P2 gate). Reviewed
deps of kept modules (cvtt, betsy, bcdec, meshoptimizer, xatlas_unwrap, lightmapper_rd,
visual_shader, jolt_physics, midi, combat, gdscript, ...) — none requires a trimmed module.

---

## 6. Phases

| # | Phase | Goblin files | Effort | Gate |
|---|-------|--------------|--------|------|
| P1 | config.py: ARGUMENTS injection + trim-list edit (28) + dead `disabled_modules` removal + canary print | `modules/goblin/config.py` | 0.5d | file review: injection before 499, guard order, no upstream touch |
| P2 | Build gates: editor build + trim-absence proof + log check | none (verification) | 0.5-1d | §7 gates 1-4 |
| P3 | Runtime gates: suite (tests=yes), editor smoke, GLES3 render, PNG import (D-07), .exr save (C-11) | none (verification) | 0.5-1d | §7 gates 5-7 |
| P4 | Reference-title validation (unvalidated-evidence gate): project loads, corpus imports, gameplay smoke | none (verification) | 1d | §7 gate 8; any break → re-enable module with ADR 0003 evidence |
| P5 | Docs: ROADMAP §5 mechanism + Impact risk line + counts + GL-compat-only note | `modules/goblin/docs/ROADMAP.md`, CODE_MAP note if needed | 0.5d | doc review (architect) |

Architect artifacts (proposed, pending write access — full text in the handoff message):
ADR 0012 (mechanism lock), ADR 0003 amendment, `.kilo/rules/rules.md` mechanism 4,
`overrides` skill, backlog B-01 row, plans/README.md index.

---

## 7. Verification gates

1. **Editor build green**: `scons platform=windows target=editor module_mono_enabled=no
   accesskit=no angle=no debug_symbols=yes -j4` (build skill) → binary + PDB present.
2. **Trim provably absent from compile**:
   - `modules/modules_enabled.gen.h` (generated from `env.module_list`, modules_builders.py:8-12)
     defines `MODULE_<X>_ENABLED` for gated modules only → grep: no `MODULE_BMP_ENABLED` etc.
   - `bin/obj/modules/` → no `module_<trimmed>` library/objects.
   - `scons --tree=all -n bin/goblin.windows.editor.x86_64.exe` → grep `modules/<trimmed>` →
     zero matches (whole-graph proof, independent of up-to-date state).
3. **Build log**: `Goblin: Module trim active (28/28 modules gated off)` + zero
   "Disabling ... module as the following dependencies are not satisfied" warnings.
4. **Canary counter-example** (optional, once): pass `module_bmp_enabled=yes` → log shows
   27/28 + bmp compiles (proves user-CLI precedence AND the gate both work).
5. **Suite green**: `tests=yes` build, full doctest run (1397/1397 today on the no-op build —
   must stay green, incl. `[SceneTree][Combat]*` on GodotPhysics3D; 2D-physics tests on dummy
   are expected to pass per test_viewport dummy handling). GDScript suite
   `--test-case "[Modules][GDScript]*"` green.
6. **Editor smoke on GLES3**: headless + windowed boot, scene open/save, PNG texture import
   produces `.import` sidecar (D-07), lightmap bake save path writes `.exr` (C-11 — tinyexr).
7. **Reference-title load**: project opens, `physics/3d/physics_engine = "Jolt Physics"`
   effective (PhysicsServer3D is Jolt, not dummy).
8. **Corpus import pass**: all reference-title assets import with only PNG/kept formats
   (this is the unvalidated ADR 0003 evidence, re-validated). Any asset format served by a
   trimmed module → re-enable that module (bmp/tga/jpg/webp/hdr/dds) with evidence.

---

## 8. Risks & contingencies

- **Suite breaks when trim engages** — the 1397/1397 was measured on the no-op build; the
  suite has never run with the trim active. Mitigation: godot_physics_3d kept (tests use it
  via default server); 2D physics dummy is handled by the harness. Contingency: re-enable
  `godot_physics_2d` if a doctest needs it.
- **Corpus has non-PNG assets** — import breaks when trim engages; contingency: re-enable the
  serving module (ADR 0003 evidence).
- **Forward+/Mobile users** — broken at runtime without glslang (RD shader compile
  `ERR_FAIL`). Accepted: fork is GL-compat-only; documented in ADR 0003 + ROADMAP.
  Contingency: re-enable glslang.
- **Editor import pipeline with compression modules disabled (D-07)** — PNG path uses
  `modules/png`; VRAM-compressed import unavailable. Verified at gate 6; contingency:
  re-enable basis_universal/ktx/astcenc/etcpak.
- **custom.py / profile re-enable beaten by injection** — documented limitation, none exists
  today; CLI remains the sanctioned override.
- **Help() shows stale defaults** (trimmed modules display `yes` default while injected `no`
  wins) — cosmetic, accepted (C would fix it but pollutes the option system).
- **Upstream drift in the options flow** (opts.Update/ARGUMENTS path changes) → injection
  silently no-ops → the configure() canary print (count < 28) catches it.
- Accepted Risk (from user brief): the trim engaging may break the 2D-physics test surface —
  consequence: possible suite patch or godot_physics_2d re-enable — why proceeding: the fork
  is 3D-genre; dummy fallback is upstream-graceful; evidence gates decide.

## 9. Rollback path

- **Whole fix**: `git revert` the single-file `config.py` change → trim returns to today's
  no-op behavior (builds still work). Zero upstream files touched → no rebase impact.
- **Single module**: remove it from `DISABLE_MODULES`, rebuild — one-line, per-module.
- **Mechanism regression**: the canary print makes it visible; revert to CLI-only disabling
  (`module_<x>_enabled=no` in the canonical build command) as a stopgap, then fix the hook.

## 10. Open questions

- Whether `godot_physics_2d` stays trimmed long-term (decided by P3/P4 gates, not by fiat).
- Whether the fork should also gate the 2D-physics dummy behavior in docs (project settings
  hint) — follow-up doc task, not this plan.
- `lightmapper_rd` stays compiled but is unusable on RD (no glslang) and disabled on GL-compat
  (no RD) — dead weight until the lightmapper_cpu plan disables it (C-02 phase 1.4). Not this
  plan's scope; noted for the C-02 handoff.
