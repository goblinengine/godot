/**************************************************************************/
/*  midi_importers.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#include "midi_importers.h"

#include "core/io/file_access.h"
#include "core/io/resource_saver.h"

#include "midi_resources.h"

void MidiImporter::_bind_methods() {
}

String MidiImporter::get_importer_name() const {
	return "midi_stream.mid";
}

String MidiImporter::get_visible_name() const {
	return "MIDI File (MidiStream)";
}

void MidiImporter::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("mid");
	p_extensions->push_back("midi");
}

String MidiImporter::get_save_extension() const {
	return "res";
}

String MidiImporter::get_resource_type() const {
	return "MidiFileResource";
}

float MidiImporter::get_priority() const {
	return 1.0f;
}

int MidiImporter::get_import_order() const {
	return IMPORT_ORDER_DEFAULT;
}

int MidiImporter::get_preset_count() const {
	return 1;
}

String MidiImporter::get_preset_name(int p_preset_index) const {
	return "Default";
}

void MidiImporter::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
}

bool MidiImporter::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error MidiImporter::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	PackedByteArray bytes = FileAccess::get_file_as_bytes(p_source_file);
	if (bytes.is_empty()) {
		ERR_PRINT(vformat("MidiStream importer: failed to read bytes: %s", p_source_file));
		return ERR_CANT_OPEN;
	}

	Ref<MidiFileResource> res = memnew(MidiFileResource);
	res->set_data(bytes);

	String out_path = p_save_path + "." + get_save_extension();
	return ResourceSaver::save(res, out_path);
}

void SoundFontImporter::_bind_methods() {
}

String SoundFontImporter::get_importer_name() const {
	return "midi_stream.sf2";
}

String SoundFontImporter::get_visible_name() const {
	return "SoundFont 2 (MidiStream)";
}

void SoundFontImporter::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("sf2");
}

String SoundFontImporter::get_save_extension() const {
	return "res";
}

String SoundFontImporter::get_resource_type() const {
	return "SoundFontResource";
}

float SoundFontImporter::get_priority() const {
	return 1.0f;
}

int SoundFontImporter::get_import_order() const {
	return IMPORT_ORDER_DEFAULT;
}

int SoundFontImporter::get_preset_count() const {
	return 1;
}

String SoundFontImporter::get_preset_name(int p_preset_index) const {
	return "Default";
}

void SoundFontImporter::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
}

bool SoundFontImporter::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error SoundFontImporter::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	PackedByteArray bytes = FileAccess::get_file_as_bytes(p_source_file);
	if (bytes.is_empty()) {
		ERR_PRINT(vformat("MidiStream importer: failed to read bytes: %s", p_source_file));
		return ERR_CANT_OPEN;
	}

	Ref<SoundFontResource> res = memnew(SoundFontResource);
	res->set_data(bytes);

	String out_path = p_save_path + "." + get_save_extension();
	return ResourceSaver::save(res, out_path);
}
