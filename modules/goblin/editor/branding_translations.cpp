/**************************************************************************/
/*  branding_translations.cpp                                             */
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

#include "branding_translations.h"

#ifdef TOOLS_ENABLED

#include "core/config/engine.h"
#include "core/string/translation_server.h"

void register_branding_translations() {
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
	// These cover strings in editor files that are not overridden at compile time.
	struct Replacement {
		const char *from;
		const char *to;
	};

	static const Replacement replacements[] = {
		// Most specific first
		{"Godot Engine", "Goblin Engine"},
		{"GODOT ENGINE", "GOBLIN ENGINE"},
		{"Godot community", "Goblin community"},
		{"Godot editor", "Goblin editor"},
		{"Godot project", "Goblin project"},
		{"Godot Projects", "Goblin Projects"},
		{"Godot Docs", "Goblin Docs"},
		{"Godot documentation", "Goblin documentation"},
		{"Godot website", "Goblin website"},
		{"Godot version", "Goblin version"},
		// Generic Godot -> Goblin (last to avoid over-matching)
		{"Godot", "Goblin"},
		{"GODOT", "GOBLIN"},
		{"godot", "goblin"},
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

#endif // TOOLS_ENABLED
