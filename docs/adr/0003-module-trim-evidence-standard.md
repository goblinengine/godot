# ADR 0003: Module Trim Evidence Standard

Status: Accepted

Date: 2026-08-12

## Context

A lightweight fork should strip modules the game does not use to reduce compile time, binary size, and maintenance surface. But trimming the wrong module breaks the editor, import pipeline, or a feature the reference title relies on. Trims must be justified by evidence, not assumption.

## Decision

A module may be disabled only when all of the following are verified:

1. The reference title does not directly reference the classes or APIs it provides (checked against `project.godot`, the script corpus, and content files).
2. It is not a dependency of another module the reference title uses (checked via `config.py` dependency chains).
3. Removing it does not break editor functionality needed for reference-title development (editor-only modules are trimmed from export templates, not the editor, unless verified safe).

The trim list is fixed in `modules/goblin/config.py` under `DISABLE_MODULES`. Networking (`enet`, `websocket`, `webrtc`, `upnp`, `multiplayer`, `jsonrpc`) is explicitly kept because multiplayer is planned. `noise` is kept for potential procedural content. All target platforms are kept because cross-platform support is desired.

The current fixed trim list disables 28 modules across image formats, audio/video, VR/XR, 3D nodes/importers, navigation, physics, shader compilation, and utility.

**Mechanism (amended 2026-08-16, ADR 0012):** the trim engages via build-time option injection, not `env.disabled_modules`. `modules/goblin/config.py` mutates `SCons.Script.ARGUMENTS` at import time (first module loop, SConstruct:474, before `opts.Update` at SConstruct:499), setting `module_<name>_enabled = "no"` for every `DISABLE_MODULES` entry the user did not set on the CLI. The gate at SConstruct:1113 then skips the modules regardless of alphabetical position. User CLI wins; `configure()` prints a canary count. The earlier `env.disabled_modules` mechanism was a no-op (never a compile gate) and is removed.

## Consequences

Positive:
- Every trim is defensible and reversible by editing one set.
- Trims are auditable against reference-title evidence.
- The trim gate now actually engages (B-01 was falsely `done` until 2026-08-16: all modules compiled).

Negative:
- The trim list is a manual decision surface; a future reference-title feature may require re-enabling a trimmed module.
- Some modules are disabled unconditionally (e.g. texture compression modules in the editor build) — this must be verified against the editor import pipeline (see backlog D-07).
- **Evidence re-validation pending:** the original "verified unused" evidence was gathered while the trim was a no-op — every module compiled, so nothing was actually exercised. Re-validation is a B-01 plan gate (corpus import pass, plan gate 8); any asset format served by a trimmed module → re-enable that module.
- **`glslang` trim = GL-compat-only fork:** `RenderingDevice::shader_compile_spirv_from_source` fails without glslang, so Forward+/Mobile runtimes are broken by design (accepted; the fork targets GL Compatibility). Re-enable glslang for RD targets.
- **`godot_physics_2d` trim = dummy 2D physics:** 2D physics falls back to `PhysicsServer2DDummy` (graceful). `godot_physics_3d` is KEPT: jolt_physics registers "Jolt Physics" without setting the default server, so GodotPhysics3D remains the registered default (boot fallback + test suite).

## Related Documents

- [ROADMAP.md](../ROADMAP.md) — §5 Module Trim Plan
- [backlog.md](../backlog.md) — D-07
