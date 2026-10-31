#pragma once
#include "dsp/polyblep_core.h"
#include "pjrc_white_noise.h"
#include "dsp/ladder_stage_core.h"
#include "dsp/analog_env.h"
#include "dsp/lfo.h"
#include "dsp/juno_chorus_core.h"
#include "dsp/voice_spread.h"
#include <array>
#include <optional>
#include <cstdint>

namespace gf {
/** @brief Piloto Juno con DSP compartido y patch inicial; todavía no es el engine completo. */
class JunoPreview {
public:
    /** @brief Prepara seis voces para la frecuencia del DAW. */
    void prepare(double sample_rate);
    /** @brief Limpia voces y chorus. */
    void reset();
    /** @brief Dispara una nota MIDI. */
    void note_on(uint8_t note, float velocity);
    /** @brief Libera o retiene una nota según el pedal. */
    void note_off(uint8_t note);
    /** @brief Cambia el estado del pedal. */
    void set_sustain(bool down);
    /** @brief Ajusta pitch bend de ±2 semitonos. */
    void set_pitch_bend(int value);
    /** @brief Ajusta los controles básicos del piloto. */
    void set_parameters(float cutoff, float resonance, float attack, float release);
    /** @brief Aplica los controles de oscilador del patch Juno. */
    void set_oscillator(uint16_t wave, float sub, float noise, float pulse_width,
                        uint8_t chorus, uint8_t hpf, uint8_t range, bool gate, float hiss);
    /** @brief Configura envolventes, filtro y sensibilidad dinámica del patch. */
    void set_patch(float decay, float sustain, float filter_attack, float filter_decay,
                   float filter_sustain, float filter_release, float env_amount, float drive,
                   float velocity_amp, float velocity_filter);
    /** @brief Configura glide en milisegundos, voces por nota, anchura en cents y DRIFT normalizado. */
    void set_voice_modulation(float glide_ms, uint8_t unison, float cents, float drift);
    /** @brief Configura forma, velocidad, destinos y retraso del LFO compartido. */
    void set_lfo(float rate, dsp::LfoShape shape, float cutoff, float pitch, float pwm, float beats, float delay_ms);
    /** @brief Configura rueda MIDI, tempo y sincronización del LFO. */
    void set_lfo_control(float wheel, float bpm, bool sync);
    /** @brief Genera una muestra estéreo. */
    void process_frame(float& left, float& right);
private:
    struct Voice {
        explicit Voice(float fs) : filter(fs) {}
        dsp::PolyBlepCore dco, sub;
        dsp::LadderStageCore filter;
        dsp::AnalogEnv amp, filter_env;
        dsp::VoiceSpread spread;
        int note = -1;
        float hz = 440, target_hz = 440, velocity = 1, pitch_ratio = 1, cutoff_ratio = 1;
        uint64_t age = 0;
        bool deferred = false;
        PjrcWhiteNoise noise;
    };
    std::array<std::optional<Voice>, 6> voices_;
    dsp::JunoChorusCore chorus_;
    dsp::JunoChorusCoreT<4096> chorus_high_rate_;
    float fs_ = 44100, bend_ = 1, cutoff_ = 1200, resonance_ = 0.7f, attack_ = 10, release_ = 300;
    float tick_ = 0;
    uint64_t age_ = 0;
    uint8_t next_ = 0;
    bool sustain_ = false;
    dsp::Drift drift_;
    dsp::Lfo lfo_;
    float lfo_rate_ = 2, lfo_cutoff_ = 0, lfo_pitch_ = 0, lfo_pwm_ = 0, lfo_beats_ = 0.5f;
    float lfo_delay_ms_ = 0, lfo_elapsed_ms_ = 0, wheel_ = 0, bpm_ = 120;
    float lfo_pitch_ratio_ = 1, lfo_cutoff_ratio_ = 1, pwm_mod_ = 0;
    uint16_t held_ = 0;
    bool lfo_sync_ = false;
    dsp::LfoShape lfo_shape_ = dsp::LfoShape::Tri;
    float glide_ms_ = 0, unison_cents_ = 0, unison_gain_ = 1;
    uint8_t unison_ = 1;
    uint16_t waveform_ = dsp::WAVE_SAW_AND_PULSE;
    uint8_t chorus_mode_ = 1, hpf_ = 1, range_ = 1;
    bool gate_ = false;
    float sub_level_ = 0.3f, noise_level_ = 0, pulse_width_ = 0.5f, hiss_ = 0;
    float decay_ = 200, sustain_level_ = 0.5f, filter_attack_ = 10, filter_decay_ = 200;
    float filter_sustain_ = 0.5f, filter_release_ = 300, env_amount_ = 0.5f, drive_ = 1.1f;
    float velocity_amp_ = 0.65f, velocity_filter_ = 0.5f;
};
}
