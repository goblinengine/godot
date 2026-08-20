# Lightmapper CPU — Implementation Breakdown & Analysis

Companion to [lightmapper-cpu-rfc.md](../rfc/lightmapper-cpu-rfc.md). Deep-dive of `modules/lightmapper_rd/`
(2175-line `lightmapper_rd.cpp` + 1348-line `lm_compute.glsl` + `lm_raster.glsl` + `lm_blendseams.glsl`)
with a stage-by-stage port map to CPU, data structures, threading, performance, and risks.
All file/line references verified against the fork (2026-08-14).

---

## 1. Pipeline overview

`LightmapGI::bake()` (scene/3d/lightmap_gi.cpp:1060) is renderer-agnostic and unchanged:
it gathers meshes/lights/probes, calls `RS::bake_render_uv2()` for albedo/emission
(pre-baked by the renderer — **GLES3 implements it**, rasterizer_scene_gles3.cpp:4333),
then hands a `Lightmapper` the data. The RD baker's `bake()` (lightmapper_rd.cpp:1104)
runs 12 stages; each maps 1:1 to CPU:

| # | Stage (RD) | Location | CPU port |
|---|-----------|----------|----------|
| S1 | Blit meshes into atlas | `_blit_meshes_into_atlas` (cpp:306) | **Already CPU** — reuse as-is (packing via `Geometry2D::partial_pack_rects`) |
| S2 | Acceleration structures | `_create_acceleration_structures` (cpp:435) | **Already CPU** — keep ~95%, drop RD buffer creation |
| S3 | Raster geometry → position/normal/unocclude tex | `_raster_geometry` + `lm_raster.glsl` | Reimplement: 2D UV rasterization + depth buffer (see 3.3) |
| S4 | Unocclude pass | `lm_compute.glsl` `MODE_UNOCCLUDE` (1045) | Direct port: 4 rays/texel + position fixup |
| S5 | Primary (direct light) | `MODE_DIRECT_LIGHT` (877) | Direct port: `trace_direct_light` (435-706) |
| S6 | Secondary (indirect/bounce) | `MODE_BOUNCE_LIGHT` (981) | Direct port: `trace_indirect_light` (722-858) |
| S7 | Light probes | `MODE_LIGHT_PROBES` (1092) | Direct port: sphere sampling + 9 SH coeffs |
| S8 | Pack L1 coeffs (SH only) | `MODE_PACK_L1_COEFFS` (1336) | Trivial |
| S9 | JNLM denoise | `MODE_DENOISE` (1179) | Direct port (phase 3; heavy) |
| S10 | Dilate | `MODE_DILATE` (1147) | Trivial (8-directional raycast, radius ≤ 4×supersampling) |
| S11 | Blend seams | `lm_blendseams.glsl` + cpp:2267 | Reimplement in texel space (see 3.11) |
| S12 | Output textures/probes | cpp:2429-2457 | Trivial (already CPU images) |

**Key insight: ~70% of the RD baker is already CPU code.** S1+S2 are pure C++ (the GLSL
only reads the buffers they produce). Only the shader math (S3-S11) needs translating, and
it is plain portable math: no GPU-only constructs except texture sampling (bilinear fetch —
trivial on CPU) and the hash RNG (portable verbatim).

---

## 2. Data structures (CPU)

Reuse the RD structs verbatim (they're plain C++ already, lightmapper_rd.h:42-295):

- `BakeParameters` (42) — world bounds, grid mapping, light count, env transform, atlas
  size, exposure, bounces, shadowmask light idx, transparency rays, supersampling.
- `Vertex` (103) — pos + normal(2+1) + uv2. Dedup via `VertexHash` HashMap (cpp:436).
- `Triangle` (196) — indices, slice, AABB, cull mode (from material, cpp:557-560).
- `ClusterAABB` (208) + `TriangleSort` (226) — grid cluster acceleration.
- `Light` (70) + `LightMetadata` (94) — full light params incl. area light quad + texture rect.
- `Probe` (140), `Seam` (173)/`Edge`/`EdgeUV2` (121-171) — seam pairs for S11.
- `MeshInstance` (64) — mesh data + slice + atlas offset.

**CPU-only changes:**
- Replace the `utexture3D grid` (GLSL) with the existing `Vector<uint32_t> grid_indices`
  (cpp:599) — already built on CPU.
- Keep `triangle_indices`, `cluster_indices`, `cluster_aabbs` as `Vector`s instead of RIDs.
- **Precision: use `float` explicitly everywhere in the tracer**, not `real_t` — the GLSL
  is 32-bit; the fork's default build is float, but the module must not silently switch
  precision in double builds (would break parity/determinism).

---

## 3. Stage-by-stage port map

### S1 — Atlas blit (reuse)

`_blit_meshes_into_atlas` (cpp:306) is CPU-only. Port verbatim:
- Per-mesh size = `albedo_on_uv2` size; padding = `(2,2).maxi(denoiser_range) × supersampling`.
- Atlas = power-of-two, bruteforce over doubling sizes for best memory (cpp:337-399).
- `partial_pack_rects` → offsets/slices; blit albedo (RGBA8) + emission (RGBAH) per slice.
- Outputs `albedo_images`, `emission_images`, `atlas_size`, `atlas_slices`, per-mesh
  `offset`/`slice`. These are CPU `Ref<Image>` already — the GPU upload (cpp:1242-1341)
  is simply omitted.

### S2 — Acceleration structure (reuse, minus GPU)

`_create_acceleration_structures` (cpp:435) is CPU-only up to the "CREATE GPU STRUCTURES"
comment (cpp:687). Port verbatim:
- Vertex dedup HashMap → `vertex_array` (cpp:436-505).
- Triangle build + per-mesh seam detection (cpp:508-546) — **this is also what S11 needs**.
- Bounds + grow (cpp:567-572), sort by slice.
- Uniform 3D grid `grid_size = 128` (cpp:1127): plot triangles into cells via octree
  subdivision (cpp:208-252), sort by cell, build per-cell cluster index + cluster AABBs
  (cpp:595-659). Cluster size 16 (cpp:1371).
- Skip: RD buffers (cpp:701-726) and `grid_texture` upload (cpp:728-743).

**Tracer decision:** keep the RD *grid + DDA* traversal (GLSL `trace_ray`, lm_compute.glsl:149-309)
instead of switching to a BVH. Reasons: (a) the C++ build code already exists verbatim;
(b) the traversal is deterministic and matches RD behavior bit-for-bit (cell containment
check at lm_compute.glsl:240-245, front/back face bias rules at 247-252, cull mode at 255-264);
(c) a BVH would be a second acceleration structure to test. A CPU BVH *could* be faster for
very sparse scenes, but the 128³ grid is bounded (≈2M cells × 8B = 16MB) and DDA cost per
ray is low. Revisit only if profiling demands.

### S3 — Raster geometry (reimplement)

RD: `lm_raster.glsl` renders UV2-space triangles into 3 atlas textures (position RGBAF,
normal RGBAH, unocclude RGBAF) with a depth buffer, **two passes per slice: solid + wireframe,
depth compare LESS** (cpp:761-817) so the wireframe pass overwrites edge pixels
(half-pixel UV offset at cpp:796-800 fixes edge-aligned artifacts — issue #69126).

CPU equivalent — a per-texel attribute buffer (position float3, normal float3, face normal +
texel size, depth float):
1. **Solid pass**: edge-function rasterization over UV2-space triangles (same as the
   extension baker's `_rasterize_mesh_direct_lighting`, godot_extensions) writing
   barycentric-interpolated world position + vertex normal; depth = triangle draw order
   (later wins — matches GL LESS with sorted draw). Empty texels keep normal = 0 (RD
   semantics: `length(normal) < 0.5` = empty, lm_compute.glsl:879).
2. **Wireframe pass**: for each triangle, walk its 3 UV edges in texel space (sample at
   texel centers along the edge) and re-evaluate attributes from that triangle with the
   same later-wins rule. This reproduces the GPU wireframe pass exactly: edge texels get
   the attributes of the last triangle covering them, fixing UV-island-border artifacts.
   Cost: O(edges × edge length in texels) — cheap.
3. **Smoothing** (`FA_SMOOTHEN_POSITION`, lm_raster.glsl:53-59, 99-148): when any two
   vertex normals of a triangle differ by <0.99, project the interpolated position onto the
   3 vertex normal planes (outward-only). Pure math — port verbatim, applied per texel at
   raster time (barycentric + vertex data already in hand).
4. `unocclude.xyz = face_normal`, `.w = texel_size` where texel size = max |dFdx/dFdy| of
   world position × √2 (lm_raster.glsl:155-158). CPU: per-triangle, compute world-space
   texel extent from the triangle's UV2 area: `texel_size = max_edge_uv_delta × √2`
   approximation — or better: compute per-texel from neighbor position distance like the
   primary pass does (lm_compute.glsl:883-889). Match RD's formula as closely as feasible;
   the exact value only scales the unocclude ray length.

**Risk handling (S3):**
- Build the RD baker's `DEBUG_TEXTURES` dumps (position/normal EXR per slice) into the CPU
  baker from day one → debug output is directly comparable.
- **Parity harness (editor-only, phase-1 gate):** with `lightmapper_rd` still in editor
  builds, bake the same scene with both bakers (denoiser off, bounces=0) and compare the
  position/normal atlas images with an epsilon. These are deterministic (no RNG in S3/S4)
  → mismatch = raster bug, not noise. If position/normal match, the lighting math is a
  direct port, so lightmaps follow.
- Visual gate: reference room — no acne on curved surfaces (smoothing), no UV-island border
  artifacts (wireframe pass).

### S4 — Unocclude (direct port)

`MODE_UNOCCLUDE` (lm_compute.glsl:1045-1090), per texel with valid position:
- 4 rays along tangent/bitangent ±, length `texel_size × denoiser_range` (cpp passes
  `denoiser_range` = p_denoiser_range if denoiser enabled else 1, cpp:1715).
- If closest hit is `RAY_BACK` within `texel_size`: pull position up backface normal
  × `bias × 10` (prevents UV-island-edge acne; ndotl.wordpress.com/2018/08/29 technique),
  set unocclude mask = 1 (used by JNLM later).
- Mutates only its own texel → parallel-safe. Needs the S2 grid tracer.

### S5 — Primary / direct light (direct port)

`MODE_DIRECT_LIGHT` (lm_compute.glsl:877-979) + `trace_direct_light` (435-706). Per texel:
- Seed: `random_seed(ivec3(atlas_pos, 43573547))` (907) — **deterministic per texel**.
- For each light:
  - Directional: dir, dist = world_size length, attenuation = ndotl.
  - Omni/spot: `get_omni_attenuation` (397-404, nd⁴ then square, × dist^-decay), spot cone
    (509-521).
  - Area: LTC diffuse via `ltc_evaluate_diff` — **fully analytic** (Hill/Heitz form-factor
    integrals + atlas texture fetch, lm_area_lights_inc.glsl:344+, no LUT needed) — portable.
  - Soft shadows + AA: 16 fixed Halton disk offsets (const array, 408-424) scaled by
    `texel_size × shadow_blur`; per sample, `ray_count/16` vogel-disk samples on the light
    disk with penumbra early-out (563-580); per sample, up to `transparency_rays`
    alpha-cutout shadow rays (590-620); `shadowing_rays_check_penumbra_denom` = 2.
  - No soft shadows: single shadow ray chain with transparency (668-702).
- Outputs: `dest_light` = Σ light × indirect_energy × exposure (bounce source),
  `accum_light` = Σ static lights (L0 + 4 SH coeffs if SH, 898-948), shadowmask for the
  shadowmask directional light (954-958).

Port notes: Halton table + vogel disk + hash RNG verbatim. Bilinear sampling of albedo
atlas for alpha-cutout (`trace_ray_closest_hit_triangle_albedo_alpha`, 317-336) → CPU
bilinear fetch from the RGBA8 albedo image (clamp-to-edge, matching `linear_sampler`).

### S6 — Secondary / indirect (direct port)

`MODE_BOUNCE_LIGHT` (981-1043) + `trace_indirect_light` (722-858). Per texel:
- `ray_count` cosine-weighted hemisphere rays (split into `ray_from..ray_to` chunks by
  `max_rays_per_pass` — GPU sync artifact; CPU runs the full range in one go).
- Each ray: `trace_indirect_light` — path loop (max_depth = max(bounces, 1)):
  - Front hit: barycentric UV → sample `source_light` (bounce source from S5) **or**
    re-trace lights (`USE_LIGHT_TEXTURE_FOR_BOUNCES` off — more accurate, 5× cost);
    albedo alpha (RGBA8 bilinear) + emission (RGBAH); throughput update with
    `bounce_indirect_energy`; transparency re-bounce (773-787); Russian roulette (791-797);
    next dir via cosine-weighted hemisphere (800).
  - Miss: environment panorama color via env transform (712-720) — **CPU: bilinear fetch
    from the RGBAF panorama image** (LightmapGI passes it; GLES3 `environment_bake_panorama`
    exists, rasterizer_scene_gles3.cpp:1167 — verified).
  - Back hit: alpha handling (805-854).
- Accumulate `light / ray_count` into `accum_light` (1038-1040); SH variant accumulates 4
  coeffs with fixed constants (1009-1024).

**Critical correctness note:** S6 reads `dest_light` (bounce source) written by S5 and
accumulates into `accum_light` — full-pass dependency → **synchronization point** between
passes (thread pool wait), unlike the region-chunked GPU loop.

### S7 — Light probes (direct port)

`MODE_LIGHT_PROBES` (1092-1145): per probe, sphere-uniform rays (`generate_sphere_uniform_direction`),
`trace_indirect_light`, accumulate 9 SH coeffs (constants at 1112-1122, incl. L2), final
`× 4/ray_count` on the last chunk (1135-1139). Port verbatim; probe count is small (8-~10k)
→ cheap.

### S8 — Pack L1 (trivial)

Divide L1 coeff slices by `L0 × 8 + 1e-6`, clamp `+0.5` (1336-1347). 10 lines.

### S9 — JNLM denoise (phase 3)

Full non-local means (1179-1334): 7×7 patch, `denoiser_range` search window, weights:
spatial / light / albedo / normal / occlusion, `filter_strength × light_bandwidth` constant.
Port verbatim (it's math + the 4 input textures). **Cost concern:** O(texels × search² ×
patch²) with search window up to 10+ → at 2048² this is ~2G patch comparisons per slice;
needs threading + is the reason it ships in phase 3. Skip = noisier at low ray counts
(quality via ray counts instead).

### S10 — Dilate (trivial)

8-directional outward raycast to nearest valid texel, radius ≤ `4 × supersampling` (1147-1177).
~30 lines.

### S11 — Blend seams (reimplement)

RD: depth-masked line + triangle rasterization in UV space (cpp:2267-2414) — 9 UV offsets
with blend factors 0.5/0.2/0.1 (cpp:2362-2372), using the seam vertex pairs from S2, with
the triangle interior as z-mask. This is GPU trickery; the CPU equivalent with the same
visual result:

For each seam (world-space shared edge with different UV2, from S2 — **includes intra-mesh
UV-island borders, which the extension baker's stitcher misses**):
1. Walk both UV2 edges in texel space (sample count = edge length in texels).
2. For offsets 0 (center, blend 0.5) and the 8 neighbors (0.2/0.1): read the paired texel
   on the other side, blend: `dst = mix(dst, other, blend)` per offset — matching the
   inward-outward z-order.
3. Interior mask = skip pairs whose edge is interior to a triangle (RD uses the z-mask;
   CPU: only process texels within 1 texel of the UV edge line — the edge walk covers
   exactly those border texels).

**Risk handling (S11):**
- The extension's `_stitch_lightmap_seams` is the starting point for the texel-walk
  mechanics, but it must be **extended**: (a) intra-mesh seams (S2's seam list already
  covers same-slice island borders — the extension only stitched *between* meshes), (b) the
  9-offset RD weighting (0.5/0.2/0.1), (c) same-slice only (RD seams are per-slice pairs).
- Unit test: two quads sharing a world edge with different UV layouts, baked with a
  gradient light → seam texels on both sides equal after blending; interior texels
  unchanged.
- Parity gate: in editor builds, compare seam regions against RD output with tolerance
  (same parity harness as S3).

### S12 — Output (trivial)

`lightmap_textures` = accum_light slices converted RGBH (cpp:2429-2434), shadowmask RGBA8→R8
(2436-2442), `probe_values` = 9 Colors/probe (2445-2457). `get_bake_mesh_uv_scale`/
`texture_slice` from stored offsets (2501-2512). Port verbatim — the rest of `LightmapGI::bake()`
(probe capture data, users, assignment) consumes exactly this.

---

## 4. Threading design

- **Pool**: `WorkerThreadPool::get_singleton()->add_group_task(callable, elements, tasks)`
  + `wait_for_group_task_completion` (core/object/worker_thread_pool.h — verified).
  Always-on; single-thread fallback when `get_thread_count() <= 1`.
- **Granularity**: one group task per pass (S4, S5, S6, S7, S10) over texel ranges; worker
  index → contiguous texel range → no atomics for writes (each texel owned by one worker).
- **Sync**: `wait_for_group_task_completion` between S5→S6 (bounce reads direct output) and
  between passes that ping-pong images.
- **Progress/abort**: `std::atomic<bool> aborted`; the `BakeStepFunc` is called from the
  main thread between passes + periodically from a progress counter (`std::atomic<int>`);
  workers check `aborted` per texel chunk.
- **Determinism**: RNG is seeded per-texel from `(atlas_pos, prime)` — output is identical
  regardless of thread count. Free.
- **Data layout**: hot passes operate on raw `PackedByteArray`/float buffers, not
  `Image::set_pixel` (which is slow). Convert to `Ref<Image>` only at S12.

---

## 5. Performance analysis

Rough cost model (grid DDA ≈ 20-100 triangle tests/ray, ~8-core desktop):

| Scene | Texels | Quality | Direct (S5) | Indirect (S6, 128 rays) | Total est. |
|-------|--------|---------|-------------|--------------------------|-----------|
| Reference room (256² × 2 slices, 4 lights) | 131k | MEDIUM | ~1-2 s | ~3-6 s | ~5-10 s |
| Reference room (512² × 2, 6 lights) | 524k | HIGH | ~4-8 s | ~10-20 s | ~15-30 s |
| Full level (2048² × 4, 12 lights) | 16.7M | HIGH | ~2-4 min | ~5-15 min | ~7-20 min |

Notes:
- Quality scales via `ray_count` (soft shadow samples) and bounce rays — same project
  settings as RD (`bake_quality/*_quality_ray_count`), so LOW is ~4× cheaper than HIGH.
- The GPU baker's region sync (`region_size`, `max_rays_per_pass`) exists to avoid GPU
  timeouts — irrelevant on CPU; keep the settings registered for surface parity but ignore
  (or use `max_rays_per_pass` as progress-chunk size).
- **Memory**: atlas arrays (albedo RGBA8, emission RGBAH, position RGBAF, normal RGBAH,
  accum RGBAH×slices(×4 SH)) at 2048² × 4 slices ≈ 500 MB CPU-side. Room-scale (≤1024²)
  ≈ 120 MB. Acceptable for v1; note as a known limit (same footprint RD has on GPU).
- Denoiser (S9) would cut required ray counts 4-16× at the cost of a heavy CPU pass —
  net win at high quality; phase 3.

---

## 6. What cannot be ported directly (decisions)

1. **Bilinear texture sampling**: RD samples via hardware samplers (linear, clamp). CPU:
   implement one small bilinear sampler (float) for albedo/emission/panorama fetches.
   Precision will differ slightly from GPU (filtering order) — parity tests must use
   tolerance, not bit equality, for any sampled value.
2. **The wireframe pass**: emulated as UV-edge pixel walk (see 3.3.2) — visual parity only.
3. **Area lights**: the reference title uses AreaLight3D on GL compat (screens/windows) — dynamic
   rendering works in the GLES3 rasterizer (unaffected). Baked contribution needs the
   area-light texture atlas: GLES3 `bake_render_area_light_atlas` returns **empty**
   (rasterizer_scene_gles3.h:971) and `_build_area_light_texture_atlas` early-returns on
   compat (lightmap_gi.cpp:912) → RD bakes nothing for area lights on compat. The CPU
   baker improves on this: build the mipmap atlas on CPU (packing layout is already
   CPU-side in `_build_area_light_texture_atlas`; mipmaps via `Image::generate_mipmaps`)
   in the lightmap_gi.cpp override, then evaluate LTC analytically (lm_area_lights_inc.glsl
   is pure math — Hill/Heitz form factors, no LUT). Baked textured area lights on compat
   become possible — a reference-title win. Scope: phase 3.
4. **Determinism vs RD**: same seeds + same math → near-identical output; float sampling
   differences only where textures are fetched. Two bakes on CPU are bit-identical.
5. **Environment panorama**: GLES3 implements `sky_bake_panorama`/`environment_bake_panorama`
   (rasterizer_scene_gles3.cpp:1053/1167, verified) → `ENVIRONMENT_MODE_SCENE`/`CUSTOM_SKY`
   work on compat. `CUSTOM_COLOR` is CPU-side already.
6. **Headless**: dummy renderer's `bake_render_uv2` returns empty images (verified earlier)
   → baking requires a real renderer context; out of scope.

---

## 7. GDScript API surface (wiring)

**Verified:** `LightmapGI::bake()` is **not bound** upstream — the bind is commented out
(`lightmap_gi.cpp:2121`). The GDScript entry point must be added by the fork. To avoid
header overrides (unsupported) and direct core edits, all GDScript API lives in the
module as `LightmapBaker : RefCounted` — the extension's class name, engine-native:

- **Node path** — `bake(from_node, output_data)`: calls the existing C++
  `LightmapGI::bake(root, "")` (in-memory mode from the goblin override) with a module
  static step function → reuses the entire engine orchestration (gather via
  `get_bake_meshes`, `bake_render_uv2` albedo/emission, probe generation, environment,
  atlas packing, user assignment). Progress/abort: the C step callback (unusable from
  GDScript) is bridged to `progress` property + `bake_progress(percent, status)` and
  `bake_finished(error)` signals + `abort()` (atomic flag checked by the lightmapper).
- **Descriptor path** — `bake_descriptors_with_lights(descriptors, lights, output, from_node)`:
  descriptor dicts → `Lightmapper::MeshData` (albedo/emission via `RS::bake_render_uv2`
  on the descriptor mesh RID — works without scene nodes) → same `LightmapperCPU` →
  in-memory `LightmapGIData` writer (users from `user_path`/`target_instance_rid` +
  `RS::instance_geometry_set_lightmap` for RID targets, mirroring the extension's
  `get_bake_assignments`). Kept only if the reference title uses it (RFC open question 5).
- **Settings surface** — the wrapper exposes the full engine bake settings (same names as
  `LightmapGI`, see RFC "Settings surface") applied to both paths; extension-only
  approximation settings are dropped by design (physical baking replaces them; mapping in
  RFC).
- **Unwrap** — `static lightmap_unwrap(mesh, transform, texel_size)`: 1-line wrapper over
  `ArrayMesh::lightmap_unwrap` (functional at runtime once the module registers
  `array_mesh_lightmap_unwrap_callback`).
- **Parity with extension**: same class name, same method names, `get_gathered_mesh_count()`,
  `get_bake_assignments()`; the `ClassDB.class_exists("LightmapBaker")` guard disappears
  (always registered). Reference-title call sites unchanged; extension unloaded.

Notes: `LightmapGI::bake()` remains C++-only in the engine path (called by the wrapper
and the editor). No `lightmap_gi.h` change needed → no direct core edit.

## 8. Implementation order

**Phase 1 — MVP (direct lighting, no probes/indirect):**
1. Module skeleton (`modules/lightmapper_cpu/`: config.py, SCsub, register_types setting
   `Lightmapper::create_cpu`, class `LightmapperCPU`, settings GLOBAL_DEFs — moved from
   lightmapper_rd's register_types.cpp:50-64).
2. Port S1, S2 (structures + grid), S3 (raster), S4 (unocclude), S5 (direct), S10 (dilate),
   S12 (output). Grid tracer (S2/S4/S5 share it).
3. Goblin overrides: `scene/3d/lightmap_gi.cpp` mirror (in-memory bake, skip
   `_save_and_reimport_atlas_textures`, build Texture2DArray directly; warnings restructure
   §9b.3b) + `editor/scene/3d/lightmap_gi_editor_plugin.cpp` mirror (bake-button gate
   §9b.3a) + runtime unwrap callback.
4. Trim changes (with permission, ADR 0003 evidence): disable `lightmapper_rd`; re-enable
   `tinyexr` (editor EXR save, §9b.3c).
5. `LightmapBaker` wrapper class: node path + unwrap + signals/progress.
6. Gate: room bakes at runtime on GL compat; no leaks; injection renders; GDScript
   `LightmapBaker.bake(root, data)` works end-to-end; editor bake button enabled + bake
   completes with the CPU baker (smoke test).

**Phase 2 — indirect + probes + SH + shadowmask + env + supersampling:**
S6 (bounce), S7 (probes), S8 (pack L1), SH accumulation in S5/S6, shadowmask, supersampling
factor plumb-through, S11 (blend seams), per-mesh `lightmap_scale` verification.

**Phase 3 — denoiser + area lights + polish:**
S9 (JNLM CPU, threaded), LTC area lights, threading tuning, memory reduction, parity
harness vs RD (editor build) with tolerance.

---

## 9. Risks

- **S3 raster fidelity** (edge pixels, smoothing) is the highest-visual-risk port — the
  extension baker's rasterizer is a good base but lacks the wireframe pass + smoothing.
- **S11 seam blending parity** — GPU depth-mask trick; CPU approximation must be visually
  verified (seam-free look).
- **CPU time at high quality** — mitigations: quality settings, threading, denoiser later.
- **Rebase churn** on the `lightmap_gi.cpp` override (localized: bake tail + save path).
- **Float vs double builds** — explicit `float` in the module (section 2).
- **Memory at 2048²+** — section 5.

## 9b. Unverified points — verification results (2026-08-14)

1. **GLES3 `bake_render_uv2` emission channel at runtime — VERIFIED.**
   - Channel order matches `RSE::BAKE_CHANNEL_*` (ALBEDO=0, NORMAL=1, ORM=2, EMISSION=3;
   rendering_server_enums.h:810-813; `_render_uv2` glDrawBuffers, rasterizer_scene_gles3.cpp:4134-4140).
   - `scene.glsl` writes `emission_output_buffer.rgb` at location 3 (line 2770); emission
   comes from material emission (srgb→linear, line 2399).
   - Emission target: RGBA16F when float textures supported, RGBA8 fallback
   (rasterizer_scene_gles3.cpp:4363-4370); readback converts to RGBAH (4466-4470) — matches
   `lightmap_gi.cpp:1172` expecting RGBAH.
   - **Zero editor dependencies** — pure GL + texture readback, no TOOLS/EditorPaths.
   - `_render_uv2` renders `PASS_MODE_MATERIAL` with lights disabled + 9 UV offsets (dilation
   of border texels) + final full pass (4157-4180) — albedo/emission edges are pre-filled,
   which the CPU baker's own S3 does not need to replicate (it bakes attributes itself).
2. **Probe capture-data path in non-editor builds — VERIFIED.**
   - Post-bake probe processing (lightmap_gi.cpp:1544-1685): Delaunay3D tetrahedralization,
   BSP plane dedup, `gi_data->set_capture_data` → RS probe capture calls. Pure core math +
   RenderingServer — no TOOLS/editor-only APIs. `Delaunay3D` is core/math. Runs in any build.
3. **Editor bake panel with `lightmapper_rd` disabled — ISSUES FOUND (3 files).**
   a. **Bake button disabled**: `editor/scene/3d/lightmap_gi_editor_plugin.cpp:197-211` —
   the `#else` branch (no `MODULE_LIGHTMAPPER_RD_ENABLED`) calls `bake->set_disabled(true)`
   with tooltip "the `lightmapper_rd` module was disabled at compile-time". Needs a second
   editor override: gate on
   `#if defined(MODULE_LIGHTMAPPER_RD_ENABLED) || defined(MODULE_LIGHTMAPPER_CPU_ENABLED)`;
   RD GPU check (`can_create_rendering_device`) applies only when RD is the baker.
   b. **Misleading node warning**: `get_configuration_warnings()` (lightmap_gi.cpp:2001-2018)
   — same restructure in the lightmap_gi.cpp override: no warning when the CPU module exists;
   shadowmask warning preserved; "cannot be baked" only when neither module exists.
   c. **Editor bake save is broken in the fork TODAY (pre-existing, verified)**: the editor
   save flow (`_save_and_reimport_atlas_textures`) writes lightmaps as `.exr`
   (lightmap_gi.cpp:894); `Image::save_exr` returns `ERR_UNAVAILABLE` without
   `save_exr_func` (image.cpp:2824-2829), registered only by `modules/tinyexr`
   (register_types.cpp:46) — and **tinyexr is in the fork's trim list**. So even with
   `lightmapper_rd` enabled, editor bakes fail at the save step today. Decision: re-enable
   `tinyexr` (remove from `DISABLE_MODULES`, evidence: the editor lightmap bake requires the
   EXR writer; ADR 0003 evidence standard) — restores the upstream editor flow 1:1 with the
   CPU baker; runtime path is unaffected (in-memory, no EXR).

**Scope update (phase 1):** +1 editor override (`lightmap_gi_editor_plugin.cpp` via
`_GOBLIN_FILE_OVERRIDES` "editor" dict — B-04 precedent), warnings restructure in the
existing lightmap_gi.cpp override, `tinyexr` trim reversal (with permission, per ADR 0003).

## 9c. C-01 — LightmapGI frustum culling (#71585) — root cause verified (2026-08-14)

**Symptom (upstream, Vulkan/RD):** lightmaps vanish from meshes once the LightmapGI
node's origin leaves the camera frustum.

**Root cause (verified in this fork):**
- `LightmapGI::get_aabb()` returns an **empty `AABB()`** (lightmap_gi.cpp:1820-1822).
  LightmapGI is a `VisualInstance3D`, so it is a cullable instance with a zero-size
  AABB at its origin.
- Cull gate (renderer_scene_cull.cpp:2930): an instance is culled unless
  `IN_FRUSTUM(...)` passes **or** `FLAG_IGNORE_ALL_CULLING` is set. `INSTANCE_LIGHTMAP`
  instances failing the gate are dropped from `cull_result.lightmaps` (2971-2972).
- **RD**: `render_scene` consumes the list per frame
  (`render_data.lightmaps = &p_lightmaps`, renderer_scene_render_rd.cpp:1454) → the
  lightmap's textures/data are not bound → lightmaps disappear. This is the #71585 path.
- **GLES3**: `p_lightmaps` is **never used** (signature only, rasterizer_scene_gles3.cpp:2388)
  — lightmap binding is per-geometry-instance (`inst->lightmap_instance` → lightmap data,
  3580-3617), set once by `instance_geometry_set_lightmap`. Culling of the node does not
  affect it. If the reference title sees the symptom on GL compat, the repro must be checked at P1 — the
  classic mechanism is RD-only.

**Upstream status:** issue open since 2023-01-17, no fix, no linked PR (2026-08-14).
Policy: our fix stays; if upstream lands one, swap theirs in (user direction).

**Fix (in the existing lightmap_gi.cpp override):** the node has no visual representation
and culling serves no correctness purpose (geometry instances select their lightmap via
their own RID reference; multiple LightmapGI nodes are fine — all instances in the list,
per-instance references pick the right one).
```cpp
// POST_ENTER_TREE (lightmap_gi.cpp:1705):
RS::get_singleton()->instance_set_ignore_culling(get_instance(), true);
```
`instance_set_ignore_culling` (rendering_server.cpp:3232, renderer_scene_cull.cpp:1156)
sets `FLAG_IGNORE_ALL_CULLING` → the gate at 2930 always admits the instance. Works for
all renderers; harmless on GLES3; no renderer changes. Upstream-acceptable shape.
Bonus: the RS method is GDScript-bindable, so the reference title's existing workaround could even be
replaced by one call today.

**Effect on the reference title:** the ~17 level-script workaround functions for lightmap injection can be
deleted once C-01 + C-02 land (reference-title-side cleanup).

## 10. Verification gates (per phase)

- Unit tests (module `tests/`): synthetic room — analytic direct falloff, shadow boundary,
  **zero leak across opaque wall** (S5), bounce energy decay (S6), determinism (two bakes,
  identical bytes), abort via step callback, seam blend uniformity (S11).
- Integration: reference room bake at runtime on GL compat → visual comparison against the
  extension baker output (minus leaks) and, in editor builds, against RD with tolerance.
- Gates per rules: full GDScript suite + reference level load.
