/**************************************************************************/
/*  projectile_3d.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "projectile_3d.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "scene/3d/physics/shape_cast_3d.h"

#include "combat_utils.h"
#include "hurtbox_3d.h"
#include "sim_server.h"

void Projectile3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("fire", "origin", "direction"), &Projectile3D::fire);
	ClassDB::bind_method(D_METHOD("stop"), &Projectile3D::stop);
	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &Projectile3D::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &Projectile3D::get_velocity);
	ClassDB::bind_method(D_METHOD("set_direction", "direction"), &Projectile3D::set_direction);
	ClassDB::bind_method(D_METHOD("get_direction"), &Projectile3D::get_direction);
	ClassDB::bind_method(D_METHOD("set_speed", "speed"), &Projectile3D::set_speed);
	ClassDB::bind_method(D_METHOD("get_speed"), &Projectile3D::get_speed);
	ClassDB::bind_method(D_METHOD("set_gravity_scale", "gravity_scale"), &Projectile3D::set_gravity_scale);
	ClassDB::bind_method(D_METHOD("get_gravity_scale"), &Projectile3D::get_gravity_scale);
	ClassDB::bind_method(D_METHOD("set_max_lifetime", "max_lifetime"), &Projectile3D::set_max_lifetime);
	ClassDB::bind_method(D_METHOD("get_max_lifetime"), &Projectile3D::get_max_lifetime);
	ClassDB::bind_method(D_METHOD("set_max_range", "max_range"), &Projectile3D::set_max_range);
	ClassDB::bind_method(D_METHOD("get_max_range"), &Projectile3D::get_max_range);
	ClassDB::bind_method(D_METHOD("set_max_bounces", "max_bounces"), &Projectile3D::set_max_bounces);
	ClassDB::bind_method(D_METHOD("get_max_bounces"), &Projectile3D::get_max_bounces);
	ClassDB::bind_method(D_METHOD("set_bounce_factor", "bounce_factor"), &Projectile3D::set_bounce_factor);
	ClassDB::bind_method(D_METHOD("get_bounce_factor"), &Projectile3D::get_bounce_factor);
	ClassDB::bind_method(D_METHOD("set_homing", "homing"), &Projectile3D::set_homing);
	ClassDB::bind_method(D_METHOD("is_homing"), &Projectile3D::is_homing);
	ClassDB::bind_method(D_METHOD("set_homing_strength", "homing_strength"), &Projectile3D::set_homing_strength);
	ClassDB::bind_method(D_METHOD("get_homing_strength"), &Projectile3D::get_homing_strength);
	ClassDB::bind_method(D_METHOD("set_target", "target"), &Projectile3D::set_target);
	ClassDB::bind_method(D_METHOD("get_target"), &Projectile3D::get_target);
	ClassDB::bind_method(D_METHOD("set_damage", "damage"), &Projectile3D::set_damage);
	ClassDB::bind_method(D_METHOD("get_damage"), &Projectile3D::get_damage);
	ClassDB::bind_method(D_METHOD("set_knockback", "knockback"), &Projectile3D::set_knockback);
	ClassDB::bind_method(D_METHOD("get_knockback"), &Projectile3D::get_knockback);
	ClassDB::bind_method(D_METHOD("set_damage_types", "damage_types"), &Projectile3D::set_damage_types);
	ClassDB::bind_method(D_METHOD("get_damage_types"), &Projectile3D::get_damage_types);
	ClassDB::bind_method(D_METHOD("set_element", "element"), &Projectile3D::set_element);
	ClassDB::bind_method(D_METHOD("get_element"), &Projectile3D::get_element);
	ClassDB::bind_method(D_METHOD("set_source", "source"), &Projectile3D::set_source);
	ClassDB::bind_method(D_METHOD("get_source"), &Projectile3D::get_source);
	ClassDB::bind_method(D_METHOD("is_flying"), &Projectile3D::is_flying);
	ClassDB::bind_method(D_METHOD("get_lifetime"), &Projectile3D::get_lifetime);
	ClassDB::bind_method(D_METHOD("get_distance_traveled"), &Projectile3D::get_distance_traveled);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity"), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "direction"), "set_direction", "get_direction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gravity_scale"), "set_gravity_scale", "get_gravity_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_lifetime"), "set_max_lifetime", "get_max_lifetime");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_range"), "set_max_range", "get_max_range");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "max_bounces"), "set_max_bounces", "get_max_bounces");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bounce_factor"), "set_bounce_factor", "get_bounce_factor");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "homing"), "set_homing", "is_homing");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "homing_strength"), "set_homing_strength", "get_homing_strength");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "target", PROPERTY_HINT_RESOURCE_TYPE, "Node3D"),
			"set_target", "get_target");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damage"), "set_damage", "get_damage");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "knockback"), "set_knockback", "get_knockback");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "damage_types"), "set_damage_types", "get_damage_types");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "element"), "set_element", "get_element");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "source", PROPERTY_HINT_RESOURCE_TYPE, "Object"),
			"set_source", "get_source");

	ADD_SIGNAL(MethodInfo("hit", PropertyInfo(Variant::DICTIONARY, "hit_data")));
	ADD_SIGNAL(MethodInfo("expired"));
}

void Projectile3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			_setup_sweep_cast();
		} break;
		case NOTIFICATION_PHYSICS_PROCESS: {
			_physics_process(get_physics_process_delta_time());
		} break;
		case NOTIFICATION_EXIT_TREE: {
			sweep_cast = nullptr; // Child of this node; freed with the parent.
		} break;
		default:
			break;
	}
}

void Projectile3D::_physics_process(double p_delta) {
	if (!flying) {
		return;
	}

	lifetime += p_delta;
	if (max_lifetime > 0.0f && lifetime >= max_lifetime) {
		_destroy();
		return;
	}

	// Homing: steer velocity toward the target.
	if (homing && target != nullptr && Object::cast_to<Object>(target) != nullptr) {
		const Vector3 to_target = target->get_global_position() - get_global_position();
		if (to_target.length_squared() > 0.0001f) {
			const Vector3 desired = to_target.normalized() * velocity.length();
			velocity = velocity.lerp(desired, homing_strength * p_delta);
		}
	}

	// Gravity.
	if (gravity_scale > 0.0f) {
		const float default_gravity = GLOBAL_GET("physics/3d/default_gravity");
		velocity.y -= default_gravity * gravity_scale * p_delta;
	}

	const Vector3 prev_position = get_global_position();

	// Swept collision: move the shape along this frame's motion, catch
	// tunneling through thin geometry.
	Vector3 hit_position;
	Vector3 hit_normal;
	Object *hit_collider = nullptr;
	if (_update_sweep(p_delta, hit_position, hit_normal, hit_collider)) {
		set_global_position(hit_position - hit_normal * 0.01);
		_on_hit(hit_position, hit_normal, hit_collider);
		return;
	}

	set_global_position(prev_position + velocity * p_delta);
	distance_traveled += (get_global_position() - prev_position).length();
	if (max_range > 0.0f && distance_traveled >= max_range) {
		_destroy();
	}
}

void Projectile3D::_setup_sweep_cast() {
	if (sweep_cast != nullptr) {
		return;
	}
	sweep_cast = memnew(ShapeCast3D);
	sweep_cast->set_name("ProjectileSweepCast");
	sweep_cast->set_collide_with_areas(true);
	sweep_cast->set_collide_with_bodies(true);
	sweep_cast->set_collision_mask(get_collision_mask());
	sweep_cast->set_exclude_parent_body(true);
	sweep_cast->set_margin(0.01f);
	add_child(sweep_cast);
}

bool Projectile3D::_update_sweep(double p_delta, Vector3 &r_hit_position, Vector3 &r_hit_normal,
		Object *&r_hit_collider) {
	if (sweep_cast == nullptr || sweep_cast->get_shape().is_null()) {
		return false;
	}
	sweep_cast->set_global_position(get_global_position());
	sweep_cast->set_target_position(velocity * p_delta);
	sweep_cast->force_shapecast_update();
	if (!sweep_cast->is_colliding()) {
		return false;
	}
	r_hit_position = sweep_cast->get_collision_point(0);
	r_hit_normal = sweep_cast->get_collision_normal(0);
	r_hit_collider = sweep_cast->get_collider(0);
	return true;
}

void Projectile3D::_on_hit(const Vector3 &p_position, const Vector3 &p_normal, Object *p_collider) {
	Dictionary hit_data = CombatUtils::build_hit_data(damage, knockback, damage_types, element,
			source, p_position, p_normal, p_collider, velocity);

	// S-05: resolve surface properties at the impact point for
	// surface-specific effects/sounds. The ray runs from just off the
	// surface back to the hit point, excluding the projectile itself.
	SimServer *sim = SimServer::get_singleton();
	if (sim != nullptr) {
		Dictionary surface_opts;
		Array exclude;
		exclude.append(get_rid());
		surface_opts["exclude"] = exclude;
		Dictionary surface_result = sim->query_surface(get_global_position(), p_position, surface_opts);
		if (surface_result.has("surface")) {
			hit_data["surface"] = surface_result["surface"];
		}
		if (surface_result.has("material_name")) {
			hit_data["material_name"] = surface_result["material_name"];
		}
		if (surface_result.has("impact_uv")) {
			hit_data["impact_uv"] = surface_result["impact_uv"];
		}
	}

	emit_signal(SNAME("hit"), hit_data);

	// Forward to receiver-side hurtboxes so health systems see the hit
	// without extra wiring. The collider may itself be the hurtbox, or carry
	// one as a child.
	Hurtbox3D *hurtbox = Object::cast_to<Hurtbox3D>(p_collider);
	if (hurtbox == nullptr) {
		Node *collider_node = Object::cast_to<Node>(p_collider);
		if (collider_node != nullptr) {
			const Array children = collider_node->find_children("*", "Hurtbox3D", true, false);
			if (!children.is_empty()) {
				hurtbox = Object::cast_to<Hurtbox3D>(children[0]);
			}
		}
	}
	if (hurtbox != nullptr) {
		hurtbox->apply_hit(this, hit_data);
	}

	if (bounce_count < max_bounces) {
		_bounce(p_normal);
		return;
	}
	_destroy();
}

void Projectile3D::_bounce(const Vector3 &p_normal) {
	velocity = velocity.bounce(p_normal) * bounce_factor;
	bounce_count++;
}

void Projectile3D::_destroy() {
	flying = false;
	emit_signal(SNAME("expired"));
	if (is_inside_tree()) {
		queue_free();
	}
}

void Projectile3D::fire(const Vector3 &p_origin, const Vector3 &p_direction) {
	set_global_position(p_origin);
	direction = p_direction.is_normalized() ? p_direction : p_direction.normalized();
	velocity = direction * speed;
	lifetime = 0.0f;
	distance_traveled = 0.0f;
	bounce_count = 0;
	flying = true;
}

void Projectile3D::stop() {
	flying = false;
}

void Projectile3D::set_velocity(const Vector3 &p_velocity) {
	velocity = p_velocity;
}

Vector3 Projectile3D::get_velocity() const {
	return velocity;
}

void Projectile3D::set_direction(const Vector3 &p_direction) {
	direction = p_direction;
}

Vector3 Projectile3D::get_direction() const {
	return direction;
}

void Projectile3D::set_speed(float p_speed) {
	speed = p_speed;
}

float Projectile3D::get_speed() const {
	return speed;
}

void Projectile3D::set_gravity_scale(float p_gravity_scale) {
	gravity_scale = p_gravity_scale;
}

float Projectile3D::get_gravity_scale() const {
	return gravity_scale;
}

void Projectile3D::set_max_lifetime(float p_max_lifetime) {
	max_lifetime = p_max_lifetime;
}

float Projectile3D::get_max_lifetime() const {
	return max_lifetime;
}

void Projectile3D::set_max_range(float p_max_range) {
	max_range = p_max_range;
}

float Projectile3D::get_max_range() const {
	return max_range;
}

void Projectile3D::set_max_bounces(int p_max_bounces) {
	max_bounces = p_max_bounces;
}

int Projectile3D::get_max_bounces() const {
	return max_bounces;
}

void Projectile3D::set_bounce_factor(float p_bounce_factor) {
	bounce_factor = p_bounce_factor;
}

float Projectile3D::get_bounce_factor() const {
	return bounce_factor;
}

void Projectile3D::set_homing(bool p_homing) {
	homing = p_homing;
}

bool Projectile3D::is_homing() const {
	return homing;
}

void Projectile3D::set_homing_strength(float p_homing_strength) {
	homing_strength = p_homing_strength;
}

float Projectile3D::get_homing_strength() const {
	return homing_strength;
}

void Projectile3D::set_target(Node3D *p_target) {
	target = p_target;
}

Node3D *Projectile3D::get_target() const {
	return target;
}

void Projectile3D::set_damage(float p_damage) {
	damage = p_damage;
}

float Projectile3D::get_damage() const {
	return damage;
}

void Projectile3D::set_knockback(float p_knockback) {
	knockback = p_knockback;
}

float Projectile3D::get_knockback() const {
	return knockback;
}

void Projectile3D::set_damage_types(const Array &p_damage_types) {
	damage_types = p_damage_types;
}

Array Projectile3D::get_damage_types() const {
	return damage_types;
}

void Projectile3D::set_element(const StringName &p_element) {
	element = p_element;
}

StringName Projectile3D::get_element() const {
	return element;
}

void Projectile3D::set_source(Object *p_source) {
	source = p_source;
}

Object *Projectile3D::get_source() const {
	return source;
}

bool Projectile3D::is_flying() const {
	return flying;
}

float Projectile3D::get_lifetime() const {
	return lifetime;
}

float Projectile3D::get_distance_traveled() const {
	return distance_traveled;
}

Projectile3D::Projectile3D() {
	// Active mover: scans for targets, is not itself a target.
	set_monitoring(true);
	set_monitorable(false);
	set_physics_process(true);
}

Projectile3D::~Projectile3D() {
	// sweep_cast is a child node; freed with this node.
}
