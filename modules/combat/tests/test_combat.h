/**************************************************************************/
/*  test_combat.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "core/object/callable_mp.h"
#include "tests/test_macros.h"

#include "modules/combat/combat_utils.h"
#include "modules/combat/hitbox_3d.h"
#include "modules/combat/hurtbox_3d.h"
#include "modules/combat/projectile_3d.h"

namespace CombatTest {

// Minimal signal recorder: counts emissions and captures the last hit data.
class SignalRecorder : public Object {
	GDCLASS(SignalRecorder, Object);

public:
	int count = 0;
	Dictionary last_data;
	Object *last_object = nullptr;

	void on_hit(Object *p_object, const Dictionary &p_data) {
		count++;
		last_object = p_object;
		last_data = p_data;
	}

	void on_data(const Dictionary &p_data) {
		count++;
		last_data = p_data;
	}

	void on_expired() {
		count++;
	}

	SignalRecorder() {
		count = 0;
	}
};

TEST_CASE("[SceneTree][Combat] Hitbox3D defaults") {
	Hitbox3D hitbox;
	CHECK(hitbox.get_damage() == 0.0f);
	CHECK(hitbox.get_knockback() == 0.0f);
	CHECK(hitbox.get_damage_types().is_empty());
	CHECK(hitbox.get_element() == StringName());
	CHECK(hitbox.get_source() == nullptr);
	CHECK(hitbox.is_active());
	CHECK(hitbox.is_monitoring());
	CHECK_FALSE(hitbox.is_monitorable());
}

TEST_CASE("[SceneTree][Combat] Hitbox3D attack data") {
	Hitbox3D hitbox;
	hitbox.set_damage(25.0f);
	hitbox.set_knockback(3.5f);
	Array types;
	types.push_back(StringName("fire"));
	types.push_back(StringName("pierce"));
	hitbox.set_damage_types(types);
	hitbox.set_element(StringName("fire"));
	hitbox.set_active(false);

	CHECK(hitbox.get_damage() == 25.0f);
	CHECK(hitbox.get_knockback() == 3.5f);
	CHECK(hitbox.get_damage_types() == types);
	CHECK(hitbox.get_element() == StringName("fire"));
	CHECK_FALSE(hitbox.is_active());

	hitbox.set_active(true);
	CHECK(hitbox.is_active());
}

TEST_CASE("[SceneTree][Combat] Hurtbox3D defaults") {
	Hurtbox3D hurtbox;
	CHECK(hurtbox.is_active());
	CHECK_FALSE(hurtbox.is_monitoring());
	CHECK(hurtbox.is_monitorable());
}

TEST_CASE("[SceneTree][Combat] Hitbox3D registers hit on Hurtbox3D") {
	Hitbox3D hitbox;
	Hurtbox3D hurtbox;
	hitbox.set_damage(25.0f);
	hitbox.set_source(&hitbox); // The hitbox itself is the attacker carrier.

	SignalRecorder hit_recorder;
	SignalRecorder hurt_recorder;
	hitbox.connect("hit", callable_mp(&hit_recorder, &SignalRecorder::on_hit));
	hurtbox.connect("hurt", callable_mp(&hurt_recorder, &SignalRecorder::on_hit));

	Dictionary hit_data = hitbox.build_hit_data(&hurtbox);
	hitbox.register_hit(&hurtbox, hit_data);

	CHECK(hit_recorder.count == 1);
	CHECK(hurt_recorder.count == 1);
	CHECK(hit_recorder.last_object == &hurtbox);
	CHECK(float(hit_recorder.last_data[CombatUtils::KEY_DAMAGE]) == 25.0f);
	CHECK(Object::cast_to<Hurtbox3D>(hit_recorder.last_data[CombatUtils::KEY_COLLIDER]) == &hurtbox);
	CHECK(Object::cast_to<Hitbox3D>(hit_recorder.last_data[CombatUtils::KEY_SOURCE]) == &hitbox);
}

TEST_CASE("[SceneTree][Combat] Hitbox3D overlap dedups per activation, reset re-arms") {
	SceneTree *tree = SceneTree::get_singleton();
	Hitbox3D *hitbox = memnew(Hitbox3D);
	Hurtbox3D *hurtbox = memnew(Hurtbox3D);
	tree->get_root()->add_child(hitbox);
	tree->get_root()->add_child(hurtbox);
	hitbox->set_damage(10.0f);

	SignalRecorder recorder;
	hitbox->connect("hit", callable_mp(&recorder, &SignalRecorder::on_hit));

	// Simulate the physics overlap callback twice: second call must be ignored.
	hitbox->_on_area_entered(hurtbox);
	hitbox->_on_area_entered(hurtbox);
	CHECK(recorder.count == 1);

	// reset() re-arms the hitbox for a new swing.
	hitbox->reset();
	hitbox->_on_area_entered(hurtbox);
	CHECK(recorder.count == 2);

	// Inactive hitbox ignores overlaps entirely.
	hitbox->set_active(false);
	hitbox->_on_area_entered(hurtbox);
	CHECK(recorder.count == 2);

	tree->get_root()->remove_child(hurtbox);
	tree->get_root()->remove_child(hitbox);
	memdelete(hurtbox);
	memdelete(hitbox);
}

TEST_CASE("[SceneTree][Combat] Hurtbox3D inactive ignores hits") {
	Hurtbox3D hurtbox;
	hurtbox.set_active(false);

	SignalRecorder recorder;
	hurtbox.connect("hurt", callable_mp(&recorder, &SignalRecorder::on_hit));

	Hitbox3D hitbox;
	hitbox.set_damage(5.0f);
	Dictionary hit_data = hitbox.build_hit_data(&hurtbox);
	hurtbox.apply_hit(&hitbox, hit_data);
	CHECK(recorder.count == 0);

	hurtbox.set_active(true);
	hurtbox.apply_hit(&hitbox, hit_data);
	CHECK(recorder.count == 1);
}

TEST_CASE("[SceneTree][Combat] Projectile3D defaults") {
	Projectile3D projectile;
	CHECK(projectile.get_speed() == 20.0f);
	CHECK(projectile.get_gravity_scale() == 0.0f);
	CHECK(projectile.get_max_lifetime() == 0.0f);
	CHECK(projectile.get_max_range() == 0.0f);
	CHECK(projectile.get_max_bounces() == 0);
	CHECK_FALSE(projectile.is_homing());
	CHECK(projectile.get_target() == nullptr);
	CHECK(projectile.is_monitoring());
	CHECK_FALSE(projectile.is_monitorable());
}

TEST_CASE("[SceneTree][Combat] Projectile3D fire and motion") {
	SceneTree *tree = SceneTree::get_singleton();
	Projectile3D *projectile = memnew(Projectile3D);
	tree->get_root()->add_child(projectile);
	projectile->set_speed(10.0f);
	projectile->fire(Vector3(), Vector3(1, 0, 0));

	CHECK(projectile->is_flying());
	CHECK(projectile->get_velocity() == Vector3(10, 0, 0));
	CHECK(projectile->get_direction() == Vector3(1, 0, 0));
	CHECK(projectile->get_lifetime() == 0.0f);

	// Drive motion directly (in tree, but no sweep shape set): position
	// advances by velocity * delta and distance accumulates.
	projectile->set_global_position(Vector3());
	projectile->_physics_process(0.5);
	CHECK(projectile->get_global_position() == Vector3(5, 0, 0));
	CHECK(projectile->get_distance_traveled() == 5.0f);
	CHECK(projectile->get_lifetime() == 0.5f);

	// Gravity pulls an arcing projectile down.
	projectile->set_gravity_scale(1.0f);
	projectile->_physics_process(0.5);
	CHECK(projectile->get_global_position().x == 10.0f);
	CHECK(projectile->get_global_position().y < 0.0f);

	tree->get_root()->remove_child(projectile);
	memdelete(projectile);
}

TEST_CASE("[SceneTree][Combat] Projectile3D bounce math") {
	Projectile3D projectile;
	projectile.set_speed(10.0f);
	projectile.set_max_bounces(2);
	projectile.set_bounce_factor(0.5f);
	projectile.fire(Vector3(), Vector3(1, 0, 0));

	// Reflect off a wall facing -X.
	projectile.set_velocity(Vector3(10, 0, 0));
	Vector3 wall_normal = Vector3(-1, 0, 0);
	projectile._bounce(wall_normal);
	CHECK(projectile.get_velocity() == Vector3(-5, 0, 0));
	CHECK(projectile.get_max_bounces() == 2);
}

TEST_CASE("[SceneTree][Combat] Projectile3D lifetime expiry") {
	SceneTree *tree = SceneTree::get_singleton();
	Projectile3D *projectile = memnew(Projectile3D);
	tree->get_root()->add_child(projectile);
	SignalRecorder recorder;
	projectile->connect("expired", callable_mp(&recorder, &SignalRecorder::on_expired));

	projectile->set_max_lifetime(1.0f);
	projectile->fire(Vector3(), Vector3(0, 0, -1));

	// Simulate lifetime accumulation: _physics_process destroys at expiry.
	// Directly verify the state transition by advancing time in small steps.
	for (int i = 0; i < 12; i++) {
		projectile->_physics_process(0.1);
		if (!projectile->is_flying()) {
			break;
		}
	}
	CHECK_FALSE(projectile->is_flying());
	CHECK(recorder.count == 1);

	tree->get_root()->remove_child(projectile);
	memdelete(projectile);
}

TEST_CASE("[SceneTree][Combat] Projectile3D hit data contract") {
	Projectile3D projectile;
	projectile.set_damage(30.0f);
	projectile.set_knockback(2.0f);
	projectile.set_element(StringName("fire"));

	Dictionary hit_data = CombatUtils::build_hit_data(
			projectile.get_damage(), projectile.get_knockback(), projectile.get_damage_types(),
			projectile.get_element(), &projectile, Vector3(1, 2, 3), Vector3(0, 1, 0), nullptr,
			projectile.get_velocity());

	CHECK(float(hit_data[CombatUtils::KEY_DAMAGE]) == 30.0f);
	CHECK(float(hit_data[CombatUtils::KEY_KNOCKBACK]) == 2.0f);
	CHECK(StringName(hit_data[CombatUtils::KEY_ELEMENT]) == StringName("fire"));
	CHECK(hit_data[CombatUtils::KEY_POSITION] == Vector3(1, 2, 3));
	CHECK(hit_data[CombatUtils::KEY_NORMAL] == Vector3(0, 1, 0));
	CHECK(hit_data.has(CombatUtils::KEY_COLLIDER));
	CHECK(hit_data.has(CombatUtils::KEY_SOURCE));
	CHECK(hit_data.has(CombatUtils::KEY_VELOCITY));
}

} // namespace CombatTest
