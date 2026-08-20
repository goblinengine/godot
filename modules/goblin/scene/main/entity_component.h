/**************************************************************************/
/*  entity_component.h                                                    */
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

class EntityNode;

// Base class for components of an EntityNode. A thin tree child: Node
// properties are authoritative (inspector/serialization/scripts read the
// Node); the SceneTree-owned pool is a compiled mirror the batched flush
// reads. No _process override -> no process-group membership -> zero
// per-frame tree cost beyond the flush.
//
// A component must be a direct child of an EntityNode to compile. Components
// outside an EntityNode (or nested deeper) are inert. Script attachment on
// components is normal Node behavior; script-exported extras live on the
// Node and are never mirrored into pools (pools hold typed server data only).
class EntityComponent : public Node {
	GDCLASS(EntityComponent, Node);

	friend class EntityRegistry;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	virtual ComponentType get_component_type() const = 0; // Per-subclass constant.

	uint32_t get_entity_id() const { return entity_id; } // 0 = not compiled.
	uint32_t get_pool_slot() const { return pool_slot; }
	bool is_compiled() const { return compiled; }
	void set_pool_slot(uint32_t p_slot) { pool_slot = p_slot; } // Called by pool swap-remove remap.

	EntityNode *get_entity_node() const; // Parent cast; nullptr when not under an EntityNode.
	EntityRegistry *_registry() const;

protected:
	// Called by the registry after pool insert (subclass copies Node state
	// into the pool slot + marks dirty) / on detach (clears pool references;
	// Node state is kept for reattach).
	virtual void _attach() {}
	virtual void _detach() {}

	uint32_t entity_id = 0; // Written by the registry (friend) on attach/detach.
	uint32_t pool_slot = 0; // Written by the pool on insert/remap.
	bool compiled = false;

	// Registry back-pointer. Set by the registry BEFORE _attach() runs: during
	// the parent EntityNode's ENTER_TREE (parent-first order) a component's
	// data.tree is not assigned yet, so _registry() cannot rely on get_tree().
	EntityRegistry *registry = nullptr;
};
