/**************************************************************************/
/*  goblin_about.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/
/* Copyright (c) 2014-present Goblin Engine contributors.                */
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

#include "goblin_about.h"

#include "core/config/engine.h"

#include "core/os/os.h"

#ifdef TOOLS_ENABLED
#include "core/object/object.h"
#include "editor/editor_node.h"
#include "editor/project_manager/project_manager.h"
#include "scene/gui/base_button.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/link_button.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/tab_container.h"
#include "scene/main/window.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#endif

GoblinBranding *GoblinBranding::singleton = nullptr;

GoblinBranding::GoblinBranding() {
	singleton = this;
}

GoblinBranding::~GoblinBranding() {
	singleton = nullptr;
}

void GoblinBranding::_bind_methods() {
#ifdef TOOLS_ENABLED
	ClassDB::bind_method(D_METHOD("_install_ui_tweaks"), &GoblinBranding::_install_ui_tweaks);
	ClassDB::bind_method(D_METHOD("_apply_ui_tweaks"), &GoblinBranding::_apply_ui_tweaks);
#endif
}

void GoblinBranding::initialize() {
	_add_translation_overrides();

#ifdef TOOLS_ENABLED
	// SceneTree may not exist at module init time (especially in Project Manager).
	// Defer installation, and retry until it succeeds.
	call_deferred(SNAME("_install_ui_tweaks"));
#endif
}

void GoblinBranding::_add_translation_overrides() {
	TranslationServer *ts = TranslationServer::get_singleton();
	if (!ts) {
		return;
	}

	const String active_locale = ts->get_locale();

	auto add_translation_for_locale = [&](const String &p_locale) {
		Ref<Translation> translation;
		translation.instantiate();
		translation->set_locale(p_locale);

	// Ordered replacements to ensure broad rebrand coverage without touching identifiers.
	struct Replacement {
		const char *from;
		const char *to;
	};

	static const Replacement replacements[] = {
		// Most specific first
		{"Thanks from the Godot community!", "Thanks from the Goblin community!"},
		{"Godot Engine contributors", "Goblin Engine contributors"},
		{"Godot Engine relies on a number of third-party free and open source libraries, all compatible with the terms of its MIT license. The following is an exhaustive list of all such third-party components with their respective copyright statements and license terms.",
				"Goblin Engine relies on a number of third-party free and open source libraries, all compatible with the terms of its MIT license. The following is an exhaustive list of all such third-party components with their respective copyright statements and license terms."},
		{"Godot Engine", "Goblin Engine"},
		{"GODOT ENGINE", "GOBLIN ENGINE"},
		{"Godot community", "Goblin community"},
		{"Godot editor", "Goblin editor"},
		{"Godot project", "Goblin project"},
		{"Godot Projects", "Goblin Projects"},
		{"Godot Docs", "Goblin Docs"},
		{"Godot documentation", "Goblin documentation"},
		{"Godot website", "Goblin website"},
		{"About Godot...", "About Goblin..."},
		{"About Godot", "About Goblin"},
		{"Godot version", "Goblin version"},
		// Generic Godot -> Goblin (last to avoid over-matching)
		{"Godot", "Goblin"},
		{"GODOT", "GOBLIN"},
		{"godot", "goblin"},
		// UI items we hide; blank them to prevent flashes
		{"Support Godot Development", ""},
		{"Donate", ""},
		{"Donors", ""},
	};

		for (const Replacement &repl : replacements) {
			translation->add_message(repl.from, repl.to);
		}

		ts->add_translation(translation);
	};

	// Add for the active locale first, then for plain English as a fallback.
	if (!active_locale.is_empty()) {
		add_translation_for_locale(active_locale);
	}
	add_translation_for_locale("en");
}

#ifdef TOOLS_ENABLED

void GoblinBranding::_install_ui_tweaks() {
	MainLoop *ml = OS::get_singleton()->get_main_loop();
	SceneTree *tree = Object::cast_to<SceneTree>(ml);
	if (!tree) {
		// Retry later (bounded).
		_ui_install_attempts++;
		if (_ui_install_attempts < 120) {
			call_deferred(SNAME("_install_ui_tweaks"));
		}
		return;
	}

	if (!tree->is_connected("node_added", callable_mp(this, &GoblinBranding::_on_node_added))) {
		tree->connect("node_added", callable_mp(this, &GoblinBranding::_on_node_added));
	}

	call_deferred(SNAME("_apply_ui_tweaks"));
}

void GoblinBranding::_apply_ui_tweaks() {
	_hide_project_manager_donate();
	_strip_support_menu_item();

	// Also strip donors tab if About dialog already exists.
	MainLoop *ml = OS::get_singleton()->get_main_loop();
	SceneTree *tree = Object::cast_to<SceneTree>(ml);
	if (!tree) {
		return;
	}
	Node *root = tree->get_root();
	if (!root) {
		return;
	}
	_strip_about_donors_tab(root);

	// Best-effort runtime patching for strings that may not be translated.
	Vector<Node *> stack;
	stack.push_back(root);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}

		if (Window *w = Object::cast_to<Window>(n)) {
			const String title = w->get_title();
			if (title.contains("Thanks from the Godot community")) {
				w->set_title("Thanks from the Goblin community!");
			}
		}

		if (Label *lbl = Object::cast_to<Label>(n)) {
			String t = lbl->get_text();
			if (t.contains("Juan Linietsky") || t.contains("Ariel Manzur")) {
				lbl->set_text("\u00A9 2007-present Goblin & Godot contributors.");
			}
		}

		const int child_count = n->get_child_count();
		for (int i = 0; i < child_count; i++) {
			stack.push_back(n->get_child(i));
		}
	}
}

void GoblinBranding::_on_node_added(Node *p_node) {
	if (!p_node) {
		return;
	}

	_strip_about_donors_tab(p_node);
	_hide_project_manager_donate();
	_strip_support_menu_item();

	// Patch About dialog labels in newly added nodes too.
	if (Window *w = Object::cast_to<Window>(p_node)) {
		const String title = w->get_title();
		if (title.contains("Thanks from the Godot community")) {
			w->set_title("Thanks from the Goblin community!");
		}
	}
	if (Label *lbl = Object::cast_to<Label>(p_node)) {
		String t = lbl->get_text();
		if (t.contains("Juan Linietsky") || t.contains("Ariel Manzur")) {
			lbl->set_text("\u00A9 2007-present Goblin & Godot contributors.");
		}
	}
}

void GoblinBranding::_hide_project_manager_donate() {
	ProjectManager *pm = ProjectManager::get_singleton();
	if (!pm) {
		return;
	}

	// Best-effort: hide any Button labelled "Donate" or with heart icon.
	// We avoid touching upstream code by scanning the runtime UI tree.
	Vector<Node *> stack;
	stack.push_back(pm);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}

		BaseButton *base_btn = Object::cast_to<BaseButton>(n);
		if (base_btn) {
			const String name = base_btn->get_name();
			String text;
			if (Button *btn = Object::cast_to<Button>(base_btn)) {
				text = btn->get_text();
			} else if (LinkButton *lbtn = Object::cast_to<LinkButton>(base_btn)) {
				text = lbtn->get_text();
			}

			const String tooltip = base_btn->get_tooltip_text();
			if (text.contains("Donate") || name.to_lower().contains("donate") || tooltip.contains("Donate") || tooltip.contains("donate")) {
				base_btn->hide();
				base_btn->set_disabled(true);
				base_btn->queue_free();
			}
		}

		const int child_count = n->get_child_count();
		for (int i = 0; i < child_count; i++) {
			stack.push_back(n->get_child(i));
		}
	}
}

void GoblinBranding::_strip_about_donors_tab(Node *p_node) {
	if (!p_node) {
		return;
	}

	// When an EditorAbout dialog is created, it contains a TabContainer with a ScrollContainer named "Donors".
	Vector<Node *> stack;
	stack.push_back(p_node);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}

		if (TabContainer *tc = Object::cast_to<TabContainer>(n)) {
			// Remove/Hide any tab titled "Donors".
			const int tab_count = tc->get_tab_count();
			for (int i = tab_count - 1; i >= 0; i--) {
				Control *tab_control = tc->get_tab_control(i);
				const String title = tc->get_tab_title(i);
				const String name = tab_control ? String(tab_control->get_name()) : String();
				if (title.contains("Donors") || name.contains("Donors")) {
					tc->set_tab_hidden(i, true);
					if (tab_control) {
						tc->remove_child(tab_control);
						tab_control->queue_free();
					}
				}
			}
		}

		const int child_count = n->get_child_count();
		for (int i = 0; i < child_count; i++) {
			stack.push_back(n->get_child(i));
		}
	}
}

void GoblinBranding::_strip_support_menu_item() {
	// Works for both full Editor (EditorNode exists) and Project Manager (no EditorNode).
	Node *scan_root = nullptr;
	if (EditorNode *en = EditorNode::get_singleton()) {
		scan_root = en;
	} else {
		MainLoop *ml = OS::get_singleton()->get_main_loop();
		SceneTree *tree = Object::cast_to<SceneTree>(ml);
		if (tree) {
			scan_root = tree->get_root();
		}
	}
	if (!scan_root) {
		return;
	}

	// Scan popup menus and remove support/donation entries.
	Vector<Node *> stack;
	stack.push_back(scan_root);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}

		if (PopupMenu *pm = Object::cast_to<PopupMenu>(n)) {
			// Remove from end to beginning to avoid index issues
			for (int i = pm->get_item_count() - 1; i >= 0; i--) {
				const String item_text = pm->get_item_text(i);
				// Remove any donation/support related menu items.
				if (item_text.contains("Support") || item_text.contains("Donate") || item_text.contains("Godot Development")) {
					pm->remove_item(i);
					continue;
				}
				// Also rename About item if needed.
				if (item_text == "About Godot..." || item_text == "About Godot") {
					pm->set_item_text(i, item_text.replace("Godot", "Goblin"));
				}
			}
		}

		const int child_count = n->get_child_count();
		for (int i = 0; i < child_count; i++) {
			stack.push_back(n->get_child(i));
		}
	}
}

#endif // TOOLS_ENABLED
