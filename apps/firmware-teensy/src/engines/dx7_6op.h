#pragma once
// AudioStream/block allocation are official PJRC; this generator/adapter is custom.
#include <Audio.h>
#include "dx7_6op_core.h"
#include <algorithm>
#include <cmath>
/** @brief Six-voice custom AudioStream adapter, engine id 7; see theory/dx7-6op.md. */
class DX7SixEngine : public AudioStream {
public:
    DX7SixEngine() : AudioStream(0, nullptr) {}
    /** @brief Prepare the shared core with the actual firmware audio clock. */
    void begin(float volume = 1.0f) { gain_ = volume; core_.prepare(AUDIO_SAMPLE_RATE_EXACT); }
    /** @brief Enable ISR processing. */
    void set_dsp_enabled(bool enabled) { on_ = enabled; }
    /** @brief Load validated voice, clearing held notes. */
    bool setPatch(const gf::dx7six::Patch& p) { AudioNoInterrupts(); const bool ok = core_.set_patch(p); AudioInterrupts(); return ok; }
    /** @brief Apply live edits to held notes. */
    bool editPatch(const gf::dx7six::Patch& p) { AudioNoInterrupts(); const bool ok = core_.edit_patch(p); AudioInterrupts(); return ok; }
    /** @brief Access patch outside ISR. */
    const gf::dx7six::Patch& patch() const { return core_.patch(); }
    /** @brief Trigger normalized MIDI velocity. */
    void noteOn(uint8_t note, float velocity = 1) {
        if (!std::isfinite(velocity)) return;
        AudioNoInterrupts(); core_.note_on(note, uint8_t(std::lround(std::clamp(velocity, 0.0f, 1.0f) * 127))); AudioInterrupts();
    }
    /** @brief Release note. */
    void noteOff(uint8_t note) { AudioNoInterrupts(); core_.note_off(note); AudioInterrupts(); }
    /** @brief Apply sustain pedal in the shared core. */
    void setSustain(bool down) { AudioNoInterrupts(); core_.set_sustain(down); AudioInterrupts(); }
    /** @brief Apply MIDI bend in unsigned 14-bit units. */
    void setPitchBend(int value) { AudioNoInterrupts(); core_.set_pitch_bend(value); AudioInterrupts(); }
    /** @brief Kill held and sustained voices. */
    void allNotesOff() { AudioNoInterrupts(); core_.reset(); AudioInterrupts(); }
    /** @brief Existing engine API exposes itself as audio output. */
    AudioStream& getOutput() { return *this; }
    /** @brief Official AudioStream scheduling entry. */
    void update() override {
        if (!on_) return;
        auto* block = allocate(); if (!block) return;
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i)
            block->data[i] = int16_t(std::clamp(core_.process_sample() * gain_, -1.0f, 1.0f) * 32767);
        transmit(block); release(block);
    }
private:
    gf::dx7six::Synth core_;
    volatile bool on_ = false;
    float gain_ = 1;
};
