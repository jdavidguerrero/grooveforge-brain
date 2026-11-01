#include "hybrid_codec.h"
#include "preset_codec.h"
#include <array>
namespace gf {
namespace {
bool validate_halves(const studio::LayerPreset& layer) {
    std::array<uint8_t, studio::PRESET_BYTES> bytes{};
    for (int side = 0; side < 2; ++side) {
        const uint8_t engine = side == 0 ? layer.eng_a : layer.eng_b;
        const auto& preset = side == 0 ? layer.a : layer.b;
        studio::Preset decoded; uint8_t decoded_engine = 255;
        if (studio::preset_encode(preset, engine, bytes.data(), bytes.size()) != bytes.size() ||
            !decode_supported_preset(bytes.data(), bytes.size(), decoded, decoded_engine) || decoded_engine != engine) return false;
    }
    return true;
}
}
bool decode_hybrid_layer(const uint8_t* data, size_t size, studio::LayerPreset& out) {
    if (!data) return false;
    studio::LayerPreset candidate;
    if (!studio::layer_decode(data, size, &candidate) || !validate_halves(candidate)) return false;
    out = candidate;
    return true;
}
bool encode_hybrid_layer(const studio::LayerPreset& layer, uint8_t* data, size_t size) {
    if (!data || size < studio::LAYER_BYTES || layer.eng_a >= GF_ENGINE_COUNT || layer.eng_b >= GF_ENGINE_COUNT ||
        layer.eng_a == layer.eng_b || !validate_halves(layer)) return false;
    std::array<uint8_t, studio::LAYER_BYTES> encoded{};
    if (studio::layer_encode(layer, encoded.data(), encoded.size()) != encoded.size()) return false;
    for (size_t i = 0; i < encoded.size(); ++i) data[i] = encoded[i];
    return true;
}
}
