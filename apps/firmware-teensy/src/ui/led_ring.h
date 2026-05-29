#pragma once

/**
 * @file led_ring.h
 * @brief WS2812B LED ring driver — GrooveForge Brain (Sprint 35)
 *
 * Controla los 16 LEDs WS2812B en cadena única (GPIO 28):
 *   LED 0-11  → anillo ENC NAV (12 LEDs en círculo)
 *   LED 12-15 → underglow keycap B1-B4
 *
 * ## Patrones por modo
 *
 * ### FX MODE (purple)
 *   - Ring: todos dim purple (#4A0099), sin cursor (ESP32 maneja cuál FX está activo)
 *   - Keycap: B1=blanco dim, B2=purple, B3=cyan dim (tap tempo), B4=rojo dim (panic)
 *
 * ### SYNTH MODE (teal)
 *   - Ring: 4 quadrants de 3 LEDs = 4 grupos de parámetros (OSC, ENV, FILTER, LFO)
 *     + 3 LEDs para motores (Moog/Juno/Prophet) mapeados en ENGINE_LIST level
 *   - Nivel 0 (ENGINE_LIST):  1 LED por engine (0, 4, 8), activo = bright teal
 *   - Nivel 1 (SUBHOME):      todos en teal suave, "listo"
 *   - Nivel 2 (GROUP_VIEW):   grupo activo = bright teal, otros = dim
 *   - Keycap: B1=blanco, B2=teal, B3=cyan dim, B4=rojo dim
 *
 * ### AI MODE (Camelot rainbow)
 *   - Ring: cada LED = una posición del Circle of Fifths con su color Camelot fijo,
 *     brillo = pitch_activity (0-255) del modelo ML. Espeja el chromagram del display.
 *   - Keycap: B1=blanco, B2=teal, B3=teal dim, B4=rojo dim
 *
 * ## Brillo global
 *   FastLED.setBrightness(80) → ~31% — cómodo para uso en estudio y
 *   seguro en corriente (max ~300mA @ 16 LEDs × 60mA full white × 31%).
 *
 * ## Timing
 *   update() se llama desde loop() cada 50ms (~20Hz). FastLED.show() toma
 *   ~0.5ms (16 LEDs × 30µs/LED), no interfiere con el audio DMA del I2S
 *   que opera en su propio interrupt de Teensyduino.
 *
 * Spec: apps/docs/sprints/29-hardware-integration.md §"LED ring" +
 *       apps/docs/01-architecture.md §3.3 (GPIO 28)
 */

#include <FastLED.h>

class LedRing {
public:
    /* ── Constantes de hardware ──────────────────────────────────────────── */

    static constexpr uint8_t LED_PIN      = 28;   ///< GPIO Teensy (arquitectura §3.3)
    static constexpr uint8_t NUM_LEDS     = 16;   ///< 12 ring + 4 keycap
    static constexpr uint8_t RING_COUNT   = 12;   ///< ENC NAV ring
    static constexpr uint8_t BRIGHTNESS   = 80;   ///< global cap (31%)

    /* ── Índices keycap ──────────────────────────────────────────────────── */

    static constexpr uint8_t LED_B1 = 12;  ///< HOME
    static constexpr uint8_t LED_B2 = 13;  ///< MODE TOGGLE
    static constexpr uint8_t LED_B3 = 14;  ///< TAP TEMPO
    static constexpr uint8_t LED_B4 = 15;  ///< PANIC

    /* ── Init ────────────────────────────────────────────────────────────── */

    /**
     * @brief Inicializa FastLED, apaga todos los LEDs.
     * Llamar una vez en setup() antes de cualquier update().
     */
    void init();

    /* ── Configuración de modo (llamar al cambiar de modo) ───────────────── */

    /**
     * @brief Configura el ring para FX mode.
     * Establece la paleta base — update_fx() mantiene el estado.
     */
    void enter_fx_mode();

    /**
     * @brief Configura el ring para SYNTH mode.
     * @param synth_level  0=ENGINE_LIST, 1=SUBHOME, 2=GROUP_VIEW
     * @param active_engine 0=Moog, 1=Juno, 2=Prophet
     * @param active_group  0=OSC, 1=ENV, 2=FILTER, 3=LFO
     */
    void enter_synth_mode(uint8_t synth_level, uint8_t active_engine, uint8_t active_group);

    /**
     * @brief Configura el ring para AI mode.
     * El ring muestra el chromagram Camelot — actualizado por update_ai().
     */
    void enter_ai_mode();

    /* ── Updates por frame (llamar desde loop cada ~50ms) ────────────────── */

    /**
     * @brief Actualiza ring en FX mode — color sólido dim purple, estático.
     * Usar en FX_SELECT (navegando entre FX). Para FX_MAIN usar update_fx_wet().
     */
    void update_fx();

    /**
     * @brief Anima el ring en FX_MAIN mostrando el nivel wet/dry.
     *
     * LEDs se llenan en sentido horario desde LED 0 proporcional al nivel wet.
     * Comportamiento por estado:
     *   bypass OFF: LEDs lit = PURPLE_BRIGHT, unlit = PURPLE_DIM — alta legibilidad.
     *   bypass ON:  LEDs lit = PURPLE_DIM,    unlit = casi apagado — preset visible
     *               pero claramente "silenciado". Contraste bajo = señal visual de mute.
     *
     * @param wet    Nivel wet/dry [0.0, 1.0]. 0 = todo dim, 1 = ring completo bright.
     * @param bypass true cuando el FX está bypasseado.
     */
    void update_fx_wet(float wet, bool bypass);

    /**
     * @brief Actualiza ring en SYNTH mode según el nivel de navegación activo.
     *
     * @param synth_level   0=ENGINE_LIST, 1=SUBHOME, 2=GROUP_VIEW
     * @param active_engine 0-2
     * @param active_group  0-3
     */
    void update_synth(uint8_t synth_level, uint8_t active_engine, uint8_t active_group);

    /**
     * @brief Actualiza ring en AI mode con actividad de pitch del MLEngine.
     *
     * @param pitch_act  Array de 12 valores (pitch class 0-11), 0-255 cada uno.
     *                   Mapeo: pitch_act[0]=C, [1]=C#, ..., [11]=B.
     *                   Provisto por g_ml_engine.get_pitch_activity_u8(pc).
     */
    void update_ai(const uint8_t pitch_act[12]);

    /**
     * @brief Pulsa LED_B3 (TAP TEMPO) en teal brillante por 80ms.
     * Llamar desde handle_buttons() cuando el usuario presiona B3.
     */
    void tap_tempo_flash();

    /**
     * @brief Pulsa todos los LEDs en rojo por 120ms (PANIC).
     * Llamar desde handle_buttons() cuando el usuario presiona B4.
     */
    void panic_flash();

    /**
     * @brief Envía el buffer actual al hardware (FastLED.show()).
     * Se llama internamente al final de cada update_*().
     */
    void show();

private:
    CRGB    _leds[NUM_LEDS] = {};
    uint8_t _mode           = 1;    ///< 0=FX, 1=SYNTH, 2=AI

    /* Colores Camelot (CoF position 0-11), coinciden con ui_theme.h GF_COLOR_CAMELOT_* */
    static const CRGB CAMELOT[12];

    /** CoF position → pitch class. p=0→C, p=1→G, p=2→D, ... */
    static const uint8_t COF_TO_PC[12];

    void _set_keycaps_fx();
    void _set_keycaps_synth();
    void _set_keycaps_ai();
};

/** Instancia global — declarada en led_ring.cpp */
extern LedRing led_ring;
