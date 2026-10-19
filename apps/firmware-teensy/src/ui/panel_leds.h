#pragma once

#include <stdint.h>

#include <protocol.h>

/**
 * @file panel_leds.h
 * @brief Qué muestran las luces del panel a partir del estado de UI (theory/04 de Studio §8). Lógica pura.
 */

namespace studio {

constexpr uint8_t RING_LEDS = 12;   ///< anillo WS2812 bajo NAV
constexpr uint8_t KEY_LEDS = 6;     ///< un WS2812 por tecla B1–B6, después del anillo en la misma cadena
constexpr uint8_t PANEL_LEDS = RING_LEDS + KEY_LEDS;   ///< cadena del pin 28 (01 §3.3: 18 LEDs)

/// Arco del anillo: primer LED y cantidad (de 12) para la posición `pos` de `count` opciones.
struct RingArc {
    uint8_t first;
    uint8_t len;
};

RingArc ring_arc(uint8_t pos, uint8_t count);

/// Posición y cantidad de opciones que recorre NAV en la página visible (paradas de OSC y de LFO / MOD por engine).
/// @param preset_rows filas de PRESETS (SAVE + presets)
void nav_position(const GF_UiState& ui, uint8_t engine, uint8_t preset_rows, uint8_t* pos, uint8_t* count);

/// Brillo de estado 0–255 de cada tecla B1–B6 (el destello al apretarla lo suma el sketch).
void key_levels(const GF_UiState& ui, uint8_t out[KEY_LEDS]);

}  // namespace studio
