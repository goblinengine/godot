# ADR 0008: Standalone Additive Feature Modules

Status: Accepted

Date: 2026-08-14

## Context

The MIDI / SoundFont feature (`MidiStream`, TinySoundFont synth, importers) is **100% additive**: zero overrides, zero branding, zero upstream mirrors. The fork treats `modules/goblin/` as a pseudo engine root: code placed there should mirror where it would live in the upstream source tree. An upstream MIDI playback feature would be a root-level module (`modules/midi/`, following the `modules/mp3/` / `modules/interactive_music/` pattern — stream + playback + importer + thirdparty + doc_classes + tests).

Two earlier placements were tried and superseded:

- `modules/goblin/midi/` (initial) — rejected: placed additive code where override code belongs, required two custom hooks (a `SCsub` SConscript and a cross-registration call from goblin's `register_types.cpp`), and denied the feature the standard module lifecycle.
- `modules/goblin/modules/midi/` (interim "rev 2") — rejected: repackaged the feature inside goblin's `modules/` mirror with custom SConscript + cross-registration wiring again, and denied independent module gating.

## Decision

Additive feature modules live at the repo root in `modules/<name>/` as standalone Godot modules, using standard module anatomy (`SCsub`, `config.py` with `can_build()`/`get_doc_classes()`/`get_icons_path()`, `register_types.{h,cpp}`, `doc_classes/`, `tests/`, in-module `thirdparty/`, `editor/icons/`). The build auto-discovers them; they get the standard module lifecycle (`MODULE_<NAME>_ENABLED`, `DISABLE_MODULES` gating, own registration via the generated `register_module_types.gen.cpp`). `modules/goblin/` remains the home of override machinery, branding, and fork documentation only.

The MIDI feature is at `modules/midi/` (2026-08-14): sources, `register_types.{h,cpp}`, `SCsub`, `config.py`, `doc_classes/`, `tests/test_midi_stream.h`, `editor/icons/`, `thirdparty/tinysoundfont/`. No goblin hooks (no SConscript, no cross-registration). TSF/TML license entries remain in `modules/goblin/core/COPYRIGHT.txt` (the fork's license generator reads only that file) with root-relative paths `modules/midi/thirdparty/...`.

Future additive features follow the same rule: standalone module in `modules/`, never inside `modules/goblin/` unless it is override/branding work.

## Consequences

Positive:

- MIDI gets the standard module lifecycle: `can_build()`, per-target gating via `DISABLE_MODULES`, own registration, own docs, own icons, own tests — all auto-discovered with zero custom hooks.
- `modules/goblin/` stays override/branding-only; the pseudo-root mirror rule holds (goblin = fork's overrides, `modules/` = fork's additive features).
- No mirror-drift discipline applies (the module mirrors nothing upstream).
- The module is upstream-portable as-is.

Negative / risks:

- Two fork code surfaces exist (goblin for overrides, `modules/` for additive features). Accepted: additive code has no upstream mirror, so the drift-tracking hazard of the override model does not apply to it.
- If upstream ever adds a `modules/midi/`, shared filenames (`register_types.cpp`, `SCsub`, `config.py`) could conflict on rebase. Accepted: nothing midi exists upstream today (verified: no MIDI playback/SoundFont anywhere in the 4.7.1 tree); handle at that time if it happens.
- `rules.md` hard rule 2 amended to permit additive feature modules under `modules/` (overrides remain goblin-only).
