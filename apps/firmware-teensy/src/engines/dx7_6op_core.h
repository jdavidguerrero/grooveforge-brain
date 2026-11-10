#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include "msfa/env.h"
#include "msfa/pitchenv.h"
#include "msfa/lfo.h"
#include "msfa/freqlut.h"

namespace gf::dx7six {
constexpr uint8_t ENGINE_ID = 7;
constexpr const char* ENGINE_NAME = "dx7_6op";
constexpr int BLOCK = 64;
constexpr int VOICES = 6;
/** @brief VCED in wire order (OP6 first), with independent OP1-first enable mask. */
struct Patch {
    std::array<uint8_t, 155> vced{};
    uint8_t enabled = 63;
};
/** @brief Bus flags in OP6→OP1 evaluation order; algorithm index is zero based. */
struct Algorithm { uint8_t ops[6]; };
extern const Algorithm algorithms[32];
/** @brief Own INIT voice; no Yamaha ROM data. */
Patch init_patch();
/** @brief Strict parameter validation, including 7-bit voice names. */
bool valid_patch(const Patch& patch);
/** @brief Output level and keyboard curves, exposed for reference validation. */
int scale_level(int note, int breakpoint, int left_depth, int right_depth, int left_curve, int right_curve);
/** @brief One note; all state and control ticks belong to its synth instance. */
class Voice {
public:
    /** @brief Prepare control generators outside the audio callback. */
    void prepare(double fs);
    /** @brief Latch note/scaling/envelopes; preserve oscillator phase if sync is off. */
    void note_on(const Patch& p, int note, int velocity);
    /** @brief Enter R4/L4 for amplitude and pitch envelopes. */
    void note_off();
    /** @brief Recalculate targets and scaling while retaining phase and envelope stage. */
    void edit_patch(const Patch& p);
    /** @brief Add 64 unnormalized Q24 samples, without allocation or locks. */
    void compute(const Patch& p, msfa::Freqlut& freq, int32_t lfo, int32_t delay, int32_t bend, int32_t* out);
    /** @brief True while any audible carrier can still contribute. */
    bool active() const { return active_; }
private:
    void apply_patch(const Patch& p, bool retrigger);
    msfa::Env env_[6]{};
    msfa::PitchEnv pitch_{};
    int32_t base_[6]{}, gain_[6]{}, fb_[2]{};
    uint32_t phase_[6]{};
    bool active_ = false, released_ = false;
    int note_ = 60, velocity_ = 100;
};
/** @brief Shared six-voice DX7 engine. Call prepare before use; one instance per audio thread. */
class Synth {
public:
    /** @brief Prepare at 8–192 kHz and clear notes; defaults to 44.1 kHz for invalid rates. */
    void prepare(double fs);
    /** @brief Kill notes and reset LFO/bend/sustain. */
    void reset();
    /** @brief Validate and load a patch atomically at the caller's audio boundary; clears notes. */
    bool set_patch(const Patch& p);
    /** @brief Validate live edits without clearing voices, sustain, phase or LFO delay. */
    bool edit_patch(const Patch& p);
    /** @brief Current patch, including the OP enable mask. */
    const Patch& patch() const { return patch_; }
    /** @brief Allocate idle, released, then oldest held voice; velocity is MIDI 1–127. */
    void note_on(uint8_t note, uint8_t velocity);
    /** @brief Release all matching keys, respecting sustain. */
    void note_off(uint8_t note);
    /** @brief Sustain pedal; releases deferred notes when lifted. */
    void set_sustain(bool down);
    /** @brief MIDI pitch bend, ±3 semitones (MSFA default). */
    void set_pitch_bend(int value);
    /** @brief Generate mono float audio, any caller block size, without allocation. */
    void render(float* out, size_t count);
    /** @brief Generate one mono sample (same scheduler as render). */
    float process_sample();
private:
    struct Slot { Voice voice{}; int note = -1; uint64_t age = 0; bool deferred = false; };
    Patch patch_ = init_patch();
    std::array<Slot, VOICES> voices_{};
    msfa::Freqlut freq_{};
    msfa::Lfo lfo_{};
    std::array<int32_t, BLOCK> buffer_{};
    double fs_ = 44100;
    int position_ = BLOCK;
    int32_t bend_ = 0;
    uint64_t age_ = 0;
    bool sustain_ = false;
};
}
