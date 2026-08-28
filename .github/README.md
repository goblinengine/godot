# GitHub Copilot Instruction Layer

Index for the AI instruction layer in `.github/`. Ported from `.kilo/` (Kilo's agent/data format) into GitHub Copilot's data format. The original `.kilo/` is left intact; verify behavior here before removing the source.

## Read-First Order

1. `copilot-instructions.md` — auto-loaded repo-wide rules, guardrails, hard rules. Always in context.
2. `.kilo/rules/master_prompt.md` — vision + decision hierarchy (living charter; maintained by architect).
3. `docs/CODE_MAP.md` — code map, override surfaces, landmines.
4. This file — map of the instruction layer.

## Layer Map

| Directory | Role |
|---|---|
| `copilot-instructions.md` | Auto-loaded repo-wide rules (ported from `.kilo/rules/rules.md`) |
| `agents/` | Role definitions (architect, developer) with `description` / `tools` / `argument-hint` frontmatter |
| `prompts/` | Task templates invoked via `/prompt-name` (ported from `.kilo/command/*.md`) |
| `skills/` | `SKILL.md` files triggered by description match on task (ported from `.kilo/skills/*`) |

## How to Extend

- New scoped rule → `instructions/<name>.instructions.md` with `description` + `applyTo` frontmatter.
- New agent → `agents/<name>.agent.md` with `description` / `tools` / `argument-hint` frontmatter.
- New workflow → `prompts/<name>.prompt.md` (single focused task; the typed input is appended to the template).
- New skill → `skills/<name>/SKILL.md` with matching `name` + `description`.

## Agent Location

Agents live in `agents/`. The old `.kilo` claim that agent behavior goes in `rules/agent-*.md` is wrong; treat `agents/` as authoritative.

## Source Note

Ported from `.kilo/` (2026-08-27) for the three-project agent alignment (game → extension → fork). Secrecy: the reference title is never named in this repo — genre framing only.
