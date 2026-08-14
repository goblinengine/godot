# Master Prompt (Goblin Engine)

> Living charter: updated when locked decisions (ADRs) change a principle, decision-hierarchy weight, or non-negotiable. Maintainer: architect.

## Identity

Fork of Godot 4.7.x. Narrow, related genre set: FPS, RPG, Shooter, Boomer Shooter, Immersive Sim, Systemic game, Low-Fi/Retro.

- Focus: Systemic + Immersive Sim (systems interacting, minimal pre-scripted behavior).
- Secondary: Low-Fi/Retro presentation.
- Cross-cutting: make GDScript better overall - a first-class product, not a side effect.
- DB is first consumer. Concrete DB pain is the primary justification, but features must serve the set above.

## What "good" means

- Godot-compatible: stays a Godot engine. Trimmed, never broken. Godot projects and GDScript keep working.
- Lean: only what the genre set needs. Trim fat. Fast builds, small binaries.
- Systemic out of the box: interaction, events/stimulus, ambient perception fields (light/acoustics), cadence scheduling, timers - engine-supported, data-driven, scriptable.
- Retro-friendly out of the box: GL Compatibility, pixel-art rendering, palettes, low-fi assets.
- Performance: hot paths are first-class.
- Quality: minimal, proper, clean, maintainable, documented. No tech debt accumulation. Over-engineered spaghetti that "works" is a failure.

## Decision hierarchy (when principles conflict)

1. Hot-path performance - engine leads; DB updates to match new shapes.
2. DB compatibility - corpus compile + 342 tests + level load, every change.
3. Minimal override surface - rebase-friendliness, stable tracking.
4. Lean scope - no speculative features.

Godot compatibility sits above the hierarchy: never traded.

## Working model

- GDScript: actively improve the language (pain-driven, DB-justified). Fewer Variant workarounds, better typing.
- Structural decisions: short ADR. Routine fixes: none.
- Agents verify + report: build, test, state unverified gaps. Never claim done without proof.
- Poorly defined feature: refuse implementation, escalate to architect. Do not "make it work" ad hoc.
- Engine-side systemic support beats scripted workarounds when DB can migrate to it.

## Non-negotiables

- Godot compatibility: no breaking Godot projects, APIs, or GDScript semantics. Trim and extend, don't fork behavior.
- Naming: upstream Godot conventions. No `Goblin*` / `goblin_*` on code.
- Hard rules: `.kilo/rules/rules.md`.
- Docs: fork docs updated with every architecture/feature change.
