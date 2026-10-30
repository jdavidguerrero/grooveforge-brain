#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "dsp/analog_env.h"
#include "dsp/ladder_stage_core.h"
#include "dsp/voice_spread.h"
#include "dsp/lfo.h"
#include "dsp/output_stage_core.h"
#include "dsp/drive_core.h"
#include "dsp/pump.h"
#include "dsp/tape_core.h"
#include "dsp/tape_modulation.h"
#include "dsp/phaser_core.h"
#include "dsp/pingpong_delay_core.h"
#include "dsp/arp_core.h"
#include "dsp/chord_memory.h"
#include "engines/prophet_osc_core.h"
#include "phase_chorus_preview.h"
#include "plate_reverb_preview.h"
#include "modal_reverb_preview.h"

namespace gf {

/** @brief Primer corte del Prophet; no implementa aún el instrumento completo del firmware. */
class ProphetPreview {
public:
    /** @brief Prepara voces independientes para la frecuencia de muestreo del host. */
    void prepare(double sample_rate);
    /** @brief Desactiva todas las voces. */
    void reset();
    /** @brief Ajusta controles básicos; tiempos expresados en milisegundos. */
    void set_parameters(float cutoff_hz, float resonance, float attack_ms, float release_ms);
    /** @brief Dispara una nota MIDI con velocity normalizada. */
    void note_on(uint8_t note, float velocity);
    /** @brief Inicia el release de la nota MIDI. */
    void note_off(uint8_t note);
    /** @brief Mantiene o libera las notas soltadas mientras se pisa el pedal. */
    void set_sustain(bool down);
    /** @brief Aplica pitch bend MIDI de 14 bits con rango de ±2 semitonos. */
    void set_pitch_bend(int value);
    /** @brief Ajusta la rueda de modulación normalizada. */
    void setWheel(float amount) { wheel_ = amount; }
    /** @brief Ajusta LFO libre y profundidades normalizadas para cutoff, pitch y PWM. */
    void set_lfo(float rate, dsp::LfoShape shape, float cutoff, float pitch, float pwm);
    /** @brief Selecciona sincronización del LFO con BPM del DAW. */
    void set_lfo_sync(bool enabled, float bpm);
    /** @brief Ajusta la longitud del ciclo sincronizado en beats. */
    void set_lfo_division(float beats);
    /** @brief Configura el efecto DRIVE de la cadena SYNTH del preset. */
    void set_synth_drive(bool enabled, uint8_t type, float amount, float tone, float mix);
    /** @brief Configura saturación TAPE y profundidades normalizadas de wow y flutter. */
    void set_synth_tape(bool enabled, float drive, float mix, float wow = 0.0f, float flutter = 0.0f);
    /** @brief Configura el PHASER estéreo de la ranura CHORUS. */
    void set_synth_phaser(bool enabled, float depth, float rate, float mix);
    /** @brief Configura el chorus de la cadena SYNTH. */
    void set_synth_chorus(bool enabled, float depth, float rate, float mix);
    /** @brief Vincula dos líneas de delay reservadas por el host, sin tomar su propiedad. */
    void set_delay_memory(int16_t* left, int16_t* right, uint32_t length);
    /** @brief Configura delay estéreo con división musical en beats. */
    void set_synth_delay(bool enabled, float beats, float feedback, float tone, float mix);
    /** @brief Vincula la memoria de líneas de la reverb PLATE. */
    void set_plate_memory(int16_t* memory, size_t count);
    /** @brief Configura PLATE con los parámetros normalizados del GFP. */
    void set_synth_plate(bool enabled, float decay, uint8_t size, float mix);
    /** @brief Configura la reverb modal de cuatro materiales. */
    void set_synth_modal(bool enabled, uint8_t material, uint8_t size, float decay, float mix);
    /** @brief Ajusta el PUMP compartido de la cadena SYNTH. */
    void set_pump(bool enabled, const dsp::PumpParams& parameters);
    /** @brief Configura el arpegiador GROOVE con las unidades del reloj musical. */
    void set_arp(bool enabled, dsp::ArpCore::Mode mode, float step, float gate, uint8_t octaves, float swing, bool latch);
    /** @brief Configura CHORD y el retraso STRUM en milisegundos por nota. */
    void set_chord(bool enabled, dsp::ChordMemory::Type type, uint8_t inversion, uint8_t key, float strum_ms);
    /** @brief Configura voces por nota y ancho total de desafinación en cents. */
    void set_unison(uint8_t voices, float cents);
    /** @brief Ajusta la variación por nota de pitch y cutoff de la macro DRIFT. */
    void set_drift(float amount);
    /** @brief Recibe tempo, posición opcional y estado del transporte del DAW. */
    void set_transport(float bpm, bool has_position, double position_beats, bool running);
    /** @brief Setters de OSC/MOD compatibles con el mapeo compartido del firmware. */
    void setOscAFreq(float x) { osc_a_st_ = x; }
    void setOscAPw(float x) { pw_a_ = x; }
    void setOscAShape(bool a, bool b) { saw_a_ = a; pulse_a_ = b; }
    void setSync(bool x) { sync_ = x; }
    void setOscBInterval(float x) { osc_b_st_ = x; }
    void setOscBFine(float x) { osc_b_fine_ = x; }
    void setOscBPw(float x) { pw_b_ = x; }
    void setOscBKeyboard(bool x) { b_keyboard_ = x; }
    void setOscBShape(bool a, bool b, bool c) { saw_b_ = a; tri_b_ = b; pulse_b_ = c; }
    void setOscBLoFreq(bool x) { b_lo_ = x; }
    void setMixer(float a, float b, float n) { level_a_ = a; level_b_ = b; level_noise_ = n; }
    void setGlide(float x) { glide_ms_ = x; }
    void setFilterKeyboard(float x) { filter_keyboard_ = x; }
    /** @brief Setters de envelopes y dinámica compatibles con el patch compartido. */
    void setAmpEnv(float attack, float decay, float sustain, float release);
    void setFilterEnv(float attack, float decay, float sustain, float release);
    void setFilterEnvAmount(float amount) { filter_env_amount_ = amount; }
    void setDrive(float drive);
    void setVelocityDepth(float amp, float filter) { velocity_amp_ = amp; velocity_filter_ = filter; }
    void setPolyMod(float a, float b, bool f, bool p, bool c) {
        pm_env_ = a; pm_b_ = b; pm_freq_ = f; pm_pw_ = p; pm_filter_ = c;
    }
    void setWheelMod(float mix, bool fa, bool fb, bool pa, bool pb, bool filter) {
        wheel_mix_ = mix; wheel_freq_a_ = fa; wheel_freq_b_ = fb; wheel_pw_a_ = pa; wheel_pw_b_ = pb; wheel_filter_ = filter;
    }
    /** @brief Suma una muestra mono de todas las voces. */
    float process_sample();
    /** @brief Genera un frame estéreo con los efectos activos. */
    void process_frame(float& left, float& right);
    /** @brief Procesa una fuente mono externa por el bus SYNTH FX. */
    void process_external_frame(float input, float& left, float& right);
    /** @brief Razón de pitch de wow/flutter calculada por el bus de efectos. */
    float tape_pitch_ratio() const { return tape_pitch_; }
    /** @brief Dirige eventos GROOVE a otro motor sin tomar propiedad del contexto. */
    void set_voice_sink(void* context, void (*on)(void*, uint8_t, float), void (*off)(void*, uint8_t));

private:
    void process_frame_impl(float& left, float& right, bool external, float input);
    void voice_note_on(uint8_t note, float velocity);
    void voice_note_off(uint8_t note);
    static void arp_note_on(uint8_t note, uint8_t velocity, void* context);
    static void arp_note_off(uint8_t note, void* context);
    void configure_arp();
    void* voice_sink_context_ = nullptr;
    void (*voice_sink_on_)(void*, uint8_t, float) = nullptr;
    void (*voice_sink_off_)(void*, uint8_t) = nullptr;
    void release_root(uint8_t root);
    void start_chord_note(uint8_t root, uint8_t index);
    void tick_strum();
    struct ChordNote { uint8_t note = 0; float delay_ms = 0.0f; bool started = false; };
    struct ChordRoot { std::array<ChordNote, dsp::ChordMemory::MAX> notes{}; uint8_t count = 0; float velocity = 0.0f; bool deferred = false; };
    struct Voice {
        explicit Voice(float fs);
        Prophet5OscCore osc;
        dsp::LadderStageCore filter;
        dsp::AnalogEnv amp;
        dsp::AnalogEnv filter_env;
        dsp::VoiceSpread spread;
        float hz = 440.0f;
        float target_hz = 440.0f;
        float velocity = 1.0f;
        float unison_ratio = 1.0f;
        float drift_cutoff = 1.0f;
        int note = -1;
        bool deferred_release = false;
        uint64_t age = 0;
    };
    std::array<std::optional<Voice>, 5> voices_;
    float sample_rate_ = 44100.0f;
    float cutoff_ = 2000.0f, resonance_ = 0.7f, attack_ = 10.0f, release_ = 500.0f;
    float decay_ = 300.0f, sustain_level_ = 0.7f;
    float filter_attack_ = 10.0f, filter_decay_ = 300.0f, filter_sustain_ = 0.7f, filter_release_ = 500.0f;
    float drive_ = 1.2f;
    uint64_t age_ = 0;
    bool sustain_ = false;
    float bend_ratio_ = 1.0f;
    float control_accumulator_ = 0.0f;
    uint8_t next_voice_ = 0;
    float osc_a_st_ = 0, osc_b_st_ = 0, osc_b_fine_ = 0, pw_a_ = 0.5f, pw_b_ = 0.5f;
    float level_a_ = 0.6f, level_b_ = 0.4f, level_noise_ = 0, glide_ms_ = 0, filter_keyboard_ = 0;
    float pm_env_ = 0, pm_b_ = 0, wheel_ = 0, wheel_mix_ = 0, wheel_noise_ = 0;
    float filter_env_amount_ = 0.4f, velocity_amp_ = 0.65f, velocity_filter_ = 0.5f;
    bool saw_a_ = true, pulse_a_ = false, sync_ = false;
    bool saw_b_ = true, tri_b_ = false, pulse_b_ = false, b_keyboard_ = true, b_lo_ = false;
    bool pm_freq_ = false, pm_pw_ = false, pm_filter_ = false;
    bool wheel_freq_a_ = true, wheel_freq_b_ = true, wheel_pw_a_ = false, wheel_pw_b_ = false, wheel_filter_ = false;
    uint32_t rng_ = 0x9E3779B9u;
    dsp::Lfo lfo_;
    float lfo_rate_ = 2.0f;
    dsp::LfoShape lfo_shape_ = dsp::LfoShape::Tri;
    float lfo_cutoff_ = 0.0f, lfo_pitch_ = 0.0f, lfo_pwm_ = 0.0f, lfo_value_ = 0.0f;
    float lfo_pitch_multiplier_ = 1.0f;
    float lfo_bpm_ = 120.0f, lfo_beats_ = 0.5f;
    bool lfo_sync_ = false;
    dsp::OutputStageCore output_stage_;
    dsp::DriveCore synth_drive_;
    bool synth_drive_enabled_ = false;
    uint8_t synth_drive_type_ = 0;
    float synth_drive_amount_ = 0.35f, synth_drive_tone_ = 0.6f, synth_drive_mix_ = 0.5f;
    dsp::TapeCore synth_tape_;
    dsp::TapeModulation tape_modulation_;
    bool synth_tape_enabled_ = false;
    float synth_tape_drive_ = 2.0f, synth_tape_mix_ = 0.6f;
    float tape_wow_ = 0.0f, tape_flutter_ = 0.0f, tape_pitch_ = 1.0f, tape_cutoff_ = 20000.0f;
    std::array<dsp::PhaserCore, 2> synth_phaser_;
    bool synth_phaser_enabled_ = false;
    float phaser_depth_ = 0.6f, phaser_rate_ = 0.5f, phaser_mix_ = 0.5f;
    std::array<PhaseChorusPreview, 2> synth_chorus_;
    bool synth_chorus_enabled_ = false;
    float chorus_depth_ = 0.7f, chorus_rate_ = 0.5f, chorus_mix_ = 0.7f;
    dsp::PingPongDelayCore synth_delay_;
    bool delay_ready_ = false, delay_enabled_ = false;
    float delay_beats_ = 0.5f, delay_feedback_ = 0.0f, delay_tone_ = 0.5f, delay_mix_ = 0.0f;
    PlateReverbPreview synth_plate_;
    bool plate_enabled_ = false;
    float plate_mix_ = 0.0f;
    std::array<ModalReverbPreview, 2> synth_modal_;
    bool modal_enabled_ = false;
    uint8_t modal_material_ = 0, modal_size_ = 1;
    float modal_decay_ = 1.0f, modal_mix_ = 0.0f;
    dsp::PumpParams pump_parameters_;
    bool pump_enabled_ = false, transport_running_ = true;
    float transport_bpm_ = 120.0f, pump_gain_ = 1.0f, pump_target_ = 1.0f;
    double transport_beats_ = 0.0;
    dsp::ArpCore arp_;
    bool arp_enabled_ = false, arp_latch_ = false;
    dsp::ArpCore::Mode arp_mode_ = dsp::ArpCore::Mode::Up;
    float arp_step_ = 0.25f, arp_gate_ = 0.6f, arp_swing_ = 0.5f;
    uint8_t arp_octaves_ = 1;
    dsp::ChordMemory chord_;
    bool chord_enabled_ = false;
    float strum_ms_ = 0.0f;
    std::array<ChordRoot, 128> roots_{};
    std::array<uint16_t, 128> sounding_notes_{};
    uint8_t unison_voices_ = 1;
    float unison_cents_ = 0.0f, unison_gain_ = 1.0f;
    dsp::Drift drift_;
};

} // namespace gf
