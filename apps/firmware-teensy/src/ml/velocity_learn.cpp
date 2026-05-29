/**
 * @file velocity_learn.cpp
 * @brief Velocity Curve Learn — calibración adaptativa del rango dinámico del usuario.
 *
 * Theory: apps/docs/sprints/44-velocity-curve-learn.md
 * Sprint 6.5 — GrooveForge Brain Fase 6 Layer 1 AI.
 */

#include "velocity_learn.h"
#include <string.h>

VelocityCurve::VelocityCurve() {
    reset();
}

void VelocityCurve::reset() {
    memset(_hist, 0, sizeof(_hist));
    _n_recorded = 0;
    _p5  = 20;    // defaults razonables antes de calibrar
    _p95 = 110;
    _calibrated = false;
}

void VelocityCurve::record(uint8_t velocity) {
    if (velocity > 127) velocity = 127;
    _hist[velocity]++;
    _n_recorded++;

    // Recalibrar cada CALIB_NOTES notas
    if (_n_recorded % CALIB_NOTES == 0) {
        _recalculate();
        _calibrated = true;
    }
}

uint8_t VelocityCurve::_percentile(uint8_t pct) const {
    if (_n_recorded == 0) return (pct < 50) ? 20 : 110;
    uint32_t target = (uint32_t)_n_recorded * pct / 100;
    uint32_t cum = 0;
    for (uint8_t i = 0; i < 128; i++) {
        cum += _hist[i];
        if (cum >= target) return i;
    }
    return 127;
}

void VelocityCurve::_recalculate() {
    uint8_t new_p5  = _percentile(P5_PCTILE);
    uint8_t new_p95 = _percentile(P95_PCTILE);
    // Evitar rango degenerado (P5 y P95 iguales o invertidos)
    if (new_p5 >= new_p95) {
        new_p5 = (new_p95 > 10) ? (uint8_t)(new_p95 - 10) : 0;
    }
    _p5  = new_p5;
    _p95 = new_p95;
}

uint8_t VelocityCurve::apply(uint8_t raw) const {
    if (!_calibrated) return raw;  // sin calibrar: passthrough
    if (raw <= _p5)  return 0;
    if (raw >= _p95) return 127;

    // Interpolación lineal [p5, p95] → [0, 127]
    uint32_t num = (uint32_t)(raw - _p5) * 127u;
    uint32_t den = (uint32_t)(_p95 - _p5);
    return (uint8_t)(num / den);
}
