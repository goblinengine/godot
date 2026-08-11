/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "register_types.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"

#if defined(GOBLIN_BRANDING_RUNTIME_ENABLED)
#include "editor/goblin_about.h"
#include "editor/goblin_export.h"

static GoblinBranding *goblin_branding = nullptr;
static GoblinExportTweaks *goblin_export = nullptr;
#endif

void preregister_goblin_types() {
	// Called before other modules for docgen
}

void initialize_goblin_module(ModuleInitializationLevel p_level) {
	// Project Manager + Editor UI are initialized at EDITOR level.
	// We also initialize at SCENE level to cover non-editor runtime usage.
#if defined(GOBLIN_BRANDING_RUNTIME_ENABLED)
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR || p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Register Goblin branding class
		GDREGISTER_CLASS(GoblinBranding);
		GDREGISTER_CLASS(GoblinExportTweaks);

		// Initialize branding overrides
		if (!goblin_branding) {
			goblin_branding = memnew(GoblinBranding);
			goblin_branding->initialize();
		}
		if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR && !goblin_export) {
			goblin_export = memnew(GoblinExportTweaks);
			goblin_export->initialize();
		}
	}
#endif
}

void uninitialize_goblin_module(ModuleInitializationLevel p_level) {
#if defined(GOBLIN_BRANDING_RUNTIME_ENABLED)
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR || p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		if (goblin_branding) {
			memdelete(goblin_branding);
			goblin_branding = nullptr;
		}
		if (goblin_export) {
			memdelete(goblin_export);
			goblin_export = nullptr;
		}
	}
#endif
}
