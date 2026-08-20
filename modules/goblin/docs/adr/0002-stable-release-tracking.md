# ADR 0002: Stable-Release Tracking

Status: Accepted

Date: 2026-08-12

## Context

The fork must stay synchronized with upstream Godot without constant merge churn. Two tracking strategies were considered: tracking upstream `master` (continuous) or tracking official stable release tags.

Tracking `master` gives early access to features but pulls in half-baked changes, breaks the fork frequently, and makes the override surface drift. The from-scratch Goblin engine already demonstrated the cost of churn; a fork used to build a production game must prioritize stability.

## Decision

All branches track official Godot stable release tags (e.g. `4.7-stable`), not `master`. Rebases happen only at stable release boundaries.

When a new stable release arrives:
1. diff `modules/goblin/modules/gdscript/` against `modules/gdscript/` and `modules/goblin/core/` against `core/`,
2. port the goblin changes onto the new base,
3. verify the reference title loads and passes its test suite before locking.

## Consequences

Positive:
- The fork is always based on a production-tested engine.
- Rebase frequency is predictable (one per stable release).
- The divergence surface is small and reviewed wholesale at each boundary.

Negative:
- New upstream features arrive slower (only at stable releases).
- A large stable release with many changes to overridden files is a bigger one-time porting effort.

## Related Documents

- [ROADMAP.md](../ROADMAP.md) — Core Principle 4
