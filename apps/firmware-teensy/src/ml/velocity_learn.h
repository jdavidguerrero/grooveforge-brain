#pragma once
#include <stdint.h>

/**
 * @brief Velocity Curve Learn — Sprint 6.5 (Fase 6).
 *
 * Aprende el rango dinámico del usuario en las primeras N notas y extiende
 * ese rango a todo el espectro 0-127. Calibración adaptativa por percentiles.
 *
 * Uso:
 *   1. record(raw_vel) — llamar en cada MIDI noteOn con velocity original
 *   2. apply(raw_vel) → vel_corrected — llamar antes de pasar al engine
 *
 * Theory: apps/docs/sprints/44-velocity-curve-learn.md
 */
class VelocityCurve {
public:
    static constexpr uint8_t  CALIB_NOTES = 64;   ///< notas para calibración inicial
    static constexpr uint8_t  P5_PCTILE   = 5;    ///< percentil bajo (jugador suave)
    static constexpr uint8_t  P95_PCTILE  = 95;   ///< percentil alto (jugador fuerte)

    VelocityCurve();

    /**
     * @brief Registra una velocidad MIDI (llamar en cada noteOn).
     * La calibración se recalcula cada CALIB_NOTES notas.
     */
    void record(uint8_t velocity);

    /**
     * @brief Aplica la curva aprendida a una velocidad raw.
     * @return Velocidad corregida 0-127.
     */
    uint8_t apply(uint8_t raw_velocity) const;

    /** @brief true si ya se recibieron CALIB_NOTES notas y la curva está activa. */
    bool is_calibrated() const { return _calibrated; }

    /** @brief Resetea histograma y curva — para nuevo usuario o recalibración. */
    void reset();

    /** @brief P5 y P95 actuales (para debug/display). */
    uint8_t p5()  const { return _p5; }
    uint8_t p95() const { return _p95; }

private:
    uint16_t _hist[128];   ///< histograma de velocity (0-127)
    uint32_t _n_recorded;  ///< notas registradas totales
    uint8_t  _p5;          ///< percentil 5 actual
    uint8_t  _p95;         ///< percentil 95 actual
    bool     _calibrated;  ///< true cuando la curva está activa

    void    _recalculate();
    uint8_t _percentile(uint8_t pct) const;
};
