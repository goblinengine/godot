/**************************************************************************/
/*  hitbox_3d.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "hitbox_3d.h"

#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "core/object/object_id.h"
#include "scene/scene_string_names.h"

#include "combat_utils.h"
#include "hurtbox_3d.h"

void Hitbox3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("build_hit_data", "collider", "position", "normal", "velocity"),
			&Hitbox3D::build_hit_data, DEFVAL(Variant()), DEFVAL(Vector3()), DEFVAL(Vector3()), DEFVAL(Vector3()));
	ClassDB::bind_method(D_METHOD("register_hit", "hurtbox", "hit_data"), &Hitbox3D::register_hit);
	ClassDB::bind_method(D_METHOD("reset"), &Hitbox3D::reset);
	ClassDB::bind_method(D_METHOD("set_damage", "damage"), &Hitbox3D::set_damage);
	ClassDB::bind_method(D_METHOD("get_damage"), &Hitbox3D::get_damage);
	ClassDB::bind_method(D_METHOD("set_knockback", "knockback"), &Hitbox3D::set_knockback);
	ClassDB::bind_method(D_METHOD("get_knockback"), &Hitbox3D::get_knockback);
	ClassDB::bind_method(D_METHOD("set_damage_types", "damage_types"), &Hitbox3D::set_damage_types);
	ClassDB::bind_method(D_METHOD("get_damage_types"), &Hitbox3D::get_damage_types);
	ClassDB::bind_method(D_METHOD("set_element", "element"), &Hitbox3D::set_element);
	ClassDB::bind_method(D_METHOD("get_element"), &Hitbox3D::get_element);
	ClassDB::bind_method(D_METHOD("set_source", "source"), &Hitbox3D::set_source);
	ClassDB::bind_method(D_METHOD("get_source"), &Hitbox3D::get_source);
	ClassDB::bind_method(D_METHOD("set_active", "active"), &Hitbox3D::set_active);
	ClassDB::bind_method(D_METHOD("is_active"), &Hitbox3D::is_active);

	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damage"), "set_damage", "get_damage");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "knockback"), "set_knockback", "get_knockback");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "damage_types"), "set_damage_types", "get_damage_types");
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "element"), "set_element", "get_element");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "source", PROPERTY_HINT_RESOURCE_TYPE, "Object"),
			"set_source", "get_source");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "active"), "set_active", "is_active");

	ADD_SIGNAL(MethodInfo("hit",
			PropertyInfo(Variant::OBJECT, "hurtbox", PROPERTY_HINT_RESOURCE_TYPE, "Hurtbox3D"),
			PropertyInfo(Variant::DICTIONARY, "hit_data")));
}

void Hitbox3D::_on_area_entered(Area3D *p_area) {
	if (!active) {
		return;
	}
	Hurtbox3D *hurtbox = Object::cast_to<Hurtbox3D>(p_area);
	if (hurtbox == nullptr) {
		return;
	}
	const ObjectID hurtbox_id = hurtbox->get_instance_id();
	if (hit_targets.has(hurtbox_id)) {
		return;
	}
	hit_targets.insert(hurtbox_id);
	register_hit(hurtbox, build_hit_data(hurtbox, hurtbox->get_global_position()));
}

Dictionary Hitbox3D::build_hit_data(Object *p_collider, const Vector3 &p_position,
		const Vector3 &p_normal, const Vector3 &p_velocity) const {
	return CombatUtils::build_hit_data(damage, knockback, damage_types, element,
			source, p_position, p_normal, p_collider, p_velocity);
}

void Hitbox3D::register_hit(Hurtbox3D *p_hurtbox, const Dictionary &p_hit_data) {
	if (p_hurtbox == nullptr || !active) {
		return;
	}
	p_hurtbox->apply_hit(this, p_hit_data);
	emit_signal(SNAME("hit"), p_hurtbox, p_hit_data);
}

void Hitbox3D::reset() {
	hit_targets.clear();
}

void Hitbox3D::set_damage(float p_damage) {
	damage = p_damage;
}

float Hitbox3D::get_damage() const {
	return damage;
}

void Hitbox3D::set_knockback(float p_knockback) {
	knockback = p_knockback;
}

float Hitbox3D::get_knockback() const {
	return knockback;
}

void Hitbox3D::set_damage_types(const Array &p_damage_types) {
	damage_types = p_damage_types;
}

Array Hitbox3D::get_damage_types() const {
	return damage_types;
}

void Hitbox3D::set_element(const StringName &p_element) {
	element = p_element;
}

StringName Hitbox3D::get_element() const {
	return element;
}

void Hitbox3D::set_source(Object *p_source) {
	source = p_source;
}

Object *Hitbox3D::get_source() const {
	return source;
}

void Hitbox3D::set_active(bool p_active) {
	active = p_active;
}

bool Hitbox3D::is_active() const {
	return active;
}

Hitbox3D::Hitbox3D() {
	// Active detector: scans for hurtboxes, is not itself a target.
	set_monitoring(true);
	set_monitorable(false);
	connect(SceneStringName(area_entered), callable_mp(this, &Hitbox3D::_on_area_entered));
}
