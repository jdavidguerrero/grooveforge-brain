#pragma once

#include <cstddef>
#include <cstdint>

#include "ui/preset_bank.h"

namespace gf {

/** @brief Decodifica un preset GFP Prophet, preservando `out` si el archivo es inválido. */
bool decode_prophet_preset(const uint8_t* data, size_t size, studio::Preset& out);
/** @brief Decodifica GFP Model D, Juno, Prophet, OB-6, DX7 o ARP 2600 y devuelve el engine sin alterar salidas ante error. */
bool decode_supported_preset(const uint8_t* data, size_t size, studio::Preset& out, uint8_t& engine);

/** @brief Carga un preset Prophet del banco de fábrica compartido con el firmware. */
bool factory_prophet_preset(uint8_t index, studio::Preset& out);

/** @brief Número de presets Prophet en el banco de fábrica. */
uint8_t factory_prophet_count();

}  // namespace gf
