/**************************************************************************/
/*  goblin_export.h                                                       */
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

#ifndef GOBLIN_EXPORT_H
#define GOBLIN_EXPORT_H

#include "core/object/class_db.h"
#include "core/string/translation.h"
#include "core/string/translation_server.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

#ifdef TOOLS_ENABLED
class Node;
class ProjectExportDialog;
class EditorInspector;
class EditorFileDialog;
class EditorExportPreset;
class EditorProperty;
#endif

class GoblinExportTweaks : public Object {
	GDCLASS(GoblinExportTweaks, Object);

private:
	static GoblinExportTweaks *singleton;

protected:
	static void _bind_methods();

public:
	static GoblinExportTweaks *get_singleton() { return singleton; }

	GoblinExportTweaks();
	~GoblinExportTweaks();

#ifdef TOOLS_ENABLED
	void initialize();

private:
	int _ui_install_attempts = 0;
	void _install_hooks();
	void _on_node_added(Node *p_node);
	void _refresh_export_dialog();
	ProjectExportDialog *_find_export_dialog() const;
	void _patch_export_dialog(ProjectExportDialog *p_dialog);
	void _retarget_architecture(EditorInspector *p_inspector, EditorExportPreset *p_preset, const Vector<String> &p_arches);
	void _hide_debug_only_properties(EditorInspector *p_inspector, bool p_has_debug_templates);
	void _trim_file_dialog_options(ProjectExportDialog *p_dialog, bool p_has_debug_templates);
	void _strip_missing_template_warnings(ProjectExportDialog *p_dialog, bool p_has_debug_templates);
	struct TemplateInfo {
		HashMap<String, bool> arch_release;
		HashMap<String, bool> arch_debug;
		bool has_debug_template = false;
	};
	String _template_prefix_for_os(const String &p_os_name) const;
	TemplateInfo _collect_template_info(const String &p_prefix) const;
	template <class T>
	T *_find_child_type(Node *p_root) const;
	Vector<class EditorProperty *> _get_export_properties(EditorInspector *p_inspector) const;
#endif
};

#endif // GOBLIN_EXPORT_H