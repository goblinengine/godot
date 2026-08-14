/**************************************************************************/
/*  test_midi_stream.h                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#pragma once

#include "tests/test_macros.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "modules/midi/midi_resources.h"
#include "modules/midi/midi_stream.h"
#include "modules/midi/midi_stream_playback.h"
#include "servers/audio/audio_driver_dummy.h"
#include "servers/audio/audio_server.h"

namespace MidiStreamTest {

// ---------------------------------------------------------------------------
// Byte-level helpers for building test fixtures (minimal SF2 + SMF).
// ---------------------------------------------------------------------------

static void put_u8(std::vector<uint8_t> &buf, uint8_t v) {
	buf.push_back(v);
}

static void put_u16(std::vector<uint8_t> &buf, uint16_t v) {
	buf.push_back(v & 0xFF);
	buf.push_back((v >> 8) & 0xFF);
}

static void put_u32(std::vector<uint8_t> &buf, uint32_t v) {
	buf.push_back(v & 0xFF);
	buf.push_back((v >> 8) & 0xFF);
	buf.push_back((v >> 16) & 0xFF);
	buf.push_back((v >> 24) & 0xFF);
}

static void put_fourcc(std::vector<uint8_t> &buf, const char *p_fourcc) {
	for (int i = 0; i < 4; i++) {
		buf.push_back((uint8_t)p_fourcc[i]);
	}
}

// Big-endian writers for the SMF header (MIDI files are network order).
static void put_u16_be(std::vector<uint8_t> &buf, uint16_t v) {
	buf.push_back((v >> 8) & 0xFF);
	buf.push_back(v & 0xFF);
}

static void put_u32_be(std::vector<uint8_t> &buf, uint32_t v) {
	buf.push_back((v >> 24) & 0xFF);
	buf.push_back((v >> 16) & 0xFF);
	buf.push_back((v >> 8) & 0xFF);
	buf.push_back(v & 0xFF);
}

// Fixed 20-byte SoundFont name field.
static void put_name(std::vector<uint8_t> &buf, const char *p_name) {
	size_t len = strlen(p_name);
	for (int i = 0; i < 20; i++) {
		buf.push_back(i < len ? (uint8_t)p_name[i] : 0);
	}
}

// Opens a chunk (fourcc + size placeholder) and returns the size field
// position for end_chunk(). The size position is per-chunk; nesting requires
// a distinct local per chunk (reusing one variable corrupts the sizes).
static uint32_t begin_chunk(std::vector<uint8_t> &buf, const char *p_fourcc) {
	put_fourcc(buf, p_fourcc);
	uint32_t size_pos = (uint32_t)buf.size();
	put_u32(buf, 0);
	return size_pos;
}

static void end_chunk(std::vector<uint8_t> &buf, uint32_t p_size_pos) {
	uint32_t size = (uint32_t)(buf.size() - p_size_pos - 4);
	buf[p_size_pos + 0] = size & 0xFF;
	buf[p_size_pos + 1] = (size >> 8) & 0xFF;
	buf[p_size_pos + 2] = (size >> 16) & 0xFF;
	buf[p_size_pos + 3] = (size >> 24) & 0xFF;
}

static void push_vlq(std::vector<uint8_t> &buf, uint32_t p_value) {
	uint8_t groups[4];
	int count = 0;
	groups[count++] = (uint8_t)(p_value & 0x7F);
	p_value >>= 7;
	while (p_value) {
		groups[count++] = (uint8_t)((p_value & 0x7F) | 0x80);
		p_value >>= 7;
	}
	for (int i = count - 1; i >= 0; i--) {
		buf.push_back(groups[i]);
	}
}

// Minimal SoundFont 2: one preset (bank 0, preset 0) referencing one
// instrument with one 512-frame sine sample at original pitch C5 (60).
static PackedByteArray make_test_soundfont() {
	std::vector<uint8_t> buf;
	put_fourcc(buf, "RIFF");
	uint32_t riff_size_pos = (uint32_t)buf.size();
	put_u32(buf, 0);
	put_fourcc(buf, "sfbk");

	// INFO list.
	uint32_t info_pos = begin_chunk(buf, "LIST");
	put_fourcc(buf, "INFO");
	uint32_t p = begin_chunk(buf, "ifil");
	put_u16(buf, 2);
	put_u16(buf, 1);
	end_chunk(buf, p);
	p = begin_chunk(buf, "isng");
	const char isng[] = "Goblin Test SoundFont";
	put_u32(buf, (uint32_t)sizeof(isng));
	buf.insert(buf.end(), isng, isng + sizeof(isng));
	end_chunk(buf, p);
	p = begin_chunk(buf, "INAM");
	const char inam[] = "Goblin Test";
	put_u32(buf, (uint32_t)sizeof(inam));
	buf.insert(buf.end(), inam, inam + sizeof(inam));
	end_chunk(buf, p);
	end_chunk(buf, info_pos);

	// sdta list: 512 mono 16-bit sine samples.
	const int sample_count = 512;
	uint32_t sdta_pos = begin_chunk(buf, "LIST");
	put_fourcc(buf, "sdta");
	p = begin_chunk(buf, "smpl");

	const double two_pi = 6.28318530717958647692;
	for (int i = 0; i < sample_count; i++) {
		int16_t s = (int16_t)(16000.0 * sin(two_pi * 440.0 * i / 44100.0));
		put_u16(buf, (uint16_t)s);
	}
	end_chunk(buf, p);
	end_chunk(buf, sdta_pos);

	// pdta list.
	uint32_t pdta_pos = begin_chunk(buf, "LIST");
	put_fourcc(buf, "pdta");
	// phdr: 1 preset + terminal (38 bytes each).
	p = begin_chunk(buf, "phdr");

	put_name(buf, "Test");
	put_u16(buf, 0); // preset
	put_u16(buf, 0); // bank
	put_u16(buf, 0); // presetBagNdx
	put_u32(buf, 0); // library
	put_u32(buf, 0); // genre
	put_u32(buf, 0); // morphology
	put_name(buf, "");
	put_u16(buf, 0xFFFF); // terminal preset
	put_u16(buf, 0);
	put_u16(buf, 1); // terminal presetBagNdx = end of preset bags
	put_u32(buf, 0);
	put_u32(buf, 0);
	put_u32(buf, 0);
	end_chunk(buf, p);
	// pbag: 1 bag + terminal (4 bytes each).
	p = begin_chunk(buf, "pbag");

	put_u16(buf, 0); // genNdx
	put_u16(buf, 0); // modNdx
	put_u16(buf, 1); // terminal genNdx
	put_u16(buf, 0);
	end_chunk(buf, p);
	// pmod: 2 terminal entries (10 bytes each).
	p = begin_chunk(buf, "pmod");

	for (int i = 0; i < 10; i++) {
		put_u16(buf, 0xFFFF);
	}
	for (int i = 0; i < 10; i++) {
		put_u16(buf, 0xFFFF);
	}
	end_chunk(buf, p);
	// pgen: 1 generator (instrument) + terminal (4 bytes each).
	p = begin_chunk(buf, "pgen");

	put_u16(buf, 41); // GenInstrument
	put_u16(buf, 0); // instrument index 0
	put_u16(buf, 0xFFFF); // terminal
	put_u16(buf, 0xFFFF);
	end_chunk(buf, p);
	// inst: 1 instrument + terminal (22 bytes each).
	p = begin_chunk(buf, "inst");

	put_name(buf, "Inst");
	put_u16(buf, 0); // instBagNdx
	put_name(buf, "");
	put_u16(buf, 1); // terminal instBagNdx
	end_chunk(buf, p);
	// ibag: 1 bag + terminal (4 bytes each).
	p = begin_chunk(buf, "ibag");

	put_u16(buf, 0); // instGenNdx
	put_u16(buf, 0); // instModNdx
	put_u16(buf, 1); // terminal instGenNdx
	put_u16(buf, 0);
	end_chunk(buf, p);
	// imod: 2 terminal entries (10 bytes each).
	p = begin_chunk(buf, "imod");

	for (int i = 0; i < 10; i++) {
		put_u16(buf, 0xFFFF);
	}
	for (int i = 0; i < 10; i++) {
		put_u16(buf, 0xFFFF);
	}
	end_chunk(buf, p);
	// igen: 1 generator (sampleID) + terminal (4 bytes each).
	p = begin_chunk(buf, "igen");

	put_u16(buf, 53); // GenSampleID
	put_u16(buf, 0); // sample index 0
	put_u16(buf, 0xFFFF); // terminal
	put_u16(buf, 0xFFFF);
	end_chunk(buf, p);
	// shdr: 1 sample + terminal (46 bytes each).
	p = begin_chunk(buf, "shdr");

	put_name(buf, "Sine");
	put_u32(buf, 0); // start
	put_u32(buf, 512); // end
	put_u32(buf, 0); // startLoop
	put_u32(buf, 512); // endLoop
	put_u32(buf, 44100); // sampleRate
	put_u8(buf, 60); // originalPitch
	put_u8(buf, 0); // pitchCorrection
	put_u16(buf, 0); // sampleLink
	put_u16(buf, 1); // sampleType (mono PCM16)
	for (int i = 0; i < 46; i++) {
		put_u8(buf, 0);
	}
	end_chunk(buf, p);
	end_chunk(buf, pdta_pos);

	end_chunk(buf, riff_size_pos);

	PackedByteArray result;
	result.resize((int)buf.size());
	memcpy(result.ptrw(), buf.data(), buf.size());
	return result;
}

// Minimal SMF (format 0, division 480, tempo 120 BPM): three quarter notes at
// 0 / 500 / 1000 ms, last one ending at 1500 ms.
static PackedByteArray make_test_midi() {
	std::vector<uint8_t> track;
	// Tempo 500000 us/quarter (120 BPM).
	track.push_back(0x00);
	track.push_back(0xFF);
	track.push_back(0x51);
	track.push_back(0x03);
	track.push_back(0x07);
	track.push_back(0xA1);
	track.push_back(0x20);
	// Program change, channel 0 -> program 0.
	track.push_back(0x00);
	track.push_back(0xC0);
	track.push_back(0x00);
	// Note on, channel 0, key 60 (C5), velocity 100.
	track.push_back(0x00);
	track.push_back(0x90);
	track.push_back(60);
	track.push_back(100);
	// +480 ticks: note off, key 60.
	push_vlq(track, 480);
	track.push_back(0x80);
	track.push_back(60);
	track.push_back(0x40);
	// +480 ticks: note on, key 64 (E5).
	push_vlq(track, 480);
	track.push_back(0x90);
	track.push_back(64);
	track.push_back(100);
	// +480 ticks: note off, key 64.
	push_vlq(track, 480);
	track.push_back(0x80);
	track.push_back(64);
	track.push_back(0x40);
	// End of track.
	track.push_back(0x00);
	track.push_back(0xFF);
	track.push_back(0x2F);
	track.push_back(0x00);

	std::vector<uint8_t> buf;
	put_fourcc(buf, "MThd");
	put_u32_be(buf, 6);
	put_u16_be(buf, 0); // format 0
	put_u16_be(buf, 1); // track count
	put_u16_be(buf, 480); // division
	put_fourcc(buf, "MTrk");
	put_u32_be(buf, (uint32_t)track.size());
	buf.insert(buf.end(), track.begin(), track.end());

	PackedByteArray result;
	result.resize((int)buf.size());
	memcpy(result.ptrw(), buf.data(), buf.size());
	return result;
}

// The resampled mix path reads AudioDriver/AudioServer singletons, which
// Main::setup2() creates but Main::test_setup() does not. Bootstrap them
// with the dummy driver (first entry of the driver table). The dummy driver's
// constructor only sets its own static singleton, so
// AudioDriver::set_singleton() must be called explicitly. The test framework
// (GodotTestCaseListener::test_case_end) deletes the AudioServer singleton
// after every test case, so recreate it whenever it is missing.
static void bootstrap_audio() {
	static bool driver_ready = false;
	if (!driver_ready) {
		driver_ready = true;
		AudioDriver *driver = AudioDriverManager::get_driver(0);
		if (driver != nullptr) {
			driver->set_singleton();
			driver->init();
		}
	}
	if (AudioServer::get_singleton() == nullptr) {
		memnew(AudioServer);
	}
}

static Ref<MidiStream> make_stream(bool p_loop = false) {
	Ref<SoundFontResource> sf_res = memnew(SoundFontResource);
	sf_res->set_data(make_test_soundfont());
	Ref<MidiFileResource> midi_res = memnew(MidiFileResource);
	midi_res->set_data(make_test_midi());

	Ref<MidiStream> stream = memnew(MidiStream);
	stream->set_soundfont(sf_res);
	stream->set_midi(midi_res);
	stream->set_loop(p_loop);
	return stream;
}

TEST_CASE("[MidiStream] stream length is derived from the MIDI file") {
	Ref<MidiStream> stream = make_stream();
	CHECK(stream->get_length() == doctest::Approx(1.5).epsilon(0.1));
	CHECK(!stream->has_loop());
	CHECK(!stream->is_monophonic());
}

TEST_CASE("[MidiStream] SF2 synthesizer renders audio from the MIDI sequence") {
	bootstrap_audio();
	Ref<MidiStream> stream = make_stream();

	Ref<AudioStreamPlayback> playback = stream->instantiate_playback();
	REQUIRE(playback.is_valid());
	playback->start(0.0);
	CHECK(playback->is_playing());

	AudioFrame buf[512];
	int mixed = playback->mix(buf, 1.0, 512);
	CHECK(mixed == 512);

	float energy = 0.0f;
	for (int i = 0; i < 512; i++) {
		energy += (float)std::fabs(buf[i].left) + (float)std::fabs(buf[i].right);
	}
	CHECK(energy > 0.0f);
	CHECK(playback->get_playback_position() > 0.0);
}

TEST_CASE("[MidiStream] playback stops after the last note ends") {
	bootstrap_audio();
	Ref<MidiStream> stream = make_stream();

	Ref<AudioStreamPlayback> playback = stream->instantiate_playback();
	REQUIRE(playback.is_valid());
	playback->start(0.0);

	// 75000 frames at 44100 Hz = 1.70 s > 1.5 s song + sample tails.
	AudioFrame buf[512];
	int total = 0;
	while (total < 75000) {
		int mixed = playback->mix(buf, 1.0, 512);
		if (mixed == 0) {
			break;
		}
		total += mixed;
	}
	CHECK(!playback->is_playing());
	CHECK(playback->get_loop_count() == 0);
	CHECK(playback->get_playback_position() <= 1.6);
}

TEST_CASE("[MidiStream] loop restarts the song") {
	bootstrap_audio();
	Ref<MidiStream> stream = make_stream(true);
	CHECK(stream->has_loop());

	Ref<AudioStreamPlayback> playback = stream->instantiate_playback();
	REQUIRE(playback.is_valid());
	playback->start(0.0);

	// 100000 frames at 44100 Hz = 2.27 s: one full pass (1.5 s) + 0.77 s of the second.
	AudioFrame buf[512];
	int total = 0;
	while (total < 100000) {
		int mixed = playback->mix(buf, 1.0, 512);
		if (mixed == 0) {
			break;
		}
		total += mixed;
	}
	CHECK(playback->is_playing());
	CHECK(playback->get_loop_count() >= 1);
	CHECK(playback->get_playback_position() < 1.0);
}

TEST_CASE("[MidiStream] manual note triggering") {
	bootstrap_audio();
	Ref<SoundFontResource> sf_res = memnew(SoundFontResource);
	sf_res->set_data(make_test_soundfont());
	Ref<MidiStream> stream = memnew(MidiStream);
	stream->set_soundfont(sf_res);

	Ref<MidiStreamPlayback> playback = stream->instantiate_playback();
	REQUIRE(playback.is_valid());
	playback->start(0.0);

	playback->note_on(0, 60, 1.0);
	AudioFrame buf[512];
	int mixed = playback->mix(buf, 1.0, 512);
	CHECK(mixed == 512);
	float energy = 0.0f;
	for (int i = 0; i < 512; i++) {
		energy += (float)std::fabs(buf[i].left) + (float)std::fabs(buf[i].right);
	}
	CHECK(energy > 0.0f);

	playback->note_off(0, 60);
	playback->note_off_all();
	playback->stop();
	CHECK(!playback->is_playing());
}

} // namespace MidiStreamTest
