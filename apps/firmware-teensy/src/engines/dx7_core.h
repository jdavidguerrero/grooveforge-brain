#pragma once

#include <stdint.h>

/**
 * @file dx7_core.h
 * @brief Una voz FM de 4 operadores con modulación de fase, 8 algoritmos, feedback en OP4 y sensibilidad a velocidad.
 *
 * Lógica pura, sin Teensy Audio Library: el wrapper Dx7Voice la llama en su update().
 * Teoría: apps/docs/sprints/46-dx7-4-operadores.md · decisión: ADR-005.
 */

constexpr int DX7_OPS = 4;
constexpr int DX7_ALGORITHMS = 8;

/// Parámetros de un operador (unidades del panel).
struct Dx7OpParams {
    float ratio = 1.0f;      ///< frecuencia = nota × ratio
    float level = 99.0f;     ///< 0–99 (~0.75 dB por paso)
    float attack_ms = 0.0f, decay_ms = 0.0f, sustain = 1.0f, release_ms = 300.0f;
};

/// Patch FM completo (lo comparten las 6 voces).
struct Dx7Patch {
    Dx7OpParams op[DX7_OPS];
    uint8_t algorithm = 5;   ///< 1–8 (familia TX81Z)
    uint8_t feedback = 0;    ///< 0–7, sobre OP4
};

class Dx7VoiceCore {
public:
    /// Arranca la nota: fases a 0, envolventes en ataque y sensibilidad a velocidad (mod index + carrier gain).
    void note_on(float hz, float velocity = 1.0f);
    /// Pasa todas las envolventes a release.
    void note_off();
    /// Frecuencia de la nota (glide / vibrato del LFO) sin reiniciar.
    void set_freq(float hz) { hz_ = hz; }
    /// true mientras alguna portadora suena.
    bool active() const { return active_; }

    /// Genera `n` muestras (rango nominal ±1) a `fs` Hz.
    void render(const Dx7Patch& p, float fs, float* out, int n);

private:
    enum Stage : uint8_t { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    float hz_ = 440.0f;
    float vel_ = 1.0f;
    float phase_[DX7_OPS] = {};
    float env_[DX7_OPS] = {};
    Stage stage_[DX7_OPS] = {};
    float fb_[2] = {};       ///< dos últimas salidas de OP4
    bool active_ = false;
};

/// Amplitud lineal de un LEVEL 0–99.
float dx7_level_amp(float level);
/// true si el operador `op` (0–3) es portadora en el algoritmo `alg` (1–8).
bool dx7_is_carrier(int alg, int op);
