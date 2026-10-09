#pragma once

#include <cstdint>

#include "state/studio_state.h"

/**
 * @brief Fuente de datos falsa para desarrollar la UI sin el Teensy.
 *
 * Barre el cutoff, anima el espectro con la respuesta del filtro, avanza la fase del compás a
 * 124 BPM y rota tonalidad/acorde cada pocos segundos. Mismas fórmulas que el diseño
 * (claude-design-2026-10-09/studio-data.js), para que se vea igual que los mockups.
 */
void demo_tick(StudioState& s, uint32_t now_ms);
