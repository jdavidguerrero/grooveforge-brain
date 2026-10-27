#pragma once

#include <stddef.h>
#include <stdint.h>

#include "layer_bank.h"
#include "preset_bank.h"

/**
 * @file session.h
 * @brief Sesión del Studio para volver a donde estabas al reiniciar (variants/studio/theory/14), lógica pura.
 *
 * Los 6 patches de SYNTH con lo editado, el preset cargado de cada engine, el engine activo, el modo, el volumen y, si
 * hay, la capa de HYBRID SYNTH como suena. Va en un archivo (`/session.gfs`) que el sketch escribe con un retardo.
 */
namespace studio {

constexpr uint8_t SESSION_FORMAT = 1;
constexpr size_t SESSION_HEADER = 4 + 6 + GF_ENGINE_COUNT + 4;   ///< firma · modo, engine, capa, cursor, foco, hay capa · presets · volumen
constexpr size_t SESSION_BYTES_MAX = SESSION_HEADER + GF_ENGINE_COUNT * PRESET_BYTES + LAYER_BYTES;

struct Session {
    uint8_t mode = 0;                          ///< 0 SYNTH · 2 HYBRID SYNTH (FX vuelve como SYNTH)
    uint8_t engine = 0;                        ///< engine activo de SYNTH
    float volume_db = 0.0f;
    uint8_t preset_loaded[GF_ENGINE_COUNT] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    Preset synth[GF_ENGINE_COUNT];             ///< patches de SYNTH con lo editado (cutoff y resonancia incluidos)
    bool has_layer = false;
    int8_t layer_idx = -1;                     ///< −1 · 0–31 preset de capa · 127 CUSTOM
    uint8_t layer_cursor = 0, layer_focus = 0;
    LayerPreset layer;                         ///< la capa como suena
};

/// @return bytes escritos (0 si `cap` no alcanza).
size_t session_encode(const Session& s, uint8_t* buf, size_t cap);
/// @return false si la firma, la versión o algún preset no se pueden leer.
bool session_decode(const uint8_t* buf, size_t len, Session* out);

}  // namespace studio
