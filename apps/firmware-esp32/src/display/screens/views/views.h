#pragma once

/**
 * @file views.h
 * @brief Declaraciones de las 23 vistas del carrusel — GrooveForge Brain
 *
 * Cada vista expone dos funciones con un contrato uniforme:
 *   void view_NN_create(lv_obj_t* parent);  — construye los widgets
 *   void view_NN_destroy(void);             — para timers recurrentes (vacia si no usa)
 *
 * El carrusel (carousel.cpp) las referencia en una tabla ordenada 01→23.
 * Ver apps/docs/sprints/18-display-ui-carousel.md.
 */

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void view_01_create(lv_obj_t* parent); void view_01_destroy(void);  /* Boot */
void view_02_create(lv_obj_t* parent); void view_02_destroy(void);  /* Synth Main */
void view_03_create(lv_obj_t* parent); void view_03_destroy(void);  /* OSC Page */
void view_04_create(lv_obj_t* parent); void view_04_destroy(void);  /* ENV / ADSR */
void view_05_create(lv_obj_t* parent); void view_05_destroy(void);  /* LFO / MOD */
void view_06_create(lv_obj_t* parent); void view_06_destroy(void);  /* Engine Select */
void view_07_create(lv_obj_t* parent); void view_07_destroy(void);  /* FX Main */
void view_08_create(lv_obj_t* parent); void view_08_destroy(void);  /* FX Select */
void view_09_create(lv_obj_t* parent); void view_09_destroy(void);  /* Mode Switch */
void view_10_create(lv_obj_t* parent); void view_10_destroy(void);  /* AI Processing — modo full */
/* Overlay API — modo Siri-like durante hold ENC NAV (spec §3.6.3d) */
void view_10_overlay_start(lv_obj_t* screen);     ///< inicia dimmer + ring + dot sobre vista actual
void view_10_overlay_cancel(void);                 ///< fade-out 200ms + delete (hold cancelado)
void view_10_overlay_destroy_immediate(void);      ///< delete sin fade (carousel_goto interrumpe hold)
void view_11_create(lv_obj_t* parent); void view_11_destroy(void);  /* Idle */
void view_12_create(lv_obj_t* parent); void view_12_destroy(void);  /* Preset Browser */
void view_13_create(lv_obj_t* parent); void view_13_destroy(void);  /* Insert Layer */
void view_14_create(lv_obj_t* parent); void view_14_destroy(void);  /* Send Layer */
void view_15_create(lv_obj_t* parent); void view_15_destroy(void);  /* Master Layer */
void view_16_create(lv_obj_t* parent); void view_16_destroy(void);  /* Scale Lock */
void view_17_create(lv_obj_t* parent); void view_17_destroy(void);  /* WiFi Setup */
void view_18_create(lv_obj_t* parent); void view_18_destroy(void);  /* AI Suggestion */
void view_19_create(lv_obj_t* parent); void view_19_destroy(void);  /* DAW Connected */
void view_20_create(lv_obj_t* parent); void view_20_destroy(void);  /* OTA Update */
void view_21_create(lv_obj_t* parent); void view_21_destroy(void);  /* Error States */
void view_22_create(lv_obj_t* parent); void view_22_destroy(void);  /* Volume Overlay */
void view_23_create(lv_obj_t* parent); void view_23_destroy(void);  /* MIDI Learn */

#ifdef __cplusplus
}
#endif
