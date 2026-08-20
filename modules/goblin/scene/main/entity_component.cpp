/**************************************************************************/
/*  entity_component.cpp                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "entity_component.h"

#include "scene/main/entity_node.h"
#include "scene/main/scene_tree.h"

void EntityComponent::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_ENTER_TREE: {
			EntityNode *entity = get_entity_node();
			if (entity && entity->get_entity_id() != 0) {
				// Parent entity already compiled: runtime add_child(component) case.
				// In a freshly-added subtree the parent compiles this child during
				// its own ENTER_TREE (parent-first order), so this is idempotent.
				EntityRegistry *registry = _registry();
				if (registry) {
					registry->component_attach(entity->get_entity_id(), this);
				}
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (compiled) {
				EntityRegistry *registry = _registry();
				if (registry) {
					registry->component_detach(this);
				}
			}
		} break;
	}
}

EntityNode *EntityComponent::get_entity_node() const {
	return Object::cast_to<EntityNode>(get_parent());
}

EntityRegistry *EntityComponent::_registry() const {
	// Prefer the attach-time back-pointer: valid even during the parent's
	// ENTER_TREE (before this node's data.tree is assigned). Falls back to the
	// tree for components compiled through the runtime add_child path.
	if (registry) {
		return registry;
	}
	SceneTree *tree = get_tree();
	return tree ? tree->entity_registry : nullptr;
}

void EntityComponent::_bind_methods() {
}
