#pragma once
#include "dsp/polyblep_core.h"
#include "dsp/sem_svf_core.h"
#include "dsp/analog_env.h"
#include "dsp/voice_spread.h"
#include <array>
#include <cstdint>

namespace gf {
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
    /** @brief Ajusta cutoff, resonancia, ataque y release. */
    void set_parameters(float cutoff, float resonance, float attack, float release);
    /** @brief Genera una muestra mono. */
    float process_sample();
private:
    struct Voice {
        dsp::PolyBlepCore vco1, vco2, sub;
        dsp::SemSvfState filter;
        dsp::SemSvfCoeffs coeff;
        dsp::AnalogEnv amp, filter_env;
        dsp::VoiceSpread spread;
        int note = -1; uint64_t age = 0; bool deferred = false;
        float hz = 440, velocity = 1;
    };
    std::array<Voice, 6> voices_{};
    float fs_ = 44100, cutoff_ = 2000, resonance_ = .707f, attack_ = 10, release_ = 500, bend_ = 1, tick_ = 0;
    double drift_left_ = 0;
    uint64_t age_ = 0;
    uint8_t next_ = 0;
    bool sustain_ = false;
};
}
