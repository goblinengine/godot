/**************************************************************************/
/*  transform_3d_component.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "transform_3d_component.h"

#include "core/object/class_db.h"

void Transform3DComponent::set_transform(const Transform3D &p_transform) {
	transform = p_transform;
	if (compiled) {
		EntityRegistry *registry = _registry();
		if (registry) {
			uint32_t slot = registry->transform_pool.slot_for(entity_id);
			if (slot != UINT32_MAX) {
				registry->transform_pool.slots.write[slot].data.transform = p_transform;
				registry->transform_pool.mark_dirty(entity_id);
			}
		}
	}
}

void Transform3DComponent::_attach() {
	if (!compiled) {
		return;
	}
	EntityRegistry *registry = _registry();
	if (!registry) {
		return;
	}
	uint32_t slot = registry->transform_pool.slot_for(entity_id);
	if (slot == UINT32_MAX) {
		return;
	}
	registry->transform_pool.slots.write[slot].data.transform = transform;
	registry->transform_pool.mark_dirty(entity_id);
}

void Transform3DComponent::_detach() {
	// Node state is kept for reattach; the pool slot is discarded by the registry.
}

void Transform3DComponent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_transform", "transform"), &Transform3DComponent::set_transform);
	ClassDB::bind_method(D_METHOD("get_transform"), &Transform3DComponent::get_transform);

	ADD_PROPERTY(PropertyInfo(Variant::TRANSFORM3D, "transform"), "set_transform", "get_transform");
}
