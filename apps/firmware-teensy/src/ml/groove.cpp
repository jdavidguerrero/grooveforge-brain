/**
 * @file groove.cpp
 * @brief Groove Humanizer — micro-timing para SmartArp.
 *
 * Theory: apps/docs/sprints/43-groove-humanizer.md
 * Sprint 6.4 — GrooveForge Brain Fase 6 Layer 1 AI.
 */

#include "groove.h"

GrooveHumanizer::GrooveHumanizer() {}

uint8_t GrooveHumanizer::_prng() {
    uint8_t lsb = _lfsr & 1;
    _lfsr >>= 1;
    if (lsb) _lfsr ^= 0xB400u;
    return (uint8_t)(_lfsr & 0xFF);
}

int32_t GrooveHumanizer::_gaussian_jitter_ms(uint32_t /*period_ms*/) {
    // Aproximación Gaussiana: suma de 4 uniformes [-128, 127] → distribución de campana.
    // σ_approx ≈ √(4/12) × rango ≈ 0.577 × rango.
    // Target σ = 15ms a amount=1 → sigma_ms ≈ 26ms para que 0.577 × 26 ≈ 15.
    int32_t sigma_ms = (int32_t)(_amount * 26.0f);
    if (sigma_ms < 1) return 0;

    // 4 muestras uniformes centradas en 0 — sum en [-512, 508], σ ≈ 147.8
    int32_t sum = 0;
    for (uint8_t i = 0; i < 4; i++) {
        sum += (int32_t)_prng() - 128;
    }
    // Escalar a [-sigma_ms, +sigma_ms]
    return (int32_t)(sum * sigma_ms / 512);
}

int32_t GrooveHumanizer::offset_ms(uint8_t step_parity, uint32_t period_ms) {
    if (_amount < 0.01f) return 0;

    switch (_style) {
    case Style::OFF:
        return 0;

    case Style::HUMAN:
        return _gaussian_jitter_ms(period_ms);

    case Style::SWING: {
        // Swing: pasos pares = largo, impares = corto.
        // Con amount=1: swing_ratio = 0.67 (2:1 clásico del jazz).
        // Con amount=0: ratio = 0.5 (grid perfecta, sin swing).
        // offset = ±(period × amount × 0.17):
        //   par  → +offset (más largo)
        //   impar → -offset (más corto)
        int32_t swing_offset = (int32_t)((float)period_ms * _amount * 0.17f);
        return (step_parity == 0) ? swing_offset : -swing_offset;
    }

    default:
        return 0;
    }
}
