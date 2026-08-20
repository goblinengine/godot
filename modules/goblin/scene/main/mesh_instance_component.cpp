/**************************************************************************/
/*  mesh_instance_component.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "mesh_instance_component.h"

#include "core/object/class_db.h"

void MeshInstanceComponent::set_mesh(const Ref<Mesh> &p_mesh) {
	mesh = p_mesh;
	if (compiled) {
		EntityRegistry *registry = _registry();
		if (registry) {
			uint32_t slot = registry->mesh_pool.slot_for(entity_id);
			if (slot != UINT32_MAX) {
				registry->mesh_pool.slots.write[slot].data.mesh = p_mesh;
				registry->mesh_pool.mark_dirty(entity_id);
			}
		}
	}
}

void MeshInstanceComponent::set_material_overrides(const Array &p_materials) {
	material_overrides = p_materials;
	if (compiled) {
		EntityRegistry *registry = _registry();
		if (registry) {
			uint32_t slot = registry->mesh_pool.slot_for(entity_id);
			if (slot != UINT32_MAX) {
				MeshInstanceComponentData &slot_data = registry->mesh_pool.slots.write[slot].data;
				slot_data.materials.clear();
				slot_data.materials.resize(p_materials.size());
				for (int i = 0; i < p_materials.size(); i++) {
					slot_data.materials.write[i] = p_materials[i];
				}
				registry->mesh_pool.mark_dirty(entity_id);
			}
		}
	}
}

void MeshInstanceComponent::_attach() {
	if (!compiled) {
		return;
	}
	EntityRegistry *registry = _registry();
	if (!registry) {
		return;
	}
	uint32_t slot = registry->mesh_pool.slot_for(entity_id);
	if (slot == UINT32_MAX) {
		return;
	}
	MeshInstanceComponentData &slot_data = registry->mesh_pool.slots.write[slot].data;
	slot_data.mesh = mesh;
	slot_data.materials.clear();
	slot_data.materials.resize(material_overrides.size());
	for (int i = 0; i < material_overrides.size(); i++) {
		slot_data.materials.write[i] = material_overrides[i];
	}
	registry->mesh_pool.mark_dirty(entity_id);
}

void MeshInstanceComponent::_detach() {
	// Node state is kept for reattach. The instance RID is freed by the
	// registry before the slot is discarded.
}

void MeshInstanceComponent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh", "mesh"), &MeshInstanceComponent::set_mesh);
	ClassDB::bind_method(D_METHOD("get_mesh"), &MeshInstanceComponent::get_mesh);
	ClassDB::bind_method(D_METHOD("set_material_overrides", "materials"), &MeshInstanceComponent::set_material_overrides);
	ClassDB::bind_method(D_METHOD("get_material_overrides"), &MeshInstanceComponent::get_material_overrides);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, "Mesh"), "set_mesh", "get_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "material_overrides", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::get_type_name(Variant::OBJECT), PROPERTY_HINT_RESOURCE_TYPE, "Material")), "set_material_overrides", "get_material_overrides");
}
