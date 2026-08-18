/**************************************************************************/
/*  projectile_3d.h                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "scene/3d/physics/area_3d.h"

class ShapeCast3D;

// Manual-velocity projectile (extends Area3D).
//
// Deliberately NOT a RigidBody3D: gameplay projectiles (missiles, arrows,
// thrown objects) fight the physics engine when simulated as rigid bodies.
// Projectile3D owns its motion in engine code and uses a ShapeCast3D for
// continuous swept collision, so fast projectiles cannot tunnel through thin
// geometry.
//
// Behavior flags:
//  - max_bounces > 0: reflects velocity off hits (bounce_factor scales the
//    reflected speed). Exceeding max_bounces destroys the projectile.
//  - homing and target set: steers toward the target each physics frame
//    (homing_strength controls turn rate).
//  - gravity_scale > 0: applies engine default gravity scaled by this value.
//  - max_lifetime > 0: destroys after that many seconds.
//  - max_range > 0: destroys after traveling that many units.
//
// On hit it emits [signal hit] with the standard hit-data dictionary
// (CombatUtils). If the collider is a Hurtbox3D, the hit is also forwarded
// via Hurtbox3D.apply_hit so receiver-side health systems see it without
// extra wiring.
class Projectile3D : public Area3D {
	GDCLASS(Projectile3D, Area3D);

	// Motion.
	Vector3 velocity = Vector3();
	Vector3 direction = Vector3(0, 0, -1);
	float speed = 20.0f;
	float gravity_scale = 0.0f;
	float homing_strength = 4.0f;

	// Lifetime / range.
	float max_lifetime = 0.0f; // 0 = infinite.
	float max_range = 0.0f; // 0 = infinite.
	float lifetime = 0.0f;
	float distance_traveled = 0.0f;

	// Bounce.
	int max_bounces = 0;
	int bounce_count = 0;
	float bounce_factor = 0.5f;

	// Homing.
	bool homing = false;
	Node3D *target = nullptr;

	// Attack data.
	float damage = 1.0f;
	float knockback = 0.0f;
	Array damage_types; // Array[StringName]
	StringName element;
	Object *source = nullptr;

	bool flying = false;
	ShapeCast3D *sweep_cast = nullptr;

	void _setup_sweep_cast();
	bool _update_sweep(double p_delta, Vector3 &r_hit_position, Vector3 &r_hit_normal,
			Object *&r_hit_collider);
	void _destroy();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	// Public for testability: called by the scene tree each physics frame.
	void _physics_process(double p_delta);
	// Reflects velocity off a surface. Public for testability.
	void _bounce(const Vector3 &p_normal);
	// Handles a collision hit: emits [signal hit], forwards to hurtboxes,
	// resolves surface properties (S-05). Public for testability.
	void _on_hit(const Vector3 &p_position, const Vector3 &p_normal, Object *p_collider);

public:
	// Starts the projectile at p_origin traveling along p_direction at
	// [member speed]. Resets lifetime/range/bounce state.
	void fire(const Vector3 &p_origin, const Vector3 &p_direction);
	void stop(); // Stops flight without destroying (script cleanup).

	void set_velocity(const Vector3 &p_velocity);
	Vector3 get_velocity() const;

	void set_direction(const Vector3 &p_direction);
	Vector3 get_direction() const;

	void set_speed(float p_speed);
	float get_speed() const;

	void set_gravity_scale(float p_gravity_scale);
	float get_gravity_scale() const;

	void set_max_lifetime(float p_max_lifetime);
	float get_max_lifetime() const;

	void set_max_range(float p_max_range);
	float get_max_range() const;

	void set_max_bounces(int p_max_bounces);
	int get_max_bounces() const;

	void set_bounce_factor(float p_bounce_factor);
	float get_bounce_factor() const;

	void set_homing(bool p_homing);
	bool is_homing() const;

	void set_homing_strength(float p_homing_strength);
	float get_homing_strength() const;

	void set_target(Node3D *p_target);
	Node3D *get_target() const;

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

	bool is_flying() const;
	float get_lifetime() const;
	float get_distance_traveled() const;

	Projectile3D();
	~Projectile3D();
};
