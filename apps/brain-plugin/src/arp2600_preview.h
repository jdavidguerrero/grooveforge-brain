#pragma once
#include "dsp/analog_env.h"
#include "dsp/ladder_stage_core.h"
#include "dsp/polyblep_core.h"
#include "dsp/voice_spread.h"
#include "dsp/note_stack.h"
#include "dsp/lfo.h"
#include "model_d_noise.h"
#include <array>
#include <cstdint>
#include <optional>

namespace gf {
/** @brief Parámetros sonoros GFP implementados en el piloto. */
struct Arp2600Patch {
    uint16_t wave1 = dsp::WAVE_SAWTOOTH, wave2 = dsp::WAVE_SINE;
    bool sync = false, mono = false;
    uint8_t unison = 1;
    float unison_cents = 0, drift = 0;
    float ratio = 1, pulse_width = .5f, level1 = .9f, level2 = 0, noise_level = 0, noise_color = 0;
    float decay = 300, sustain = .6f, filter_attack = 10, filter_decay = 300;
    float filter_sustain = .6f, filter_release = 500, env_amount = .5f, drive = 1.4f;
    float velocity_amp = 1, velocity_filter = 0;
    float vco3_rate = 2, vco3_pitch = 0, vco3_cutoff = 0, glide_ms = 0;
    float lfo_rate = 2, lfo_cutoff = 0, lfo_pitch = 0, lfo_pwm = 0, lfo_beats = .5f;
    dsp::LfoShape lfo_shape = dsp::LfoShape::Tri;
};
/** @brief Primer núcleo portable del ARP 2600, con cuatro voces y ring modulation. */
class Arp2600Preview {
public:
    /** @brief Prepara las voces para la frecuencia del host. */
    void prepare(double sample_rate);
    /** @brief Reinicia el estado de las voces. */
    void reset();
    /** @brief Dispara una nota. */
    void note_on(uint8_t note, float velocity);
    /** @brief Libera una nota, o la retiene si el pedal está activo. */
    void note_off(uint8_t note);
    /** @brief Aplica pitch bend MIDI en el rango fijo de ±2 semitonos. */
    void set_pitch_bend(int value);
    /** @brief Cambia el estado del pedal de sustain. */
    void set_sustain(bool down);
    /** @brief Cambia el filtro y los tiempos de ataque y liberación. */
    void set_parameters(float cutoff, float resonance, float attack, float release);
    /** @brief Aplica los parámetros GFP admitidos sin reiniciar voces. */
    void set_patch(const Arp2600Patch& patch);
    /** @brief Ajusta la cantidad de ring modulation. */
    void set_ring_mix(float amount);
    /** @brief Ajusta rueda, presión, tempo y sincronización del LFO. */
    void set_lfo_control(float wheel, float pressure, float bpm, bool sync);
    /** @brief Produce una muestra mono. */
    float process_sample();
private:
    struct Voice {
        explicit Voice(float fs) : filter(fs) {}
        dsp::PolyBlepCore vco1, vco2;
        ModelDNoise noise;
        dsp::LadderStageCore filter;
        dsp::AnalogEnv amp, filter_env;
        dsp::VoiceSpread spread;
        int note = -1;
        float hz = 440, target_hz = 440, velocity = 1, pitch_ratio = 1, cutoff_ratio = 1;
        uint64_t age = 0;
        bool deferred = false;
    };
    std::array<std::optional<Voice>, 4> voices_;
    Arp2600Patch patch_;
    dsp::NoteStack mono_notes_;
    dsp::Lfo lfo_;
    dsp::Drift drift_;
    std::array<bool, 128> mono_deferred_{};
    void release_mono_note(uint8_t note);
    float fs_ = 44100, tick_ = 0, cutoff_ = 1200, resonance_ = .707f, attack_ = 10, release_ = 500, ring_ = .2f;
    float vco3_phase_ = 0, vco3_pitch_ratio_ = 1, vco3_cutoff_ratio_ = 1, bend_ = 1;
    float wheel_ = 0, pressure_ = 0, bpm_ = 120, lfo_pitch_ratio_ = 1, lfo_cutoff_ratio_ = 1, lfo_pwm_ = 0;
    bool lfo_sync_ = false;
    uint64_t age_ = 0;
    size_t next_ = 0;
    bool sustain_ = false;
};
}
