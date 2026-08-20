# Goblin Engine — FastSceneTree RFC

> **IMPLEMENTED 2026-08-17 (in-place SceneTree optimizations).** Original
> module-era design below (`FastSceneTree : public MainLoop` + `BaseSceneTree`
> core-header seam + `modules/fast_scene_tree/`) was **REJECTED as scope-too-large
> 2026-08-17 (user directive)**. Direction shifted to: modify `SceneTree` in place
> via the goblin mirror `modules/goblin/scene/main/scene_tree.cpp` + a narrow
> direct edit to `scene/main/scene_tree.h` (+7 lines). No module, no base-class
> seam, no retype ripple — the optimized tree runs everywhere (editor, PM, games)
> for free. The module-era design (§2–§10 below) is kept for history only.
>
> - **Companion plan:** `modules/goblin/docs/plans/fast-scene-tree-plan.md`
> - **Backlog:** M-14
> - **Edit home:** `modules/goblin/scene/main/scene_tree.cpp` (config.py `"scene"`
>   library override) + `scene/main/scene_tree.h` (the only upstream file touched)

---

## 1. Purpose

Make `SceneTree` hot paths allocation-free and copy-free with zero behavior
change — same public API, same signals, same semantics, same process-group
ordering. Target: systemic/immersive-sim genre cadence (5k+ nodes, 60 Hz tick,
group calls per cadence). No new class, no new module, no main-loop selection.

## 2. Why in-place (not a new MainLoop)

A `FastSceneTree : public MainLoop` re-implementation (the original RFC design)
would require:
- A `BaseSceneTree` core-header seam + `node.h` retype (~326 call sites, ~40 friend
  accesses, ~30 engine files) — retype ripple the architect judged too broad.
- Semantic drift risk across the full contract (notifications, group flags, timer
  modes, pause/suspend, scene change, multiplayer).
- A/B benchmark baseline (separate tree, separate code paths).

The 2026-08-17 direction targets the actual hot paths directly in `SceneTree`
itself — the private `ProcessGroup`/`group_map` machinery is accessible, no
retype or seam needed, and tree-only edits touch 2 files. Lost vs the module
design: A/B baseline, per-project opt-out. Acceptance = suite green + no regressions.

## 3. What's optimized (shipped)

### T1 — Copy-free process iteration

`_process_group()` no longer copies the group's node list per frame. Removals
null-mark slots (no vector shift, no iterator invalidation, no `nodes_copy`
alloc). New `_compact_process_nodes()` compacts lazily on the next pass.
Re-sorts only when order changed (`*_node_order_dirty`). The
`nodes_removed_on_group_call` per-node lookup in `_process_group` was removed
(provably redundant — exit-tree ordering null-marks before `node_removed`).

**Files:** `scene/main/scene_tree.h` (+2 `ProcessGroup` flags) + goblin mirror.

### T6 — Copy-free group calls + timer efficiency

`call_group_flagsp` / `notify_group_flags` / `set_group_flags` /
`_call_input_pause`: read the CoW-shared group vector via `ptr()` instead of
`ptrw()`. The old `ptrw()` force-detached the copy on **every** group call
(full alloc + memcpy); the shared buffer now only duplicates if the group
actually mutates mid-call. `process_timers()`: binds the stored `Ref`
(`Ref<SceneTreeTimer> &timer`) — no refcount churn per timer per frame (matches
`process_tweens`). Cached signal names (`process_frame`, `physics_frame`,
`timeout`) replace per-frame/per-timer `SNAME()` lookups.

**Files:** `scene/main/scene_tree.h` (+3 `StringName` members) + goblin mirror.

## 4. What's deferred (out of scope / behavior-visible)

| Tier | What | Why not done under tree-only constraint |
|---|---|---|
| **T2** | Intrusive group lists, direct Callable dispatch, incremental order | Needs `node.h` (per-node group links) — upstream touch beyond scene_tree |
| T3 | Ordered unique-group-call flush (`_flush_ugc`) | Same copy cost as T6 — already fixed |
| **T4** | Subtree pause/suspend mode flags (skip paused subtrees wholesale) | Needs node-side process/pause state in `node.h` |
| T5 | Cadence API (`register_cadence`) | SimServer integration point; standalone not needed yet |
| **M4** | Incremental children cache (no full dump+sort on remove) | `node.cpp:1771` — not scene_tree.cpp |
| **M5** | Coalesced `tree_changed()` (dirty flag, emit once at frame end) | Signal timing is observable — behavior-visible |
| **M6** | Iterative `_propagate_*` (flat worklist vs recursive O(subtree)) | `node.cpp:595` propagation — not scene_tree.cpp |
| **M7** | `SceneTreeTimer` free-list pooling | Marginal; `Ref<>` reuse (T6) covers the hot path |

## 5. Compatibility (unchanged)

No API, signal, or semantic changes. Process-group ordering, notification
order, group-call flags, timer modes, pause/suspend, scene change — all
identical to upstream. Verified via:
- Doctest suite: 1337/1337 passed, 0 failed, 1 skipped
- GDScript suite: 516 assertions ✓ | Completion: 635 ✓ | LSP: 57,556 ✓
- 4000-node churn stress: group counts exact (2000/6000), survivors keep
  processing, disabled stop, re-added process — all invariants PASS
- Editor/PM/game boots; byte-identical script output; timing 1100→416 usec

## 6. Risks

| Risk | Status |
|---|---|
| `scene_tree.h` is upstream-touch | Narrowest possible: 7 lines (5 data members + 2 flags). User-sanctioned for this work. Re-diff on rebase. |
| Memory churn semantics | Null-mark + lazy compaction preserves iteration order (stable) and exit-tree ordering. Stress test confirms. |
| Mirror drift | Porting-skill mirror discipline applies; `config.py` swap ensures the mirror compiles, not upstream. |

## 7. Open questions (resolved)

1. **Per-project opt-out:** not provided (in-place = always on). Accepted trade-off.
2. **A/B benchmark:** replaced by stress-test + doctest (above).
3. **T5 cadence API:** folds into SimServer S-01 — not needed for SceneTree itself.

---

## 8. Original module-era design (history — NOT followed)

The sections below describe the rejected `FastSceneTree : public MainLoop` +
`BaseSceneTree` seam approach. **Do not follow.** Kept for architectural context
and git-history readability.

### 8.1 Purpose (module era)

A fork-owned, SceneTree-compatible main loop class, re-implemented from `MainLoop`:
- New class `FastSceneTree : public MainLoop` — zero ClassDB conflict, selected
  per-project via `application/run/main_loop_type`.
- Full re-implementation of the SceneTree contract — same public API, same
  signals, same semantics — new internals: no per-frame process-list copies,
  no per-call group copies, no string dispatch, deterministic ordering.
- Extensible afterward (cadence API, flat-data EntityNode stage).

### 8.2 Why MainLoop and not SceneTree (rejected)

- Extending `SceneTree` inherits the machinery we need to replace:
  `_process_group` (per-frame copy), group-call copies, `_flush_ugc` hash order,
  and `ProcessGroup`/`group_map` are private non-virtual — a subclass can only
  add parallel systems, never replace internals.
- `BaseSceneTree : MainLoop` core-header seam + `node.h` retype — ~326 get_tree()
  call sites + ~40 friend accesses, 30+ engine files. User judged the ripple too
  large relative to the direct in-place gains.

### 8.3 The Node seam (rejected)

```
BaseSceneTree : MainLoop           ← NEW additive core header: scene/main/base_scene_tree.h
├── SceneTree : BaseSceneTree     ← narrow upstream edit
└── FastSceneTree : BaseSceneTree ← module implementation
```

- `BaseSceneTree` in core as `scene/main/base_scene_tree.h` (header-only, pure
  virtuals — core header cannot include a module header).
- `node.h` edit: `data.tree` / `_set_tree` / `get_tree()` → `BaseSceneTree *`.
- `scene_tree.h` edit: `class SceneTree : public BaseSceneTree` + move 4 private
  members up.

### 8.4 Architecture (rejected)

- **Module:** `modules/fast_scene_tree/` (ADR 0008), also hosts EntityNode layer.
- **Class:** `FastSceneTree : public BaseSceneTree`, GDCLASS, ClassDB-registered.
- **Selection:** `application/run/main_loop_type = "FastSceneTree"`.

### 8.5 What we optimize (module era)

| Tier | What | Replaces |
|---|---|---|
| T1 | Flat process lists, tombstone/epoch iteration | per-frame `nodes_copy` + `nodes_removed_on_group_call` |
| T2 | Intrusive group lists, direct Callable dispatch | group-call copies, StringName dispatch |
| T3 | Ordered unique-group-call flush | `_flush_ugc` HashMap order |
| T4 | Subtree mode flags | per-node `can_process()` |
| T5 | Cadence API + SimServer hooks | scheduler `call_group` hot path |
| T6 | Allocation-free iteration for C++ consumers | `get_children()` TypedArray |

### 8.6 Memory-churn specifics (module era)

| # | Upstream cost | Replacement |
|---|---|---|
| M1 | per-frame process vector copy (~40KB/5k nodes, 60×/s) | zero-copy epoch iteration (T1) |
| M2 | `get_children()` TypedArray alloc + refcounts | allocation-free iteration API (T6) |
| M3 | group-call vector copies per cadence | intrusive group lists (T2) |
| M4 | children cache full dump+sort on remove | incremental maintenance (node.cpp) |
| M5 | `tree_changed()` per mutation storm | coalesced dirty flag |
| M6 | recursive `_propagate_*` O(subtree) | flat worklist + dirty flags |
| M7 | `SceneTreeTimer` alloc per `create_timer` | free-list pooling |

### 8.7 Risks (module era)

| Risk | Mitigation |
|---|---| 
| node.h edit upstream-touch | ADR 0009 additive-field precedent; ~20 lines |
| base-scene-tree semantics drift | P1 contract matrix + suite + corpus per phase |
| Type-identity divergence | Corpus audit; documented |
| Scope | Hard phase gates; P2 pure-contract baseline first |
| Rebase drift | Porting-skill mirror discipline |
