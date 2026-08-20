# RFC: Runtime CPU Lightmapper (`lightmapper_cpu`)

Status: **proposed** (directionally approved 2026-08-14, awaiting architect review)
Area: core rendering / lightmapping
Supersedes: ADR 0006 direction ("promote extension `LightmapBaker` to public API")

## Context

The genre set's reference title needs **runtime lightmap baking**: levels are generated on the fly from pre-saved
data, with many static lights + shadows, on the **GL Compatibility** renderer.

Today the only runtime baker is the GDExtension `LightmapBaker`
(`godot_extensions/src/main/lightmap_baker.*`), which has structural problems:

- **Indirect lighting is a 2D blur of the lightmap image in UV space**
  (`_bake_indirect_light` samples neighboring texels with zero world-space
  visibility) → **light leaks through walls by construction**, plus island-edge
  bleed from the dilation pass.
- Shadow rays are brute-force over every triangle (per-mesh AABB + flat tri list,
  no BVH, single-threaded) → O(texels × tris).
- Texel density is a lossy mapping of project settings → "stuck at one voxel
  setting" (no per-mesh `lightmap_scale`).
- No probes, no environment sky, no shadowmask, no supersampling, no denoiser.
- Requires `ClassDB.class_exists("LightmapBaker")` guard (backlog C-02).

Godot's editor baker `LightmapperRD` is **not runtime-capable**: it is Vulkan
compute via RenderingDevice (no RD under GL Compatibility; compute shaders absent
at the compat floor GL 3.3/GLES 3.0/WebGL2) and editor-only
(`modules/lightmapper_rd/config.py` → `can_build = env.editor_build`).

**Upstream left the door open:** `scene/3d/lightmapper.cpp` defines
`Lightmapper::create_cpu` — a static CreateFunc checked after `create_gpu` in
`Lightmapper::create()` — **never assigned by anything**. `LightmapGI::bake()`
is not TOOLS-guarded (compiles in runtime builds) and `RS::bake_render_uv2`
(albedo/emission pre-bake) is implemented in GLES3
(`drivers/gles3/rasterizer_scene_gles3.cpp:4333`).

## Options considered

1. **Expose LightmapperRD at runtime** — rejected: needs RD (absent under GL
   compat), editor-only module gate, build-flag changes. (Backlog rejected section.)
2. **Keep/fix the GDExtension baker** — rejected: leak is structural (UV-space
   blur), no probes/environment/shadowmask, permanent GDExtension + ClassDB guard,
   duplicated xatlas integration with in-place mesh mutation.
3. **Renderer workarounds** (compute on GL, GLES2 forward-port, compat-on-RD) —
   rejected: break the portability floor / add a third unmaintained renderer / RD
   has no GL backend.
4. **Recommended: engine CPU lightmapper module** — see direction.

## Recommended direction (locked)

### Module

New **additive module** `modules/lightmapper_cpu/` at the repo root (ADR 0008 —
additive features live in `modules/<name>/`, not inside `modules/goblin/`).
Standard module anatomy: own `SCsub`/`config.py` (`can_build` = all platforms),
`register_types.{h,cpp}`, `doc_classes/`, `tests/`.

Class name: `LightmapperCPU` (upstream conventions; parallel to `LightmapperRD`).

### Contract (same as `LightmapperRD`, best effort parity)

- Implements the full `Lightmapper` interface (`scene/3d/lightmapper.h`):
  `add_mesh` (points/uv2/normal/material/albedo_on_uv2/emission_on_uv2),
  `add_directional_light`/`omni`/`spot`/`area` + `add_area_light_atlas`,
  `add_probe`, `bake(...)` (all params: quality, denoiser, bounces,
  bounce_indirect_energy, bias, max_texture_size, bake_sh, shadowmask,
  texture_for_bounces, generate_probes, environment panorama + transform,
  step callback + userdata, exposure normalization, supersampling),
  `get_bake_texture`/`get_shadowmask_texture`/`get_bake_mesh_*`/`get_bake_probe_*`.
- **Owns the bake project settings.** When `lightmapper_rd` is disabled its
  `register_types` GLOBAL_DEFs vanish
  (`rendering/lightmapping/bake_quality/*_quality_ray_count`,
  `bake_performance/*`) — the CPU module must define the same settings so the
  configuration surface is identical.
- Registration: `register_types.cpp` sets `Lightmapper::create_cpu` at
  `MODULE_INITIALIZATION_LEVEL_SCENE`. `Lightmapper::create()` order stays
  custom → gpu → cpu, so a future GPU re-enable needs no code change.
- Texel density: mesh `lightmap_size_hint` × per-mesh `lightmap_scale` × global
  `texel_scale` (fixes the "stuck voxel size" problem).

### Algorithm (CPU, world-space — not a shader port)

- **Own BVH raycaster** over world-space triangles (embree-style `Ray` struct per
  the `LightmapRaycaster` interface; alpha-cutout texture support). The embree
  module's raycaster registration is TOOLS-gated, so the runtime path is the
  module's own BVH — no dependency on `LightmapRaycaster::create()` for MVP.
- **Direct**: per-texel gather from all lights, shadow rays via BVH, bias from
  the bake params. No leaks: real visibility tests.
- **Indirect**: N bounces — texels emit accumulated radiance (texture_for_bounces),
  next bounce gathers via hemisphere sampling with visibility. World-space, so no
  UV-space leakage.
- **Probes**: SH capture via hemisphere ray samples (phase 2).
- **Environment**: sample the panorama image passed by `LightmapGI::bake()`.
- **Shadowmask**: per-texel shadow bits per light (phase 2).
- **Supersampling + quality → ray counts** from the shared project settings
  (32/128/512/2048 pattern, matching RD).
- **Threading**: `WorkerThreadPool` (`core/object/worker_thread_pool.h` —
  verified: `add_group_task(elements, tasks)`, `wait_for_group_task_completion`).
  Chunked per-texel groups; step callback drives progress and abort
  (`BAKE_ERROR_USER_ABORTED`).

### Engine changes (minimal)

1. **One file override (core)**: `scene/3d/lightmap_gi.cpp` mirror in
   `modules/goblin/` via `goblin_add_library()` (new `"scene"` library entry in
   `_GOBLIN_FILE_OVERRIDES`, modules/goblin/config.py). Change: **in-memory bake
   mode** — when there is no save path (`p_image_data_path` empty and
   `get_light_data()->get_path()` is not a resource file), skip
   `_save_and_reimport_atlas_textures` (which also avoids the trimmed `tinyexr`
   EXR writer) and build the `Texture2DArray` from
   `lightmapper->get_bake_texture(i)` directly → `set_lightmap_textures` +
   users. Everything else in `bake()` (gather, prep, probe gen, assignment,
   injection) is unchanged and already runtime-safe. Same override also
   restructures `get_configuration_warnings()`: no "cannot be baked" warning
   when `MODULE_LIGHTMAPPER_CPU_ENABLED` (verified 2026-08-14 — see plan §9b.3b).
2. **One file override (editor)**: `editor/scene/3d/lightmap_gi_editor_plugin.cpp`
   mirror (B-04 precedent — "editor" library dict): bake-button gate becomes
   `MODULE_LIGHTMAPPER_RD_ENABLED || MODULE_LIGHTMAPPER_CPU_ENABLED`; the RD GPU
   check applies only when RD is the baker (verified: button currently hard-disabled
   without `lightmapper_rd`, plan §9b.3a).
3. **Trim changes** (with permission, ADR 0003 evidence): disable `lightmapper_rd`
   (evidence: the reference title bakes at runtime; one baker everywhere; removes RD + OIDN from the
   pipeline). Re-enable `tinyexr` (evidence: editor lightmap bake writes `.exr`;
   `save_exr` is `ERR_UNAVAILABLE` without it — verified; editor bake is broken in
   the fork today, plan §9b.3c).
4. **Runtime unwrap**: the module sets the `array_mesh_lightmap_unwrap_callback`
   extern (`scene/resources/mesh.cpp:2071`) using xatlas vendored at
   `thirdparty/xatlas` (present). Guard: skip when `MODULE_XATLAS_UNWRAP_ENABLED`
   (editor builds already get the callback from that module; runtime builds get
   it from us). `xatlas_unwrap` stays editor-gated and untouched.
5. **GDExtension retirement**: the reference title migrates from `LightmapBaker` to
   the engine-native `LightmapBaker` (module) — same call sites, no
   `class_exists` guard (closes C-02); extension class eventually removed.

### GDScript API surface (runtime, programmatic)

**Verified gap:** `LightmapGI::bake()` is **not bound** upstream — the bind is commented
out (`lightmap_gi.cpp:2121`); only the editor calls it from C++. The fork must expose it.

**Mechanism decision:** the `lightmap_gi.cpp` override exists anyway for in-memory bake
mode, but it adds **no bound methods**. Rationale: `bake()`'s signature takes
`Lightmapper::BakeStepFunc` — a C function pointer with no Variant conversion, so the bind
can't compile as-is (likely why upstream commented it out); fixing it needs a new
declaration in `lightmap_gi.h` — headers are not swappable and a direct header edit is the
sanctioned last resort we don't need. **All GDScript API ships in the module.**

`LightmapBaker : RefCounted` (registered by the CPU module) — mirrors the extension's
API 1:1 so the reference title migrates without call-site changes:
- `bake(from_node, output_data)` / `bake_descriptors(descriptors, output_data, from_node)`
  / `bake_descriptors_with_lights(descriptors, light_descriptors, output_data, from_node)`
  — node path calls the existing C++ `LightmapGI::bake(root, "")` (in-memory mode, module
  static step function) reusing all engine orchestration; descriptor path maps dicts to
  `Lightmapper::MeshData` + lights fed to the same `LightmapperCPU`, output written
  in-memory by the same writer logic.
- `bake_async(...)` variants — run on `WorkerThreadPool`, emit `bake_progress(percent,
  status)` + `bake_finished(error)` signals, abortable via `abort()`. Runtime UX: level
  editor shows progress while baking a chunk.
- `static lightmap_unwrap(mesh, transform, texel_size)` — thin wrapper over
  `ArrayMesh::lightmap_unwrap` (works once the module registers the unwrap callback).
- `progress` property + signals; `get_gathered_mesh_count()`/`get_bake_assignments()` kept.
- Kills the `ClassDB.class_exists("LightmapBaker")` guard — class always exists.
3. **Reference-title migration:** unload the extension's `LightmapBaker`; same call sites, same class
   name, no guard. `bake_descriptors*` kept only if the reference title uses it (TBD — see open questions).

### Denoiser

`LightmapDenoiser::create_function` is defined (`scene/3d/lightmapper.cpp:33`)
but **nothing assigns it** — LightmapGI never consumes it. RD's JNLM is a GPU
compute shader; OIDN is an editor-only external exe. Therefore:

- **Phase 1: no denoiser.** Quality via ray counts + supersampling.
- **Phase 3 (optional): CPU JNLM port** (threaded, internal to the module).

### C-01 dependency

Frustum-culling fix (backlog C-01, #71585) lands in the same lightmap_gi.cpp override:
root cause verified — empty `get_aabb()` (lightmap_gi.cpp:1820) + the cull gate dropping
`INSTANCE_LIGHTMAP` from the per-frame list (renderer_scene_cull.cpp:2930/2971); RD binds
from that list per frame (renderer_scene_render_rd.cpp:1454), GLES3 does not. Fix:
`RS::instance_set_ignore_culling(get_instance(), true)` in POST_ENTER_TREE (one line,
renderer-agnostic; upstream issue open since 2023-01, no fix — details in plan §9c).
Injection correctness is independent of baking and required for both bakers.

## Phases + gates

| Phase | Scope | Effort | Gate |
|-------|-------|--------|------|
| P1 | Module skeleton, own BVH, direct lighting, 1-2 bounces, atlas output, in-memory bake override, unwrap callback, settings ownership, WorkerThreadPool, progress/abort | ~1-1.5 wk | Bake a reference room at runtime on GL compat: visually matches extension output minus leaks; no light through opaque walls; injection renders correctly |
| P2 | SH probes, environment panorama, supersampling, shadowmask, quality ray-count mapping, per-mesh lightmap_scale verification | ~1-1.5 wk | Probe-lit dynamic objects match static bake; settings parity with RD documented |
| P3 | CPU JNLM denoiser, area lights, texture_for_bounces refinement | optional | Noise reduction at low ray counts |

Tests: module `tests/` — synthetic room: analytic direct-light falloff, shadow
boundary, **zero leak across opaque wall**, bounce energy decay, determinism
(seeded RNG), abort via step callback. Gates per rules: full GDScript suite +
reference level load.

## Open questions (resolved 2026-08-14)

1. **Determinism** — fixed seeded RNG per bake (positional seeds, same scheme as RD).
   Two CPU bakes are bit-identical; parallel-safe regardless of thread count.
2. **Class registration** — register `LightmapperCPU` with ClassDB (parity with
   `LightmapperRD`, debuggability). `LightmapBaker` wrapper registered (it is the API).
3. **Area lights** — the reference title uses them on GL compat (screens/windows) as dynamic lights;
   the GLES3 renderer renders them fine. Baked contribution: RD bakes nothing on compat
   (GLES3 `bake_render_area_light_atlas` returns an empty atlas — verified
   rasterizer_scene_gles3.h:971; `_build_area_light_texture_atlas` early-returns on compat,
   lightmap_gi.cpp:912). The CPU baker **improves on RD here**: build the area-light
   mipmap atlas on CPU (packing layout is already CPU-side in `_build_area_light_texture_atlas`;
   mipmaps via `Image::generate_mipmaps`) via the lightmap_gi.cpp override, so **baked
   textured area lights work on compat**. LTC evaluation ported (analytic, no LUT).
   Scope: phase 3.
4. **Editor UX** — resolved 2026-08-14 (plan §9b.3): editor flow keeps calling the
   regular C++ `bake()` (now routed to the CPU baker). Two required follow-ups found by
   verification: (a) bake-button gate + node warning both hard-gated on
   `MODULE_LIGHTMAPPER_RD_ENABLED` → second editor override +
   warnings restructure (RFC "Engine changes" 1-2); (b) editor `.exr` save is broken in
   the fork today (`tinyexr` trimmed) → re-enable `tinyexr` (RFC "Engine changes" 3).
   Descriptor API is GDScript-only (never editor-facing).
5. **Descriptor API** — the reference title uses both `bake()` (scene root) and
   `bake_descriptors_with_lights()` (Dictionary input). Both ship in the `LightmapBaker`
   wrapper (descriptor path ≈150 lines reusing engine infra).

## Settings surface (GDScript)

`LightmapBaker` exposes the full engine bake settings (same names as `LightmapGI`:
`bake_quality`, `bounces`, `bounce_indirect_energy`, `bias`, `texel_scale`,
`max_texture_size`, `supersampling_enabled`/`factor`, `use_denoiser`,
`denoiser_strength`/`range`, `use_texture_for_bounces`, `generate_probes`,
`environment_mode`/`custom_sky`/`custom_color`/`custom_energy`, `directional`,
`shadowmask_mode`, `camera_attributes`) — applied to both bake paths. Extension-only
settings have no engine equivalent and are dropped by design (approximations replaced by
physical baking): `use_material_albedo`/`use_lambert_normalization`/`light_falloff_mode`
→ albedo + Lambert come from materials/`bake_render_uv2`; `ambient_energy`/
`use_environment_ambient` → environment modes; `lightmap_energy_scale` →
`camera_attributes` exposure; `auto_unwrap_uv2` → explicit `lightmap_unwrap()` call;
`atlas_size_override`/`atlas_padding` → engine atlas packing;
`mesh_layer_mask` → scene structure under the bake root (or wrapper-side filter if the reference title
needs it — revisit at P1 if parity tests demand).

## Threading

Always-on via `WorkerThreadPool` (`core/object/worker_thread_pool.h` — verified:
`add_group_task(elements, tasks)`, `wait_for_group_task_completion`, `get_thread_count()`).
Per-texel positional seeds make output identical for any thread count; single-thread
fallback when `get_thread_count() <= 1`. This is the compute-shader substitute: CPU bakes
scale with core count instead of GPU width.

## Risks

- **CPU bake time** on large scenes — mitigated: quality settings, threading,
  chunked progress/abort, optional denoiser later.
- **Rebase churn** on the `lightmap_gi.cpp` override (churn-heavy file) —
  override is small and localized (bake tail + save path); porting-skill
  discipline; mirror-drift check (B-10).
- **Settings parity** when `lightmapper_rd` is disabled — module owns the
  GLOBAL_DEFs (explicit in contract above).
- **Embree unavailable at runtime** — own BVH is the MVP path by design.

## Decision needed

Approve direction + phase plan; lock open questions 1-4; authorize
`lightmapper_rd` disable as part of P1.

## Implementation breakdown

Stage-by-stage port map, data structures, threading, performance model, and risks:
[lightmapper-cpu-plan.md](../plans/lightmapper-cpu-plan.md) (2026-08-14).
