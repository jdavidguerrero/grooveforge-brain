#pragma once

#include <cstddef>
#include <cstdint>

#include "ui/preset_bank.h"
#include "dx7_6op_preview.h"

namespace gf {
/** @brief Tagged GFP payload; the six-op voice has its own representation, preserving old Studio layouts. */
struct EnginePreset {
    uint8_t engine = 2;
    studio::Preset studio;
    dx7six::Patch dx7_6op = dx7six::init_patch();
};
/** @brief Decode GFP 2–5 or DX7 GFP 6; malformed input preserves out. SysEx uses decode_dx7_6op_preset. */
bool decode_engine_preset(const uint8_t* data, size_t size, EnginePreset& out);


/** @brief Decodifica un preset GFP Prophet, preservando `out` si el archivo es inválido. */
bool decode_prophet_preset(const uint8_t* data, size_t size, studio::Preset& out);
/** @brief Decodifica GFP Model D, Juno, Prophet, OB-6, DX7 o ARP 2600 y devuelve el engine sin alterar salidas ante error. */
bool decode_supported_preset(const uint8_t* data, size_t size, studio::Preset& out, uint8_t& engine);

/** @brief Carga un preset Prophet del banco de fábrica compartido con el firmware. */
bool factory_prophet_preset(uint8_t index, studio::Preset& out);

/** @brief Número de presets Prophet en el banco de fábrica. */
uint8_t factory_prophet_count();

}  // namespace gf
