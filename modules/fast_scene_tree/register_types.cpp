/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/*                        https://goblin-engine.org                       */
/**************************************************************************/

#include "register_types.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"

#include "fast_scene_tree.h"

void initialize_fast_scene_tree_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_CORE) {
		GDREGISTER_CLASS(FastSceneTree);

		// Fork default main loop: FastSceneTree when this module is present.
		// Upstream registers `application/run/main_loop_type` with default
		// "SceneTree" (core/config/project_settings.cpp); override the default
		// here at CORE level, i.e. BEFORE project.godot is loaded in
		// Main::start, so an explicit project setting still wins.
		// With the module disabled this never runs and the engine keeps the
		// upstream "SceneTree" default.
		ProjectSettings *ps = ProjectSettings::get_singleton();
		ps->set(SNAME("application/run/main_loop_type"), "FastSceneTree");
		ps->set_initial_value(SNAME("application/run/main_loop_type"), "FastSceneTree");
	}
}

void uninitialize_fast_scene_tree_module(ModuleInitializationLevel p_level) {
}
