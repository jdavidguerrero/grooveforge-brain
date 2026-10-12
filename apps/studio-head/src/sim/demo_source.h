#pragma once

#include <cstdint>

#include "state/studio_state.h"

/**
 * @brief Fuente de datos falsa para desarrollar la UI sin el Teensy.
 *
 * Barre el cutoff, anima el espectro con la respuesta del filtro, avanza la fase del compás a
 * 124 BPM y rota tonalidad/acorde cada pocos segundos. Para FX MAIN simula una secuencia de
 * notas (110 / 110 / 146.83 / 98 Hz cada 0.6 s) y la pasa por el FX del slot a la vista.
 * Mismas fórmulas que el diseño (studio-data.js y GrooveForge Studio FX Viz), para que se vea
 * igual que los mockups.
 */
void demo_tick(StudioState& s, uint32_t now_ms);

/// Carga el banco de presets de ejemplo y los valores por defecto de los FX.
void demo_init(StudioState& s);
