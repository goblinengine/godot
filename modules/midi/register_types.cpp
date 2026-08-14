/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "register_types.h"

#include "core/io/resource_importer.h"
#include "core/object/class_db.h"

#include "midi_importers.h"
#include "midi_resources.h"
#include "midi_stream.h"
#include "midi_stream_playback.h"

void initialize_midi_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GDREGISTER_CLASS(MidiFileResource);
		GDREGISTER_CLASS(SoundFontResource);
		GDREGISTER_CLASS(MidiStream);
		GDREGISTER_CLASS(MidiStreamPlayback);
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		// Importers must be registered before the editor filesystem scan.
		GDREGISTER_CLASS(MidiImporter);
		GDREGISTER_CLASS(SoundFontImporter);
		ResourceFormatImporter::get_singleton()->add_importer(memnew(MidiImporter));
		ResourceFormatImporter::get_singleton()->add_importer(memnew(SoundFontImporter));
	}
#endif // TOOLS_ENABLED
}

void uninitialize_midi_module(ModuleInitializationLevel p_level) {
	// Importers are owned by ResourceFormatImporter; no per-level cleanup.
}
