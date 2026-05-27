#pragma once

#include <stdint.h>

/**
 * @file carousel.h
 * @brief Carrusel de las 23 vistas de la UI — GrooveForge Brain
 *
 * Modo demo sin hardware de entrada: muestra cada vista 5s y avanza sola, en
 * bucle infinito, con transicion radial-wipe entre vistas.
 *
 * Ver apps/docs/sprints/18-display-ui-carousel.md.
 */

/**
 * @brief Arranca el carrusel. Llamar una sola vez, despues de la animacion
 *        de boot. Construye la vista 01 y programa el avance automatico.
 */
void carousel_start(void);

/**
 * @brief Salta inmediatamente a la vista indicada (0-based), interrumpiendo
 *        el dwell actual. Seguro llamar desde cualquier contexto — internamente
 *        reutiliza el mismo path de transicion que el avance automatico.
 *
 * Uso tipico: bridge handler llama carousel_goto(VIEW_IDX_FX_MAIN) cuando
 * recibe ENGINE_CHANGED del Teensy.
 *
 * @param idx Indice de la vista en la tabla s_views (0 = BOOT, 6 = FX MAIN…).
 */
void carousel_goto(uint8_t idx);

/**
 * @brief Pausa el avance automatico del carrusel indefinidamente.
 *        Llamar cuando el Teensy esta conectado y el usuario controla la UI.
 *        El carrusel sigue mostrando la vista actual; solo deja de avanzar solo.
 */
void carousel_pause(void);

/**
 * @brief Reanuda el avance automatico (deshace carousel_pause).
 *        Llamar cuando se pierde la conexion con el Teensy (heartbeat timeout).
 */
void carousel_resume(void);

/**
 * @brief Retorna el indice de la vista actualmente visible (0-based).
 *        Util para que los bridge handlers eviten rebuilds innecesarios
 *        cuando la vista activa no es la que se quiere actualizar.
 */
uint8_t carousel_get_current(void);

/* ── AI Overlay API ────────────────────────────────────────────────────────
 *
 * Durante el hold de ENC NAV (0→4s), el carrusel NO reemplaza la vista
 * activa. En su lugar, crea elementos de overlay sobre ella:
 *
 *   carousel_ai_overlay_start()    → primer tick 0xFB > 0 (hold comenzó)
 *   carousel_ai_overlay_cancel()   → 0xFB < 0 (usuario soltó antes de 4s)
 *   carousel_ai_overlay_complete() → 0xFB = 1.0 (hold completado → flash → full view_10)
 *   carousel_ai_overlay_active()   → true si el overlay está activo ahora
 *
 * Spec: apps/docs/sprints/30-navigation-rmx-style.md §3.6.3d
 */

/** Inicia el overlay Siri-like sobre la vista activa. */
void carousel_ai_overlay_start(void);

/** Cancela el overlay (fade-out 200ms, la vista anterior reaparece). */
void carousel_ai_overlay_cancel(void);

/** Hold completó (0xFB=1.0): flash blanco 100ms → carousel_goto(AI_PROC). */
void carousel_ai_overlay_complete(void);

/** True si el overlay AI está activo en este momento. */
bool carousel_ai_overlay_active(void);

/* Indices utiles (0-based, siguiendo el orden de s_views en carousel.cpp). */
static constexpr uint8_t VIEW_IDX_SYNTH_MAIN    = 1;   ///< view_02 — SYNTH sub-home (arcos cutoff/resonance)
static constexpr uint8_t VIEW_IDX_SYNTH_SUBHOME = 1;   ///< alias Sprint 31: mismo que SYNTH_MAIN
static constexpr uint8_t VIEW_IDX_SYNTH_OSC     = 2;   ///< view_03 — OSC group
static constexpr uint8_t VIEW_IDX_SYNTH_ENV     = 3;   ///< view_04 — ENV ADSR
static constexpr uint8_t VIEW_IDX_SYNTH_LFO     = 4;   ///< view_05 — LFO mod
static constexpr uint8_t VIEW_IDX_ENGINE_LIST   = 5;   ///< view_06 — engine selector
static constexpr uint8_t VIEW_IDX_FX_MAIN       = 6;   ///< view_07 — FX activo + wet/dry
static constexpr uint8_t VIEW_IDX_FX_SELECT     = 7;   ///< view_08 — lista de FX
static constexpr uint8_t VIEW_IDX_MODE_SWITCH   = 8;   ///< view_09 — transicion de modo
static constexpr uint8_t VIEW_IDX_AI_PROC       = 9;   ///< view_10 — AI processing
