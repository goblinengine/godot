---
name: toon-format
description: Minimal-token response style - telegraphic prose + TOON structured output. Load when user requests TOON/telegraphic/minimal style, or when terse structured output fits the task.
---

# TOON Format + Telegraphic Style

Goal: fewer tokens, no meaning loss.

## Telegraphic prose

- No articles (a, an, the). No pronouns (I, you, it). No filler.
- Logic symbols: `->` implies/leads to. `!` not/error. `?` condition. `+` and.
- Reasoning: flat chain, no nested prose: `Step1 -> Step2 -> Result`.
- Examples:
  - Verbose: "If the user inputs a valid token, then the system should process it."
  - Telegraphic: "Input valid token -> System process."
  - Verbose: "Because the database is full, we cannot save the new record."
  - Telegraphic: "DB full ! -> ! Save record."

## TOON (Token-Oriented Object Notation)

Structured data output:

- Objects: `key: value`
- Nesting: indentation
- Arrays: `[N]` length marker
- Uniform arrays: `[N,]{fields}:` header + rows

Example:

```
users[2,]{id,name}:
  1,Alice
  2,Bob
```

## When to use

- User asks: "TOON", "telegraphic", "minimal", "token-efficient".
- Structured data in responses: config diffs, plans, specs, lists.
- Prose: default plain minimal (no articles). No preamble, no closing filler.

## Never use TOON in

- Files tools must parse: `kilo.jsonc` (JSONC), `project.godot` (Godot config), SCons (Python), C++/GDScript source. Those keep native syntax.
- Code examples: real syntax always wins over notation.
