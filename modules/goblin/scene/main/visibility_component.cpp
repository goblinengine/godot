/**************************************************************************/
/*  visibility_component.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "visibility_component.h"

#include "core/object/class_db.h"

void VisibilityComponent::set_visible(bool p_visible) {
	visible = p_visible;
	if (compiled) {
		EntityRegistry *registry = _registry();
		if (registry) {
			uint32_t slot = registry->visibility_pool.slot_for(entity_id);
			if (slot != UINT32_MAX) {
				registry->visibility_pool.slots.write[slot].data.visible = p_visible;
				registry->visibility_pool.mark_dirty(entity_id);
			}
		}
	}
}

void VisibilityComponent::set_cast_shadows(int32_t p_cast_shadows) {
	cast_shadows = p_cast_shadows;
	if (compiled) {
		EntityRegistry *registry = _registry();
		if (registry) {
			uint32_t slot = registry->visibility_pool.slot_for(entity_id);
			if (slot != UINT32_MAX) {
				registry->visibility_pool.slots.write[slot].data.cast_shadows = p_cast_shadows;
				registry->visibility_pool.mark_dirty(entity_id);
			}
		}
	}
}

void VisibilityComponent::_attach() {
	if (!compiled) {
		return;
	}
	EntityRegistry *registry = _registry();
	if (!registry) {
		return;
	}
	uint32_t slot = registry->visibility_pool.slot_for(entity_id);
	if (slot == UINT32_MAX) {
		return;
	}
	registry->visibility_pool.slots.write[slot].data.visible = visible;
	registry->visibility_pool.slots.write[slot].data.cast_shadows = cast_shadows;
	registry->visibility_pool.mark_dirty(entity_id);
}

void VisibilityComponent::_detach() {
	// Node state is kept for reattach.
}

void VisibilityComponent::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_visible", "visible"), &VisibilityComponent::set_visible);
	ClassDB::bind_method(D_METHOD("get_visible"), &VisibilityComponent::get_visible);
	ClassDB::bind_method(D_METHOD("set_cast_shadows", "cast_shadows"), &VisibilityComponent::set_cast_shadows);
	ClassDB::bind_method(D_METHOD("get_cast_shadows"), &VisibilityComponent::get_cast_shadows);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "visible"), "set_visible", "get_visible");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadows", PROPERTY_HINT_ENUM, "Off,On,Double Sided,Shadows Only"), "set_cast_shadows", "get_cast_shadows");
}
