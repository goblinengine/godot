/**************************************************************************/
/*  hitbox_3d.h                                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "core/templates/hash_set.h"
#include "scene/3d/physics/area_3d.h"

class Hurtbox3D;

// Active damage detector (extends Area3D).
//
// The Hitbox3D is attached to an attack (sword swing, bullet, explosion) and
// scans the physics space for Hurtbox3D receivers. It carries the attack's
// damage metadata (damage, knockback, damage types, element, source) and
// emits [signal hit] with the receiver and the standard hit-data dictionary
// whenever it registers a hit. It also forwards the hit to the hurtbox via
// Hurtbox3D.apply_hit, so the receiver-side health system gets the same data
// through its own [signal Hurtbox3D.hurt].
//
// Monitoring is on (it scans) and monitorable is off (attacks are not
// targets). A hurtbox is only detected when its collision layer is in this
// hitbox's collision_mask; set up the layers so attacks and receivers meet.
//
// Dedup: each hurtbox is hit at most once per activation. Call reset() to
// re-arm the hitbox for a new swing/attack.
class Hitbox3D : public Area3D {
	GDCLASS(Hitbox3D, Area3D);

	float damage = 0.0f;
	float knockback = 0.0f;
	Array damage_types; // Array[StringName]
	StringName element;
	Object *source = nullptr; // Attacker node or object.
	bool active = true;

	HashSet<ObjectID> hit_targets;

protected:
	static void _bind_methods();

public:
	// Overlap callback (connected to area_entered). Public for testability.
	void _on_area_entered(Area3D *p_area);

	// Builds the standard hit-data dictionary from this hitbox's attack data.
	Dictionary build_hit_data(Object *p_collider = nullptr, const Vector3 &p_position = Vector3(),
			const Vector3 &p_normal = Vector3(), const Vector3 &p_velocity = Vector3()) const;

	// Registers a hit against a hurtbox: dedups, forwards to the hurtbox via
	// Hurtbox3D.apply_hit, and emits [signal hit]. Used internally on overlap
	// and callable directly for scripted attacks.
	void register_hit(Hurtbox3D *p_hurtbox, const Dictionary &p_hit_data);
	// Clears the already-hit set so the hitbox can hit again (new swing).
	void reset();

	void set_damage(float p_damage);
	float get_damage() const;

	void set_knockback(float p_knockback);
	float get_knockback() const;

	void set_damage_types(const Array &p_damage_types);
	Array get_damage_types() const;

	void set_element(const StringName &p_element);
	StringName get_element() const;

	void set_source(Object *p_source);
	Object *get_source() const;

	void set_active(bool p_active);
	bool is_active() const;

	Hitbox3D();
};
