/**************************************************************************/
/*  test_sim.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "core/object/callable_mp.h"
#include "tests/test_macros.h"

#include "modules/sim/combat_utils.h"
#include "modules/sim/hitbox_3d.h"
#include "modules/sim/hurtbox_3d.h"
#include "modules/sim/projectile_3d.h"
#include "modules/sim/sim_server.h"

namespace SimTest {

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

// =========================================================================
// S-01: SimServer clock, cadence, and stimulus bus tests
// =========================================================================

// Recorder with a callback for schedule dispatch and
// a Dictionary-arg callback for stimulus listener delivery.
class TickRecorder : public Object {
	GDCLASS(TickRecorder, Object);

public:
	int call_count = 0;
	int delivery_count = 0;
	Dictionary last_delivery;

 	void on_tick(const StringName &p_kind) {
 		call_count++;
 	}

 	void on_cadence() {
 		call_count++;
 	}

 	void on_delivery(const Dictionary &p_data) {
		delivery_count++;
		last_delivery = p_data;
	}
};

TEST_CASE("[Modules][SimServer] clock starts at tick 0") {
	CHECK(SimServer::get_singleton() != nullptr);
	CHECK(SimServer::get_singleton()->get_tick() == 0);
}

TEST_CASE("[Modules][SimServer] schedule_at_tick dispatches at target tick") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary()); // reset to tick 0

	TickRecorder recorder;
	Callable cb = callable_mp(&recorder, &TickRecorder::on_tick);

	sim->schedule_at_tick(5, "test_tick", cb, "my_tag", 0);

	CHECK(sim->get_tick() == 0);
	CHECK(sim->is_deadline_active(5)); // pending, not yet dispatched

	sim->advance_ticks(5);
	CHECK(sim->get_tick() == 5);
	CHECK(recorder.call_count == 1);
	CHECK(!sim->is_deadline_active(5)); // dispatched and removed
}

TEST_CASE("[Modules][SimServer] schedule_in_seconds converts correctly") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary());

	sim->schedule_in_seconds(1.0, "test_secs", Callable(), "t1", 0.0);

	CHECK(sim->is_deadline_active(60)); // 1.0s / (1/60s) = 60 ticks
	CHECK(sim->deadline_remaining_seconds(60) == 1.0);

	sim->advance_ticks(60);
	CHECK(!sim->is_deadline_active(60));
}

TEST_CASE("[Modules][SimServer] cancel_by_tag removes matching entries") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary());

	Callable noop;
	sim->schedule_at_tick(100, "kind_a", noop, "group1", 0);
	sim->schedule_at_tick(200, "kind_b", noop, "group1", 0);
	sim->schedule_at_tick(300, "kind_c", noop, "group2", 0);

	CHECK(sim->cancel_by_tag("group1") == 2);
	CHECK(!sim->is_deadline_active(100));
	CHECK(!sim->is_deadline_active(200));
	CHECK(sim->is_deadline_active(300));
}

TEST_CASE("[Modules][SimServer] repeat schedule re-arms") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary());

	TickRecorder recorder;
	Callable cb = callable_mp(&recorder, &TickRecorder::on_tick);

	// Repeat every 10 ticks, due at tick 10.
	sim->schedule_at_tick(10, "repeat_kind", cb, "repeat", 10);

	sim->advance_ticks(10);
	CHECK(recorder.call_count == 1);
	CHECK(sim->is_deadline_active(20)); // re-scheduled

	sim->advance_ticks(10);
	CHECK(recorder.call_count == 2);
	CHECK(sim->is_deadline_active(30));
}

TEST_CASE("[Modules][SimServer] register_cadence fires at correct rate") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary());

	TickRecorder recorder;
	Callable cb = callable_mp(&recorder, &TickRecorder::on_cadence);

	// 30 Hz = every 2 ticks at 60 Hz.
	sim->register_cadence("test_30hz", 30.0, cb);

	sim->advance_ticks(2);
	CHECK(recorder.call_count == 1);
	sim->advance_ticks(2);
	CHECK(recorder.call_count == 2);

	sim->unregister_cadence("test_30hz");
}

TEST_CASE("[Modules][SimServer] time state save/restore round-trip") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary());

	sim->advance_ticks(100);
	Dictionary state = sim->get_time_state();
	CHECK((int)state["tick"] == 100);

	sim->restore_time_state(state);
	CHECK(sim->get_tick() == 100);
	CHECK(sim->now_seconds() == 100.0 * (1.0 / 60.0));
}

TEST_CASE("[Modules][SimServer] schedule state save/restore round-trip") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary());
	sim->restore_schedule_state(Dictionary());

	Callable noop;
	sim->schedule_at_tick(50, "saved_kind", noop, "saved_tag", 0);
	CHECK(sim->is_deadline_active(50));

	Dictionary sched_state = sim->get_schedule_state();
	CHECK(Array(sched_state["entries"]).size() == 1);

	sim->restore_schedule_state(sched_state);
	CHECK(sim->is_deadline_active(50));
}

TEST_CASE("[Modules][SimServer] stimulus bus emit and query") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary());

	Dictionary payload;
	payload["source"] = "test";

	RID sid = sim->emit_stimulus("sound", Vector3(0, 0, 0), 5.0f, payload, Dictionary());
	CHECK(sid.is_valid());

	// Query at origin, radius 10, for "sound" type.
	Array types;
	types.append("sound");
	Array results = sim->query_stimulus(Vector3(0, 0, 0), 10.0f, types, 0);
	CHECK(results.size() == 1);

	Dictionary result = results[0];
	CHECK((StringName)result["type"] == "sound");
	CHECK(result["position"] == Vector3(0, 0, 0));
	CHECK(float(result["radius"]) == 5.0f);

	// Query with since_tick > emit_tick returns nothing.
	results = sim->query_stimulus(Vector3(0, 0, 0), 10.0f, types, 100);
	CHECK(results.size() == 0);
}

TEST_CASE("[Modules][SimServer] stimulus listener delivery") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary());

	TickRecorder recorder;
	Callable listener_cb = callable_mp(&recorder, &TickRecorder::on_delivery);

	AABB aoi(Vector3(-10, -10, -10), Vector3(20, 20, 20));
	RID listener_rid = sim->register_stimulus_listener(aoi, Callable(), listener_cb);
	CHECK(listener_rid.is_valid());

	// Emit stimulus inside the listener's area.
	sim->emit_stimulus("sound", Vector3(0, 0, 0), 1.0f, Dictionary(), Dictionary());
	sim->advance_ticks(1);

	CHECK(recorder.delivery_count == 1);
	CHECK((StringName)recorder.last_delivery["type"] == "sound");

	// Emit stimulus outside the listener's area.
	sim->emit_stimulus("sound", Vector3(100, 0, 0), 1.0f, Dictionary(), Dictionary());
	sim->advance_ticks(1);

	CHECK(recorder.delivery_count == 1); // still only 1 delivery

	sim->unregister_stimulus_listener(listener_rid);
}

TEST_CASE("[Modules][SimServer] stimulus log prunes by retention") {
	SimServer *sim = SimServer::get_singleton();
	sim->restore_time_state(Dictionary());

	// Retention is 10 ticks. Emit at tick 0.
	sim->emit_stimulus("sound", Vector3(0, 0, 0), 1.0f, Dictionary(), Dictionary());

	// Advance past retention window.
	sim->advance_ticks(15);

 	// Query should not find stale stimuli.
 	Array results = sim->query_stimulus(Vector3(0, 0, 0), 100.0f, Array(), 0);
 	CHECK(results.size() == 0);
 }

 } // namespace SimTest
