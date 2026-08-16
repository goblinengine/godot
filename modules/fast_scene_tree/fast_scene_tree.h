/**************************************************************************/
/*  fast_scene_tree.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/*                        https://goblin-engine.org                       */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

// FastSceneTree: a fork-owned re-implementation of the SceneTree contract
// (fast-scene-tree-rfc.md / fast-scene-tree-plan.md, backlog M-14).
//
// Same public API, same signals, same semantics as SceneTree — new internals.
// Selected per-project via `application/run/main_loop_type` (the fork default
// when this module is present; editor keeps the base SceneTree).
//
// The engine compiles nodes against `BaseSceneTree` (scene/main/base_scene_tree.h),
// so every call site resolves through virtual dispatch — no node swaps needed.

#include "core/object/message_queue.h"
#include "core/os/thread_safe.h"
#include "core/templates/paged_allocator.h"
#include "core/templates/self_list.h"
#include "scene/main/base_scene_tree.h"
#include "scene/main/scene_tree_fti.h"

class ArrayMesh;
class InputEvent;
class Material;
class MultiplayerAPI;
class Node;
class Node3D;
class PackedScene;
class Tween;
class Viewport;
class Window;

class FastSceneTree : public BaseSceneTree {
	_THREAD_SAFE_CLASS_

	GDCLASS(FastSceneTree, BaseSceneTree);

public:
	enum {
		// Keep in sync with CanvasItem and Node3D.
		NOTIFICATION_TRANSFORM_CHANGED = 2000
	};

private:
	CallQueue::Allocator *process_group_call_queue_allocator = nullptr;

	struct ProcessGroupSort {
		_FORCE_INLINE_ bool operator()(const ProcessGroup *p_left, const ProcessGroup *p_right) const;
	};

	// Process-priority comparators for the process group node lists (the
	// Node-private ones are unreachable from a non-friend class).
	struct PrioritySort {
		_FORCE_INLINE_ bool operator()(const Node *p_a, const Node *p_b) const { return compare_node_with_priority(p_a, p_b); }
	};

	struct PhysicsPrioritySort {
		_FORCE_INLINE_ bool operator()(const Node *p_a, const Node *p_b) const { return compare_node_with_physics_priority(p_a, p_b); }
	};

	PagedAllocator<ProcessGroup, true> group_allocator; // Allocate groups on pages, to enhance cache usage.

	LocalVector<ProcessGroup *> process_groups;
	LocalVector<ProcessGroup *> local_process_group_cache; // Used when processing to group what needs to
	uint64_t process_last_pass = 1;

	bool node_threading_disabled = false;

#ifndef _3D_DISABLED
	struct ClientPhysicsInterpolation {
		SelfList<Node3D>::List _node_3d_list;
		void physics_process();
	} _client_physics_interpolation;
#endif

	Window *root = nullptr;

	double physics_process_time = 0.0;
	double process_time = 0.0;
	bool accept_quit = true;
	bool quit_on_go_back = true;

#ifdef DEBUG_ENABLED
	bool debug_collisions_hint = false;
	bool debug_paths_hint = false;
	bool debug_navigation_hint = false;
#endif
	bool paused = false;
	bool suspended = false;

	HashMap<StringName, SceneTreeGroup> group_map;
	bool _quit = false;

	SceneTreeFTI scene_tree_fti;

	StringName tree_changed_name = "tree_changed";
	StringName node_added_name = "node_added";
	StringName node_removed_name = "node_removed";
	StringName node_renamed_name = "node_renamed";

	int64_t current_frame = 0;

#ifdef TOOLS_ENABLED
	Node *edited_scene_root = nullptr;
#endif
	struct UGCall {
		StringName group;
		StringName call;

		static uint32_t hash(const UGCall &p_val) {
			return p_val.group.hash() ^ p_val.call.hash();
		}
		bool operator==(const UGCall &p_with) const { return group == p_with.group && call == p_with.call; }
		bool operator<(const UGCall &p_with) const { return group == p_with.group ? call < p_with.call : group < p_with.group; }
	};

	// Safety for when a node is deleted while a group is being called.

	int nodes_removed_on_group_call_lock = 0;
	HashSet<Node *> nodes_removed_on_group_call; // Skip erased nodes.

	List<ObjectID> delete_queue;

	uint64_t accessibility_upd_per_sec = 0;
	bool accessibility_force_update = true;
	HashSet<ObjectID> accessibility_change_queue;
	uint64_t accessibility_last_update = 0;

	HashMap<UGCall, Vector<Variant>, UGCall> unique_group_calls;
	bool ugc_locked = false;
	void _flush_ugc();

	_FORCE_INLINE_ void _update_group_order(SceneTreeGroup &g);

	TypedArray<Node> _get_nodes_in_group(const StringName &p_group);

	Node *current_scene = nullptr;
	ObjectID prev_scene_id;
	ObjectID pending_new_scene_id;

	Color debug_collisions_color;
	Color debug_collision_contact_color;
	Color debug_paths_color;
	float debug_paths_width = 1.0f;
	Ref<ArrayMesh> debug_contact_mesh;
	Ref<Material> debug_paths_material;
	Ref<Material> collision_material;
	int collision_debug_contacts;

	void _flush_scene_change();

	List<Ref<SceneTreeTimer>> timers;
	List<Ref<Tween>> tweens;

	///network///

	Ref<MultiplayerAPI> multiplayer;
	HashMap<NodePath, Ref<MultiplayerAPI>> custom_multiplayers;
	bool multiplayer_poll = true;

	void process_timers(double p_delta, bool p_physics_frame);
	void process_tweens(double p_delta, bool p_physics_frame);

	void _process_group(ProcessGroup *p_group, bool p_physics);
	void _process_groups_thread(uint32_t p_index, bool p_physics);
	void _process(bool p_physics);

	void _call_group_flags(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	void _call_group(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	void _flush_delete_queue();

	void _main_window_focus_in();
	void _main_window_close();
	void _main_window_go_back();

	// BaseSceneTree private seam (friend-accessed by nodes/viewports).
	void tree_changed() override;
	void node_added(Node *p_node) override;
	void node_removed(Node *p_node) override;
	void node_renamed(Node *p_node) override;

	SceneTreeGroup *add_to_group(const StringName &p_group, Node *p_node) override;
	void remove_from_group(const StringName &p_group, Node *p_node) override;

	void _remove_process_group(Node *p_node) override;
	void _add_process_group(Node *p_node) override;
	void _remove_node_from_process_group(Node *p_node, Node *p_owner) override;
	void _add_node_to_process_group(Node *p_node, Node *p_owner) override;

	void _call_input_pause(const StringName &p_group, CallInputType p_call_type, const Ref<InputEvent> &p_input, Viewport *p_viewport) override;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	RequiredResult<Window> get_root() const override;

	void call_group_flagsp(uint32_t p_call_flags, const StringName &p_group, const StringName &p_function, const Variant **p_args, int p_argcount) override;
	void notify_group_flags(uint32_t p_call_flags, const StringName &p_group, int p_notification) override;
	void set_group_flags(uint32_t p_call_flags, const StringName &p_group, const String &p_name, const Variant &p_value) override;

	// `notify_group()` is immediate by default since Godot 4.0.
	void notify_group(const StringName &p_group, int p_notification) override;
	// `set_group()` is immediate by default since Godot 4.0.
	void set_group(const StringName &p_group, const String &p_name, const Variant &p_value) override;

	void flush_transform_notifications() override;

	bool is_accessibility_enabled() const override;
	bool is_accessibility_supported() const override;
	void _accessibility_force_update() override;
	void _accessibility_notify_change(const Node *p_node, bool p_remove = false) override;
	void _flush_accessibility_changes() override;
	void _process_accessibility_changes(int p_window_id) override; // Effectively DisplayServerEnums::WindowID

	virtual void initialize() override;

	virtual void iteration_prepare() override;

	virtual bool physics_process(double p_time) override;
	virtual void iteration_end() override;
	virtual bool process(double p_time) override;

	virtual void finalize() override;

	bool is_auto_accept_quit() const override;
	void set_auto_accept_quit(bool p_enable) override;

	bool is_quit_on_go_back() const override;
	void set_quit_on_go_back(bool p_enable) override;

	void quit(int p_exit_code = EXIT_SUCCESS) override;

	double get_physics_process_time() const override { return physics_process_time; }
	double get_process_time() const override { return process_time; }

	void set_pause(bool p_enabled) override;
	bool is_paused() const override;
	void set_suspend(bool p_enabled) override;
	bool is_suspended() const override;

#ifdef DEBUG_ENABLED
	void set_debug_collisions_hint(bool p_enabled) override;
	bool is_debugging_collisions_hint() const override;

	void set_debug_paths_hint(bool p_enabled) override;
	bool is_debugging_paths_hint() const override;

	void set_debug_navigation_hint(bool p_enabled) override;
	bool is_debugging_navigation_hint() const override;
#endif

	void set_debug_collisions_color(const Color &p_color) override;
	Color get_debug_collisions_color() const override;

	void set_debug_collision_contact_color(const Color &p_color) override;
	Color get_debug_collision_contact_color() const override;

	void set_debug_paths_color(const Color &p_color) override;
	Color get_debug_paths_color() const override;

	void set_debug_paths_width(float p_width) override;
	float get_debug_paths_width() const override;

	Ref<Material> get_debug_paths_material() override;
	Ref<Material> get_debug_collision_material() override;
	Ref<ArrayMesh> get_debug_contact_mesh() override;

	int get_collision_debug_contact_count() override { return collision_debug_contacts; }

	int64_t get_frame() const override;

	int get_node_count() const override;

	void queue_delete(RequiredParam<Object> rp_object) override;

	Vector<Node *> get_nodes_in_group(const StringName &p_group) override;
	Node *get_first_node_in_group(const StringName &p_group) override;
	bool has_group(const StringName &p_identifier) const override;
	int get_node_count_in_group(const StringName &p_group) const override;

	void set_edited_scene_root(Node *p_node) override;
	Node *get_edited_scene_root() const override;

	void set_current_scene(Node *p_scene) override;
	Node *get_current_scene() const override;
	Error change_scene_to_file(const String &p_path) override;
	Error change_scene_to_packed(RequiredParam<PackedScene> rp_scene) override;
	Error change_scene_to_node(RequiredParam<Node> rp_node) override;
	Error reload_current_scene() override;
	void unload_current_scene() override;

	RequiredResult<SceneTreeTimer> create_timer(double p_delay_sec, bool p_process_always = true, bool p_process_in_physics = false, bool p_ignore_time_scale = false) override;
	RequiredResult<Tween> create_tween() override;
	void remove_tween(const Ref<Tween> &p_tween) override;
	TypedArray<Tween> get_processed_tweens() override;

	//used by Main::start, don't use otherwise
	void add_current_scene(Node *p_current) override;

	//network API

	RequiredResult<MultiplayerAPI> get_multiplayer(const NodePath &p_for_path = NodePath()) const override;
	void set_multiplayer(Ref<MultiplayerAPI> p_multiplayer, const NodePath &p_root_path = NodePath()) override;
	void set_multiplayer_poll_enabled(bool p_enabled) override;
	bool is_multiplayer_poll_enabled() const override;

	void set_disable_node_threading(bool p_disable) override;
	//default texture settings

	void set_physics_interpolation_enabled(bool p_enabled) override;
	bool is_physics_interpolation_enabled() const override { return _physics_interpolation_enabled; }

#ifndef _3D_DISABLED
	void client_physics_interpolation_add_node_3d(SelfList<Node3D> *p_elem) override;
	void client_physics_interpolation_remove_node_3d(SelfList<Node3D> *p_elem) override;
#endif

	SceneTreeFTI &get_scene_tree_fti() override { return scene_tree_fti; }

	// The running FastSceneTree, if the main loop is one (mirrors
	// SceneTree::get_singleton()); null otherwise.
	static FastSceneTree *get_singleton() { return singleton; }

	FastSceneTree();
	~FastSceneTree();

private:
	static FastSceneTree *singleton;
};

