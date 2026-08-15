/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "register_types.h"

#include "core/object/class_db.h"

#include "hitbox_3d.h"
#include "hurtbox_3d.h"
#include "projectile_3d.h"

void initialize_combat_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(Hitbox3D);
		GDREGISTER_CLASS(Hurtbox3D);
		GDREGISTER_CLASS(Projectile3D);
	}
}

void uninitialize_combat_module(ModuleInitializationLevel p_level) {
}
