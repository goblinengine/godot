/**************************************************************************/
/*  surface_properties.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "core/io/resource.h"
#include "scene/resources/physics_material.h"

// SurfaceProperties — metadata for a surface type (ice, metal, stone, flesh, ...).
//
// Assigned to a collider via SimServer.set_surface_properties() or resolved
// automatically by query_surface() via the material-name table fallback.
// query_surface() returns this resource alongside the physics raycast result,
// so combat/impact systems read surface properties (absorption, penetration,
// surface_type) without walker-heavy collider hierarchy walks.
//
// PhysicsMaterial stays core (PhysicsServer owns friction/bounce as floats).
// SurfaceProperties.physics_material is a read reference merged into the
// query record — two layers, one merged read.
class SurfaceProperties : public Resource {
	GDCLASS(SurfaceProperties, Resource);

private:
	StringName _surface_type;
	StringName _impact_sound;
	StringName _footstep_sound;
	float _penetration = 0.0f;
	float _absorption = 1.0f;
	StringName _decal;
	Ref<PhysicsMaterial> _physics_material;

protected:
	static void _bind_methods();

public:
	StringName get_surface_type() const { return _surface_type; }
	void set_surface_type(const StringName &p_value) { _surface_type = p_value; }

	StringName get_impact_sound() const { return _impact_sound; }
	void set_impact_sound(const StringName &p_value) { _impact_sound = p_value; }

	StringName get_footstep_sound() const { return _footstep_sound; }
	void set_footstep_sound(const StringName &p_value) { _footstep_sound = p_value; }

	float get_penetration() const { return _penetration; }
	void set_penetration(float p_value) { _penetration = p_value; }

	float get_absorption() const { return _absorption; }
	void set_absorption(float p_value) { _absorption = p_value; }

	StringName get_decal() const { return _decal; }
	void set_decal(const StringName &p_value) { _decal = p_value; }

	Ref<PhysicsMaterial> get_physics_material() const { return _physics_material; }
	void set_physics_material(const Ref<PhysicsMaterial> &p_value) { _physics_material = p_value; }

	SurfaceProperties() = default;
	~SurfaceProperties() = default;
};
