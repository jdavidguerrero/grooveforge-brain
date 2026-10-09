#pragma once

#include <cstdint>

#include "lvgl.h"

/**
 * @file input.h
 * @brief Eventos de control de la UI: una sola cola para todas las fuentes.
 *
 * Hoy los generan el touch (atajos sobre las barras) y el teclado del simulador. Cuando llegue
 * el bridge, los controles físicos del Teensy (ADR-004) entrarán por la misma cola.
 */
enum class Ctl : uint8_t {
    NavNext,  ///< NAV gira +1 (siguiente vista: PERFORM → AI → SPECTRUM)
    NavPrev,  ///< NAV gira −1
    NavPush,  ///< NAV push (acción de la pantalla)
    B1, B2, B3, B4,
    Mode,     ///< B5 MODE: SYNTH ⇄ FX
    Shift,    ///< B6 (en el demo alterna; en el hardware es momentáneo)
};

void input_post(Ctl c);
bool input_poll(Ctl& out);

/// Hace tocable un objeto y publica @p c al tocarlo (atajo táctil, theory doc §6).
void input_attach_tap(lv_obj_t* obj, Ctl c);

/// Teclado del simulador SDL: ← → NAV, Enter push, 1–4 B1–B4, M MODE, S SHIFT. No-op en FBDEV.
void input_keyboard_init();
