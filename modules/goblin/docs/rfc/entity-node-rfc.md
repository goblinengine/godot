# Goblin Engine — EntityNode / EntityComponent RFC (revised 2026-08-20)

- **Proposal file:** `modules/goblin/docs/rfc/entity-node-rfc.md`
- **Date:** 2026-08-16 (revised 2026-08-20)
- **Status:** **Runtime core SHIPPED 2026-08-20** — deferral gate MET (M-14 batch 1+2 / T1+T6 landed 2026-08-17, build green, PM/editor/game boot verified). T2/T4/M4 are full-benefit items only, not gates. Implemented per `docs/plans/entity-node-plan.md` (P1–P6); 10/10 entity doctest cases green, GDScript smoke `ENTITY_SMOKE_OK`, editor headless boot clean, 5 class icons registered. Slice 2 (editor 3D gizmo/transform-editing, LightComponent, bodies) documented in the plan §9.
- **Base:** Godot 4.7.1-stable
- **Backlog:** D-20

---

## 0. Status history

- **2026-08-16:** design locked (hybrid tree + ECS on tree-owned pools; Component : Object sibling).
- **2026-08-17:** FastSceneTree module design REJECTED (user directive) — direction shifted to in-place `SceneTree` optimizations (mirror `scene/main/scene_tree.cpp` + narrow `scene/main/scene_tree.h` edit). EntityRegistry + ComponentPools concept transfers unchanged to SceneTree-owned pools. Implementation deferred pending the tree optimizations.
- **2026-08-20:** (a) Deferral gate met — implementation may start; (b) **§2.3 overridden** (user directive: native editor piggyback requires components to be tree children); (c) scope trimmed per user directive 1 (bodies/collision deferred); (d) editor integration reduced to "native via Node components" (no custom rows/serialization work).

---

## 1. Purpose

A **hybrid tree + ECS layer** on top of `SceneTree`: entities that *appear and
behave like normal nodes* in the editor and in scripts, but whose hot data is
**compiled into `SceneTree`-owned per-type component pools** (SoA) at runtime.
Goal: cache-friendly iteration and batched server flush for the repetitive,
high-volume 3D objects (transforms, meshes, visibility) while keeping full
Godot compatibility and **all-native editor behavior** for everything else.

**Not a full ECS.** No entity system, no archetypes, no systems pipeline. Each
EntityNode is a container of components; data is non-local; the objects are
"a collection of functionality" assembled via the editor tree.

## 2. Why hybrid (locked decisions)

1. **Naming:** `EntityNode` / `EntityComponent` — plain names, upstream style.
2. **Why EntityNode : Node (full Node):** scripts, `get_tree()`, inspector,
   `.tscn` serialization, undo/redo all work for free. The Node shell is
   acceptable — data lives in pools, iteration never touches the shell.
3. **Why EntityComponent : Node (OVERRIDES 2026-08-16 §2.3 "Object sibling"):**
   the user's editor direction (native tree rows, native `.tscn`, native undo,
   `+` menu) requires components to be tree children — SceneState serializes
   child **Nodes**; non-Node children need a custom scene-saver (the custom
   editor work the user wants to avoid). The old Object-sibling perf delta
   (~100ns vs ~µs attach) is **spawn-path only** — one-time `add_child` hash
   insert + cache-dirty rebuild + enter/ready propagation. Per-frame cost is
   identical in both designs: pool iteration. A Node component with no
   `_process` override is not in any process group → **zero per-frame tree
   cost**. One-time spawn tax accepted for all-native editor/serialization/
   undo/scripts/signals. (Objection record: see plan §1 Accepted Risks — S1.)

## 3. Architecture

### 3.1 Class hierarchy

```
Object
 └── Node
      ├── EntityNode          → full Node subclass; entity_id + type mask; scripts
      ├── EntityComponent     → Node subclass base (abstract); thin tree child + pool handle
      │     ├── Transform3DComponent
      │     ├── MeshInstanceComponent
      │     ├── VisibilityComponent
      │     └── ... (LightComponent, Sprite3DComponent → slice 2 candidates)
      └── (regular nodes)     → hybrid: plain Node children of an EntityNode stay ordinary
```

- **`EntityComponent`** = thin Node child of an EntityNode. Node properties are
  **authoritative** (inspector/serialization/scripts read the Node); the pool is a
  compiled mirror (iteration/flush read the pool). Setters write both.
- **`EntityNode`** = full Node subclass carrying `entity_id`, `type_mask`,
  script. On tree entry it compiles its **direct `EntityComponent` children**
  into the pools; regular Node children are untouched (hybrid, directive 2).
- Components attach to the **nearest EntityNode ancestor**; nested entities
  compile to the inner entity.

### 3.2 Data model (SceneTree-owned)

```
SceneTree
└── EntityRegistry *entity_registry     // forward-declared pointer member (ODR-safe)
    ├── entity ids     : free-list + counter (0 reserved = invalid)
    ├── type masks     : per-entity bitset of attached component types
    └── ComponentPools (per type — SoA-style dense Vector slots + entity_to_slot map)
        ├── Transform3DComponentPool : { transform, dirty } + world anchor (nearest Node3D)
        ├── MeshInstanceComponentPool: { mesh, materials, instance_rid, dirty }
        └── VisibilityComponentPool  : { visible, visibility_aabb, cast_shadows, dirty }
    └── per-pool dirty-slot lists (batched flush at frame boundary)
```

- `EntityNode::get_component(type)` — O(direct children) convenience; scripts
  also use `$ComponentName` natively. `has_component(type)` — O(1) mask test.
- Attach/detach = pool insert/swap-remove + id remap + dirty mark, O(1).
- **Frame flow:** entity scripts run in the normal node process pass → component
  setters mark pool slots dirty → `SceneTree::process()`/`physics_process()`
  (mirror) call `entity_registry->flush_dirty()` once per frame at the end —
  one dense batched pass per component type to RenderingServer (transform,
  instance base/materials, visibility/shadows) instead of N per-node
  notification dispatches.
- **Transform composition:** component transform is entity-local; flush applies
  `nearest Node3D ancestor global × local` (hybrid seam — EntityNode under a
  moving Node3D inherits it; Node3D globals are engine-cached).

### 3.3 Components as batched server drivers

Each component is the stripped runtime of a legacy node: `Transform3DComponent`
= transform + dirty flag → `RenderingServer::instance_set_transform`;
`MeshInstanceComponent` = mesh/materials + instance RID (create/free/base);
`VisibilityComponent` = visible/visibility AABB/shadow setting → instance flags.
~24–64B of data per slot plus a thin Node handle.

## 4. Editor integration

| Item | Scope (2026-08-20) |
|------|---------------------|
| Tree display | **Native** — components are Node children (rows come free) |
| Scene save/load + undo | **Native** — SceneState serializes child Nodes; undo is native |
| `+` menu | **Native** — components add as child nodes |
| CreateDialog `EntityComponent` category + orange icons | Optional polish, NOT required (user 2026-08-20) |
| Collapsible section under EntityNode | Optional, NOT required (user 2026-08-20) |

**No custom editor work is required.** The 2026-08-16 "custom tree rows /
custom serialization / undo entries" items are deleted from scope.

Editor rules: components only inside EntityNodes (guard: `EntityComponent`
outside an EntityNode parent compiles nothing — inert, documented); script
attachment on components is normal Node behavior (allowed — script-exported
extras live on the Node, not in pools).

## 5. Scope

### Batch 1 (this slice — spatial, repetitive, 3D)

- `Transform3DComponent`
- `MeshInstanceComponent`
- `VisibilityComponent` (sensible extra — systemic hide/show + shadow control;
  rides the mesh instance RID; justification in plan §3.4)

### Deferred

| Candidate | Why deferred |
|-----------|-------------|
| Camera | single-instance, stateful, hard to compose |
| Physics bodies (RigidBody/CharacterBody) | body RIDs want one owner; compose poorly (user directive 1) |
| CollisionShapeComponent | shape needs a body-RID owner — no owner in this slice (user directive 1) |
| Light / Sprite3D / billboards | slice-2 candidates (same machinery: new pool + flush) |

## 6. Compatibility

- Regular nodes work everywhere, mixed freely with EntityNodes — including as
  **children of** an EntityNode (hybrid, user directive 2).
- EntityNode is a real Node: scripts, `get_tree()`, signals, `.tscn`, undo —
  standard.
- EntityComponent is a real (thin) Node: serializes, undo, signals, scripts.
  No `_process` override → no process-group membership → zero per-frame cost.
- SceneTree handles both: legacy nodes via normal paths; EntityNodes via
  registry + pools. SceneTree's header changes are additive (+5 lines: forward
  decl, 2 friends, pointer member) — upstream behavior unchanged.

## 7. Risks

| Risk | Mitigation |
|------|-----------|
| One-time `add_child` spawn tax on spawn-heavy scenes (old §2.3 objection) | Spawn-time only; per-frame cost flat (pools). Future bulk-spawn bypass documented if profiling demands |
| Editor tree/serialization/undo for components | **Eliminated by design** — native Node children (2026-08-20) |
| Pools + entity_id remap bugs on despawn | swap-remove remap tested at pool level; churn doctest cases; determinism via readback |
| Server flush batching changes visual timing vs per-node notify | frame-boundary flush keeps per-frame semantics; visual regression suite |
| RS instance lifecycle (double-free/leak/attach order) | RID owned solely by the mesh pool; attach applies current transform → order-independent |
| `scene_tree.h` seam widening | Additive + inert; pointer-only member (B-14 ODR rule) |

## 8. Sequencing (2026-08-20)

EntityNode/EntityComponent implementation **starts now** — the in-place
SceneTree optimizations (T1/T6) are validated (build green, boots verified);
T2 (intrusive groups) / T4 (subtree flags) / M4 (children cache) are
full-benefit items, not gates.

Components live in the goblin mirror (`modules/goblin/scene/main/`), same home
as `scene_tree.cpp` — a core-file swap via `config.py` `goblin_add_library()`
hook extended with a `_GOBLIN_FILE_ADDITIONS` dict for the new sources (no
upstream stem to replace). `SceneTree` gains an `EntityRegistry *` pointer
member (small additive core header edit, same seam as the existing scene-tree
header edits). Registration from `modules/goblin/register_types.cpp` at
MODULE_INITIALIZATION_LEVEL_SCENE (precedent: `modules/sim/register_types.cpp`).

Phases: P1 registry core → P2 EntityNode/EntityComponent bases → P3
Transform3D + MeshInstance → P4 Visibility → P5 tests + verification → P6 docs.
Full breakdown: `docs/plans/entity-node-plan.md`.

## 9. History (2026-08-16 decisions — superseded where noted)

- **§2.3 was:** "Component : Object sibling (NOT Node)" — slim ~100–150B base,
  direct `_attach()`/`_detach()`, ~100ns attach vs ~µs Node path, zero tree
  notifications/cache rebuilds/signal cascades. **OVERRIDDEN 2026-08-20 (§2.3)**
  — the delta is spawn-time only; native editor requires tree children.
- **§4 was:** custom editor work (tree rows, component serialization, undo
  entries, CreateDialog patch). **REPLACED 2026-08-20 (§4)** — native via Node
  components; CreateDialog category optional.
- **§5 was:** Batch 1 = Transform3D / MeshInstance / CollisionShape; camera +
  physics bodies deferred. **AMENDED 2026-08-20** — CollisionShape deferred
  (no body-RID owner in slice); VisibilityComponent added as the batch-1 extra.
- **§8 was:** Phase 2, gated on T2/M4 shipping. **REPLACED 2026-08-20** — gate
  is T1/T6 (MET); T2/T4/M4 are full-benefit only.
