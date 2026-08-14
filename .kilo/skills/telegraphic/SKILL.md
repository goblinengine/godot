---
name: telegraphic
description: Minimal-token response style - telegraphic prose. Load when user requests telegraphic/minimal style, or when terse responses fit the task.
---

# Telegraphic Style

Goal: fewer tokens, no meaning loss. Plain prompts, telegraphic outputs.

Applies to reasoning AND answers. No pleasantries, no preamble, no closing filler, no narration ("I've updated...", "Let me..."). Lead with result.

## Telegraphic prose

- No articles (a, an, the). No pronouns (I, you, it). No filler.
- Keep high-entropy tokens verbatim: identifiers, paths, numbers, type hints (`func(x: int) -> str`). Never rename `user_id` -> `u`.
- Logic chains: `->` implies/leads to. `!` not/error. `?` condition.
- Reasoning: flat chain: `Step1 -> Step2 -> Result`.
- Do not paste full internal reasoning back into context; summarize the conclusion.
- Code blocks: real syntax always wins. Never compress inside ```.

Examples:
- Verbose: "If the user inputs a valid token, then the system should process it."
- Telegraphic: "Input valid token -> System process."
- Verbose: "Because the database is full, we cannot save the new record."
- Telegraphic: "DB full -> save blocked."

## When to use

- User asks: "telegraphic", "minimal", "token-efficient".
- Default for terse responses: no articles, no filler, lead with result.

## Never use telegraphic in

- Files tools must parse: `kilo.jsonc` (JSONC), `project.godot` (Godot config), SCons (Python), C++/GDScript source. Native syntax.
- Code examples: real syntax always wins.
