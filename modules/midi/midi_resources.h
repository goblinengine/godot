/**************************************************************************/
/*  midi_resources.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#ifndef MIDI_RESOURCES_H
#define MIDI_RESOURCES_H

#include "core/io/resource.h"
#include "core/variant/variant.h"

// Raw container for a parsed MIDI file (.mid / .midi), produced by the
// MidiImporter. The bytes are fed to the TinyMidiLoader at playback time.
class MidiFileResource : public Resource {
	GDCLASS(MidiFileResource, Resource);

public:
	void set_data(const PackedByteArray &p_data);
	PackedByteArray get_data() const;

protected:
	static void _bind_methods();

private:
	PackedByteArray data;
};

// Raw container for a SoundFont 2 file (.sf2), produced by the
// SoundFontImporter. The bytes are fed to the TinySoundFont synthesizer at
// playback time.
class SoundFontResource : public Resource {
	GDCLASS(SoundFontResource, Resource);

public:
	void set_data(const PackedByteArray &p_data);
	PackedByteArray get_data() const;

protected:
	static void _bind_methods();

private:
	PackedByteArray data;
};

#endif // MIDI_RESOURCES_H
