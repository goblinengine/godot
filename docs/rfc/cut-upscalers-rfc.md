# RFC: Engine-side CUT Upscalers for GL Compatibility (GLES3 3D Scaling)

Proposed 2026-08-14 (architect). Exploratory companion to the locked implementation spec
`../plans/cut-upscalers-plan.md`. Direction approved; this RFC records the
options evaluated for the exploratory decisions (license path, enum strategy, scope, config
surface) and the reasoning behind the locked choices. Locked decisions live in ADR 0009 +
the plan.

## Context

- GLES3 3D scaling today: OFF/BILINEAR/NEAREST only; every other mode downgrades to
  bilinear with `WARN_PRINT_ONCE` (`render_scene_buffers_gles3.cpp:158-169`).
- FSR1/FSR2 are compute-only (`servers/rendering/renderer_rd/effects/fsr.cpp`) — RD's
  upscalers cannot run on GL Compatibility (no compute).
- Fork genre set is retro/low-fi; pixel-art content is CUT's strength (CUT > FSR1 on
  stylized, loses on photographic). Canvas shaders cover 2D only — the 3D upscale
  path (`post_copy` in `drivers/gles3/effects/post_effects.cpp:91`, call site
  `rasterizer_scene_gles3.cpp:3067`) is unreachable from shader-side workarounds.
- Algorithm source: https://github.com/Swordfish90/cheap-upscaling-triangulation
  (GPL-3.0). Cannot be copied or adapted. The "MIT fork" is license-invalid (MIT-licensed
  fork of GPL code remains GPL-bound derivative work). Clean-room is the only safe path.

## Options considered

### O1 — Enum strategy
- **O1a: Direct core edit of `rendering_server_enums.h`** (LOCKED). Add 3 values + extend
  the inline `scaling_3d_mode_type()`. Swap mechanism operates on compiled sources;
  headers included by upstream files are unreachable. Backlog precedent (2026-08-13)
  sanctions direct edits when the mechanism cannot reach the change. Additive-only.
  First header-instance → recorded in ADR 0009.
- O1b: GLES3 reinterprets an existing slot (FSR = CUT1). Rejected: user-facing naming
  lies, hint string wrong, RD clamp ambiguous, breaks if FSR ever lands in GLES3.
- O1c: No enum change; private project setting. Rejected: hidden behavior, no
  viewport-level control.

### O2 — Mechanism
- **O2a: Mirror+swap set** (LOCKED): post_effects.{h,cpp}, rasterizer_scene_gles3.cpp,
  render_scene_buffers_gles3.{h,cpp}, renderer_viewport.cpp, viewport.cpp via
  `_GOBLIN_FILE_OVERRIDES` (new lib keys `drivers`/`servers`/`scene`); new goblin
  `cut.glsl` via the existing `GLES3_GLSL` builder (gles3_builders.py pattern).
- O2b: Whole-module `drivers/gles3` copy. Rejected: ~200-file mirror.
- O2c: Additive module + injection seam. Rejected: GLES3 has no custom-pass injection
  point; post_copy call site is hard-wired.

### O3 — Scope
- **O3a: Full family, shared core, per-variant ship gates** (LOCKED). Variants differ in
  pass count, sample count, soft-edge handling only; each shippable independently
  (CUT1 after P2).
- O3b: CUT1 only, CUT2/3 later. Rejected: shared core makes CUT2/3 ~60% marginal cost;
  staging inside one ticket keeps the early-ship option without rewrite cost.

### O4 — Config surface
- **O4a: Global `rendering/scaling_3d/cut_*`, 7 keys** (LOCKED). FSR precedent
  (`rendering/scaling_3d/intensity`). Registered from goblin PostEffects ctor — no
  `project_settings.cpp` swap.
- O4b: Per-viewport params. Rejected: viewport already carries mode + scale; bloat with
  no reference-title need.

### O5 — RD behavior
- **O5a: CUT → FSR on Forward+/Mobile** (LOCKED, `WARN_PRINT_ONCE`); Mobile falls through
  the existing FSR→bilinear clamp (renderer_viewport.cpp:162).
- O5b: Clamp to bilinear. Rejected: silently worse than the default RD path.

### O6 — Intermediates
- **O6a: `cut1`/`cut2` owned by RenderSceneBuffersGLES3** (LOCKED): format
  `color_internal_format`, sized `internal_size`, `internal3d` allocation pattern.
- O6b: Owned by PostEffects. Rejected: format unknown there; buffer header swapped anyway.

### O7 — 2D canvas CUT
- Additive module (ADR 0008 anatomy). DEFERRED (plan OQ-3): not needed for 3D scaling;
  canvas shaders already cover the genre set's 2D needs.

## Recommended direction

O1a + O2a + O3a + O4a + O5a + O6a as locked in the plan; ADR 0009 records the clean-room
boundary and header-edit precedent. 2D canvas CUT deferred.

## Open questions

Plan §8 (dynamic-blend knob exposure, fast-luma default, 2D module timing, tonemap parity
fallback, HDR intermediate format). None block implementation.

## References

Plan `../plans/cut-upscalers-plan.md`; ADR 0009; feature doc
`cut-upscalers.md`; backlog C-12/C-13.
