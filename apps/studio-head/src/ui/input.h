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
    Shift,    ///< B6 en SYNTH (en el demo alterna; en el hardware es momentáneo)
    HoldOn,   ///< B6 apretado en FX (momentáneo, estilo RMX-1000)
    HoldOff,  ///< B6 suelto en FX
    HoldToggle,
    EncLPush, ///< ENC L push: filter bypass
    EngineCard,   ///< tap en la card del engine (PERFORM) → ENGINE SELECT
    Pick0, Pick1, Pick2, Pick3, Pick4, Pick5,  ///< tap en una tile de ENGINE SELECT
    FxWave, FxSpectrum, FxSignature,  ///< tap en una pestaña de FX MAIN
    FxViewNext,   ///< tap en la gráfica de FX MAIN
    VolUp, VolDown,   ///< pot VOL (en el demo, teclado)
    InUp, InDown,     ///< pot IN
    PadCycle,         ///< switch INPUT PAD trasero: 0 → −10 → −20 dB
    LinkToggle,       ///< demo: simula la pérdida del enlace con el motor
    SwipeUp,          ///< deslizar hacia arriba (listas: siguiente página)
    SwipeDown,        ///< deslizar hacia abajo (listas: página anterior)
};

void input_post(Ctl c);
bool input_poll(Ctl& out);

/// Hace tocable un objeto y publica @p c al tocarlo (atajo táctil, theory doc §6).
void input_attach_tap(lv_obj_t* obj, Ctl c);

/**
 * @brief Gestos sobre una pantalla: deslizar a izquierda / derecha = NAV ±1, arriba / abajo =
 *        SwipeUp / SwipeDown. Un toque que termina en gesto no cuenta como tap.
 */
void input_attach_gestures(lv_obj_t* screen);

/// Publica @p down al apretar y @p up al soltar (controles momentáneos como HOLD).
void input_attach_hold(lv_obj_t* obj, Ctl down, Ctl up);

/**
 * @brief Teclado del simulador SDL. No-op en FBDEV.
 *
 * ← → NAV · Enter push · 1–4 B1–B4 · M MODE · S SHIFT · H HOLD · B ENC L push · E ENGINE SELECT ·
 * V vista de FX · − = VOL · [ ] IN · P PAD · L enlace perdido.
 */
void input_keyboard_init();
