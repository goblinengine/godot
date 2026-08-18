/**************************************************************************/
/*  surface_properties.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "surface_properties.h"

#include "core/object/class_db.h"

void SurfaceProperties::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_surface_type"), &SurfaceProperties::get_surface_type);
	ClassDB::bind_method(D_METHOD("set_surface_type", "surface_type"), &SurfaceProperties::set_surface_type);
	ClassDB::bind_method(D_METHOD("get_impact_sound"), &SurfaceProperties::get_impact_sound);
	ClassDB::bind_method(D_METHOD("set_impact_sound", "impact_sound"), &SurfaceProperties::set_impact_sound);
	ClassDB::bind_method(D_METHOD("get_footstep_sound"), &SurfaceProperties::get_footstep_sound);
	ClassDB::bind_method(D_METHOD("set_footstep_sound", "footstep_sound"), &SurfaceProperties::set_footstep_sound);
	ClassDB::bind_method(D_METHOD("get_penetration"), &SurfaceProperties::get_penetration);
	ClassDB::bind_method(D_METHOD("set_penetration", "penetration"), &SurfaceProperties::set_penetration);
	ClassDB::bind_method(D_METHOD("get_absorption"), &SurfaceProperties::get_absorption);
	ClassDB::bind_method(D_METHOD("set_absorption", "absorption"), &SurfaceProperties::set_absorption);
	ClassDB::bind_method(D_METHOD("get_decal"), &SurfaceProperties::get_decal);
	ClassDB::bind_method(D_METHOD("set_decal", "decal"), &SurfaceProperties::set_decal);
	ClassDB::bind_method(D_METHOD("get_physics_material"), &SurfaceProperties::get_physics_material);
	ClassDB::bind_method(D_METHOD("set_physics_material", "physics_material"), &SurfaceProperties::set_physics_material);

	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "surface_type"), "set_surface_type", "get_surface_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "impact_sound"), "set_impact_sound", "get_impact_sound");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "footstep_sound"), "set_footstep_sound", "get_footstep_sound");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "penetration"), "set_penetration", "get_penetration");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "absorption"), "set_absorption", "get_absorption");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "decal"), "set_decal", "get_decal");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "physics_material", PROPERTY_HINT_RESOURCE_TYPE, "PhysicsMaterial"), "set_physics_material", "get_physics_material");
}
