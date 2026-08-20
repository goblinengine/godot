# FastSceneTree — Implementation Plan

> **PIVOT 2026-08-17 (user directive): module + BaseSceneTree seam REJECTED.**
> Direction: **modify `SceneTree` in place** via the goblin mirror
> `modules/goblin/scene/main/scene_tree.cpp` (single core-file swap; content =
> faithful upstream copy). No module, no base-class seam, no retype ripple:
> `get_tree()`/`SceneTree::get_singleton()`/editor/PM stay upstream-typed, and
> the optimized tree runs everywhere (editor, PM, games) for free. The old
> module-era plan below (§2–§10) is kept for history only.
> **Acceptance = suite green + no regressions + smoke boots** (A/B benchmark
> baseline and per-project opt-out are lost vs the module design — accepted).

- **Spec (superseded):** `modules/goblin/docs/rfc/fast-scene-tree-rfc.md`
- **Backlog:** M-14
- **Edit home:** `modules/goblin/scene/main/scene_tree.cpp` + direct core edit
  `scene/main/scene_tree.h` (user-sanctioned: 7 lines — 2 ProcessGroup flags
  + 3 cached StringName members; the only upstream file touched)

---

## What shipped (2026-08-17)

### Batch 1 — T1: copy-free process iteration

`_process_group()` no longer copies every group's node list per
frame/physics tick. Removals null-mark the slot (no vector shift, no iterator
invalidation, no per-frame `nodes_copy` alloc). New `_compact_process_nodes()`
helper compacts lazily on the next pass; re-sorts only when order changed.
`nodes_removed_on_group_call` lookup in `_process_group` removed (provably
redundant — exit-tree null-marks before `node_removed`). Live-list iteration
with captured count + per-iteration re-read + null-skip.

**Files:** `scene/main/scene_tree.h` (+2 ProcessGroup flags) + mirror.

### Batch 2 — T6: copy-free group calls + timer ref efficiency

`call_group_flagsp` / `notify_group_flags` / `set_group_flags` /
`_call_input_pause`: read the CoW-shared group vector via `ptr()` instead of
`ptrw()` — old code force-detached the copy on **every** group call (full
alloc + memcpy); shared buffer now only copies if the group mutates mid-call.
`process_timers()`: binds the stored `Ref<SceneTreeTimer>&` (no refcount churn
per timer per frame — matches `process_tweens`). Cached signal names
(`process_frame`, `physics_frame`, `timeout`) replace per-frame/per-timer
`SNAME()` lookups.

**Files:** `scene/main/scene_tree.h` (+3 StringName members) + mirror.

### Verification (2026-08-17)

- Build green (trim 28/28, `godot_physics_2d` re-enabled)
- Doctest suite: **1337/1337 passed, 0 failed, 1 skipped** (baseline match)
- GDScript suite: 516 assertions ✓ | Completion: 635 ✓ | LSP: 57,556 ✓
- 4000-node churn stress: group counts exact (2000/6000), survivors process
  20 frames, disabled stop at 5, re-added process 11 frames — all PASS
- Editor/PM/game boots clean; byte-identical script output
- Timing print: 1100 → 416 usec (~62% faster)

### Found, not introduced here (separate GDScript-VM ticket recommended)

`get_tree().call_group()` with 0-arg / nested-callp dispatch is flaky in the
fork — dispatched calls reach `GDScriptFunction::call` (err=0) but
script-member writes don't stick (heisenbug; debug prints alter behavior).
The group-call **iteration** code is fine (verified: `_process` increments via
the same dispatch path stick; `notify_group` and `call_group_flags` with args
work; `Object.call()` works). All machinery in the failing path (VM
`variant_addresses`/instruction_args, MethodBindVarVarArg, compiler emission,
tree binds) is upstream-identical; fork diffs (shaped-dict / `then`/`elthen`)
are unrelated.

---

## What's deferred (out of scope under tree-only constraint)

Per the 2026-08-17 pivot, only `scene_tree.{cpp,h}` edits ship. Items below
touch other files or have observable behavior effects — deferred unless a
narrower sanction is granted:

| Tier / M-item | What | Why deferred |
|---|---|---|
| **T2** | Intrusive group lists, direct Callable dispatch, incremental order | Needs `node.h` (per-node group membership links) — out of tree-only scope |
| **T3** | Ordered unique-group-call flush | Replaced `_flush_ugc` HashMap order — same copy cost as T6 already fixed |
| **T4** | Subtree pause/suspend mode flags (skip paused subtrees wholesale) | Needs node-side `Tree` data (process mode/pause state) — `node.h` edit |
| **M4** | Incremental children cache (no full dump+sort on remove) | `node.cpp:1771` — not `scene_tree.cpp` |
| **M5** | Coalesced `tree_changed()` (dirty flag, emit once at frame end) | Signal timing is observable — behavior-visible change |
| **M6** | Iterative `_propagate_*` (flat worklist vs recursive O(subtree)) | `node.cpp:595` propagation — not scene_tree.cpp |
| **M7** | `SceneTreeTimer` free-list pooling | Marginal (~2x speedup on timer-heavy tests only); `Ref<>` reuse (batch 2) covers the hot path |

## Original plan (history — module-era design, NOT followed)

The sections below describe the rejected full-reimplementation approach
(`FastSceneTree : public MainLoop` + `BaseSceneTree` core header seam +
`modules/fast_scene_tree/` module). They are kept for architectural context;
**do not follow them.** The actual implementation is the in-place approach
documented above.

### Phases (historical — module era)

| Phase | What | Gate |
|-------|------|------|
| P1 | Contract test matrix (reference behavior) | Matrix green on base SceneTree |
| P2 | Module skeleton + BaseSceneTree seam + node.h retype | Game runs on FastSceneTree |
| P3 | Full contract implementation | Matrix + suite + corpus green |
| P4 | Groups + determinism | Group-flag matrix green |
| P5 | Optimizations (T1–T6, M4–M7) | Behavior identical |
| P6 | T5 cadence API + SimServer hooks | Cadence-equivalent |
| P7 | Full validation | All gates green |
| P8 | A/B benchmark | Numbers recorded |
| P9 | Docs lock | Docs consistent |

### Risks (historical)

- node.h scene edit was upstream-touch — now **not needed** (in-place approach
  only touches scene_tree.{cpp,h}).
- Re-implementation semantics drift — **not a concern** (in-place edits, no
  new contract to match).
- Type-identity divergence — **not a concern** (SceneTree still IS the tree).
- Scope — reduced to copy-free iteration + name caching (no full re-implementation).
