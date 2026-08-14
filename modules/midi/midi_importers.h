/**************************************************************************/
/*  midi_importers.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#ifndef MIDI_IMPORTERS_H
#define MIDI_IMPORTERS_H

#include "core/io/resource_importer.h"

// Imports .mid/.midi files into MidiFileResource (.res). Importer name kept
// as "midi_stream.mid" for compatibility with projects imported by the
// legacy MidiStream GDExtension.
class MidiImporter : public ResourceImporter {
	GDCLASS(MidiImporter, ResourceImporter);

public:
	virtual String get_importer_name() const override;
	virtual String get_visible_name() const override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual String get_save_extension() const override;
	virtual String get_resource_type() const override;
	virtual float get_priority() const override;
	virtual int get_import_order() const override;
	virtual int get_preset_count() const override;
	virtual String get_preset_name(int p_preset_index) const override;
	virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const override;
	virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override;
	virtual Error import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) override;

protected:
	static void _bind_methods();
};

// Imports .sf2 files into SoundFontResource (.res). Importer name kept as
// "midi_stream.sf2" for compatibility with projects imported by the legacy
// MidiStream GDExtension.
class SoundFontImporter : public ResourceImporter {
	GDCLASS(SoundFontImporter, ResourceImporter);

public:
	virtual String get_importer_name() const override;
	virtual String get_visible_name() const override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual String get_save_extension() const override;
	virtual String get_resource_type() const override;
	virtual float get_priority() const override;
	virtual int get_import_order() const override;
	virtual int get_preset_count() const override;
	virtual String get_preset_name(int p_preset_index) const override;
	virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const override;
	virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override;
	virtual Error import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) override;

protected:
	static void _bind_methods();
};

#endif // MIDI_IMPORTERS_H
