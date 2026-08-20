/**************************************************************************/
/*  entity_registry.cpp                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "entity_registry.h"

#include "scene/3d/node_3d.h"
#include "scene/main/entity_component.h"
#include "scene/main/entity_node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/rendering/rendering_server.h"

template <typename T>
uint32_t ComponentPool<T>::insert(uint32_t p_entity_id, EntityComponent *p_node, const T &p_data) {
	ERR_FAIL_COND_V(entity_to_slot.has(p_entity_id), UINT32_MAX);
	uint32_t index = slots.size();
	Slot s;
	s.data = p_data;
	s.entity_id = p_entity_id;
	s.node = p_node;
	slots.push_back(s);
	entity_to_slot[p_entity_id] = index;
	p_node->set_pool_slot(index);
	return index;
}

template <typename T>
void ComponentPool<T>::remove(uint32_t p_entity_id) {
	uint32_t *slot_ptr = entity_to_slot.getptr(p_entity_id);
	if (!slot_ptr) {
		return; // Not attached; idempotent.
	}
	uint32_t index = *slot_ptr;
	uint32_t last = slots.size() - 1;

	for (uint32_t i = 0; i < dirty_slots.size(); i++) {
		if (dirty_slots[i] == index) {
			dirty_slots.remove_at(i);
			break;
		}
	}

	if (index != last) {
		slots.write[index] = slots[last];
		Slot &moved = slots.write[index];
		moved.node->set_pool_slot(index);
		entity_to_slot[moved.entity_id] = index;
		for (uint32_t i = 0; i < dirty_slots.size(); i++) {
			if (dirty_slots[i] == last) {
				dirty_slots[i] = index;
				break;
			}
		}
	}
	slots.remove_at(last);
	entity_to_slot.erase(p_entity_id);
}

// Explicit instantiations: only EntityRegistry uses insert/remove (the
// component classes reach the pools directly for slot data).
template struct ComponentPool<Transform3DComponentData>;
template struct ComponentPool<MeshInstanceComponentData>;
template struct ComponentPool<VisibilityComponentData>;

uint32_t EntityRegistry::entity_create() {
	if (!free_entity_ids.is_empty()) {
		uint32_t id = free_entity_ids[free_entity_ids.size() - 1];
		free_entity_ids.remove_at(free_entity_ids.size() - 1);
		return id;
	}
	return next_entity_id++;
}

void EntityRegistry::entity_destroy(uint32_t p_entity_id) {
	if (p_entity_id == 0) {
		return;
	}
	// Collect attached components first — detaching mutates the pools we iterate.
	Vector<EntityComponent *> to_detach;
	for (const ComponentPool<Transform3DComponentData>::Slot &s : transform_pool.slots) {
		if (s.entity_id == p_entity_id) {
			to_detach.push_back(s.node);
		}
	}
	for (const ComponentPool<MeshInstanceComponentData>::Slot &s : mesh_pool.slots) {
		if (s.entity_id == p_entity_id) {
			to_detach.push_back(s.node);
		}
	}
	for (const ComponentPool<VisibilityComponentData>::Slot &s : visibility_pool.slots) {
		if (s.entity_id == p_entity_id) {
			to_detach.push_back(s.node);
		}
	}
	for (EntityComponent *component : to_detach) {
		component_detach(component);
	}
	world_anchors.erase(p_entity_id);
	free_entity_ids.push_back(p_entity_id);
}

void EntityRegistry::component_attach(uint32_t p_entity_id, EntityComponent *p_component) {
	ERR_FAIL_COND(p_entity_id == 0);
	ERR_FAIL_NULL(p_component);
	if (p_component->is_compiled()) {
		return; // Idempotent.
	}

	ComponentType type = p_component->get_component_type();
	uint32_t slot = UINT32_MAX;
	switch (type) {
		case ComponentType::TRANSFORM3D:
			slot = transform_pool.insert(p_entity_id, p_component, Transform3DComponentData());
			break;
		case ComponentType::MESH_INSTANCE:
			slot = mesh_pool.insert(p_entity_id, p_component, MeshInstanceComponentData());
			break;
		case ComponentType::VISIBILITY:
			slot = visibility_pool.insert(p_entity_id, p_component, VisibilityComponentData());
			break;
		default:
			ERR_FAIL_MSG("EntityRegistry: unknown component type.");
	}
	ERR_FAIL_COND(slot == UINT32_MAX);

	p_component->entity_id = p_entity_id;
	p_component->compiled = true;
	p_component->registry = this; // Set before _attach(): _registry() must not use get_tree() during the parent's ENTER_TREE.
	p_component->_attach(); // Subclass copies Node state into the pool slot + marks dirty.

	EntityNode *entity = p_component->get_entity_node();
	if (entity) {
		entity->type_mask |= (1u << uint32_t(type));
	}
}

void EntityRegistry::component_detach(EntityComponent *p_component) {
	ERR_FAIL_NULL(p_component);
	if (!p_component->is_compiled()) {
		return; // Idempotent.
	}

	uint32_t entity_id = p_component->get_entity_id();
	ComponentType type = p_component->get_component_type();
	switch (type) {
		case ComponentType::TRANSFORM3D:
			transform_pool.remove(entity_id);
			break;
		case ComponentType::MESH_INSTANCE: {
			uint32_t slot = mesh_pool.slot_for(entity_id);
			if (slot != UINT32_MAX && mesh_pool.slots[slot].data.instance_rid.is_valid()) {
				RenderingServer::get_singleton()->free_rid(mesh_pool.slots[slot].data.instance_rid);
			}
			mesh_pool.remove(entity_id);
		} break;
		case ComponentType::VISIBILITY:
			visibility_pool.remove(entity_id);
			break;
	}

	p_component->_detach();
	p_component->entity_id = 0;
	p_component->compiled = false;
	p_component->registry = nullptr;

	EntityNode *entity = p_component->get_entity_node();
	if (entity) {
		entity->type_mask &= ~(1u << uint32_t(type));
	}
}

uint32_t EntityRegistry::entity_get_component_slot(uint32_t p_entity_id, ComponentType p_type) const {
	switch (p_type) {
		case ComponentType::TRANSFORM3D:
			return transform_pool.slot_for(p_entity_id);
		case ComponentType::MESH_INSTANCE:
			return mesh_pool.slot_for(p_entity_id);
		case ComponentType::VISIBILITY:
			return visibility_pool.slot_for(p_entity_id);
	}
	return UINT32_MAX;
}

RID EntityRegistry::entity_get_instance_rid(uint32_t p_entity_id) const {
	uint32_t slot = mesh_pool.slot_for(p_entity_id);
	return slot != UINT32_MAX ? mesh_pool.slots[slot].data.instance_rid : RID();
}

Node3D *EntityRegistry::entity_get_world_anchor(uint32_t p_entity_id) const {
	Node3D *const *anchor = world_anchors.getptr(p_entity_id);
	return anchor ? *anchor : nullptr;
}

void EntityRegistry::entity_set_world_anchor(uint32_t p_entity_id, Node3D *p_anchor) {
	world_anchors[p_entity_id] = p_anchor;
}

void EntityRegistry::flush_dirty() {
	RenderingServer *rs = RenderingServer::get_singleton();

	// 1. Mesh pool first: ensures instances exist before the transform/visibility
	// passes read them, so a spawn that attaches mesh + transform in one frame
	// lands as one correctly-based instance with the transform applied.
	for (uint32_t i = 0; i < mesh_pool.dirty_slots.size(); i++) {
		uint32_t slot = mesh_pool.dirty_slots[i];
		MeshInstanceComponentData &data = mesh_pool.slots.write[slot].data;
		uint32_t entity_id = mesh_pool.slots[slot].entity_id;

		if (data.mesh.is_valid()) {
			if (data.instance_rid.is_null()) {
				RID scenario;
				EntityComponent *node = mesh_pool.slots[slot].node;
				Viewport *viewport = node ? node->get_viewport() : nullptr;
				Ref<World3D> world = viewport ? viewport->get_world_3d() : Ref<World3D>();
				if (world.is_valid()) {
					scenario = world->get_scenario();
				}
				data.instance_rid = rs->instance_create2(data.mesh->get_rid(), scenario);
				// Editor picking: associate the instance with the entity node so
				// clicking the mesh in the 3D viewport selects the EntityNode.
				EntityNode *entity = node ? node->get_entity_node() : nullptr;
				if (entity) {
					rs->instance_attach_object_instance_id(data.instance_rid, entity->get_instance_id());
				}
			} else {
				rs->instance_set_base(data.instance_rid, data.mesh->get_rid());
			}
			int surface_count = data.mesh->get_surface_count();
			for (int s = 0; s < surface_count && s < data.materials.size(); s++) {
				RID material_rid = data.materials[s].is_valid() ? data.materials[s]->get_rid() : RID();
				rs->instance_set_surface_override_material(data.instance_rid, s, material_rid);
			}
		} else if (data.instance_rid.is_valid()) {
			rs->free_rid(data.instance_rid);
			data.instance_rid = RID();
		}
	}

	// 2. Transform pool: entity-local transform composed with the world anchor.
	for (uint32_t i = 0; i < transform_pool.dirty_slots.size(); i++) {
		uint32_t slot = transform_pool.dirty_slots[i];
		Transform3DComponentData &data = transform_pool.slots.write[slot].data;
		uint32_t entity_id = transform_pool.slots[slot].entity_id;

		RID instance = entity_get_instance_rid(entity_id);
		if (instance.is_null()) {
			continue;
		}
		Transform3D global = Transform3D();
		Node3D *anchor = entity_get_world_anchor(entity_id);
		if (anchor) {
			global = anchor->get_global_transform();
		}
		rs->instance_set_transform(instance, global * data.transform);
	}

	// 3. Visibility pool: instance flags.
	for (uint32_t i = 0; i < visibility_pool.dirty_slots.size(); i++) {
		uint32_t slot = visibility_pool.dirty_slots[i];
		VisibilityComponentData &data = visibility_pool.slots.write[slot].data;
		uint32_t entity_id = visibility_pool.slots[slot].entity_id;

		RID instance = entity_get_instance_rid(entity_id);
		if (instance.is_null()) {
			continue;
		}
		rs->instance_set_visible(instance, data.visible);
		rs->instance_geometry_set_cast_shadows_setting(instance, (RSE::ShadowCastingSetting)data.cast_shadows);
	}

	mesh_pool.clear_dirty();
	transform_pool.clear_dirty();
	visibility_pool.clear_dirty();
}

bool EntityRegistry::is_empty() const {
	return transform_pool.slots.is_empty() && mesh_pool.slots.is_empty() && visibility_pool.slots.is_empty();
}

uint32_t EntityRegistry::get_transform_slot(uint32_t p_entity_id) const {
	return transform_pool.slot_for(p_entity_id);
}

uint32_t EntityRegistry::get_mesh_slot(uint32_t p_entity_id) const {
	return mesh_pool.slot_for(p_entity_id);
}

uint32_t EntityRegistry::get_visibility_slot(uint32_t p_entity_id) const {
	return visibility_pool.slot_for(p_entity_id);
}

uint32_t EntityRegistry::get_pool_dirty_count(ComponentType p_type) const {
	switch (p_type) {
		case ComponentType::TRANSFORM3D:
			return transform_pool.dirty_slots.size();
		case ComponentType::MESH_INSTANCE:
			return mesh_pool.dirty_slots.size();
		case ComponentType::VISIBILITY:
			return visibility_pool.dirty_slots.size();
	}
	return 0;
}

const Transform3D &EntityRegistry::get_pool_transform(uint32_t p_entity_id) const {
	static const Transform3D s_identity = Transform3D();
	uint32_t slot = transform_pool.slot_for(p_entity_id);
	return slot != UINT32_MAX ? transform_pool.slots[slot].data.transform : s_identity;
}

bool EntityRegistry::get_pool_visible(uint32_t p_entity_id) const {
	uint32_t slot = visibility_pool.slot_for(p_entity_id);
	return slot != UINT32_MAX ? visibility_pool.slots[slot].data.visible : true;
}
