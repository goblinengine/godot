# CUT (Cheap Upscaling Triangulation) Upscalers â€” Implementation Breakdown & Locked Spec

Locked 2026-08-14 (architect). Engine-side CUT1/CUT2/CUT3 as GLES3 3D-scaling modes.
Clean-room reimplementation (GPL-3.0 boundary, ADR 0009). Companion to
[`../rfc/cut-upscalers-rfc.md`](../rfc/cut-upscalers-rfc.md)
and [ADR 0009](../adr/0009-clean-room-cut-upscalers.md).

All line references verified against the fork tree (2026-08-14). Mechanism decisions follow
the `overrides` skill + ADR 0001/0007. **This is a planning artifact â€” no engine code written.**

---

## 1. Purpose & locked semantics

GL Compatibility (GLES3) has no fragment-only 3D upscaler: FSR is compute-only
(`servers/rendering/renderer_rd/effects/fsr.cpp`), and GLES3 3D scaling today is
OFF/BILINEAR/NEAREST only (`render_scene_buffers_gles3.cpp:158-169`). The retro/low-fi genre
set is the fork's primary consumer and CUT beats FSR1 on stylized/pixel-art content
(loses on photographic). Canvas-shader upscalers cannot reach the 3D upscale path â€” this is
not a resurrection of the rejected "native upscaler nodes" (backlog Â§5).

Locked semantics (all variants):

| Aspect | Locked |
|---|---|
| Scope | Full family CUT1+CUT2+CUT3, ONE shared core (2Ã—2 luma-plane triangulation + pattern recognition). Ship gates per variant (P2/P3/P4 below). |
| Core algorithm | Su & Willis 2004 data-dependent triangulation on luma plane; Reshetov 2009-style neighbor pattern recognition; interpolation with computed blend weights. Implemented from the papers + GPL repo README (description only). |
| Variant behavior | CUT1: 1 pass, 4 samples, 45Â° angle resolution, no soft-edge handling. CUT2: 2 passes (intermediate at input res), 12Â·I + 5Â·O samples, 30Â° resolution, soft edges (threshold 0.20, sharpening 0.75). CUT3: 3 passes, 12Â·I + 4Â·DÂ·I + 5Â·O samples, configurable edge-search distance D (1â€“8), MIN_CONTRAST 0.5. |
| Sampling | All CUT sampling uses GL_NEAREST (point). The algorithm computes its own interpolation weights â€” hardware bilinear would double-filter. Deterministic, matches the math. |
| Modes exposed | `ViewportScaling3DMode`: `VIEWPORT_SCALING_3D_MODE_CUT1 = 6`, `CUT2 = 7`, `CUT3 = 8` (before `MAX`). Existing values untouched (BILINEAR 0 â€¦ NEAREST 5). |
| Config | Global project settings under `rendering/scaling_3d/cut_*` (7 keys, Â§3). NOT per-viewport (minimal surface; viewport keeps `scaling_3d_mode` + `scaling_3d_scale` only). |
| RD renderers (Forward+/Mobile) | CUT modes clamp to FSR 1.0 + `WARN_PRINT_ONCE`; Mobile then hits the existing FSRâ†’bilinear clamp (renderer_viewport.cpp:162). RD behavior otherwise untouched. |
| Multiview | CUT invalid when `view_count > 1` â†’ clamp to bilinear + `WARN_PRINT_ONCE` (XR modules trimmed from fork; multiview is dead path, no CUT support). |
| Scale 1.0 | CUT is `VIEWPORT_SCALING_3D_TYPE_SPATIAL` â†’ existing "scale â‰ˆ 1.0 â†’ OFF" logic (renderer_viewport.cpp:147) applies unchanged. |
| Fast paths | NEAREST/BILINEAR/OFF paths byte-identical to upstream (post_copy body for non-CUT modes is the upstream body verbatim). |
| 2D path | The second `post_copy` call (rasterizer_scene_gles3.cpp:3141, canvas/2D-in-3D) keeps its default param â€” CUT applies only to the 3D upscale path (3067). |

---

## 2. Mechanism / placement (ADR refs: 0001, 0007, 0009)

### 2.1 Direct core edit (documented exception, ADR 0009)

**`servers/rendering/rendering_server_enums.h`** â€” one file, two edits:
1. Add `VIEWPORT_SCALING_3D_MODE_CUT1/2/3` after `..._NEAREST` (values 6/7/8), before `..._MAX`.
2. `scaling_3d_mode_type()` (line 474, inline) â€” add CUT modes to the SPATIAL branch.

Why direct edit: this is a HEADER; the `goblin_add_library()` swap mechanism operates on
compiled sources and cannot reach headers included by upstream files. Backlog dependency
note (2026-08-13) sanctions direct core edits when the swap mechanism cannot reach the
change. This is the first header-instance of that precedent â€” recorded in ADR 0009.
The enum change is additive-only (no existing value moves).

### 2.2 Mirror + swap (goblin_add_library `_GOBLIN_FILE_OVERRIDES`)

`config.py` dict gains three new library keys (`drivers`, `servers`, `scene` â€” all use
`env.add_library`, hook is already generic). Mirrors live under
`modules/goblin/drivers/â€¦`, `modules/goblin/servers/â€¦`, `modules/goblin/scene/â€¦`
(pseudo-root mirror layout). Each mirror is upstream-identical except the listed delta;
drift discipline per `porting` skill.

| # | Upstream file (swap) | Delta from upstream |
|---|---|---|
| S1 | `drivers/gles3/effects/post_effects.cpp` + `.h` | (a) `post_copy()` gains `RSE::ViewportScaling3DMode p_scaling_mode = RSE::VIEWPORT_SCALING_3D_MODE_BILINEAR` param; body for non-CUT modes identical (filter bool derived from `mode != NEAREST` as today). (b) new `post_cut()` executing the CUT1/2/3 pipelines (Â§4.2). (c) new `cut` shader member (`CutShaderGLES3`, from goblin `cut.glsl.gen.h`), initialized in ctor. (d) register the 7 project settings (`ProjectSettings::register_setting`, ctor). |
| S2 | `drivers/gles3/rasterizer_scene_gles3.cpp` | Call site 3067-3070: pass `p_render_data->render_buffers->scaling_3d_mode` instead of the NEAREST bool. Call site 3141 unchanged (default param). Rest of the 4821-line file mirrors upstream. |
| S3 | `drivers/gles3/storage/render_scene_buffers_gles3.cpp` + `.h` | (a) Mode switch (165-168): CUT1/2/3 no longer downgrade; add multiview guard (CUT + `view_count > 1` â†’ BILINEAR + WARN). (b) New members: `cut1`/`cut2` (color `GLuint` + fbo `GLuint`) + accessors `get_cut1_color()/get_cut2_color()/get_cut1_fbo()/get_cut2_fbo()`; allocated in `_check_render_buffers()` when mode is CUT2/CUT3 (sized `internal_size`, format = `color_internal_format` â€” same as `internal3d`), freed in the existing teardown paths. |
| S4 | `servers/rendering/renderer_viewport.cpp` | GL-compat clamp (156-160): allow CUT modes on `gl_compatibility`. New clamp: CUT mode on non-GL-compat â†’ `VIEWPORT_SCALING_3D_MODE_FSR` + `WARN_PRINT_ONCE`. |
| S5 | `scene/main/viewport.cpp` | Property hint string (5372): append `,CUT 1 (Fast):6,CUT 2 (Average):7,CUT 3 (Slow):8`. |

### 2.3 New goblin files (no mirror)

| File | Purpose |
|---|---|
| `modules/goblin/drivers/gles3/shaders/effects/cut.glsl` | The CUT shader. Modes: `MODE_FINAL` (CUT1: 4-sample, no soft edge), `MODE_PASS1` (12-sample, CUT2/3 pass 1), `MODE_EDGE_SEARCH` (4Â·D-sample, CUT3 pass 2), `MODE_FINAL_SOFT` (5-sample + soft edge, CUT2/3 final). Specializations mirror post.glsl conventions: `USE_GLOW`, `USE_SSAO_*`, `USE_LUMINANCE_MULTIPLIER`. Final passes carry the tonemap/glow/SSAO composition equivalent to post.glsl `MODE_DEFAULT` (port of Godot's own MIT-licensed tonemap block â€” new file, no upstream modification). |
| `modules/goblin/SCsub` addition | `env.GLES3_GLSL("drivers/gles3/shaders/effects/cut.glsl")` + `env.Depends(gen.h, upstream *_inc.glsl + #gles3_builders.py)` â€” same pattern as `drivers/gles3/shaders/SCsub`. Produces `cut.glsl.gen.h` in-tree (committed, like other `.gen.h`). |

### 2.4 Rejected alternatives (why not)

- **Whole-module `drivers/gles3` copy** â€” ~200 files, huge mirror. Rejected.
- **GLES3 reinterprets the FSR slot as CUT1** â€” user-facing naming lies, hint string wrong, RD clamp ambiguous, breaks when FSR ever lands in GLES3. Rejected.
- **Intermediates owned by PostEffects** â€” format unknown there; `render_scene_buffers_gles3.h` is swapped anyway (S3); buffers belong with the buffer class (upstream pattern). Rejected.
- **No viewport.cpp swap (stale hint)** â€” modes would be code-only. Rejected: editor dropdown must show them; swap cost accepted.
- **2D canvas CUT as additive module now** â€” deferred (open question OQ-3). Not part of this ticket.

---

## 3. Config surface (locked, exact keys)

Registered via `ProjectSettings::register_setting()` in the goblin `PostEffects` ctor
(no `project_settings.cpp` swap). Values already present in `project.godot` are preserved
(register_setting keeps stored values, only metadata is set). Editor shows them under
`rendering/scaling_3d/`.

| Key | Type | Range | Default | Used by |
|---|---|---|---|---|
| `rendering/scaling_3d/cut_blend_sharpness` | float | 0.0â€“1.0 | 0.5 | CUT1/2/3 (static blend sharpness) |
| `rendering/scaling_3d/cut_edge_min_value` | float | 0.0â€“1.0 | 0.05 | CUT1/2/3 (min edge value) |
| `rendering/scaling_3d/cut_fast_luma` | bool | â€” | false | CUT1/2/3 (fast-luma toggle) |
| `rendering/scaling_3d/cut_soft_threshold` | float | 0.0â€“1.0 | 0.20 | CUT2/3 (soft-edge blend threshold) |
| `rendering/scaling_3d/cut_sharpening_amount` | float | 0.0â€“2.0 | 0.75 | CUT2/3 (soft-edge sharpening) |
| `rendering/scaling_3d/cut_search_min_contrast` | float | 0.0â€“1.0 | 0.50 | CUT3 (edge-search min contrast) |
| `rendering/scaling_3d/cut_search_distance` | int | 1â€“8 | 4 | CUT3 (edge-search distance D) |

Dynamic-blend internals (min/max contrast, min/max sharpness) are NOT exposed â€” fixed
constants at reference defaults, documented in `docs/cut-upscalers.md`
(OQ-1). Read per `post_cut()` call via `GLOBAL_GET` (few dict lookups/frame; FSR precedent).

---

## 4. GLES3 plumbing (locked)

### 4.1 Buffers

- CUT1: no new buffers. Reads `rb->get_internal_color()` (existing `fbo_int` path), writes
  `fbo_rt` (target size) â€” same FBO flow as today's upscale copy.
- CUT2: 1 intermediate `cut1` (internal_size = input res). Pass 1: internal â†’ `cut1`.
  Pass 2 (final): `cut1` â†’ `fbo_rt`.
- CUT3: 2 intermediates `cut1`, `cut2` (both internal_size). Pass 1: internal â†’ `cut1`.
  Pass 2 (edge search): `cut1` â†’ `cut2`. Pass 3 (final): `cut2` â†’ `fbo_rt`.
- Format: `color_internal_format` (matches the RT/internal3d buffer â€” RGBA8 in practice on
  GLES3). Filtering: GL_NEAREST on every CUT read. No mipmaps, no MSAA.
- Allocation lazily in `_check_render_buffers()` (mirrors `internal3d` pattern), freed in
  the existing buffer teardown; recreated on resize via the existing size-change checks.

### 4.2 Final pass fusion with tonemap+copy

The goblin `post_copy()` dispatches: CUT modes â†’ `post_cut()`; everything else â†’ upstream
body verbatim. `post_cut()` signature (internal API):
`post_cut(dest_fbo, dest_size, source_color, cut1_color, cut1_fbo, cut2_color, cut2_fbo,
source_size, mode, luminance_multiplier, glow_buffers, glow_intensity, srgb_white, spec_constants)`.

Final pass parity target: tonemap + glow levels + SSAO + luminance multiplier + BCS
(spec constants) â€” same composition as post.glsl `MODE_DEFAULT`, with CUT interpolation
replacing bilinear. Fallback gate (only if shader complexity explodes during P2):
restrict CUT to the no-glow/no-SSAO path and fall back to bilinear for those frames,
documented as a limitation (OQ-4). The existing main-pass tonemap disable for scaling
(rasterizer_scene_gles3.cpp:2399-2400) applies unchanged.

### 4.3 Not-regressed paths

- NEAREST/BILINEAR/OFF: post_copy body identical; screenshots must be byte-identical.
- `fbo_int` / MSAA resolve flow: untouched (CUT reads the resolved internal color).
- Glow processing (`glow->process_glow`, 3063): runs at internal_size before the CUT
  passes â€” unchanged.
- Depth copy blit (3073-3075): unchanged.

---

## 5. Clean-room + validation (locked)

### 5.1 Hygiene (ADR 0009)

- Sources allowed: Su & Willis (2004) paper, Reshetov (2009) paper, the GPL repo's README
  description (as relayed in this ticket), Godot's own MIT code (tonemap block).
- FORBIDDEN: reading/adapting any code from the GPL repo OR from its "MIT fork"
  (a MIT-licensed fork of GPL code is still GPL-bound â€” license-invalid relicensing;
  copying from it is copying from the GPL work). No GLSL/CPP from either, ever.
- Validation may be black-box only: render the reference (built from the GPL repo) and
  compare OUTPUT images; never its source.
- Provenance note (sources + date) in `docs/cut-upscalers.md`.

### 5.2 Test plan (pass criteria per variant)

Synthetic test scenes (engine test project `tests/` or a dedicated verification scene):
(a) 45Â° and 30Â° diagonal edges, (b) checkerboard at 4:1 and 6:1 scale-down, (c) 1px lines
and small text, (d) stylized sprite sheet. Renders at 320Ã—180 â†’ 1280Ã—720 (4Ã—) and
480p â†’ 1080p.

| Variant | Gate |
|---|---|
| CUT1 | Straight-edge continuity on 45Â° edges; no ringing/overshoot >1 step; edge position within Â±1 px of black-box reference output; sample count 4/output texel. |
| CUT2 | 30Â° resolution: edges that CUT1 breaks are continuous; soft-edge blend visible at threshold 0.20, sharpening 0.75; Â±1 px vs reference; 12+5 samples. |
| CUT3 | Edge search at D=1â€¦8 finds edges up to D px; MIN_CONTRAST 0.5 rejects noise on flat regions; per-D visual diff monotone; 12+4D+5 samples. |

Regression gates (every phase): full build green (windows editor + template, `build`
skill; never `scons -c`); full GDScript suite green (1384 cases); reference corpus + 342 tests +
level load (decision hierarchy #2); NEAREST/BILINEAR/OFF before/after screenshots
identical; RD Forward+/Mobile screenshots unchanged (CUT clamps to FSR); glow/SSAO on CUT
modes within CUT tolerance of bilinear equivalents. Perf deltas recorded (indicative):
CUT1 â‰¤ +0.5 ms, CUT2 â‰¤ +1.0 ms, CUT3 â‰¤ +1.5 ms at 1080p desktop vs BILINEAR.

---

## 6. Phases (files, effort, gates)

| Phase | Deliverable | Files | Effort | Gate |
|---|---|---|---|---|
| P1 | Enum + clamps + config + plumbing skeleton | rendering_server_enums.h (direct edit); viewport.cpp (S5); renderer_viewport.cpp (S4); config.py dict (drivers/servers/scene); goblin SCsub GLES3_GLSL; post_effects.{h,cpp} mirror created upstream-identical; cut.glsl skeleton | 2â€“3 d | Build green; existing modes byte-identical; RD clamp verified; settings visible in editor; hint shows 9 modes |
| P2 | Shared core + CUT1 | cut.glsl (MODE_FINAL); post_effects post_copy dispatch + post_cut; rasterizer_scene_gles3.cpp (S2); render_scene_buffers_gles3.{h,cpp} (S3: mode switch only) | ~5 d (1 wk) | Â§5.2 CUT1 gates + regression gates |
| P3 | CUT2 | cut.glsl MODE_PASS1 + MODE_FINAL_SOFT; cut1 buffers (S3) | 2â€“3 d | Â§5.2 CUT2 gates |
| P4 | CUT3 | cut.glsl MODE_EDGE_SEARCH; cut2 buffers (S3) | 3â€“5 d | Â§5.2 CUT3 gates |
| P5 | Validation + docs closeout | verification scenes; `docs/cut-upscalers.md` final; ROADMAP Â§8; backlog/rfc/adr/plans index status flip | 2â€“3 d | Full suite green; perf deltas recorded; backlog C-12/C-13 status updated |

Total â‰ˆ 2.5â€“3.5 weeks. CUT1-only milestone at end of P2 (â‰ˆ1.5 wk to first shippable mode).

---

## 7. Risks

| Risk | Consequence â†’ Mitigation |
|---|---|
| Mirror drift on 5 upstream files (incl. 4821-line rasterizer_scene_gles3.cpp, 5897-line viewport.cpp) | Rebase churn. â†’ `porting` skill discipline; mirrors are upstream-identical except tiny deltas; B-10 drift tooling. Accepted: no mechanism reaches these call sites otherwise. |
| Clean-room violation (accidental adaptation) | License exposure. â†’ Only listed sources; black-box validation only; provenance documented; reviewer checks no CUT-derived code shape (ADR 0009). |
| Tonemap/glow/SSAO parity in final pass | Visual divergence vs bilinear when glow/SSAO active. â†’ Parity target in cut.glsl; fallback gate (restrict CUT when glow/SSAO) documented if needed (OQ-4). |
| Perf on weak GPUs (CUT3 17 M samples/frame at 1080p) | Frame time. â†’ Variant choice is user's; CUT1 is 4 samples; document cost ladder in feature doc. |
| HDR banding on RGBA8 intermediates | Posterization in bright scenes. â†’ Format matches RT (`color_internal_format`); GLES3 is LDR in practice; revisit if a scene shows banding (OQ-5). |
| Enum values 6/7/8 hard-linked to hint string | If upstream adds modes, ordering shifts. â†’ Additive append keeps upstream values; rebase collision handled by porting skill; hint string updated in same mirror. |

---

## 8. Open questions (not locked â€” recommendations)

| OQ | Question | Recommendation |
|---|---|---|
| OQ-1 | Expose dynamic-blend knobs (min/max contrast, min/max sharpness)? | No â€” internal constants at reference defaults. Expose only if the reference title asks. |
| OQ-2 | `cut_fast_luma` default? | false (quality-first; retro content is luma-dense). |
| OQ-3 | 2D canvas CUT as additive module (`modules/cut_2d/`, ADR 0008)? | Defer. Distinct ticket when a game needs it. |
| OQ-4 | Fallback gate (no-glow/SSAO CUT) if parity explodes? | Acceptable, documented limitation. Target parity first. |
| OQ-5 | Intermediate format under HDR content? | Keep RT-matching; revisit on evidence. |

---

## 9. Registration

- Backlog: C-12 (CUT1 + shared core, P1), C-13 (CUT2/3, P2) â€” `docs/backlog.md` Â§2.
- RFC: `docs/rfc/cut-upscalers-rfc.md` + rfc/README row.
- ADR: `docs/adr/0009-clean-room-cut-upscalers.md` + adr/README row.
- Feature doc: `docs/cut-upscalers.md`.
- ROADMAP: Â§8 renderer strategy note.
