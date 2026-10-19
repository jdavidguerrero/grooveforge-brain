#pragma once

#include <stdint.h>

/**
 * @file dx7_core.h
 * @brief Una voz FM de 4 operadores con modulación de fase, 8 algoritmos, feedback en OP4 y sensibilidad a velocidad.
 *
 * Lógica pura, sin Teensy Audio Library: el wrapper Dx7Voice la llama en su update().
 * Teoría: apps/docs/sprints/46-dx7-4-operadores.md · decisión: ADR-005 · envolventes en dB, formas W1–W8, detune y
 * escalado de teclado: apps/docs/theory/fidelidad-engines.md §11.1.
 */

constexpr int DX7_OPS = 4;
constexpr int DX7_ALGORITHMS = 8;
constexpr int DX7_WAVES = 8;               ///< W1–W8 del TX81Z
constexpr float DX7_EG_FLOOR_DB = 96.0f;   ///< atenuación de silencio
constexpr float DX7_DETUNE_CENTS = 2.0f;   ///< por paso de DETUNE (−3…+3)

/// Parámetros de un operador (unidades del panel).
struct Dx7OpParams {
    float ratio = 1.0f;      ///< frecuencia = nota × ratio
    float level = 99.0f;     ///< 0–99 (~0.75 dB por paso)
    float attack_ms = 0.0f, decay_ms = 0.0f, sustain = 1.0f, release_ms = 300.0f;
    uint8_t wave = 0;        ///< 0–7 = W1–W8
    int8_t detune = 0;       ///< −3…+3
};

/// Patch FM completo (lo comparten las 6 voces).
struct Dx7Patch {
    Dx7OpParams op[DX7_OPS];
    uint8_t algorithm = 5;   ///< 1–8 (familia TX81Z)
    uint8_t feedback = 0;    ///< 0–7, sobre OP4
    float kbd_level = 0.0f;  ///< 0–99: atenuación de moduladores en el agudo (hasta −12 dB / oct sobre C3)
    uint8_t kbd_rate = 0;    ///< 0–3: envolventes más rápidas en el agudo
    float d2_ms = 0.0f;      ///< decay 2 con la tecla apretada: ms para 60 dB (0 = sostiene)
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
    /// Atenuación actual de la envolvente del operador en dB (0 = máximo, 96 = silencio). Para tests.
    float eg_db(int op) const { return att_[op]; }

    /// Genera `n` muestras (rango nominal ±1) a `fs` Hz.
    void render(const Dx7Patch& p, float fs, float* out, int n);

private:
    enum Stage : uint8_t { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE };
    float hz_ = 440.0f;
    float vel_ = 1.0f;
    float phase_[DX7_OPS] = {};
    float att_[DX7_OPS] = {DX7_EG_FLOOR_DB, DX7_EG_FLOOR_DB, DX7_EG_FLOOR_DB, DX7_EG_FLOOR_DB};
    float note_ = 69.0f;     ///< número de nota (para el escalado de teclado)
    Stage stage_[DX7_OPS] = {};
    float fb_[2] = {};       ///< dos últimas salidas de OP4
    bool active_ = false;
};

/// Amplitud lineal de un LEVEL 0–99.
float dx7_level_amp(float level);
/// Forma de onda `wave` (0–7 = W1–W8 del TX81Z) en la fase `ph` (ciclos).
float dx7_wave(int wave, float ph);
/// true si el operador `op` (0–3) es portadora en el algoritmo `alg` (1–8).
bool dx7_is_carrier(int alg, int op);
