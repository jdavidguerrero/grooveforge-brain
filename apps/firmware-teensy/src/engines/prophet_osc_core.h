#pragma once

#include <stdint.h>

/**
 * @file prophet_osc_core.h
 * @brief Osciladores de una voz del Prophet-5 muestra por muestra: A, B, ruido, POLY-MOD y sync (código custom).
 *
 * Lógica pura, sin Teensy Audio Library: el wrapper Prophet5Osc la llama en su update(). Saw y pulse con polyBLEP
 * (Välimäki y Huovilainen, IEEE SPM 2007), tri ingenuo. Teoría: apps/docs/theory/prophet-5-completo.md §2.
 */

/// Parámetros que el engine fija una vez por bloque (tasa de control).
struct Prophet5OscParams {
    float inc_a = 0.0f, inc_b = 0.0f;  ///< frecuencia / fs (ciclos por muestra)
    bool a_saw = true, a_pulse = false;
    bool b_saw = true, b_tri = false, b_pulse = false;
    bool sync = false;                 ///< A se reinicia cuando B da la vuelta
    float pw_a = 0.5f, pw_b = 0.5f;    ///< ancho de pulso 0.05–0.95, ya con LFO / WHEEL-MOD sumados
    float pm_b = 0.0f;                 ///< POLY-MOD: cantidad del OSC B (0–1)
    float pm_env = 0.0f;               ///< POLY-MOD: cantidad del envelope del filtro (0–1)
    float env = 0.0f;                  ///< nivel del envelope del filtro de la voz (0–1)
    bool pm_freq_a = false, pm_pw_a = false;   ///< destinos de POLY-MOD dentro del oscilador
    float lvl_a = 0.5f, lvl_b = 0.5f, lvl_n = 0.0f;  ///< MIXER
};

constexpr float PROPHET_PM_OCT = 4.0f;   ///< POLY-MOD → FREQ A a escala completa: ±4 octavas

class Prophet5OscCore {
public:
    /// Genera `n` muestras en `out` (rango nominal ±1).
    void render(const Prophet5OscParams& p, float* out, int n);
    void reset() { pa_ = pb_ = 0.0f; }
    float phase_a() const { return pa_; }
    float phase_b() const { return pb_; }

private:
    float pa_ = 0.0f, pb_ = 0.0f;
    uint32_t rng_ = 0x2545F491u;
};

/// 2^x rápido para |x| ≤ ~8 (error < 0.1 %): parte entera por exponente, fracción por polinomio de 3.er grado.
float prophet_fast_exp2(float x);
