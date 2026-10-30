#pragma once

#include "engines/dx7_core.h"
#include "dsp/analog_env.h"
#include "dsp/ladder_stage_core.h"
#include "dsp/voice_spread.h"
#include "dsp/lfo.h"
#include <array>
#include <optional>
#include <cstdint>

namespace gf {
/** @brief Piloto DX7: seis voces, patch E.PIANO y filtro de tono. */
class Dx7Preview {
public:
    /** @brief Inicializa las voces para la frecuencia del host. */
    void prepare(double fs);
    /** @brief Limpia notas y colas. */
    void reset();
    /** @brief Dispara una nota MIDI con velocity normalizada. */
    void note_on(uint8_t note, float velocity);
    /** @brief Libera una nota o la retiene con sustain. */
    void note_off(uint8_t note);
    /** @brief Ajusta el pedal de sustain. */
    void set_sustain(bool down);
    /** @brief Ajusta pitch bend MIDI con rango de dos semitonos. */
    void set_pitch_bend(int value);
    /** @brief Aplica operadores FM, filtro de tono y envolvente común del preset. */
    void set_patch(const Dx7Patch& patch, float cutoff, float resonance, float drive,
                   float attack, float decay, float sustain, float release, float velocity_depth);
    /** @brief Obtiene el patch de operadores activo en el hilo de audio. */
    const Dx7Patch& operator_patch() const { return patch_; }
    /** @brief Actualiza los operadores y sincroniza el release de la VCA. */
    void set_operator_patch(const Dx7Patch& patch);
    /** @brief Ajusta número de voces por nota, desafinación y DRIFT. */
    void set_unison(uint8_t count, float cents, float drift);
    /** @brief Configura glide y modulación LFO. */
    void set_modulation(float glide_ms, float rate, dsp::LfoShape shape, float pitch, float amp, float beats);
    /** @brief Aplica rueda MIDI, tempo y sincronización. */
    void set_lfo_control(float wheel, float bpm, bool sync);
    /** @brief Recibe la modulación de pitch de TAPE. */
    void set_tape_pitch(float ratio) { tape_pitch_ = ratio; }
    /** @brief Genera una muestra mono. */
    float process_sample();
private:
    struct Voice { Dx7VoiceCore core; dsp::AnalogEnv amp; int note = -1; uint64_t age = 0; float hz = 440, target_hz = 440, ratio = 1; bool deferred = false; };
    void release(uint8_t note);
    std::array<Voice, 6> voices_{};
    std::optional<dsp::LadderStageCore> tone_;
    Dx7Patch patch_;
    float fs_ = 44100, bend_ = 1;
    float velocity_depth_ = 1.0f;
    float amp_attack_ = 2.0f, amp_decay_ = 50.0f, amp_sustain_ = 1.0f, amp_release_ = 600.0f;
    float amp_release_requested_ = 600.0f;
    uint64_t age_ = 0;
    uint8_t next_ = 0;
    bool sustain_ = false;
    dsp::Drift drift_;
    dsp::Lfo lfo_;
    uint8_t unison_ = 1;
    float unison_cents_ = 0, unison_gain_ = 1, glide_ms_ = 0;
    float lfo_rate_ = 2, lfo_pitch_ = 0, lfo_amp_ = 0, lfo_beats_ = 0.5f, wheel_ = 0, lfo_gain_ = 1;
    float lfo_frequency_ = 1, control_accumulator_ = 0;
    float tape_pitch_ = 1;
    dsp::LfoShape lfo_shape_ = dsp::LfoShape::Tri;
    bool lfo_sync_ = false;
    float lfo_bpm_ = 120;
};
}
