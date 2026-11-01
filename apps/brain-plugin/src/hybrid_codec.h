#pragma once
#include "ui/layer_bank.h"
#include <cstddef>
#include <cstdint>
namespace gf {
/** @brief Valida y decodifica una capa GFL v1/v2 sin alterar la salida si es inválida. */
bool decode_hybrid_layer(const uint8_t* data, size_t size, studio::LayerPreset& out);
/** @brief Serializa una capa validada como GFL v2, sin alterar la salida ante error. */
bool encode_hybrid_layer(const studio::LayerPreset& layer, uint8_t* data, size_t size);
}
