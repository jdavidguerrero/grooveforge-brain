#pragma once

#include <stdint.h>

/**
 * @file lfo.h
 * @brief LFO global de un engine, evaluado a tasa de control (código custom, no Teensy Audio Library).
 *
 * Un acumulador de fase en [0, 1) que avanza `rate · dt` en cada tick; la salida es bipolar [-1, 1].
 * Corre libre (no se reinicia con las notas), como el LFO del Juno-106.
 * Teoría: apps/docs/theory/lfo-modulacion.md §4.
 */

namespace dsp {

enum class LfoShape : uint8_t { Sine = 0, Tri, Saw, Sqr, SampleHold };

class Lfo {
public:
    /// RATE libre en Hz (se recorta a 0.05–20).
    void set_rate_hz(float hz);
    void set_shape(LfoShape s) { shape_ = s; }

    /// SYNC: si `on`, la velocidad sale de `bpm` y de los beats por ciclo de la división.
    void set_sync(bool on, float bpm, float beats_per_cycle);

    /// Avanza `dt_s` segundos y devuelve la salida nueva en [-1, 1].
    float tick(float dt_s);

    float value() const { return value_; }
    float phase() const { return phase_; }
    /// Velocidad efectiva (libre o sincronizada), en Hz.
    float rate_hz() const;

private:
    float sample();

    float phase_ = 0.0f;
    float value_ = 0.0f;
    float rate_hz_ = 2.0f;
    float sync_hz_ = 2.0f;
    bool sync_ = false;
    LfoShape shape_ = LfoShape::Tri;
    float held_ = 0.0f;                ///< S&H: valor del ciclo actual
    uint32_t rng_ = 0x9E3779B9u;       ///< xorshift32, determinista para los tests
};

}  // namespace dsp
