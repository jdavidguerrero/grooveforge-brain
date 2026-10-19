#pragma once

/**
 * @file lfo_mod.h
 * @brief Modulación del LFO que recibe cada engine en su update() (theory/lfo-modulacion.md §5.1).
 *
 * El sketch evalúa el LFO global y las cantidades por destino, y le pasa al engine activo el resultado ya
 * convertido. El engine lo aplica a tasa de control (su update(), ~1 kHz) sobre las voces que suenan.
 */
struct LfoMod {
    float pitch_ratio = 1.0f;   ///< multiplica la frecuencia de cada oscilador (vibrato)
    float cutoff_mul = 1.0f;    ///< multiplica el cutoff ya modulado por el envelope
    float pwm = 0.0f;           ///< −1..1: ancho de pulso = 0.5 + 0.45 · pwm (solo afecta a WAVEFORM_PULSE)
    float amp = 1.0f;           ///< ganancia de salida 0–1 (LFO → AMP, trémolo)
    float lfo = 0.0f;           ///< salida cruda del LFO global (−1..1): fuente de WHEEL-MOD del Prophet
};
