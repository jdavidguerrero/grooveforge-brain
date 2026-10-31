#pragma once
#include "dsp/polyblep_core.h"
#include "pjrc_white_noise.h"
#include "dsp/sem_svf_core.h"
#include "dsp/analog_env.h"
#include "dsp/lfo.h"
#include "dsp/voice_spread.h"
#include <array>
#include <cstdint>

namespace gf {
/** @brief Parámetros del subconjunto GFP reproducido por el piloto OB-6. */
struct Ob6SoundPatch {
    float shape1 = 0, shape2 = 0, level1 = .45f, level2 = .45f, sub = .1f, noise = 0;
    float pw1 = .5f, pw2 = .5f, interval = 0, detune = 7, morph = 0;
    bool bandpass = false;
    float xmod_env = 0, xmod_vco2 = 0;
    bool xmod_freq = false, xmod_morph = false, xmod_pw = false, sync = false;
    float amp_decay = 300, amp_sustain = .6f, filter_attack = 10, filter_decay = 300;
    float filter_sustain = .6f, filter_release = 500, filter_amount = .4f, drive = 1.3f;
    float velocity_amp = .65f, velocity_filter = .5f;
    float glide_ms = 0, unison_cents = 0, drift = 0;
    uint8_t unison = 1;
    float lfo_rate = 2, lfo_cutoff = 0, lfo_pitch = 0, lfo_pwm = 0, lfo_beats = .5f;
    dsp::LfoShape lfo_shape = dsp::LfoShape::Tri;
};
/** @brief Piloto inicial OB-6 con DSP compartido; no implementa el patch completo. */
class Ob6Preview {
public:
    /** @brief Prepara las voces a la frecuencia del host. */
    void prepare(double sample_rate);
    /** @brief Restablece voces, fases y pedal. */
    void reset();
    /** @brief Dispara una nota con velocity normalizada. */
    void note_on(uint8_t note, float velocity);
    /** @brief Libera una nota o la retiene con sustain. */
    void note_off(uint8_t note);
    /** @brief Cambia el pedal de sustain. */
    void set_sustain(bool down);
    /** @brief Aplica pitch bend de dos semitonos. */
    void set_pitch_bend(int value);
    /** @brief Aplica la razón de afinación instantánea calculada por el efecto tape. */
    void set_tape_pitch(float ratio);
    /** @brief Ajusta cutoff, resonancia, ataque y release. */
    void set_parameters(float cutoff, float resonance, float attack, float release);
    /** @brief Aplica el subconjunto reproducido del patch GFP. */
    void set_sound_patch(const Ob6SoundPatch& patch);
    /** @brief Ajusta rueda MIDI, presión de canal y sincronización con el tempo del host. */
    void set_lfo_control(float wheel, float bpm, bool sync, float pressure = 0.0f);
    /** @brief Genera una muestra mono. */
    float process_sample();
private:
    struct Voice {
        dsp::PolyBlepCore vco1, vco2, sub;
        PjrcWhiteNoise noise;
        dsp::SemSvfState filter;
        dsp::SemSvfCoeffs coeff;
        dsp::AnalogEnv amp, filter_env;
        dsp::VoiceSpread spread;
        int note = -1; uint64_t age = 0; bool deferred = false;
        float hz = 440, target_hz = 440, velocity = 1, pitch_ratio = 1, cutoff_ratio = 1;
    };
    std::array<Voice, 6> voices_{};
    Ob6SoundPatch patch_;
    bool patch_loaded_ = false;
    float vco2_ratio_ = 1.0040511f, tape_pitch_ = 1;
    float fs_ = 44100, cutoff_ = 2000, resonance_ = .707f, attack_ = 10, release_ = 500, bend_ = 1, tick_ = 0;
    double drift_left_ = 0;
    uint64_t age_ = 0;
    uint8_t next_ = 0;
    bool sustain_ = false;
    dsp::Drift note_drift_;
    dsp::Lfo lfo_;
    float unison_gain_ = 1, wheel_ = 0, pressure_ = 0, bpm_ = 120, lfo_pitch_ratio_ = 1, lfo_cutoff_ratio_ = 1, lfo_pwm_ = 0;
    bool lfo_sync_ = false;
};
}
