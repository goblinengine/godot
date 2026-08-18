/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "register_types.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"

#include "hitbox_3d.h"
#include "hurtbox_3d.h"
#include "projectile_3d.h"
#include "sim_server.h"

#ifdef TESTS_ENABLED
#include "tests/test_sim.h"
#endif

void initialize_sim_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(Hitbox3D);
		GDREGISTER_CLASS(Hurtbox3D);
		GDREGISTER_CLASS(Projectile3D);
		GDREGISTER_CLASS(SimServer);
		Engine::get_singleton()->add_singleton(
				Engine::Singleton("SimServer", memnew(SimServer)));
	}
}

void uninitialize_sim_module(ModuleInitializationLevel p_level) {
}
