/**************************************************************************/
/*  entity_node.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "entity_node.h"

#include "core/object/class_db.h"
#include "scene/3d/node_3d.h"
#include "scene/main/entity_component.h"
#include "scene/main/scene_tree.h"

void EntityNode::_notification(int p_notification) {
	switch (p_notification) {
		case NOTIFICATION_ENTER_TREE:
			_compile();
			break;
		case NOTIFICATION_EXIT_TREE:
			_decompile();
			break;
	}
}

void EntityNode::_compile() {
	if (entity_id != 0) {
		return; // Idempotent.
	}
	EntityRegistry *registry = _registry();
	ERR_FAIL_NULL(registry);

	entity_id = registry->entity_create();
	_update_world_anchor();

	// Parent-first ENTER_TREE: children are already in the tree (data set by
	// _propagate_enter_tree) when this runs, so attach them directly.
	for (int i = 0; i < get_child_count(); i++) {
		EntityComponent *component = Object::cast_to<EntityComponent>(get_child(i));
		if (component) {
			registry->component_attach(entity_id, component);
		}
	}
}

void EntityNode::_decompile() {
	if (entity_id == 0) {
		return; // Idempotent.
	}
	EntityRegistry *registry = _registry();
	if (registry) {
		registry->entity_destroy(entity_id);
	}
	entity_id = 0;
	type_mask = 0;
	world_anchor = nullptr;
}

void EntityNode::_update_world_anchor() {
	Node3D *anchor = nullptr;
	Node *parent = get_parent();
	while (parent) {
		Node3D *node_3d = Object::cast_to<Node3D>(parent);
		if (node_3d) {
			anchor = node_3d;
			break;
		}
		parent = parent->get_parent();
	}
	world_anchor = anchor;
	EntityRegistry *registry = _registry();
	if (entity_id != 0 && registry) {
		registry->entity_set_world_anchor(entity_id, anchor);
	}
}

EntityRegistry *EntityNode::_registry() const {
	SceneTree *tree = get_tree();
	return tree ? tree->entity_registry : nullptr;
}

bool EntityNode::has_component(uint32_t p_type) const {
	return p_type < 32 && (type_mask & (1u << p_type)) != 0;
}

EntityComponent *EntityNode::get_component(uint32_t p_type) const {
	for (int i = 0; i < get_child_count(); i++) {
		EntityComponent *component = Object::cast_to<EntityComponent>(get_child(i));
		if (component && uint32_t(component->get_component_type()) == p_type) {
			return component;
		}
	}
	return nullptr;
}

void EntityNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_component", "type"), &EntityNode::has_component);
	ClassDB::bind_method(D_METHOD("get_component", "type"), &EntityNode::get_component);
	ClassDB::bind_method(D_METHOD("get_entity_id"), &EntityNode::get_entity_id);
	ClassDB::bind_method(D_METHOD("get_type_mask"), &EntityNode::get_type_mask);

	BIND_ENUM_CONSTANT(TRANSFORM3D);
	BIND_ENUM_CONSTANT(MESH_INSTANCE);
	BIND_ENUM_CONSTANT(VISIBILITY);
}
