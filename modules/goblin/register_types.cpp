/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "register_types.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"

#include "scene/main/entity_component.h"
#include "scene/main/entity_node.h"
#include "scene/main/mesh_instance_component.h"
#include "scene/main/transform_3d_component.h"
#include "scene/main/visibility_component.h"

#ifdef TOOLS_ENABLED
#include "editor/branding_translations.h"
#endif

#ifdef TESTS_ENABLED
#include "tests/test_entity_node.h"
#endif

void preregister_goblin_types() {
	// Called before other modules for docgen
}

void initialize_goblin_module(ModuleInitializationLevel p_level) {
	// Project Manager + Editor UI are initialized at EDITOR level.
	// We also initialize at SCENE level to cover non-editor runtime usage.
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR || p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Runtime translation overrides rebrand remaining "Godot" strings in the
		// editor UI (shortcut names, tooltips).
		register_branding_translations();
	}
#endif

	// EntityNode/EntityComponent hybrid tree+ECS layer (D-20). Registered at
	// SCENE level: main.cpp runs register_scene_types() before module init at
	// this level (precedent: modules/sim/register_types.cpp).
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(EntityNode);
		GDREGISTER_ABSTRACT_CLASS(EntityComponent); // Abstract: pure virtual get_component_type().
		GDREGISTER_CLASS(Transform3DComponent);
		GDREGISTER_CLASS(MeshInstanceComponent);
		GDREGISTER_CLASS(VisibilityComponent);
	}
}

void uninitialize_goblin_module(ModuleInitializationLevel p_level) {
	// No per-level cleanup: branding translations are not unregistered (parity
	// with the previous runtime singletons, which never removed them).
}
