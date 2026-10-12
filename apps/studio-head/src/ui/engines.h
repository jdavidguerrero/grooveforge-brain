#pragma once

#include <cstdint>

#include "lvgl.h"

/// Metadatos estáticos de un engine (studio-data.js: ENGINES).
struct EngineInfo {
    const char* name;
    const char* tag;    ///< línea corta, p. ej. "POLY 6 · CHORUS"
    const char* arch;   ///< arquitectura, p. ej. "POLY 6 VOICES · DCO · BBD CHORUS"
};

constexpr int ENGINE_COUNT = 6;

/// Por engine_id 0x00–0x05 (02-bridge-protocol.md §2.2: Model D, Juno, Prophet, OB-6, DX7, ARP).
const EngineInfo& engine_info(uint8_t id);

/**
 * @brief Ilustración del engine (studio-data.js: illo) dibujada con rects, círculos y líneas.
 *
 * El original es un SVG de 280×180 con preserveAspectRatio "xMidYMid slice": se escala para
 * cubrir w×h y se recorta centrado. Se dibuja solo al invalidarse (es estática).
 */
lv_obj_t* engine_art_create(lv_obj_t* parent, int x, int y, int w, int h, uint8_t engine);
void engine_art_set(lv_obj_t* art, uint8_t engine);
