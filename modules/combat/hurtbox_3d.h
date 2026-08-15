/**************************************************************************/
/*  hurtbox_3d.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "scene/3d/physics/area_3d.h"

// Passive damage receiver (extends Area3D).
//
// The Hurtbox3D does not scan for anything itself: monitoring is off so it
// performs no overlap queries. It stays monitorable so active detectors
// (Hitbox3D, Projectile3D) can find it through the physics space.
//
// Gameplay flow: a Hitbox3D (or Projectile3D) resolves a hit against this
// hurtbox and calls [method apply_hit]; the hurtbox validates its own state
// (active) and emits the [signal hurt] signal with the attacker and the
// standard hit-data dictionary. The owning character's health system connects
// to that signal and applies damage. C++ subclasses can override
// [method apply_hit] to intercept or transform damage before the signal.
//
// Collision layers: place hurtboxes on a dedicated collision layer and point
// the attacking hitbox's collision_mask at it.
class Hurtbox3D : public Area3D {
	GDCLASS(Hurtbox3D, Area3D);

	bool active = true;

protected:
	static void _bind_methods();

public:
	// Accepts a hit from any attacker carrying hit data (Hitbox3D,
	// Projectile3D, or scripted attackers). Emits [signal hurt] when active.
	// Virtual so C++ subclasses can intercept/transform damage.
	virtual void apply_hit(Object *p_attacker, const Dictionary &p_hit_data);

	void set_active(bool p_active);
	bool is_active() const;

	Hurtbox3D();
};
