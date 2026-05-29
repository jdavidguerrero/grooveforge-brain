#pragma once
#include <stdint.h>

/**
 * @brief Groove Humanizer — Sprint 6.4 (Fase 6).
 *
 * Aplica micro-timing a los pasos del Smart Arpeggiator para humanizar el ritmo.
 * Dos estilos disponibles:
 *   HUMAN — jitter Gaussiano aproximado (~σ=15ms × amount)
 *   SWING — ratio 2:1 alternado entre pasos pares e impares
 *
 * Theory: apps/docs/sprints/43-groove-humanizer.md
 */
class GrooveHumanizer {
public:
    enum class Style : uint8_t {
        OFF   = 0,   ///< sin humanización — grid perfecta
        HUMAN = 1,   ///< jitter aleatorio correlacionado — timing orgánico
        SWING = 2,   ///< pares largo/impar corto — shuffle feel
    };

    GrooveHumanizer();

    void set_style(Style s)  { _style  = s; }
    void set_amount(float a) { _amount = (a < 0.0f) ? 0.0f : (a > 1.0f ? 1.0f : a); }
    Style style()  const     { return _style; }
    float amount() const     { return _amount; }

    /**
     * @brief Retorna el offset en ms a agregar al step period actual.
     *
     * @param step_parity  0 = paso par, 1 = paso impar (para swing).
     * @param period_ms    Duración nominal del paso (sin humanización).
     * @return Offset en ms (puede ser negativo = paso anticipado).
     */
    int32_t offset_ms(uint8_t step_parity, uint32_t period_ms);

private:
    Style    _style  = Style::OFF;
    float    _amount = 0.5f;
    uint16_t _lfsr   = 0x7EA3;   ///< PRNG LFSR Galois 16-bit

    uint8_t _prng();
    /// Aproximación de Gaussiana por suma de uniformes (Box-Muller simplificado)
    int32_t _gaussian_jitter_ms(uint32_t period_ms);
};
