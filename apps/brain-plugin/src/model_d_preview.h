#pragma once
#include "dsp/analog_env.h"
#include "model_d_noise.h"
#include "dsp/ladder_stage_core.h"
#include "dsp/lfo.h"
#include "dsp/polyblep_core.h"
#include "dsp/voice_spread.h"
#include <array>
#include <cstdint>

namespace gf {
/** @brief Subconjunto GFP del Model D admitido por el piloto. */
struct ModelDSoundPatch {
    std::array<uint8_t, 3> wave{0, 0, 0};
    std::array<float, 3> level{.45f, .45f, .3f};
    float detune = 5, interval3 = -12, detune3 = -3, pulse_width = .5f, noise_level = 0;
    float amp_decay = 200, amp_sustain = .8f, filter_attack = 20, filter_decay = 300;
    float filter_sustain = .4f, filter_release = 400, filter_amount = .6f, drive = 1.35f;
    float glide_ms = 0, velocity_amp = 0, velocity_filter = 0;
    bool decay_switch = false, noise_pink = false;
    bool vco3_keyboard = true, vco3_low = false;
    float overdrive = 0, filter_keyboard = 0;
    float mod_mix = 0, lfo_rate = 2, lfo_cutoff = 0, lfo_pitch = 0, lfo_pwm = 0, lfo_beats = .5f, drift = 0;
    bool osc_mod = false, filter_mod = false;
    dsp::LfoShape lfo_shape = dsp::LfoShape::Tri;
};
/** @brief Primer piloto mono Model D con DSP portable compartido con el firmware. */
class ModelDPreview {
public:
    /** @brief Prepara el motor para la frecuencia de muestreo del host. */
    void prepare(double sample_rate);
    /** @brief Reinicia voces, osciladores y estados de filtro. */
    void reset();
    /** @brief Agrega una nota a la pila mono con prioridad de última nota. */
    void note_on(uint8_t note, float velocity);
    /** @brief Libera una nota o la retiene mientras el pedal está activo. */
    void note_off(uint8_t note);
    /** @brief Controla el pedal de sustain. */
    void set_sustain(bool down);
    /** @brief Aplica pitch bend MIDI de ±2 semitonos. */
    void set_pitch_bend(int value);
    /** @brief Aplica la razón instantánea de afinación del efecto tape. */
    void set_tape_pitch(float ratio);
    /** @brief Ajusta cutoff, resonancia, ataque y release. */
    void set_parameters(float cutoff, float resonance, float attack, float release);
    /** @brief Aplica parámetros GFP disponibles sin reiniciar el estado de audio. */
    void set_sound_patch(const ModelDSoundPatch& patch);
    /** @brief Actualiza rueda, presión y sincronización del LFO con el host. */
    void set_modulation_control(float wheel, float pressure, float bpm, bool sync);
    /** @brief Genera una muestra mono del piloto. */
    float process_sample();
private:
    void release_note(uint8_t note);
    std::array<dsp::PolyBlepCore, 3> oscillators_{};
    dsp::LadderStageCore filter_{44100};
    dsp::AnalogEnv amp_, filter_env_;
    ModelDNoise noise_;
    dsp::Lfo lfo_;
    dsp::Drift note_drift_;
    ModelDSoundPatch patch_;
    std::array<float, 3> ratios_{1.0f, 1.003935f, .498500f}, levels_{.43125f, .43125f, .2875f};
    std::array<uint8_t, 8> notes_{};
    std::array<bool, 128> deferred_{};
    uint8_t count_ = 0;
    bool sustain_ = false;
    float fs_ = 44100, tick_ = 0, hz_ = 440, target_hz_ = 440, bend_ = 1, tape_pitch_ = 1;
    float velocity_gain_ = 1, velocity_brightness_ = 1;
    float noise_gain_ = 0;
    float drift_pitch_ = 1, drift_cutoff_ = 1;
    float wheel_ = 0, pressure_ = 0, bpm_ = 120, lfo_pitch_ = 1, lfo_cutoff_ = 1, lfo_pwm_ = 0;
    float mod_phase_ = 0, noise_smooth_ = 0, osc_mod_ratio_ = 1, mod_signal_ = 0;
    uint32_t mod_rng_ = 0x5EED1234u;
    bool lfo_sync_ = false;
    float cutoff_ = 800, resonance_ = .707f, attack_ = 10, release_ = 400;
    double drift_left_ = 0;
};
}
