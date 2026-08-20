/**************************************************************************/
/*  entity_node.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

// Same-directory include (file-relative): this header is pulled in from
// multiple environments (scene library overlay, module registration, tests)
// that do not all carry the goblin tree on the include path.
#include "entity_registry.h"

#include "scene/main/node.h"

class EntityComponent;
class Node3D;

// Hybrid tree + ECS entity. A full Node subclass (scripts, get_tree(),
// inspector, .tscn, undo all work) that compiles its direct EntityComponent
// children into SceneTree-owned per-type pools on tree entry. Regular Node
// children stay ordinary child nodes (hybrid: mix regular bodies/nodes with
// entity components). Data lives in the pools; iteration never touches the
// Node shell.
class EntityNode : public Node {
	GDCLASS(EntityNode, Node);

	friend class EntityRegistry;

public:
	enum ComponentType : uint32_t {
		TRANSFORM3D = 1, // Mirror of the registry enum for GDScript.
		MESH_INSTANCE = 2,
		VISIBILITY = 3,
	};

	bool has_component(uint32_t p_type) const; // Type mask test, O(1).
	EntityComponent *get_component(uint32_t p_type) const; // O(direct children); convenience.
	uint32_t get_entity_id() const { return entity_id; } // 0 = not compiled.
	uint32_t get_type_mask() const { return type_mask; }

protected:
	void _notification(int p_notification);
	static void _bind_methods();

private:
	void _compile(); // ENTER_TREE: entity_create + attach direct EntityComponent children + cache anchor.
	void _decompile(); // EXIT_TREE: registry->entity_destroy(entity_id); zero id/mask.
	void _update_world_anchor(); // ENTER_TREE + MOVED_IN_PARENT.

	EntityRegistry *_registry() const;

	uint32_t entity_id = 0; // 0 = not compiled.
	uint32_t type_mask = 0; // Bits = attached component types (1 << ComponentType).
	Node3D *world_anchor = nullptr; // Nearest Node3D ancestor (transform composition seam).
};

VARIANT_ENUM_CAST(EntityNode::ComponentType);
