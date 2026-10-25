#pragma once

#include <math.h>
#include <stdint.h>

/**
 * @file groove_clock.h
 * @brief Reloj común del Brain: BPM y posición en beats para PUMP, ARP, DELAY y el SYNC del LFO (código custom).
 *
 * Lógica pura (native). Tres fuentes con prioridad automática (theory/09 de Studio §1): clock MIDI (24 ppq) con
 * START / CONTINUE / STOP / SONG POSITION; beat de la IA (BPM + fase); BPM interno con TAP. La posición es continua
 * (en beats desde el arranque): saber dónde cae el beat, no solo la velocidad, es lo que pone el PUMP en fase con el
 * kick. Tiempos en microsegundos (uint32_t, con vuelta cada ~71 min manejada por restas).
 */
namespace dsp {

class GrooveClock {
public:
    enum class Source : uint8_t { Auto = 0, Midi, Ai, Internal };

    static constexpr uint32_t MIDI_TIMEOUT_US = 300000;    ///< sin clock en 300 ms, la fuente MIDI se considera caída
    static constexpr uint32_t AI_TIMEOUT_US = 2000000;
    static constexpr float BPM_MIN = 30.0f, BPM_MAX = 300.0f;

    // ── Clock MIDI ────────────────────────────────────────────────────────────
    void midi_clock(uint32_t t) {
        if (midi_last_ && t - midi_last_ < 200000) {
            const float dt = float(t - midi_last_);
            midi_interval_ = midi_interval_ > 0.0f ? 0.85f * midi_interval_ + 0.15f * dt : dt;
        }
        midi_last_ = t;
        if (midi_running_) ++midi_ticks_;
    }
    void midi_start() {
        midi_running_ = true;
        midi_ticks_ = 0;
    }
    void midi_continue() { midi_running_ = true; }
    void midi_stop() { midi_running_ = false; }
    /// SONG POSITION POINTER: posición en semicorcheas (6 ticks cada una).
    void midi_song_position(uint16_t sixteenths) { midi_ticks_ = uint32_t(sixteenths) * 6u; }

    // ── Beat de la IA ─────────────────────────────────────────────────────────
    /// `phase` 0–1 dentro del beat en el instante `t`.
    void ai_beat(uint32_t t, float bpm, float phase) {
        phase -= floorf(phase);
        // Base de beats enteros continua: el beat entero más cercano a donde iba la posición
        ai_base_ = ai_seen_ ? floor(ai_pos(t) - double(phase) + 0.5) : 0.0;
        ai_t_ = t;
        ai_bpm_ = clamp_bpm(bpm);
        ai_phase_ = phase;
        ai_seen_ = true;
    }

    // ── Interno + TAP ─────────────────────────────────────────────────────────
    void set_internal_bpm(float bpm, uint32_t t) {
        int_pos0_ = internal_pos(t);
        int_t0_ = t;
        int_bpm_ = clamp_bpm(bpm);
    }
    /// Un toque: re-fasa a un beat entero; con 2–4 toques seguidos (0.25–2 s) fija el BPM con su promedio.
    void tap(uint32_t t) {
        if (taps_ && (t - tap_t_[taps_ - 1] > 2000000u || t - tap_t_[taps_ - 1] < 250000u)) taps_ = 0;
        if (taps_ == 4) {
            for (int i = 0; i < 3; ++i) tap_t_[i] = tap_t_[i + 1];
            taps_ = 3;
        }
        tap_t_[taps_++] = t;
        if (taps_ >= 2) int_bpm_ = clamp_bpm(60e6f * float(taps_ - 1) / float(t - tap_t_[0]));
        int_pos0_ = floorf(internal_pos(t) + 0.5f);   // el toque cae en un beat entero
        int_t0_ = t;
    }

    void force_source(Source s) { forced_ = s; }

    // ── Lectura ───────────────────────────────────────────────────────────────
    Source source(uint32_t t) const {
        if (forced_ != Source::Auto) return forced_;
        if (midi_ok(t)) return Source::Midi;
        if (ai_ok(t)) return Source::Ai;
        return Source::Internal;
    }
    float bpm(uint32_t t) const {
        switch (source(t)) {
            case Source::Midi: return midi_interval_ > 0.0f ? clamp_bpm(60e6f / (midi_interval_ * 24.0f)) : int_bpm_;
            case Source::Ai: return ai_bpm_;
            default: return int_bpm_;
        }
    }
    /// Posición en beats (continua). Con MIDI parado, la posición queda fija donde se detuvo.
    double position(uint32_t t) const {
        switch (source(t)) {
            case Source::Midi: {
                double pos = double(midi_ticks_) / 24.0;
                if (midi_running_ && midi_interval_ > 0.0f && midi_last_) {
                    const float frac = fminf(1.0f, float(t - midi_last_) / midi_interval_);   // entre ticks
                    pos += double(frac) / 24.0;
                }
                return pos;
            }
            case Source::Ai: return ai_pos(t);
            default: return internal_pos(t);
        }
    }
    bool running(uint32_t t) const { return source(t) != Source::Midi || midi_running_; }

private:
    static float clamp_bpm(float b) { return b < BPM_MIN ? BPM_MIN : b > BPM_MAX ? BPM_MAX : b; }
    bool midi_ok(uint32_t t) const { return midi_last_ && t - midi_last_ < MIDI_TIMEOUT_US; }
    bool ai_ok(uint32_t t) const { return ai_seen_ && t - ai_t_ < AI_TIMEOUT_US; }
    double internal_pos(uint32_t t) const { return int_pos0_ + double(t - int_t0_) * 1e-6 * double(int_bpm_) / 60.0; }
    double ai_pos(uint32_t t) const { return ai_base_ + double(ai_phase_) + double(t - ai_t_) * 1e-6 * double(ai_bpm_) / 60.0; }

    Source forced_ = Source::Auto;
    uint32_t midi_last_ = 0, midi_ticks_ = 0;
    float midi_interval_ = 0.0f;
    bool midi_running_ = false;
    uint32_t ai_t_ = 0;
    float ai_bpm_ = 120.0f, ai_phase_ = 0.0f;
    double ai_base_ = 0.0;
    bool ai_seen_ = false;
    float int_bpm_ = 120.0f;
    double int_pos0_ = 0.0;
    uint32_t int_t0_ = 0;
    uint32_t tap_t_[4] = {};
    uint8_t taps_ = 0;
};

}  // namespace dsp
