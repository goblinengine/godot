/**************************************************************************/
/*  entity_registry.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "core/math/transform_3d.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"
#include "core/templates/vector.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"

class EntityComponent;
class EntityNode;
class Node3D;

// Component type ids. Also the bit index in EntityNode::type_mask
// (bit = 1 << uint32_t(ComponentType)).
enum class ComponentType : uint32_t {
	TRANSFORM3D = 1,
	MESH_INSTANCE = 2,
	VISIBILITY = 3,
};

// Per-type pool slot data. `dirty` dedups the pending-flush list (one entry
// per slot per frame regardless of how many setters fired).
struct Transform3DComponentData {
	Transform3D transform;
	uint8_t dirty = 0;
};

struct MeshInstanceComponentData {
	Ref<Mesh> mesh; // Authoritative mesh ref; held so the RS instance base stays alive.
	Vector<Ref<Material>> materials; // Per-surface overrides.
	RID instance_rid; // RS instance (created at flush when a mesh is present).
	uint8_t dirty = 0;
};

struct VisibilityComponentData {
	bool visible = true;
	int32_t cast_shadows = 0; // RenderingServer::ShadowCastingSetting.
	uint8_t dirty = 0;
};

// Dense slot pool for one component type. Swap-remove on detach; the moved
// slot's node backref is remapped so get_pool_slot() stays valid under churn.
template <typename T>
struct ComponentPool {
	struct Slot {
		T data;
		uint32_t entity_id = 0;
		EntityComponent *node = nullptr;
	};

	Vector<Slot> slots; // Dense; swap-remove.
	HashMap<uint32_t, uint32_t> entity_to_slot;
	LocalVector<uint32_t> dirty_slots; // Slot indices pending flush (deduped by data.dirty).

	_FORCE_INLINE_ uint32_t slot_for(uint32_t p_entity_id) const {
		const uint32_t *slot = entity_to_slot.getptr(p_entity_id);
		return slot ? *slot : UINT32_MAX;
	}

	// Defined in entity_registry.cpp: swap-remove remaps the moved slot's node
	// backref, which requires a complete EntityComponent (header only sees the
	// forward declaration).
	uint32_t insert(uint32_t p_entity_id, EntityComponent *p_node, const T &p_data);
	void remove(uint32_t p_entity_id);

	void mark_dirty(uint32_t p_entity_id) {
		uint32_t *slot_ptr = entity_to_slot.getptr(p_entity_id);
		if (!slot_ptr) {
			return;
		}
		Slot &s = slots.write[*slot_ptr];
		if (s.data.dirty) {
			return; // Already queued for this frame.
		}
		s.data.dirty = 1;
		dirty_slots.push_back(*slot_ptr);
	}

	void clear_dirty() {
		for (uint32_t i = 0; i < dirty_slots.size(); i++) {
			slots.write[dirty_slots[i]].data.dirty = 0;
		}
		dirty_slots.clear();
	}
};

// SceneTree-owned entity + component registry (internal class, not script-visible).
//
// Owns per-type SoA pools plus entity ids (free-list + counter; 0 = invalid).
// Node properties are authoritative; pools are compiled mirrors the batched
// flush reads. One flush_dirty() per frame per tree, at the end of
// SceneTree::process()/physics_process() (mirror seam).
class EntityRegistry {
	friend class Transform3DComponent;
	friend class MeshInstanceComponent;
	friend class VisibilityComponent;

public:
	uint32_t entity_create(); // Pops the free list or increments the counter (0 reserved = invalid).
	void entity_destroy(uint32_t p_entity_id); // Detaches all attached components + frees the id.

	void component_attach(uint32_t p_entity_id, EntityComponent *p_component); // Idempotent.
	void component_detach(EntityComponent *p_component); // Idempotent.

	uint32_t entity_get_component_slot(uint32_t p_entity_id, ComponentType p_type) const; // Slot or UINT32_MAX.
	RID entity_get_instance_rid(uint32_t p_entity_id) const; // Cross-pool: mesh pool lookup.
	Node3D *entity_get_world_anchor(uint32_t p_entity_id) const;
	void entity_set_world_anchor(uint32_t p_entity_id, Node3D *p_anchor);

	void flush_dirty(); // One dense pass per pool; clears dirty. Frame boundary only.
	bool is_empty() const;

	// Test introspection.
	uint32_t get_transform_slot(uint32_t p_entity_id) const;
	uint32_t get_mesh_slot(uint32_t p_entity_id) const;
	uint32_t get_visibility_slot(uint32_t p_entity_id) const;
	uint32_t get_pool_dirty_count(ComponentType p_type) const;
	const Transform3D &get_pool_transform(uint32_t p_entity_id) const;
	bool get_pool_visible(uint32_t p_entity_id) const;

private:
	ComponentPool<Transform3DComponentData> transform_pool;
	ComponentPool<MeshInstanceComponentData> mesh_pool;
	ComponentPool<VisibilityComponentData> visibility_pool;
	HashMap<uint32_t, Node3D *> world_anchors; // entity_id -> nearest Node3D ancestor (transform composition seam).
	Vector<uint32_t> free_entity_ids;
	uint32_t next_entity_id = 1;
};
