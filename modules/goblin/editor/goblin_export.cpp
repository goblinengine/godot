/**************************************************************************/
/*  goblin_export.cpp                                                     */
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

#include "goblin_export.h"

#include "core/config/engine.h"
#include "core/io/dir_access.h"
#include "core/os/os.h"
#include "core/version.h"

#ifdef TOOLS_ENABLED
#include "core/object/object.h"
#include "editor/file_system/editor_paths.h"
#include "editor/export/editor_export.h"
#include "editor/export/editor_export_platform.h"
#include "editor/export/editor_export_preset.h"
#include "editor/export/project_export.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/inspector/editor_properties.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/box_container.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"

GoblinExportTweaks *GoblinExportTweaks::singleton = nullptr;

void GoblinExportTweaks::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_install_hooks"), &GoblinExportTweaks::_install_hooks);
	ClassDB::bind_method(D_METHOD("_refresh_export_dialog"), &GoblinExportTweaks::_refresh_export_dialog);
	ClassDB::bind_method(D_METHOD("_patch_export_dialog", "dialog"), &GoblinExportTweaks::_patch_export_dialog);
}

GoblinExportTweaks::GoblinExportTweaks() {
	singleton = this;
}

GoblinExportTweaks::~GoblinExportTweaks() {
	singleton = nullptr;
}

void GoblinExportTweaks::initialize() {
	call_deferred(SNAME("_install_hooks"));
}

void GoblinExportTweaks::_install_hooks() {
	MainLoop *ml = OS::get_singleton()->get_main_loop();
	SceneTree *tree = Object::cast_to<SceneTree>(ml);
	if (!tree) {
		_ui_install_attempts++;
		if (_ui_install_attempts < 120) {
			call_deferred(SNAME("_install_hooks"));
		}
		return;
	}
	if (!tree->is_connected("node_added", callable_mp(this, &GoblinExportTweaks::_on_node_added))) {
		tree->connect("node_added", callable_mp(this, &GoblinExportTweaks::_on_node_added));
	}

	EditorExport *ee = EditorExport::get_singleton();
	if (ee) {
		Callable refresh = callable_mp(this, &GoblinExportTweaks::_refresh_export_dialog);
		if (!ee->is_connected("export_presets_updated", refresh)) {
			ee->connect("export_presets_updated", refresh);
		}
	}
}

void GoblinExportTweaks::_on_node_added(Node *p_node) {
	if (!p_node) {
		return;
	}
	// ProjectExportDialog gets its UI rebuilt as presets change.
	// Patch on dialog creation and when inspector-related nodes are added under it.
	if (Object::cast_to<ProjectExportDialog>(p_node) || Object::cast_to<EditorInspector>(p_node) || Object::cast_to<EditorProperty>(p_node) || Object::cast_to<EditorFileDialog>(p_node)) {
		Node *walk = p_node;
		for (int i = 0; i < 32 && walk; i++) {
			if (Object::cast_to<ProjectExportDialog>(walk)) {
				call_deferred(SNAME("_refresh_export_dialog"));
				break;
			}
			walk = walk->get_parent();
		}
	}
}

void GoblinExportTweaks::_refresh_export_dialog() {
	ProjectExportDialog *dlg = _find_export_dialog();
	if (!dlg) {
		return;
	}
	call_deferred(SNAME("_patch_export_dialog"), dlg);
}

ProjectExportDialog *GoblinExportTweaks::_find_export_dialog() const {
	MainLoop *ml = OS::get_singleton()->get_main_loop();
	SceneTree *tree = Object::cast_to<SceneTree>(ml);
	if (!tree) {
		return nullptr;
	}
	Node *root = tree->get_root();
	if (!root) {
		return nullptr;
	}
	Vector<Node *> stack;
	stack.push_back(root);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}
		if (ProjectExportDialog *dlg = Object::cast_to<ProjectExportDialog>(n)) {
			return dlg;
		}
		for (int i = 0; i < n->get_child_count(); i++) {
			stack.push_back(n->get_child(i));
		}
	}
	return nullptr;
}

template <class T>
T *GoblinExportTweaks::_find_child_type(Node *p_root) const {
	if (!p_root) {
		return nullptr;
	}
	Vector<Node *> stack;
	stack.push_back(p_root);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}
		if (T *found = Object::cast_to<T>(n)) {
			return found;
		}
		for (int i = 0; i < n->get_child_count(); i++) {
			stack.push_back(n->get_child(i));
		}
	}
	return nullptr;
}

Vector<EditorProperty *> GoblinExportTweaks::_get_export_properties(EditorInspector *p_inspector) const {
	Vector<EditorProperty *> out;
	if (!p_inspector) {
		return out;
	}
	Vector<Node *> stack;
	stack.push_back(p_inspector);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}
		if (EditorProperty *prop = Object::cast_to<EditorProperty>(n)) {
			out.push_back(prop);
		}
		for (int i = 0; i < n->get_child_count(); i++) {
			stack.push_back(n->get_child(i));
		}
	}
	return out;
}

void GoblinExportTweaks::_patch_export_dialog(ProjectExportDialog *p_dialog) {
	if (!p_dialog || !p_dialog->is_inside_tree() || !p_dialog->is_visible()) {
		return;
	}

	EditorInspector *inspector = _find_child_type<EditorInspector>(p_dialog);
	if (!inspector) {
		return;
	}
	Object *edited = inspector->get_edited_object();
	EditorExportPreset *preset = Object::cast_to<EditorExportPreset>(edited);
	if (!preset) {
		return;
	}
	Ref<EditorExportPlatform> platform = preset->get_platform();
	if (platform.is_null()) {
		return;
	}

	String prefix = _template_prefix_for_os(platform->get_os_name());
	TemplateInfo info = _collect_template_info(prefix);
	const bool debug_available = info.has_debug_template;

	// Disable "Export With Debug" checkbox in file dialogs when no debug template exists.
	Vector<Node *> stack;
	stack.push_back(p_dialog);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}
		if (EditorFileDialog *fd = Object::cast_to<EditorFileDialog>(n)) {
			Vector<Node *> wstack;
			wstack.push_back(fd);
			while (!wstack.is_empty()) {
				Node *wn = wstack[wstack.size() - 1];
				wstack.resize(wstack.size() - 1);
				if (!wn) {
					continue;
				}
				if (CheckBox *cb = Object::cast_to<CheckBox>(wn)) {
					if (cb->get_text() == TTR("Export With Debug")) {
						cb->set_disabled(!debug_available);
						if (!debug_available && cb->is_pressed()) {
							cb->set_pressed(false);
						}
						break;
					}
				}
				for (int ci = 0; ci < wn->get_child_count(); ci++) {
					wstack.push_back(wn->get_child(ci));
				}
			}
		}
		for (int i = 0; i < n->get_child_count(); i++) {
			stack.push_back(n->get_child(i));
		}
	}

	// Don't filter architecture - show all platform defaults.
	// Always strip missing debug template warnings.
	_strip_missing_template_warnings(p_dialog, false);
}

void GoblinExportTweaks::_strip_missing_template_warnings(ProjectExportDialog *p_dialog, bool p_has_debug_templates) {
	if (p_has_debug_templates || !p_dialog) {
		return;
	}

	Vector<Node *> stack;
	stack.push_back(p_dialog);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}
		if (Label *lbl = Object::cast_to<Label>(n)) {
			String text = lbl->get_text();
			if (text.is_empty()) {
				continue;
			}
			PackedStringArray lines = text.split("\n", false);
			PackedStringArray kept;
			bool prev_missing_tpl = false;
			for (int i = 0; i < lines.size(); i++) {
				String line = lines[i];
				String lc = line.to_lower();
				bool looks_like_missing_template = lc.find("export template") != -1 || lc.find("expected path") != -1 || lc.find("template file") != -1;
				bool mentions_debug = lc.find("debug") != -1;
				if ((looks_like_missing_template && mentions_debug) || (prev_missing_tpl && mentions_debug)) {
					prev_missing_tpl = false;
					continue;
				}
				prev_missing_tpl = looks_like_missing_template;
				kept.push_back(line);
			}
			if (kept.is_empty()) {
				lbl->hide();
				lbl->set_text("");
			} else {
				lbl->set_text(String("\n").join(kept));
			}
		}
		for (int i = 0; i < n->get_child_count(); i++) {
			stack.push_back(n->get_child(i));
		}
	}
}

void GoblinExportTweaks::_trim_file_dialog_options(ProjectExportDialog *p_dialog, bool p_has_debug_templates) {
	if (p_has_debug_templates || !p_dialog) {
		return;
	}

	Vector<EditorFileDialog *> dialogs;
	Vector<Node *> stack;
	stack.push_back(p_dialog);
	while (!stack.is_empty()) {
		Node *n = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (!n) {
			continue;
		}
		if (EditorFileDialog *fd = Object::cast_to<EditorFileDialog>(n)) {
			dialogs.push_back(fd);
		}
		for (int i = 0; i < n->get_child_count(); i++) {
			stack.push_back(n->get_child(i));
		}
	}

	const String debug_label = TTR("Export With Debug");
	for (EditorFileDialog *fd : dialogs) {
		if (!fd) {
			continue;
		}
		int count = fd->get_option_count();
		if (count == 0) {
			continue;
		}
		struct OptData {
			String name;
			Vector<String> values;
			int def = 0;
		};
		Vector<OptData> kept;
		kept.reserve(count);
		for (int idx = 0; idx < count; idx++) {
			OptData od;
			od.name = fd->get_option_name(idx);
			od.values = fd->get_option_values(idx);
			od.def = fd->get_option_default(idx);
			if (od.name == debug_label) {
				continue;
			}
			kept.push_back(od);
		}

		fd->set_option_count(kept.size());
		for (int idx = 0; idx < kept.size(); idx++) {
			fd->set_option_name(idx, kept[idx].name);
			fd->set_option_values(idx, kept[idx].values);
			fd->set_option_default(idx, kept[idx].def);
		}
	}
}

void GoblinExportTweaks::_hide_debug_only_properties(EditorInspector *p_inspector, bool p_has_debug_templates) {
	if (!p_inspector || p_has_debug_templates) {
		return;
	}
	Vector<EditorProperty *> props = _get_export_properties(p_inspector);
	for (EditorProperty *prop : props) {
		if (!prop) {
			continue;
		}
		StringName name = prop->get_edited_property();
		if (name == StringName("custom_template/debug") || name == StringName("debug/export_console_wrapper")) {
			prop->hide();
		}
	}
}

void GoblinExportTweaks::_retarget_architecture(EditorInspector *p_inspector, EditorExportPreset *p_preset, const Vector<String> &p_arches) {
	if (!p_inspector || !p_preset || p_arches.is_empty()) {
		return;
	}

	String current = p_preset->get("binary_format/architecture");
	int fallback = 0;
	int selected = -1;
	for (int i = 0; i < p_arches.size(); i++) {
		if (p_arches[i] == current) {
			selected = i;
			break;
		}
	}
	if (selected == -1) {
		selected = fallback;
	}

	Vector<EditorProperty *> props = _get_export_properties(p_inspector);
	for (EditorProperty *prop : props) {
		if (!prop) {
			continue;
		}
		if (prop->get_edited_property() != StringName("binary_format/architecture")) {
			continue;
		}
		OptionButton *opt = _find_child_type<OptionButton>(prop);
		if (!opt) {
			break;
		}
		opt->clear();
		for (int i = 0; i < p_arches.size(); i++) {
			opt->add_item(p_arches[i]);
		}
		opt->select(selected);
		String chosen = p_arches[selected];
		if (chosen != current) {
			p_preset->set("binary_format/architecture", chosen);
			prop->update_property();
		}
		break;
	}
}


String GoblinExportTweaks::_template_prefix_for_os(const String &p_os_name) const {
	String key = p_os_name.to_lower();
	key = key.replace(" ", "").replace("/", "").replace("-", "").replace(".", "");
	if (key.is_empty()) {
		return String();
	}

	// Derive available template prefixes from installed templates, so new platforms work automatically.
	String base_dir = EditorPaths::get_singleton()->get_export_templates_dir().path_join(GODOT_VERSION_FULL_CONFIG);
	Ref<DirAccess> da = DirAccess::open(base_dir);
	if (da.is_null()) {
		return key;
	}

	HashMap<String, bool> prefixes;
	da->list_dir_begin();
	while (true) {
		String f = da->get_next();
		if (f.is_empty()) {
			break;
		}
		if (da->current_is_dir()) {
			continue;
		}
		int sep = f.find("_");
		if (sep <= 0) {
			sep = f.find(".");
		}
		if (sep <= 0) {
			continue;
		}
		String prefix = f.substr(0, sep).to_lower();
		prefix = prefix.replace(" ", "").replace("/", "").replace("-", "").replace(".", "");
		if (!prefix.is_empty()) {
			prefixes[prefix] = true;
		}
	}
	da->list_dir_end();

	if (prefixes.has(key)) {
		return key;
	}

	// Fallback: pick a prefix contained in the key.
	String best;
	for (const KeyValue<String, bool> &E : prefixes) {
		const String &p = E.key;
		if (p.is_empty()) {
			continue;
		}
		if (key.find(p) != -1) {
			if (p.length() > best.length()) {
				best = p;
			}
		}
	}
	return best.is_empty() ? key : best;
}

GoblinExportTweaks::TemplateInfo GoblinExportTweaks::_collect_template_info(const String &p_prefix) const {
	TemplateInfo info;
	if (p_prefix.is_empty()) {
		return info;
	}

	String base_dir = EditorPaths::get_singleton()->get_export_templates_dir().path_join(GODOT_VERSION_FULL_CONFIG);
	Ref<DirAccess> da = DirAccess::open(base_dir);
	if (da.is_null()) {
		return info;
	}

	da->list_dir_begin();
	while (true) {
		String f = da->get_next();
		if (f.is_empty()) {
			break;
		}
		if (da->current_is_dir()) {
			continue;
		}
		if (!f.begins_with(p_prefix + String("_")) && !f.begins_with(p_prefix + String("."))) {
			continue;
		}
		if (f.begins_with(p_prefix + String("."))) {
			// Platforms like macOS can use a single archive template (e.g. macos.zip).
			info.arch_release["universal"] = true;
			continue;
		}
		String base = f;
		if (base.get_extension() != String()) {
			base = base.trim_suffix("." + base.get_extension());
		}
		base = base.substr(p_prefix.length() + 1);
		bool is_debug = base.begins_with("debug_") || base.begins_with("debug.");
		bool is_release = base.begins_with("release_") || base.begins_with("release.");
		if (is_debug) {
			info.has_debug_template = true;
		}
		if (!is_debug && !is_release) {
			continue;
		}
		if (is_debug) {
			if (base.begins_with("debug_")) {
				base = base.substr(6);
			} else if (base.begins_with("debug.")) {
				base = base.substr(6);
			}
		} else if (is_release) {
			if (base.begins_with("release_")) {
				base = base.substr(8);
			} else if (base.begins_with("release.")) {
				base = base.substr(8);
			}
		}
		String arch = base;
		if (arch.is_empty()) {
			continue;
		}
		if (is_debug) {
			info.arch_debug[arch] = true;
		}
		if (is_release) {
			info.arch_release[arch] = true;
		}
	}
	da->list_dir_end();
	return info;
}

#endif // TOOLS_ENABLED