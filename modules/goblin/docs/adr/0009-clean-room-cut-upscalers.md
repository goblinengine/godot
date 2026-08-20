# ADR 0009: Clean-Room CUT Upscalers + First Header Direct-Edit Precedent

Status: Accepted

Date: 2026-08-14

## Context

Engine-side CUT1/CUT2/CUT3 upscalers for GLES3 3D scaling (backlog C-12/C-13). The
algorithm's reference implementation is GPL-3.0 (Swordfish90/cheap-upscaling-triangulation)
and cannot be copied or adapted. An "MIT fork" exists, but MIT-licensed forks of GPL code
are license-invalid: the code remains GPL-bound derivative work. Separately, the
`ViewportScaling3DMode` enum lives in `servers/rendering/rendering_server_enums.h` — a
header included by upstream files; the `goblin_add_library()` swap mechanism replaces
compiled sources and cannot reach it.

## Decision 1 — Clean-room boundary

Implement CUT from the primary literature (Su & Willis 2004 data-dependent triangulation;
Reshetov 2009 pattern recognition) plus the reference project's README description only.
NEVER read or adapt source code from the GPL repository or from its "MIT fork" (same legal
work). Validation is black-box only: render reference output from the GPL project and
compare images; never its GLSL/CPP. Godot's own MIT code (e.g. the post.glsl tonemap block)
may be reused in new goblin files. Provenance documented in
`modules/goblin/docs/cut-upscalers.md`.

## Decision 2 — Header direct-edit precedent

`rendering_server_enums.h` is edited directly (additive enum values 6/7/8 + SPATIAL branch
in the inline `scaling_3d_mode_type()`). This extends the 2026-08-13 dependency note
("direct core edits allowed only when the swap mechanism cannot reach the change") to
headers: headers included by upstream files are unreachable by source-swap and therefore
qualify. This is the first instance; the mechanism remains unchanged.

## Consequences

Positive: legally safe implementation path; additive enum (no existing value moves, no
behavior change on RD); user-facing mode naming correct; precedent documented for future
header-touching features.

Negative / risks:

- Direct edit creates a small rebase surface in one header. Accepted: unavoidable —
  no mechanism reaches it; delta is additive and self-contained.
- Clean-room discipline must be enforced per implementation phase (reviewers check no
  CUT-derived code shapes appear). Accepted: hygiene rules + black-box-only validation.
- Five upstream files become mirrors (post_effects, rasterizer_scene_gles3,
  render_scene_buffers_gles3.{h,cpp}, renderer_viewport, viewport). Accepted: no other
  mechanism reaches the post-copy call site; drift handled by the porting skill.

Implementation spec: `../plans/cut-upscalers-plan.md`.
