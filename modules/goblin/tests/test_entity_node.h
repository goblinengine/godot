/**************************************************************************/
/*  test_entity_node.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "tests/test_macros.h"

#include "modules/goblin/scene/main/entity_component.h"
#include "modules/goblin/scene/main/entity_node.h"
#include "modules/goblin/scene/main/entity_registry.h"
#include "modules/goblin/scene/main/mesh_instance_component.h"
#include "modules/goblin/scene/main/transform_3d_component.h"
#include "modules/goblin/scene/main/visibility_component.h"

#include "scene/3d/node_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/3d/primitive_meshes.h"

// ==========================================================================
// EntityNode / EntityComponent hybrid tree+ECS layer (D-20).
//
// Registry-level cases ([EntityRegistry]) exercise the pools directly; the
// SceneTree cases drive the full compile-on-enter / attach / flush pipeline.
// Pool state is asserted via the registry's test-introspection accessors
// (the test harness rendering server is the dummy — no server readback).
// ==========================================================================

TEST_CASE("[EntityRegistry] create/destroy + id reuse") {
	EntityRegistry registry;
	CHECK(registry.is_empty()); // Fresh registry is empty.

	uint32_t a = registry.entity_create();
	CHECK(a != 0);
	uint32_t b = registry.entity_create();
	CHECK(b != a);

	registry.entity_destroy(a);
	uint32_t c = registry.entity_create();
	CHECK(c == a); // Free list reuse (LIFO).

	registry.entity_destroy(b);
	registry.entity_destroy(c);
	CHECK(registry.is_empty());
}

TEST_CASE("[EntityRegistry] pool insert/remove/remap") {
	EntityRegistry registry;
	Transform3DComponent c1;
	Transform3DComponent c2;
	Transform3DComponent c3;

	uint32_t e1 = registry.entity_create();
	uint32_t e2 = registry.entity_create();
	uint32_t e3 = registry.entity_create();

	registry.component_attach(e1, &c1);
	registry.component_attach(e2, &c2);
	registry.component_attach(e3, &c3);

	CHECK(c1.get_pool_slot() == 0);
	CHECK(c2.get_pool_slot() == 1);
	CHECK(c3.get_pool_slot() == 2);
	CHECK(registry.get_transform_slot(e1) == 0);
	CHECK(registry.get_transform_slot(e2) == 1);
	CHECK(registry.get_transform_slot(e3) == 2);

	// Remove the middle: swap-remove moves slot 2 -> 1, remaps the moved node.
	registry.component_detach(&c2);
	CHECK_FALSE(c2.is_compiled());
	CHECK(c2.get_pool_slot() == 1); // Stale but unused; node no longer compiled.
	CHECK(registry.get_transform_slot(e1) == 0);
	CHECK(registry.get_transform_slot(e3) == 1);
	CHECK(c3.get_pool_slot() == 1); // Remapped backref.

	registry.component_detach(&c1);
	registry.component_detach(&c3);
	CHECK(registry.is_empty());
}

TEST_CASE("[SceneTree][EntityNode] dirty dedup") {
	SceneTree *tree = SceneTree::get_singleton();
	REQUIRE(tree != nullptr);

	EntityNode *entity = memnew(EntityNode);
	Transform3DComponent *transform = memnew(Transform3DComponent);
	entity->add_child(transform);
	tree->get_root()->add_child(entity);

	EntityRegistry *registry = transform->_registry();
	REQUIRE(registry != nullptr);
	uint32_t id = entity->get_entity_id();

	// Two writes in one frame -> one pending flush entry (deduped by the dirty flag).
	transform->set_transform(Transform3D(Basis(), Vector3(1, 0, 0)));
	transform->set_transform(Transform3D(Basis(), Vector3(9, 8, 7)));
	CHECK(registry->get_pool_dirty_count(ComponentType::TRANSFORM3D) == 1);
	// Pool mirror holds the last write (S3: Node property authoritative, pool compiled mirror).
	CHECK(registry->get_pool_transform(id).origin == Vector3(9, 8, 7));

	registry->flush_dirty();
	CHECK(registry->get_pool_dirty_count(ComponentType::TRANSFORM3D) == 0);

	tree->get_root()->remove_child(entity);
	memdelete(entity);
}

TEST_CASE("[SceneTree][EntityNode] compile on enter") {
	SceneTree *tree = SceneTree::get_singleton();
	REQUIRE(tree != nullptr);

	EntityNode *entity = memnew(EntityNode);
	Transform3DComponent *transform = memnew(Transform3DComponent);
	transform->set_transform(Transform3D(Basis(), Vector3(2, 3, 4)));
	MeshInstanceComponent *mesh = memnew(MeshInstanceComponent);
	mesh->set_mesh(Ref<BoxMesh>(memnew(BoxMesh)));
	entity->add_child(transform);
	entity->add_child(mesh);

	tree->get_root()->add_child(entity);

	CHECK(entity->get_entity_id() != 0);
	CHECK(entity->has_component(EntityNode::TRANSFORM3D));
	CHECK(entity->has_component(EntityNode::MESH_INSTANCE));
	CHECK_FALSE(entity->has_component(EntityNode::VISIBILITY));
	CHECK(transform->is_compiled());
	CHECK(mesh->is_compiled());
	CHECK(entity->get_component(EntityNode::TRANSFORM3D) == transform);

	EntityRegistry *registry = transform->_registry();
	REQUIRE(registry != nullptr);
	CHECK(registry->entity_get_component_slot(entity->get_entity_id(), ComponentType::TRANSFORM3D) != UINT32_MAX);
	// Node-side authoritative state copied into the pool at attach.
	CHECK(registry->get_pool_transform(entity->get_entity_id()).origin == Vector3(2, 3, 4));

	registry->flush_dirty();
	// Instance RID created for the mesh component.
	CHECK(registry->entity_get_instance_rid(entity->get_entity_id()).is_valid());

	tree->get_root()->remove_child(entity);
	memdelete(entity);
}

TEST_CASE("[SceneTree][EntityNode] churn remap") {
	SceneTree *tree = SceneTree::get_singleton();
	REQUIRE(tree != nullptr);

	const int entity_count = 8;
	Vector<EntityNode *> entities;
	for (int i = 0; i < entity_count; i++) {
		EntityNode *entity = memnew(EntityNode);
		Transform3DComponent *transform = memnew(Transform3DComponent);
		transform->set_transform(Transform3D(Basis(), Vector3(i, 0, 0)));
		entity->add_child(transform);
		tree->get_root()->add_child(entity);
		entities.push_back(entity);
	}

	EntityRegistry *registry = entities[0]->get_component(EntityNode::TRANSFORM3D)->_registry();
	REQUIRE(registry != nullptr);

	// Capture before destruction (EXIT_TREE zeroes the entity id). Free list is
	// LIFO: the next spawn reuses the LAST destroyed id.
	uint32_t first_id = entities[0]->get_entity_id();
	uint32_t last_destroyed_id = entities[entity_count - 2]->get_entity_id();

	// Destroy every other entity; the survivors' slot backrefs must stay consistent.
	for (int i = 0; i < entity_count; i += 2) {
		tree->get_root()->remove_child(entities[i]);
	}
	for (int i = 1; i < entity_count; i += 2) {
		uint32_t id = entities[i]->get_entity_id();
		EntityComponent *comp = entities[i]->get_component(EntityNode::TRANSFORM3D);
		CHECK(comp->is_compiled());
		CHECK(comp->get_pool_slot() == registry->get_transform_slot(id));
	}

	// A destroyed id is reused by the next spawn (LIFO: the last destroyed id).
	EntityNode *replacement = memnew(EntityNode);
	Transform3DComponent *replacement_transform = memnew(Transform3DComponent);
	replacement->add_child(replacement_transform);
	tree->get_root()->add_child(replacement);
	CHECK(replacement->get_entity_id() == last_destroyed_id);
	CHECK(first_id != 0);

	tree->get_root()->remove_child(replacement);
	memdelete(replacement);
	for (int i = 0; i < entity_count; i++) {
		tree->get_root()->remove_child(entities[i]);
		memdelete(entities[i]);
	}
}

TEST_CASE("[SceneTree][EntityNode] runtime add/remove component") {
	SceneTree *tree = SceneTree::get_singleton();
	REQUIRE(tree != nullptr);

	EntityNode *entity = memnew(EntityNode);
	tree->get_root()->add_child(entity);

	// add_child(component) onto an in-tree entity compiles it.
	Transform3DComponent *transform = memnew(Transform3DComponent);
	entity->add_child(transform);
	CHECK(transform->is_compiled());
	CHECK(entity->has_component(EntityNode::TRANSFORM3D));

	EntityRegistry *registry = transform->_registry();
	REQUIRE(registry != nullptr);

	// remove_child detaches and frees the mesh instance RID.
	MeshInstanceComponent *mesh = memnew(MeshInstanceComponent);
	mesh->set_mesh(Ref<BoxMesh>(memnew(BoxMesh)));
	entity->add_child(mesh);
	registry->flush_dirty();
	RID instance = registry->entity_get_instance_rid(entity->get_entity_id());
	CHECK(instance.is_valid());

	entity->remove_child(mesh);
	CHECK_FALSE(mesh->is_compiled());
	registry->flush_dirty();
	CHECK(registry->entity_get_instance_rid(entity->get_entity_id()).is_null());

	tree->get_root()->remove_child(entity);
	memdelete(entity);
}

TEST_CASE("[SceneTree][EntityNode] hybrid child untouched") {
	SceneTree *tree = SceneTree::get_singleton();
	REQUIRE(tree != nullptr);

	EntityNode *entity = memnew(EntityNode);
	Transform3DComponent *transform = memnew(Transform3DComponent);
	entity->add_child(transform);
	Node3D *plain_child = memnew(Node3D); // Regular node child: stays a normal node.
	entity->add_child(plain_child);
	tree->get_root()->add_child(entity);

	CHECK(entity->has_component(EntityNode::TRANSFORM3D));
	CHECK_FALSE(entity->has_component(EntityNode::MESH_INSTANCE));
	CHECK(entity->get_component(EntityNode::MESH_INSTANCE) == nullptr);
	// The plain child is an ordinary child, not a component.
	CHECK(plain_child->is_inside_tree());
	CHECK_FALSE(plain_child->has_method("get_entity_id"));
	CHECK(entity->get_child_count() == 2);

	tree->get_root()->remove_child(entity);
	memdelete(entity);
}

TEST_CASE("[SceneTree][EntityNode] nested entities") {
	SceneTree *tree = SceneTree::get_singleton();
	REQUIRE(tree != nullptr);

	EntityNode *outer = memnew(EntityNode);
	Transform3DComponent *outer_transform = memnew(Transform3DComponent);
	outer->add_child(outer_transform);

	EntityNode *inner = memnew(EntityNode); // Nested entity = regular Node child of outer.
	Transform3DComponent *inner_transform = memnew(Transform3DComponent);
	inner->add_child(inner_transform);
	outer->add_child(inner);

	tree->get_root()->add_child(outer);

	CHECK(outer->get_entity_id() != 0);
	CHECK(inner->get_entity_id() != 0);
	CHECK(inner->get_entity_id() != outer->get_entity_id());
	// The inner entity's components compile to the inner entity.
	CHECK(inner_transform->is_compiled());
	CHECK(inner_transform->get_entity_id() == inner->get_entity_id());
	CHECK(outer_transform->get_entity_id() == outer->get_entity_id());

	tree->get_root()->remove_child(outer);
	memdelete(outer);
}

TEST_CASE("[SceneTree][EntityNode] transform composition anchor") {
	SceneTree *tree = SceneTree::get_singleton();
	REQUIRE(tree != nullptr);

	// EntityNode under a Node3D: the Node3D is cached as the world anchor.
	Node3D *platform = memnew(Node3D);
	platform->set_position(Vector3(10, 0, 0));
	tree->get_root()->add_child(platform);

	EntityNode *entity = memnew(EntityNode);
	Transform3DComponent *transform = memnew(Transform3DComponent);
	transform->set_transform(Transform3D(Basis(), Vector3(1, 0, 0)));
	entity->add_child(transform);
	platform->add_child(entity);

	EntityRegistry *registry = transform->_registry();
	REQUIRE(registry != nullptr);
	CHECK(registry->entity_get_world_anchor(entity->get_entity_id()) == platform);
	CHECK(registry->get_pool_transform(entity->get_entity_id()).origin == Vector3(1, 0, 0));

	registry->flush_dirty(); // No crash; composition is a flush-time read of the anchor.

	tree->get_root()->remove_child(platform);
	memdelete(platform);
}

TEST_CASE("[SceneTree][EntityNode] visibility flush") {
	SceneTree *tree = SceneTree::get_singleton();
	REQUIRE(tree != nullptr);

	EntityNode *entity = memnew(EntityNode);
	MeshInstanceComponent *mesh = memnew(MeshInstanceComponent);
	mesh->set_mesh(Ref<BoxMesh>(memnew(BoxMesh)));
	VisibilityComponent *visibility = memnew(VisibilityComponent);
	entity->add_child(mesh);
	entity->add_child(visibility);
	tree->get_root()->add_child(entity);

	EntityRegistry *registry = visibility->_registry();
	REQUIRE(registry != nullptr);
	registry->flush_dirty();
	CHECK(registry->entity_get_instance_rid(entity->get_entity_id()).is_valid());

	visibility->set_visible(false);
	visibility->set_cast_shadows(1);
	CHECK(registry->get_pool_visible(entity->get_entity_id()) == false);
	registry->flush_dirty(); // No crash; dummy server accepts instance flags.

	tree->get_root()->remove_child(entity);
	memdelete(entity);
}
