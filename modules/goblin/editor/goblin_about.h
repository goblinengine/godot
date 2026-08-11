/**************************************************************************/
/*  goblin_about.h                                                        */
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

#ifndef GOBLIN_ABOUT_H
#define GOBLIN_ABOUT_H

#include "core/object/class_db.h"
#include "core/string/translation.h"
#include "core/string/translation_server.h"

#ifdef TOOLS_ENABLED
class Node;
#endif

// Goblin branding override system
// This patches UI strings at runtime without modifying core code
class GoblinBranding : public Object {
	GDCLASS(GoblinBranding, Object);

private:
	static GoblinBranding *singleton;

protected:
	static void _bind_methods();

public:
	static GoblinBranding *get_singleton() { return singleton; }

	GoblinBranding();
	~GoblinBranding();

	// Initialize branding overrides (and Goblin-only UI tweaks).
	void initialize();

private:
	void _add_translation_overrides();

#ifdef TOOLS_ENABLED
	int _ui_install_attempts = 0;
	void _install_ui_tweaks();
	void _apply_ui_tweaks();
	void _on_node_added(Node *p_node);
	void _hide_project_manager_donate();
	void _strip_about_donors_tab(Node *p_node);
	void _strip_support_menu_item();
#endif
};

#endif // GOBLIN_ABOUT_H
