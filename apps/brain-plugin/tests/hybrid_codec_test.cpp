#include "hybrid_codec.h"
#include "ui/layers.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

int main() {
    if (studio::factory_layer_count() != studio::LAYER_COUNT) return 1;
    for (uint8_t i = 0; i < studio::factory_layer_count(); ++i) {
        studio::LayerPreset original, decoded;
        std::array<uint8_t, studio::LAYER_BYTES> bytes{}, roundtrip{};
        if (!studio::factory_layer(i, &original) || !gf::encode_hybrid_layer(original, bytes.data(), bytes.size()) ||
            !gf::decode_hybrid_layer(bytes.data(), bytes.size(), decoded) ||
            !gf::encode_hybrid_layer(decoded, roundtrip.data(), roundtrip.size()) || bytes != roundtrip) {
            std::fprintf(stderr, "GFL factory roundtrip %u\n", unsigned(i)); return 1;
        }
        if (decoded.eng_a == decoded.eng_b || decoded.eng_a != original.eng_a || decoded.eng_b != original.eng_b ||
            decoded.morph != original.morph || decoded.delay_b_ms != original.delay_b_ms ||
            decoded.transpose_b != original.transpose_b || std::strcmp(decoded.name, original.name)) return 1;
        auto bad = bytes; bad[0] ^= 1;
        if (gf::decode_hybrid_layer(bad.data(), bad.size(), decoded) || decoded.eng_a != original.eng_a) return 1;
        if (gf::decode_hybrid_layer(bytes.data(), bytes.size() - 1, decoded)) return 1;
        bad = bytes; bad[4 + studio::LAYER_NAME_LEN + 1] = original.eng_a;
        if (gf::decode_hybrid_layer(bad.data(), bad.size(), decoded)) return 1;
        bad = bytes;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        constexpr size_t preset_start = 4 + studio::LAYER_NAME_LEN + 7;
        std::memcpy(bad.data() + preset_start + 5 + studio::PRESET_NAME_LEN, &nan, sizeof(nan));
        if (gf::decode_hybrid_layer(bad.data(), bad.size(), decoded)) return 1;
        std::array<uint8_t, studio::LAYER_BYTES_V1> old{};
        std::memcpy(old.data(), bytes.data(), preset_start);
        old[3] = 1;
        std::memcpy(old.data() + preset_start, bytes.data() + preset_start, studio::PRESET_BYTES_V4);
        std::memcpy(old.data() + preset_start + studio::PRESET_BYTES_V4, bytes.data() + preset_start + studio::PRESET_BYTES, studio::PRESET_BYTES_V4);
        old[preset_start + 3] = 4; old[preset_start + studio::PRESET_BYTES_V4 + 3] = 4;
        if (!gf::decode_hybrid_layer(old.data(), old.size(), decoded) || decoded.eng_a != original.eng_a || decoded.eng_b != original.eng_b) return 1;
        original.eng_b = original.eng_a;
        roundtrip.fill(0xa5);
        if (gf::encode_hybrid_layer(original, roundtrip.data(), roundtrip.size()) || roundtrip[0] != 0xa5) return 1;
    }
    return 0;
}
