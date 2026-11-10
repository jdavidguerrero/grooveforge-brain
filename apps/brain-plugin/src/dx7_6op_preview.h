#pragma once
#include "engines/dx7_6op_core.h"
#include "engines/dx7_6op_params.h"
#include "engines/dx7_6op_sysex.h"
#include <algorithm>
#include <cmath>
namespace gf {
/** @brief JUCE-free preview; same six-voice scheduler as firmware, no extra ADSR/filter. */
class Dx7SixPreview {
public:
    /** @brief Initialize at the host's sample rate. */
    void prepare(double fs) { synth_.prepare(fs); }
    /** @brief Clear keys, modulation and audio tail. */
    void reset() { synth_.reset(); }
    /** @brief Validated patch load at an audio boundary. */
    bool set_patch(const dx7six::Patch& p) { return synth_.set_patch(p); }
    /** @brief Apply live edits while retaining sounding envelopes. */
    bool edit_patch(const dx7six::Patch& p) { return synth_.edit_patch(p); }
    /** @brief Read active patch. */
    const dx7six::Patch& patch() const { return synth_.patch(); }
    /** @brief Trigger MIDI note with normalized velocity; zero means note off. */
    void note_on(uint8_t note, float velocity) {
        if (!std::isfinite(velocity)) return;
        synth_.note_on(note, uint8_t(std::lround(std::clamp(velocity, 0.0f, 1.0f) * 127)));
    }
    /** @brief Release key. */
    void note_off(uint8_t note) { synth_.note_off(note); }
    /** @brief Apply sustain pedal. */
    void set_sustain(bool on) { synth_.set_sustain(on); }
    /** @brief Apply MIDI bend, ±3 semitones. */
    void set_pitch_bend(int value) { synth_.set_pitch_bend(value); }
    /** @brief Render mono at the prepared rate. */
    float process_sample() { return synth_.process_sample(); }
private:
    dx7six::Synth synth_;
};
/** @brief Decode GFP v6 or complete SysEx, preserving patch on failure. */
bool decode_dx7_6op_preset(const uint8_t* bytes, size_t size, dx7six::Patch& out, uint8_t bank_index = 0);
}
