/**************************************************************************/
/*  test_fast_scene_tree.h                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/*                        https://goblin-engine.org                       */
/**************************************************************************/

#pragma once

#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "scene/main/scene_tree.h"
#include "tests/test_macros.h"

#include "modules/fast_scene_tree/fast_scene_tree.h"

namespace FastSceneTreeTest {

// P1 contract matrix (fast-scene-tree-plan.md): every scenario runs against
// BOTH the base SceneTree (reference behavior pin) and FastSceneTree. The
// matrix IS the executable spec for the re-implementation.

class Counter : public Node {
	GDCLASS(Counter, Node);

public:
	int process_count = 0;
	int physics_process_count = 0;

	void _notification(int p_what) {
		switch (p_what) {
			case NOTIFICATION_PROCESS:
				process_count++;
				break;
			case NOTIFICATION_PHYSICS_PROCESS:
				physics_process_count++;
				break;
		}
	}
};

class SignalRecorder : public Object {
	GDCLASS(SignalRecorder, Object);

public:
	int count = 0;
	Object *last_object = nullptr;

	void on_node(Object *p_node) {
		count++;
		last_object = p_node;
	}

	void on_timeout() {
		count++;
	}

	void on_no_args() {
		count++;
	}
};

class GroupCallTarget : public Node {
	GDCLASS(GroupCallTarget, Node);

protected:
	// Group calls dispatch through `callp` -> ClassDB lookup, so the target
	// methods must be bound.
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("on_self"), &GroupCallTarget::on_self);
		ClassDB::bind_method(D_METHOD("on_call", "arg"), &GroupCallTarget::on_call);
	}

public:
	String calls;
	static String call_sequence; // Records per-node identity in call order.

	void on_self() {
		calls += get_name();
		call_sequence += get_name();
	}

	void on_call(const String &p_arg) {
		calls += p_arg;
	}
};

String GroupCallTarget::call_sequence;

static void scenario_group_call_order(BaseSceneTree *p_tree) {
	GroupCallTarget *a = memnew(GroupCallTarget);
	GroupCallTarget *b = memnew(GroupCallTarget);
	GroupCallTarget *c = memnew(GroupCallTarget);
	a->set_name("A");
	b->set_name("B");
	c->set_name("C");
	p_tree->get_root()->add_child(a);
	p_tree->get_root()->add_child(b);
	p_tree->get_root()->add_child(c);

	a->add_to_group("g");
	b->add_to_group("g");
	c->add_to_group("g");

	// Insertion order.
	p_tree->call_group(SNAME("g"), SNAME("on_call"), String("1"));
	p_tree->call_group(SNAME("g"), SNAME("on_call"), String("2"));
	CHECK(a->calls == "12");
	CHECK(b->calls == "12");
	CHECK(c->calls == "12");

	// Forward iteration order: A, B, C.
	GroupCallTarget::call_sequence = "";
	p_tree->call_group(SNAME("g"), SNAME("on_self"));
	CHECK(GroupCallTarget::call_sequence == "ABC");

	// Reverse iteration order: C, B, A.
	GroupCallTarget::call_sequence = "";
	p_tree->call_group_flags(BaseSceneTree::GROUP_CALL_REVERSE, SNAME("g"), SNAME("on_self"));
	CHECK(GroupCallTarget::call_sequence == "CBA");
}

static void scenario_group_unique_deferred(BaseSceneTree *p_tree) {
	GroupCallTarget *a = memnew(GroupCallTarget);
	a->set_name("A");
	p_tree->get_root()->add_child(a);
	a->add_to_group("g");

	// Unique + deferred: repeated identical calls collapse into one per flush.
	p_tree->call_group_flags(BaseSceneTree::GROUP_CALL_UNIQUE | BaseSceneTree::GROUP_CALL_DEFERRED, SNAME("g"), SNAME("on_self"));
	p_tree->call_group_flags(BaseSceneTree::GROUP_CALL_UNIQUE | BaseSceneTree::GROUP_CALL_DEFERRED, SNAME("g"), SNAME("on_self"));
	CHECK(a->calls.is_empty()); // Not yet flushed.
	p_tree->call_group_flags(BaseSceneTree::GROUP_CALL_UNIQUE | BaseSceneTree::GROUP_CALL_DEFERRED, SNAME("g"), SNAME("on_self"));
	p_tree->process(0.0);
	CHECK(a->calls == "A"); // Exactly one call after flush.

	// A different call in the same group is independent.
	p_tree->call_group_flags(BaseSceneTree::GROUP_CALL_UNIQUE | BaseSceneTree::GROUP_CALL_DEFERRED, SNAME("g"), SNAME("on_call"), String("z"));
	p_tree->process(0.0);
	CHECK(a->calls == "Az");
}

static void scenario_timer_modes(BaseSceneTree *p_tree) {
	SignalRecorder *rec = memnew(SignalRecorder);
	Ref<SceneTreeTimer> idle_timer = p_tree->create_timer(0.1);
	Ref<SceneTreeTimer> physics_timer = p_tree->create_timer(0.1, true, true);

	idle_timer->connect(SNAME("timeout"), callable_mp(rec, &SignalRecorder::on_timeout));
	physics_timer->connect(SNAME("timeout"), callable_mp(rec, &SignalRecorder::on_timeout));

	p_tree->process(0.2);
	CHECK(rec->count == 1); // Idle timer fired; physics timer did not (idle frame).

	// Time left of the physics timer was untouched by the idle frame.
	CHECK_MESSAGE(physics_timer->get_time_left() > 0.05, "Physics timer must not tick on idle frames.");

	p_tree->physics_process(0.2);
	CHECK(rec->count == 2); // Physics timer fired on the physics frame.
}

static void scenario_pause_modes(BaseSceneTree *p_tree) {
	Counter *pausable = memnew(Counter);
	pausable->set_process_mode(Node::PROCESS_MODE_PAUSABLE);
	pausable->set_process(true);
	pausable->set_physics_process(true);
	p_tree->get_root()->add_child(pausable);

	Counter *always = memnew(Counter);
	always->set_process_mode(Node::PROCESS_MODE_ALWAYS);
	always->set_process(true);
	p_tree->get_root()->add_child(always);

	p_tree->process(0.1);
	p_tree->process(0.1);
	CHECK(pausable->process_count == 2);
	CHECK(always->process_count == 2);

	p_tree->set_pause(true);
	p_tree->process(0.1);
	p_tree->process(0.1);
	CHECK_MESSAGE(pausable->process_count == 2, "Pausable nodes are skipped while paused.");
	CHECK_MESSAGE(always->process_count == 4, "PROCESS_MODE_ALWAYS nodes keep running while paused.");
	CHECK_MESSAGE(pausable->physics_process_count == 0, "Physics processing skipped while paused.");

	p_tree->set_pause(false);
	p_tree->process(0.1);
	CHECK(pausable->process_count == 3);
	CHECK(always->process_count == 5);
}

static void scenario_scene_change(BaseSceneTree *p_tree) {
	SignalRecorder *rec = memnew(SignalRecorder);
	p_tree->connect(SNAME("scene_changed"), callable_mp(rec, &SignalRecorder::on_no_args));

	Node *old_scene = memnew(Node);
	old_scene->set_name("OldScene");
	p_tree->add_current_scene(old_scene);
	CHECK(p_tree->get_current_scene() == old_scene);
	CHECK(old_scene->is_inside_tree());

	Node *new_scene = memnew(Node);
	new_scene->set_name("NewScene");
	Error err = p_tree->change_scene_to_node(new_scene);
	CHECK(err == OK);

	// Change is pending until the next process().
	CHECK_MESSAGE(!new_scene->is_inside_tree(), "Pending scene is not in the tree yet.");
	CHECK_MESSAGE(p_tree->get_current_scene() == nullptr, "Current scene is cleared when a change is requested.");
	ObjectID old_scene_id = old_scene->get_instance_id(); // Capture before the flush frees it.

	p_tree->process(0.0);

	CHECK(rec->count == 1); // scene_changed emitted.
	CHECK(p_tree->get_current_scene() == new_scene);
	CHECK(new_scene->is_inside_tree());
	CHECK_MESSAGE(!ObjectDB::get_instance(old_scene_id), "Old scene freed on flush.");
}

static void scenario_node_signals(BaseSceneTree *p_tree) {
	SignalRecorder *rec = memnew(SignalRecorder);
	p_tree->connect(SNAME("node_added"), callable_mp(rec, &SignalRecorder::on_node));
	p_tree->connect(SNAME("node_removed"), callable_mp(rec, &SignalRecorder::on_node));

	Node *n = memnew(Node);
	p_tree->get_root()->add_child(n);
	CHECK(rec->count == 1);
	CHECK(rec->last_object == n);

	p_tree->get_root()->remove_child(n);
	CHECK(rec->count == 2);
	CHECK(rec->last_object == n);

	memdelete(n);
}

static void scenario_queue_delete(BaseSceneTree *p_tree) {
	Node *n = memnew(Node);
	p_tree->get_root()->add_child(n);
	ObjectID id = n->get_instance_id();
	p_tree->queue_delete(n);
	CHECK_MESSAGE(ObjectDB::get_instance(id) != nullptr, "Still alive before flush.");
	p_tree->process(0.0);
	CHECK_MESSAGE(ObjectDB::get_instance(id) == nullptr, "Freed after flush.");
}

static void scenario_quit_flag(BaseSceneTree *p_tree) {
	p_tree->process(0.0);
	CHECK_MESSAGE(!p_tree->process(0.0), "No quit requested.");
	p_tree->quit();
	CHECK_MESSAGE(p_tree->process(0.0), "quit() makes the next frame return true.");
}

static void scenario_frame_and_node_count(BaseSceneTree *p_tree) {
	CHECK(p_tree->get_frame() == 0);
	// The frame counter advances on physics frames (upstream semantics).
	p_tree->physics_process(0.0);
	p_tree->physics_process(0.0);
	CHECK(p_tree->get_frame() == 2);

	int before = p_tree->get_node_count();
	Node *n = memnew(Node);
	p_tree->get_root()->add_child(n);
	CHECK(p_tree->get_node_count() == before + 1);
	memdelete(n);
	CHECK(p_tree->get_node_count() == before);
}

static void scenario_group_membership(BaseSceneTree *p_tree) {
	CHECK(!p_tree->has_group(SNAME("g")));
	Node *n = memnew(Node);
	p_tree->get_root()->add_child(n);
	n->add_to_group("g");
	CHECK(p_tree->has_group(SNAME("g")));
	CHECK(p_tree->get_node_count_in_group(SNAME("g")) == 1);
	CHECK(p_tree->get_first_node_in_group(SNAME("g")) == n);
	Vector<Node *> nodes = p_tree->get_nodes_in_group(SNAME("g"));
	CHECK(nodes.size() == 1);
	CHECK(nodes[0] == n);
	n->remove_from_group("g");
	CHECK(!p_tree->has_group(SNAME("g")));
	CHECK(p_tree->get_node_count_in_group(SNAME("g")) == 0);
}

// ---------------------------------------------------------------------------
// Test harness: a self-managed tree (created per test, driven manually).
// The doctest [SceneTree] harness tree is NOT used, so each scenario is fully
// isolated. `p_tree_class` selects which implementation runs the scenario.
// ---------------------------------------------------------------------------

template <class T>
static T *create_tree() {
	T *tree = memnew(T);
	tree->initialize();
	return tree;
}

template <class T>
static void destroy_tree(T *p_tree) {
	p_tree->finalize();
	memdelete(p_tree);
}

TEST_CASE("[SceneTree][FastSceneTree] Base SceneTree reference behavior (P1 matrix pin)") {
	SceneTree *tree = create_tree<SceneTree>();
	SUBCASE("group call order") { scenario_group_call_order(tree); }
	SUBCASE("unique deferred group calls") { scenario_group_unique_deferred(tree); }
	SUBCASE("timer modes") { scenario_timer_modes(tree); }
	SUBCASE("pause modes") { scenario_pause_modes(tree); }
	SUBCASE("scene change") { scenario_scene_change(tree); }
	SUBCASE("node signals") { scenario_node_signals(tree); }
	SUBCASE("queue delete") { scenario_queue_delete(tree); }
	SUBCASE("quit flag") { scenario_quit_flag(tree); }
	SUBCASE("frame and node count") { scenario_frame_and_node_count(tree); }
	SUBCASE("group membership") { scenario_group_membership(tree); }
	destroy_tree(tree);
}

TEST_CASE("[SceneTree][FastSceneTree] FastSceneTree contract (same scenarios as base)") {
	FastSceneTree *tree = create_tree<FastSceneTree>();
	SUBCASE("group call order") { scenario_group_call_order(tree); }
	SUBCASE("unique deferred group calls") { scenario_group_unique_deferred(tree); }
	SUBCASE("timer modes") { scenario_timer_modes(tree); }
	SUBCASE("pause modes") { scenario_pause_modes(tree); }
	SUBCASE("scene change") { scenario_scene_change(tree); }
	SUBCASE("node signals") { scenario_node_signals(tree); }
	SUBCASE("queue delete") { scenario_queue_delete(tree); }
	SUBCASE("quit flag") { scenario_quit_flag(tree); }
	SUBCASE("frame and node count") { scenario_frame_and_node_count(tree); }
	SUBCASE("group membership") { scenario_group_membership(tree); }
	destroy_tree(tree);
}

} // namespace FastSceneTreeTest
