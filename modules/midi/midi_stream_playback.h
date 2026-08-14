/**************************************************************************/
/*  midi_stream_playback.h                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            GOBLIN ENGINE                               */
/**************************************************************************/

#ifndef MIDI_STREAM_PLAYBACK_H
#define MIDI_STREAM_PLAYBACK_H

#include "core/templates/vector.h"
#include "servers/audio/audio_stream.h"

class MidiStream;
struct tsf;
struct tml_message;

// Playback side of MidiStream. Lazily loads the SoundFont into the
// TinySoundFont synthesizer and the MIDI file into the TinyMidiLoader event
// list, then renders scheduled events through the synthesizer in the audio
// thread. Also exposes live note triggering (note_on/note_off) for manual
// or gameplay-driven playback.
class MidiStreamPlayback : public AudioStreamPlaybackResampled {
	GDCLASS(MidiStreamPlayback, AudioStreamPlaybackResampled);

public:
	MidiStreamPlayback();
	~MidiStreamPlayback();

	void set_stream(const Ref<MidiStream> &p_stream);
	Ref<MidiStream> get_stream() const;

	// Manual note triggering (live playback, soundfont only).
	void note_on(int32_t p_preset_index, int32_t p_key, float p_velocity);
	void note_off(int32_t p_preset_index, int32_t p_key);
	void note_off_all();

	virtual void start(double p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;
	virtual int get_loop_count() const override;
	virtual double get_playback_position() const override;
	virtual void seek(double p_time) override;

protected:
	virtual int _mix_internal(AudioFrame *p_buffer, int p_frames) override;
	virtual float get_stream_sampling_rate() override;

	static void _bind_methods();

private:
	void _ensure_loaded();
	void _reset_synth();
	void _apply_event(const tml_message *p_msg);
	void _process_events_until_ms(uint32_t p_time_ms);
	void _seek_internal(double p_position_sec);

	Ref<MidiStream> stream;

	int sample_rate = 44100;
	bool playing = false;
	int32_t loop_count = 0;
	double position_sec = 0.0;

	tsf *sf = nullptr;
	tml_message *midi = nullptr;
	tml_message *event_cursor = nullptr;
	uint32_t midi_length_ms = 0;

	Vector<float> interleaved;
};

#endif // MIDI_STREAM_PLAYBACK_H
