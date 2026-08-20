# EntityNode / EntityComponent — Implementation Plan (locked 2026-08-20)

**STATUS: COMPLETED 2026-08-20** — runtime-core slice shipped (P1–P6). Verification: 10/10 entity doctest cases green (suite 1366/1369, 3 pre-existing failures proven registration-independent); GDScript smoke `ENTITY_SMOKE_OK` on the standard build; editor headless boot clean; 5 class icons registered. Remaining work (slice 2, §9): editor 3D gizmo/transform-editing support, LightComponent, bodies/CollisionShape mini-RFC.

Runtime-core slice of D-20. Companion to `modules/goblin/docs/rfc/entity-node-rfc.md`
(revision 2026-08-20: **`EntityComponent : Node`** — §2.3 of the 2026-08-16 RFC overridden).

- **Backlog:** D-20 (flipped to `doing` 2026-08-20)
- **Deferral gate:** MET. M-14 batch 1+2 (T1/T6) landed 2026-08-17, build green, PM/editor/game boot verified. T2/T4/M4 are full-benefit items only — **not gates**.
- **Edit home:** `modules/goblin/scene/main/` (new sources) + `modules/goblin/config.py` (additions dict) + `modules/goblin/register_types.cpp` (SCENE-level registration) + direct narrow edit `scene/main/scene_tree.h` (sanctioned seam, +5 lines) + mirror `modules/goblin/scene/main/scene_tree.cpp` (ctor/dtor + flush calls).
- **Injection mechanism:** core-file swap hook `goblin_add_library()` in `modules/goblin/config.py` — extended with a new `_GOBLIN_FILE_ADDITIONS` dict (new sources, no upstream stem). See §4.

---

## 1. Locked semantics (decision record)

| # | Decision | Rationale |
|---|----------|-----------|
| S1 | **`EntityComponent : Node`** (overrides RFC §2.3 "Component : Object sibling") | User directive 2026-08-20: native tree display, native `.tscn` serialization, native undo, `+` menu → components MUST be tree children. SceneState serializes child **Nodes**; non-Node children need a custom scene-saver = exactly the editor work the user wants to avoid. The §2.3 perf delta (~100ns vs ~µs attach) is **spawn-path only** (one-time `add_child` hash insert + cache-dirty rebuild + ENTER_TREE/READY propagation). Per-frame cost is identical in both designs: pool iteration. A Node component with no `_process` override is not in any process group → **zero per-frame tree cost**. Accepted one-time spawn tax in exchange for all-native editor/serialization/undo/scripts/signals. |
| S2 | EntityNode compiles its **direct `EntityComponent` children into SceneTree-owned SoA pools** on tree entry; regular Node children stay ordinary child nodes (hybrid) | User directive 2: "A component added as a child to an EntityNode acts like an entity component but a regular node acts like a child node." |
| S3 | **Node properties are authoritative; pools are compiled mirrors.** Component setters write the Node property AND the pool slot + dirty flag. Attach copies Node→pool; detach discards the slot (Node keeps its value for reattach). | Inspector/serialization/scripts all read the Node; iteration/flush read the pool. One source of truth, native surface free. |
| S4 | Components attach to the **nearest `EntityNode` ancestor**; nested entities are regular Node children of their parent entity (components of the inner entity compile to the inner entity's pools) | Clean nesting; `EntityNode::_notification(ENTER_TREE)` scans only direct children and type-checks `EntityComponent`. |
| S5 | `Transform3DComponent` transform is **entity-local**; the flush applies `world_anchor->get_global_transform() × component_transform` where `world_anchor` = nearest `Node3D` ancestor (cached at compile, refreshed on ENTER_TREE / MOVED_IN_PARENT). No transform inheritance inside the EC model; `Node3D` ancestry is the hybrid composition seam. | "Regular Node bodies mixed with entity components": an EntityNode under a moving platform Node3D inherits it. Node3D global transforms are engine-cached → O(1) read at flush. |
| S6 | Component type identified by `uint32_t` type-id + per-entity **type mask** | Fast `has_component(type)` / pool routing; bitmask free. |
| S7 | `SceneTree` owns the registry via a **forward-declared pointer member** (ODR-safe form; no goblin-only type by value in an upstream-instantiated class — B-14 hazard) | `scene_tree.h` is the sanctioned direct-edit seam (+10-line precedent); all TUs see one header, so `sizeof(SceneTree)` growth is uniform. |
| S8 | Registry is an **internal plain C++ class** (no ClassDB, not script-visible). Scripts reach components as child nodes (`$Transform3DComponent`) or via `EntityNode::get_component(type)` (O(direct children), convenience only) | No public API widening beyond the sanctioned header edit. |
| S9 | Batched flush: one `EntityRegistry::flush_dirty()` per frame per tree — called at the **end of both `SceneTree::process()` and `SceneTree::physics_process()`** (mirror), after `_flush_delete_queue()`. Per-type dirty-slot lists, one dense pass per pool. | Dense cache-friendly iteration instead of per-node notification dispatch; delete-queued entities already detached via EXIT_TREE → flush touches only live entities. |
| S10 | No per-component per-frame cost beyond the flush: components don't override `_process`/`_physics_process` unless the user's script does. | S1 corollary. |
| S11 | **Naming:** `EntityNode`, `EntityComponent`, `Transform3DComponent`, `MeshInstanceComponent`, `VisibilityComponent`, `EntityRegistry`, `ComponentPool`; files `entity_node.{h,cpp}` etc. Upstream style, no `Goblin*`/`goblin_*`. | Hard rule. |

### Accepted Risks (objection override record)

- **S1 (Object→Node):** the 2026-08-16 objection was spawn-cost. Consequence: one-time `add_child` tax per component at scene instantiation. Why proceeding: spawn cost is amortized (one-time, not per-frame), and the user's editor direction (native rows/serialization/undo/`+`) is unobtainable with Object-siblings without custom editor work measured in days — worse total. Reversible if profiling later shows spawn-bound scenes (pools are already detached from the tree cost; a future "bulk spawn bypass" can add a deferred-tree path).
- **S5 (transform composition):** component transforms are not parent-relative to other components (no component→component inheritance in v1). Consequence: reference-title content needing nested component transforms composes via nested EntityNodes. Why proceeding: v1 scope; documented semantics, not silent behavior.

---

## 2. Scope

### In this slice (runtime core, implements the EC runtime end-to-end)

- `EntityRegistry` (pools + entities + dirty flush).
- `EntityNode` (Node subclass; compile-on-enter; type mask; `get_component`).
- `EntityComponent` (Node subclass base; attach/detach hooks; type-id).
- `Transform3DComponent`, `MeshInstanceComponent` (batch-1 core), `VisibilityComponent` (the "sensible extra" — justification in §3).
- Scene-tree seams (header edit + mirror ctor/dtor/flush).
- Build injection (`config.py` additions dict) + registration + docgen classes.
- Doctest suite + GDScript smoke scene.

### Explicitly deferred (recorded, not forgotten)

| Item | Why deferred |
|------|-------------|
| Physics body components (RigidBody/CharacterBody) + `CollisionShapeComponent` | User directive 1: bodies deferred (difficult as components); shape needs a body-RID owner — no owner exists in this slice. Shape data would need `body_rid` plumbing into `EntityRegistry`; revisit with the body-component design. |
| `CameraComponent` | Single-instance, stateful, poor mix-and-match (RFC §5). |
| `LightComponent` | RID advantage is real (torch/stealth interplay with S-03 field rebake) but the surface is large (shadow-typed params, LightData); machinery in this slice makes it a new pool + flush function. **Slice-2 candidate.** |
| `Sprite3DComponent` / billboard | Overlaps MeshInstanceComponent + quad mesh; low marginal value v1. **Slice-2 candidate.** |
| Editor custom work (custom tree rows, component-only palette, CreateDialog category, collapsible section) | **No longer needed as custom work** — S1 makes tree/undo/serialization/`+` native. Optional extras (CreateDialog `EntityComponent` category + orange icons, collapsible section under EntityNode) are user-marked **not required**. Only the CreateDialog category patch remains as an optional polish item in slice 2. |
| Full-benefit tree items (T2 intrusive groups, T4 subtree flags, M4 children cache) | Not gates (user-confirmed); revisit when profiling demands. |

---

## 3. Class surface (exact API shapes)

### 3.1 `EntityRegistry` — `modules/goblin/scene/main/entity_registry.{h,cpp}`

Internal class (no GDCLASS, no bindings). Lives in the scene library (compiled via additions) because the mirror `scene_tree.cpp` (same library, overlay env) includes and calls it.

```cpp
// Component type ids (also the bit index in EntityNode::type_mask).
enum class ComponentType : uint32_t {
    TRANSFORM3D = 1,
    MESH_INSTANCE = 2,
    VISIBILITY = 3,
};

// Pool slot data per type. `dirty` dedups the pending-flush list.
struct Transform3DComponentData {
    Transform3D transform;
    uint8_t dirty = 0;
};

struct MeshInstanceComponentData {
    Ref<Mesh> mesh;                  // authoritative mesh ref (held so the RID stays alive)
    Vector<Ref<Material>> materials; // per-surface overrides
    RID instance_rid;                // RS instance (created on attach when mesh present)
    uint8_t dirty = 0;
};

struct VisibilityComponentData {
    bool visible = true;
    AABB visibility_aabb;            // mesh-derived at attach unless overridden
    int32_t cast_shadows = 0;        // RenderingServer::ShadowCastingSetting
    uint8_t dirty = 0;
};

template <typename T>
struct ComponentPool {
    struct Slot { T data; uint32_t entity_id; EntityComponent *node; };
    Vector<Slot> slots;                    // dense, swap-remove
    HashMap<uint32_t, uint32_t> entity_to_slot;
    LocalVector<uint32_t> dirty_slots;     // slot indices pending flush (deduped by data.dirty)
    uint32_t slot_for(uint32_t p_entity_id) const;      // HashMap lookup, UINT32_MAX if absent
    uint32_t insert(uint32_t p_entity_id, EntityComponent *p_node, const T &p_data);
    void remove(uint32_t p_entity_id);                 // swap-remove + remap moved slot's node->set_pool_slot()
    void mark_dirty(uint32_t p_entity_id);             // sets data.dirty=1, appends slot once
    void clear_dirty();
};

class EntityRegistry {
public:
    uint32_t entity_create();                 // pops free list or increments counter (0 reserved = invalid)
    void entity_destroy(uint32_t p_entity_id); // detaches all attached components + frees id
    void component_attach(uint32_t p_entity_id, EntityComponent *p_component);
    void component_detach(EntityComponent *p_component);
    uint32_t entity_get_component_type(uint32_t p_entity_id, ComponentType p_type) const; // slot or UINT32_MAX
    RID entity_get_instance_rid(uint32_t p_entity_id) const;  // cross-pool: mesh pool lookup
    Node3D *entity_get_world_anchor(uint32_t p_entity_id) const;
    void entity_set_world_anchor(uint32_t p_entity_id, Node3D *p_anchor);
    void flush_dirty();                        // one dense pass per pool; clears dirty
    bool is_empty() const;
private:
    ComponentPool<Transform3DComponentData> transform_pool;
    ComponentPool<MeshInstanceComponentData> mesh_pool;
    ComponentPool<VisibilityComponentData> visibility_pool;
    HashMap<uint32_t, Node3D *> world_anchors; // entity_id -> nearest Node3D ancestor (S5)
    Vector<uint32_t> free_entity_ids;
    uint32_t next_entity_id = 1;
};
```

`flush_dirty()` ordering per pool (mirrors `MeshInstance3D::_update_instance` semantics):

1. **transform pool** — per dirty slot: `instance = entity_get_instance_rid(id)`; if valid, `RenderingServer::get_singleton()->instance_set_transform(instance, anchor_global * data.transform)`.
2. **mesh pool** — per dirty slot: if `data.mesh.is_valid()` and `instance_rid.is_null()` → create instance (`RenderingServer::get_singleton()->instance_create2(mesh->get_rid(), scenario)`; scenario from the EntityNode's `get_viewport()->get_world_3d()->get_scenario()`; defer creation to flush so a transform+mesh attach in one frame lands as one instance with the right base + transform). If `data.mesh` changed → `instance_set_base()`. Apply per-surface `instance_set_surface_override_material()`. If `data.mesh` invalid and instance valid → `instance_free`, clear RID.
3. **visibility pool** — per dirty slot: `instance_set_visible`, `instance_set_visibility_aabb`, `instance_set_cast_shadows` (only when an instance RID exists).

Detach (mesh pool): `instance_free` if valid, then slot removal + remap. `entity_destroy` detaches every attached component (see §3.4).

### 3.2 `EntityNode` — `modules/goblin/scene/main/entity_node.{h,cpp}`

```cpp
class EntityNode : public Node {
    GDCLASS(EntityNode, Node);
protected:
    void _notification(int p_notification);
    static void _bind_methods();
public:
    enum ComponentType : uint32_t { /* mirror of registry enum for GDScript */ };
    bool has_component(uint32_t p_type) const;   // type mask test, O(1)
    EntityComponent *get_component(uint32_t p_type) const; // O(direct children); convenience
    uint32_t get_entity_id() const;
    uint32_t get_type_mask() const;
private:
    uint32_t entity_id = 0;    // 0 = not compiled
    uint32_t type_mask = 0;
    Vector<EntityComponent *> components; // direct EntityComponent children, compile order
    Node3D *world_anchor = nullptr;       // nearest Node3D ancestor (S5), cached
    void _compile();    // ENTER_TREE: entity_create + attach each direct EntityComponent child + cache anchor
    void _decompile();  // EXIT_TREE: registry->entity_destroy(entity_id); entity_id = 0
    void _update_world_anchor(); // ENTER_TREE + MOVED_IN_PARENT
    EntityRegistry *_registry() const; // static_cast<SceneTree *>(get_tree())->entity_registry (friend)
};
```

Notifications:
- `NOTIFICATION_ENTER_TREE`: `_compile()` — `entity_create`, then per direct child `EntityComponent` → `registry->component_attach(id, child)`; cache `world_anchor`. (Children of a freshly-added EntityNode receive ENTER_TREE before the parent — postorder — so all direct component children are already in-tree.)
- `NOTIFICATION_EXIT_TREE`: `_decompile()` — `registry->entity_destroy(entity_id)` (detaches pools + frees instance RIDs), zero the id/mask. Idempotent.
- `NOTIFICATION_MOVED_IN_PARENT`: `_update_world_anchor()`.

Bound methods (GDScript): `has_component(type: int) -> bool`, `get_component(type: int) -> Node`, `get_entity_id() -> int`, `get_type_mask() -> int`, plus `enum ComponentType { TRANSFORM3D, MESH_INSTANCE, VISIBILITY }`.

### 3.3 `EntityComponent` — `modules/goblin/scene/main/entity_component.{h,cpp}`

```cpp
class EntityComponent : public Node {
    GDCLASS(EntityComponent, Node);
protected:
    void _notification(int p_notification);
    static void _bind_methods();
public:
    virtual ComponentType get_component_type() const = 0; // pure virtual: per-subclass constant
    uint32_t get_entity_id() const;     // entity the component is compiled into (0 = not compiled)
    uint32_t get_pool_slot() const;     // slot in its type's pool (for remap)
    bool is_compiled() const;
    void set_pool_slot(uint32_t p_slot); // called by ComponentPool::remove() remap
    EntityNode *get_entity_node() const; // parent cast
    EntityRegistry *_registry() const;
protected:
    virtual void _attach();  // called by registry after pool insert: subclass copies Node state -> pool
    virtual void _detach();  // called by registry on detach: subclass clears pool references (Node state kept)
private:
    uint32_t entity_id = 0;
    uint32_t pool_slot = 0;
    bool compiled = false;
};
```

Notifications:
- `NOTIFICATION_ENTER_TREE`: if parent is an `EntityNode` **already compiled** (`entity_id != 0`), `registry->component_attach(parent_id, this)` — this is the runtime `add_child(component)` case. If the parent EntityNode is not yet compiled (fresh subtree), do nothing (the parent's ENTER_TREE compile pass handles it).
- `NOTIFICATION_EXIT_TREE`: if `compiled`, `registry->component_detach(this)`.
- Guard against re-entry: `compiled` flag + `entity_id` check; `component_attach` is idempotent (no-op if this node already attached).

`EntityComponent` subclasses define `get_component_type()` and implement `_attach`/`_detach`.

### 3.4 `Transform3DComponent`, `MeshInstanceComponent`, `VisibilityComponent`

```cpp
class Transform3DComponent : public EntityComponent {
    GDCLASS(Transform3DComponent, EntityComponent);
    // exports:
    //   transform: Transform3D  (set_transform -> Node property + pool slot + mark_dirty)
    ComponentType get_component_type() const override; // TRANSFORM3D
protected:
    void _attach() override; // copy this->transform into transform_pool slot
    void _detach() override;
    void _bind_methods();
};

class MeshInstanceComponent : public EntityComponent {
    GDCLASS(MeshInstanceComponent, EntityComponent);
    // exports:
    //   mesh: Ref<Mesh>
    //   material_overrides: Array[Ref<Material>]  (per-surface)
    ComponentType get_component_type() const override; // MESH_INSTANCE
protected:
    void _attach() override; // copy mesh/materials into mesh_pool slot + mark_dirty
    void _detach() override; // pool removal frees the RS instance
    void _bind_methods();
};

class VisibilityComponent : public EntityComponent {
    GDCLASS(VisibilityComponent, EntityComponent);
    // exports:
    //   visible: bool = true
    //   cast_shadows: RenderingServer.ShadowCastingSetting = 0
    //   (visibility_aabb auto-derived from the entity's mesh at flush; no export in v1)
    ComponentType get_component_type() const override; // VISIBILITY
protected:
    void _attach() override;
    void _detach() override;
    void _bind_methods();
};
```

**Mix-and-match story (user directive 3):** every component is a horizontal slice shared across composed entities:
- Transform3D + MeshInstance = static/dynamic 3D prop entity (replaces `MeshInstance3D` for high-volume props).
- Transform3D + MeshInstance + Visibility = toggleable/stealth-managed prop (hide/show, shadow control for low-fi perf).
- Transform3D alone = pure-logic anchor entity (no server object; `entity_get_instance_rid` returns invalid → flush skips).
- Regular Node children (e.g., an `AudioStreamPlayer3D`, a `CollisionShape3D` under a body Node) mix freely inside the same EntityNode — **hybrid** (directive 1).

**Why VisibilityComponent is in, light is not:** Visibility rides the mesh instance RID already owned by the pool, is ~3 RS calls + 24B, and has immediate systemic value (stealth hide/show, shadow/perf management in low-fi). Light needs a whole `Light3D`-equivalent parameter surface + its own light RID creation — real value (torch flicker ↔ S-03 rebake) but a separate phase; it slots into the same machinery later (slice 2).

---

## 4. Mechanism & build injection (exact seams)

### 4.1 New scene-library sources — `_GOBLIN_FILE_ADDITIONS` in `modules/goblin/config.py`

**Chosen mechanism:** a new dict in `config.py`, consumed by `goblin_add_library()`:

```python
# New sources ADDED to a library (no upstream counterpart — the override dict
# above only replaces existing stems; these have no stem to replace).
_GOBLIN_FILE_ADDITIONS = {
    "scene": {
        "entity_node": os.path.join(_goblin_dir, "scene", "main", "entity_node.cpp"),
        "entity_component": os.path.join(_goblin_dir, "scene", "main", "entity_component.cpp"),
        "entity_registry": os.path.join(_goblin_dir, "scene", "main", "entity_registry.cpp"),
    },
}
```

Hook change (inside `goblin_add_library`, after the replace loop):

```python
additions = _GOBLIN_FILE_ADDITIONS.get(lib_name)
if additions:
    for _stem, _path in additions.items():
        if os.path.isfile(_path):
            if self_env.get("verbose"):
                print(f"Goblin: Adding {_stem} -> {_path}")
            _new_source.append(_goblin_env.Object(_path))
```

Why a separate dict over extending `_GOBLIN_FILE_OVERRIDES`: the override hook's semantics are replace-by-stem (`if _stem in overrides`) — an addition entry would never match an upstream source and silently compile nothing. A dedicated additions dict is explicit, greppable, and keeps the two concepts distinct. Both compile in the overlay env (`_goblin_env`, CPPPATH prepended) so root-relative includes resolve to the goblin tree first.

Safety: `modules/goblin/scene/main/` is NOT globbed by any SCsub (upstream `scene/SCsub` globs its own tree; `modules/goblin/SCsub` globs only module-root `*.cpp`). No double-compile.

### 4.2 `scene/main/scene_tree.h` — direct narrow edit (+5 lines, sanctioned seam)

1. Forward-declare with the other class forward decls: `class EntityRegistry;` (near `class Node;`).
2. In `class SceneTree` private section: `friend class EntityNode;` and `friend class EntityComponent;` (near the existing `friend class Node;`).
3. Private member: `EntityRegistry *entity_registry = nullptr;`

ODR note: direct header edit → every TU (upstream + goblin) sees one `sizeof(SceneTree)`. Pointer member only; no by-value goblin-only type (B-14 rule).

### 4.3 Mirror `modules/goblin/scene/main/scene_tree.cpp`

- Include: `#include "scene/main/entity_registry.h"` (resolves via the overlay CPPPATH).
- `SceneTree::SceneTree()` ctor (end, after `process_groups.push_back(&default_process_group);`): `entity_registry = memnew(EntityRegistry);`
- `SceneTree::~SceneTree()` (end, after process-group cleanup, before `singleton = nullptr`): `memdelete(entity_registry);`
- `SceneTree::process()` — after `_flush_delete_queue();` (mirror line ~746): `if (entity_registry) { entity_registry->flush_dirty(); }`
- `SceneTree::physics_process()` — after `_flush_delete_queue();` (mirror line ~677): same call.

### 4.4 Registration — `modules/goblin/register_types.cpp`

At `MODULE_INITIALIZATION_LEVEL_SCENE` (main.cpp:833 runs `register_scene_types()` first; precedent `modules/sim/register_types.cpp`):

```cpp
#include "scene/main/entity_node.h"
#include "scene/main/entity_component.h"
#include "scene/main/transform_3d_component.h"
#include "scene/main/mesh_instance_component.h"
#include "scene/main/visibility_component.h"
...
if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
    GDREGISTER_CLASS(EntityNode);
    GDREGISTER_ABSTRACT_CLASS(EntityComponent); // abstract: pure virtual get_component_type()
    GDREGISTER_CLASS(Transform3DComponent);
    GDREGISTER_CLASS(MeshInstanceComponent);
    GDREGISTER_CLASS(VisibilityComponent);
}
```

Include resolution: `register_types.cpp` compiles in the module env (no goblin CPPPATH). Quoted include `"scene/main/entity_node.h"` resolves **relative to the including file's directory first** → `modules/goblin/scene/main/entity_node.h`. The scene-library TUs resolve the same physical header via the overlay CPPPATH. One header, two resolution paths → ODR-safe.

`config.py` `get_doc_classes()`: add the five classes (docgen; matches `modules/sim/config.py` pattern).

### 4.5 Upstream files touched (complete list)

| File | Change | Sanction |
|------|--------|----------|
| `scene/main/scene_tree.h` | +5 lines (forward decl, 2 friends, pointer member) | Direct narrow edit precedent (+10 lines, be9cea89a9) |
| *(everything else)* | none | — |

All other files are under `modules/goblin/`.

---

## 5. Phases

| Phase | Content | Goblin files | Effort |
|-------|---------|--------------|--------|
| P1 | Registry core: pools, entity_create/destroy, free-list, swap-remove + slot remap, dirty tracking, cross-pool instance-RID lookup, flush skeleton (transform only). Header edit + mirror ctor/dtor + config.py additions dict + registration stub (classes registered, no behavior). | `entity_registry.{h,cpp}`, `scene_tree.h` edit, mirror `scene_tree.cpp`, `config.py`, `register_types.cpp` | 2-3d |
| P2 | `EntityNode` + `EntityComponent` bases: compile-on-enter (S2/S4), type mask, `get_component`, world-anchor cache (S5), runtime add/remove component attach/detach (idempotent). | `entity_node.{h,cpp}`, `entity_component.{h,cpp}` | 2d |
| P3 | `Transform3DComponent` + `MeshInstanceComponent`: RS instance lifecycle (create on attach/at flush, base, per-surface materials, free on detach), transform composition + flush, MeshInstanceComponent `_attach` ordering (transform applied on attach so attach order is irrelevant). | `transform_3d_component.{h,cpp}`, `mesh_instance_component.{h,cpp}` | 2-3d |
| P4 | `VisibilityComponent`: instance flags flush (visible, visibility_aabb from mesh, cast_shadows). | `visibility_component.{h,cpp}` | 1d |
| P5 | Tests: doctest suite (§6) + GDScript smoke scene + one-off `tests=yes` verification build + editor boot smoke. | `tests/test_entity_node.h`, `tests/entity_node_smoke.gd` (+ minimal scene/script harness) | 1-2d |
| P6 | Docs: CODE_MAP.md entry, `docs/README.md` + `plans/README.md` + `rfc/README.md` index rows, `docs/gdscript_features.md` only if a language-visible surface lands (it does not in this slice). | `modules/goblin/docs/*` | 0.5d |

**Total: ~9-12d.** Order dependency: P1 → P2 → P3 → P4 (P5/P6 after P3; P4 can fold into P5 if time-boxed).

---

## 6. Test gates

### 6.1 Doctest suite — `modules/goblin/tests/test_entity_node.h`

Convention match: `modules/sim/tests/test_sim.h` (TEST_CASEs in a header, included from the module's `register_types.cpp` under `#ifdef TESTS_ENABLED`, run by the doctest runner in a `tests=yes` build). `[SceneTree]`-prefixed cases need a booted SceneTree (sim precedent: physics-server bootstrap).

| Case | What it proves |
|------|----------------|
| `[EntityRegistry] create/destroy + id reuse` | free-list reuse, 0 = invalid |
| `[EntityRegistry] pool insert/remove/remap` | swap-remove updates the moved slot's node backref (`get_pool_slot`), HashMap consistent under churn |
| `[EntityRegistry] dirty dedup` | repeated `mark_dirty` on one slot → one entry in `dirty_slots`; flush clears |
| `[EntityRegistry] cross-pool instance RID` | transform-only entity → invalid RID; transform+mesh entity → valid RID after flush |
| `[SceneTree][EntityNode] compile on enter` | EntityNode + components added to root → pools populated, instance RID created, RS transform applied on flush |
| `[SceneTree][EntityNode] churn remap` | N entities spawn/despawn; a survivor's transform flush lands on the correct instance (server transform readback) |
| `[SceneTree][EntityNode] runtime add/remove component` | `add_child(component)` onto an in-tree entity compiles it; `remove_child` detaches and frees the instance RID |
| `[SceneTree][EntityNode] hybrid child untouched` | a plain `Node`/`Node3D` child of an EntityNode is never compiled, stays tree-normal |
| `[SceneTree][EntityNode] nested entities` | components attach to the nearest EntityNode ancestor; inner entity's components compile to the inner entity |
| `[SceneTree][EntityNode] transform composition` | entity under a Node3D with a transform → flush applies `anchor_global × local` (RS readback) |

### 6.2 GDScript smoke — `modules/goblin/tests/entity_node_smoke.gd`

Headless-run script (mirrors the C-07 midi verification pattern): builds an EntityNode + 3 components in `_ready`, mutates transform/visibility, asserts via `RenderingServer` instance queries, prints `ENTITY_SMOKE_OK`. Verification step: `bin/goblin.windows.editor.x86_64.exe --headless --path <scratch-project-with-smoke-script>` → 0 errors + marker line.

### 6.3 `tests=yes` justification (hard rule: build command fixed)

The committed build command does NOT change. Module doctest suites (sim, midi, this one) live under `#ifdef TESTS_ENABLED` and are validated by a **one-off** `tests=yes` build — fork precedent B-01 ("tests=yes full doctest suite 1337/1337"), C-07 ("1384/1384"), M-14. Justification: scene-level behavior (tree enter/exit, RS RID lifecycle, server readback) is not unit-testable in the standard build, and the test header contributes zero code to the shipped binary (TESTS_ENABLED-gated). After verification, rebuild with the standard command and re-verify boot.

### 6.4 Build / verify commands (see `build` skill)

```
# one-off test build (P5 verification)
scons platform=windows target=editor module_mono_enabled=no accesskit=no angle=no debug_symbols=yes tests=yes -j4

# standard committed command (final state)
scons platform=windows target=editor module_mono_enabled=no accesskit=no angle=no debug_symbols=yes -j4

# boot smoke
bin/goblin.windows.editor.x86_64.exe --path <project>
```

Never `scons -c`, never delete in `bin/`. Mirror swaps verified by `--debug=explain` if a swap looks stale.

### 6.5 Gate list

1. P1–P4: incremental builds green after each phase; no new warnings.
2. P5: doctest suite green (all `[EntityRegistry]`/`[SceneTree][EntityNode]` cases) on the one-off `tests=yes` build.
3. P5: standard build green; editor boots; `--headless` smoke prints `ENTITY_SMOKE_OK`; reference title loads with zero new errors (regression gate — corpus + 342 tests rule).
4. P6: docs indexes consistent; `docs/CODE_MAP.md` documents the new seam.
5. Checklist: no `Goblin*`/`goblin_*` identifiers; no upstream file touched beyond the sanctioned header edit; no build-flag change in the committed command.

---

## 7. Risks

| Risk | Mitigation |
|------|-----------|
| One-time `add_child` spawn tax regresses spawn-heavy scenes (the original S1 objection) | Amortized (spawn-time only); pools keep per-frame cost flat. If profiling later shows spawn-bound scenes, add a deferred-tree bulk-spawn path (documented future option). |
| RS instance lifecycle bugs (double-free, leak on detach, attach-order dependence) | Instance RID owned exclusively by the mesh pool; freed on detach + on `entity_destroy`; attach applies current transform → attach order irrelevant; doctest churn + readback cases. |
| Slot remap corruption under churn | `ComponentPool::remove` remaps the moved slot's node backref; registry-level churn test asserts `get_pool_slot` consistency. |
| Registry pointer lifetime vs `Node` teardown order | `entity_registry` created in SceneTree ctor, freed in dtor after root teardown (entities already detached). `component_detach` is idempotent. |
| Flush timing changes visual semantics vs per-node notify | Frame-boundary flush (end of `process`/`physics_process`) keeps per-frame semantics; visual regression = reference-title boot + smoke scene. |
| GDScript access to component data goes through Node property setters → pool mirror can drift from Node state | Setters write both (S3); doctest mutates via the public API and reads back server state. |
| `scene_tree.h` friend decls / member widen the sanctioned seam | Additive, inert for upstream TUs; documented in CODE_MAP. B-14: pointer-only member. |
| Quoted-include resolution of `"scene/main/entity_node.h"` from `register_types.cpp` relies on file-relative search | Verified mechanism (C++ quoted-include rule); the same physical header is found via overlay CPPPATH in the scene library. If a build toolchain ever misresolves, fall back to prepending `modules/goblin` to the module env CPPPATH (shadow-checked: no header collisions outside `drivers/gles3`, `core/variant`, which `register_types.cpp` never includes). |

---

## 8. Open questions (explicit, unresolved)

1. **Mesh instance scenario RID:** `instance_create2(base, scenario)` needs the `World3D` scenario. Spec: entity's `get_viewport()->get_world_3d()->get_scenario()` at flush time. If an EntityNode is ever outside a viewport tree this is invalid — v1 assumes always-in-tree (ENTER_TREE guarantees it). Edge: `SubViewport` worlds — resolved naturally per-viewport. No action.
2. **`VisibilityComponent.visibility_aabb` export:** v1 derives from the entity's mesh AABB at flush (no export). If the reference title needs custom culling AABBs (partitioned/occlusion-driven content), add an export in slice 2. No action in this slice.
3. **Component script classes:** a GDScript `class_name MyComponent extends EntityComponent` with exported fields is expected to work (native Node machinery). `_attach` copies only the C++ known fields; script-exported extras live on the Node and are NOT mirrored into pools (correct by S3 — pools only hold typed server-facing data). Confirmed by design; no open action.
4. **`EntityComponent` abstract registration:** register as abstract (`GDREGISTER_ABSTRACT_CLASS`) so GDScript cannot `EntityComponent.new()`. Plan states abstract; developer confirms the macro name in the fork (upstream `GDREGISTER_ABSTRACT_CLASS`).
5. **Bodies revisited:** physics body components + CollisionShapeComponent are deferred (directive 1) — the design space (body RID owner, shape↔body binding in pools) should get its own mini-RFC before implementation, so the "no owner in this slice" gap is not silently re-solved ad hoc.

---

## 9. Slice 2 (after this slice validates in a real scene)

- `LightComponent` (new pool + flush; light RID create/param/type; torch-flicker ↔ S-03 rebake interplay).
- `Sprite3DComponent`/billboard if the reference title needs high-volume billboards.
- CreateDialog `EntityComponent` category + orange component icons (optional polish, user: not required).
- Optional collapsible section under EntityNode (user: not required).
- Bodies/CollisionShape mini-RFC (open question 5).
- Perf measurement: spawn/iteration/flush numbers on the reference title; revisit T2/T4/M4 only if profiling demands.

---

*Written 2026-08-20 by architect. Locked decisions S1-S11. No engine code written by this document.*
