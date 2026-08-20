# CUT Upscalers (GL Compatibility 3D Scaling)

Engine-side CUT1/CUT2 modes for `scaling_3d_mode` on the Compatibility (GLES3)
renderer. Fragment-only — GLES3 has no compute, so FSR-family upscalers cannot run.

## Modes

| Mode | Passes | Samples | Angle resolution | Soft edges |
|------|--------|---------|------------------|------------|
| `CUT 1` (value 6) | 1 | 4/px | 45° | no |
| `CUT 2` (value 7) | 2 (intermediate at input res) | 12·I + 5·O | 30° | yes (threshold 0.20, sharpening 0.75) |
| `CUT 3` (value 8) | 3 (two intermediates) | 12·I + 4·D·I + 5·O | configurable via D | yes |

Best on stylized/pixel-art content; FSR remains the better photographic upscaler (RD only).
On Forward+/Mobile, CUT modes fall back to FSR 1.0 (warning printed once). MSAA, glow,
SSAO, and tonemapping compose normally.

## Settings (`rendering/scaling_3d/`)

| Key | Default | Notes |
|-----|---------|-------|
| `cut_blend_sharpness` | 0.5 | static blend sharpness (0–1) |
| `cut_edge_min_value` | 0.05 | minimum edge value for triangulation |
| `cut_fast_luma` | false | fast-luma toggle |
| `cut_soft_threshold` | 0.20 | soft-edge blend threshold (CUT 2/3) |
| `cut_sharpening_amount` | 0.75 | soft-edge sharpening (CUT 2/3) |
| `cut_search_min_contrast` | 0.50 | edge-search min contrast (CUT 3) |
| `cut_search_distance` | 4 | edge-search distance D, 1–8 (CUT 3) |

Dynamic-blend internals (min/max contrast, min/max sharpness) are fixed at reference
defaults; not exposed.

## Provenance

Clean-room implementation (ADR 0009): from Su & Willis (2004) and Reshetov (2009) plus the
README description of https://github.com/Swordfish90/cheap-upscaling-triangulation
(GPL-3.0 — no code read or adapted, including its "MIT fork"). Validation black-box
(output comparison only). Spec: `plans/cut-upscalers-plan.md`.

The "CUT" name and the algorithm design are attributed to Matteo (Swordfish90) — used descriptively; no code is derived from his GPL implementation.

## Implementation (2026-08-15)

All three variants implemented per the plan (P1-P4). Smoke-verified on a synthetic 4-zone
edge pattern at 4x (internal 160x90 -> 640x360): CUT1/2/3 render and differ from bilinear
(430/57600 sample points, max channel delta 93/255, all on edge regions); CUT2/3 additionally
differ from CUT1 (pass 1 + descriptor). P5 (black-box comparison vs the reference project,
corpus + level-load gates) still pending.

### Variant surface (collapsed 2026-08-15)

CUT 1 + CUT 2 ship. CUT 3 was removed entirely (enum value, search pass,
settings, `cut2` intermediate) after in-game evaluation: the edge-search pass
only modulated the already-saturated strength blend, so CUT 3 was perceptually
identical to CUT 2 while costing an extra pass. The real reference CUT 3
differs via angle-resolution edge-following (Reshetov-style), which is not
implemented; if P5 black-box validation shows it is worth having, it can be
re-added as a new enum value without compat concerns (nothing shipped).

### Algorithm reconstruction (documented inference)

The reference's exact tap math is not published; the locked observables (pass counts, sample
counts, thresholds, angle classes) come from the README. What was derived:

- **CUT1** (MODE_FINAL, 4 taps): Su & Willis data-dependent triangulation — diagonal chosen by
  min(|La-Ld|, |Lb-Lc|), barycentric interpolation in the containing triangle, flat squares
  (< `cut_edge_min_value`) fall back to bilinear, result blended with bilinear by
  `cut_blend_sharpness` (static blend; dynamic-blend internals not implemented, per plan).
- **CUT2** (MODE_PASS1 + MODE_FINAL_SOFT): pass 1 (12 taps = 2x2 square + 8 side-adjacent
  ring, input res) computes a per-square edge descriptor (3-bit orientation class + 5-bit
  strength) in the alpha channel and performs soft-edge sharpening: antialiased pixels
  (smooth softness from the relative score, normalized against `cut_soft_threshold` x4 so
  the whole band softens) are pulled toward the dominant luma cluster by
  `cut_sharpening_amount` (default 0.4; 0.75 hardened soft content into "outlines").
   Final pass (5 taps: 4 corners + descriptor) reconstructs per class: diagonal ->
   triangulation, horizontal/vertical -> step at the middle boundary, strength-modulated
   sharpness blend.
- **CUT3** (search pass): removed — see "Variant surface" above.

### Deviations from the plan (all minor, recorded for review)

- `ProjectSettings::register_setting()` does not exist in this fork (only `GLOBAL_DEF`);
  settings registered via `GLOBAL_DEF(PropertyInfo(...))` in the goblin PostEffects ctor.
- post_copy call site 3141 (multiview) passes `scaling_3d_mode` explicitly instead of using
  the default param — preserves the existing NEAREST-in-multiview behavior exactly.
- `post_cut()` carries depth + SSAO params (the plan's listed signature omitted them; the
  locked §4.2 parity target requires SSAO in the final passes).
- `scaling_3d_mode_type()` rewritten to exclusion form (TEMPORAL list, explicit OFF -> NONE,
  everything else SPATIAL) — the positive SPATIAL list grew with every mode; behavior
  identical for all existing modes.
- Two additional renderer_viewport clamps discovered by smoke testing: the FSR-availability
  clamp and the render-size switch both had to admit CUT modes or CUT silently fell back to
  bilinear / "unknown mode".
- Mirror include style: the 3 same-dir-relative includes (`rasterizer_scene_gles3.h`,
  `viewport.h`, `viewport.compat.inc`, `renderer_viewport.h`) were rewritten to
  root-relative so the CPPPATH overlay resolves them.

### Files

- Direct edit: `servers/rendering/rendering_server_enums.h` (additive enum 6/7/8 + SPATIAL,
  ADR 0009 precedent).
- Mirrors: `modules/goblin/{drivers/gles3/effects/post_effects.{h,cpp},
  drivers/gles3/rasterizer_scene_gles3.cpp, drivers/gles3/storage/render_scene_buffers_gles3.{h,cpp},
  servers/rendering/renderer_viewport.cpp, scene/main/viewport.cpp}` (swapped via
  `_GOBLIN_FILE_OVERRIDES` in `modules/goblin/config.py`; mirror objects compile with the
  goblin tree as CPPPATH overlay).
- Shader: `modules/goblin/drivers/gles3/shaders/effects/cut.glsl` + committed
  `cut.glsl.gen.h` (generated by the upstream `GLES3_GLSL` builder); byte-identical MIT
  copies of the 4 `*_inc.glsl` includes in `modules/goblin/drivers/gles3/shaders/`.
- Buffers: `cut1` (internal_size, **explicit GL_RGBA8** — NOT `color_internal_format`:
  on GLES3 without HDR the internal buffer is GL_RGB10_A2, whose 2-bit alpha
  quantizes the edge descriptor to 4 levels and silently breaks the state-driven
  reconstruction; found 2026-08-15 via the "170 = 2/3" readback anomaly),
  NEAREST + CLAMP_TO_EDGE, owned by RenderSceneBuffersGLES3; CUT2 uses cut1,
  CUT1 has no intermediate.
- The CUT shader state is a file-scope static in post_effects.cpp, NOT a PostEffects
  member: the class is instantiated by the upstream rasterizer_gles3.cpp with the
  upstream header's sizeof, so the mirror header must not change the class size
  (B-14 — this exact mistake corrupted the heap at every renderer init).
- The final passes output opaque alpha (`frag_color = vec4(color.rgb, 1.0)`): the
  sampled texels' alpha carries the edge descriptor, so without forcing 1.0 the
  viewport texture / captures came out transparent (alpha ~0 in flat regions),
  which any alpha-compositing viewer shows as the "skeleton / gray background"
  look (found 2026-08-15).
