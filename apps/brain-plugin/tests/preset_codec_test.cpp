#include "preset_codec.h"
#include "engines/prophet_patch_apply.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static bool check(bool ok, const char* message) {
    if (!ok) std::fprintf(stderr, "%s\n", message);
    return ok;
}

struct Receiver {
    float af = 0, apw = 0, bi = 0, bf = 0, bpw = 0, a = 0, b = 0, noise = 0, glide = 0, tracking = 0;
    float pm_env = 0, pm_b = 0, wheel = 0;
    bool asaw = false, apulse = false, sync = false, keyboard = false, bsaw = false, btri = false;
    bool bpulse = false, low = false, pm_freq = false, pm_pw = false, pm_filter = false;
    bool wf_a = false, wf_b = false, wp_a = false, wp_b = false, wfilter = false;
    void setOscAFreq(float x) { af = x; } void setOscAPw(float x) { apw = x; }
    void setOscAShape(bool x, bool y) { asaw = x; apulse = y; } void setSync(bool x) { sync = x; }
    void setOscBInterval(float x) { bi = x; } void setOscBFine(float x) { bf = x; }
    void setOscBPw(float x) { bpw = x; } void setOscBKeyboard(bool x) { keyboard = x; }
    void setOscBShape(bool x, bool y, bool z) { bsaw = x; btri = y; bpulse = z; }
    void setOscBLoFreq(bool x) { low = x; }
    void setMixer(float x, float y, float z) { a = x; b = y; noise = z; }
    void setGlide(float x) { glide = x; } void setFilterKeyboard(float x) { tracking = x; }
    void setPolyMod(float x, float y, bool u, bool v, bool w) { pm_env = x; pm_b = y; pm_freq = u; pm_pw = v; pm_filter = w; }
    void setWheelMod(float x, bool a1, bool b1, bool a2, bool b2, bool f) {
        wheel = x; wf_a = a1; wf_b = b1; wp_a = a2; wp_b = b2; wfilter = f;
    }
};

static bool near(float a, float b) { return std::fabs(a - b) < 1.0e-5f; }

int main() {
    {
        gf::EnginePreset decoded;
        std::array<uint8_t, studio::PRESET_BYTES> bytes{};
        auto six = gf::dx7six::init_patch(); six.enabled = 21;
        const size_t n = gf::dx7six::encode_gfp(six, bytes.data(), bytes.size());
        if (!check(gf::decode_engine_preset(bytes.data(),n,decoded) && decoded.engine==7 && decoded.dx7_6op.enabled==21,"GFP v6 tagged decoder")) return 1;
        bytes[4]=4;
        if (!check(!gf::decode_engine_preset(bytes.data(),n,decoded) && decoded.engine==7,"invalid GFP preserves tagged preset")) return 1;
        studio::Preset four; studio::factory_preset(4,0,&four);
        studio::preset_encode(four,4,bytes.data(),bytes.size());
        for (uint8_t version : {2,3,4,5}) {
            bytes[3]=version;
            const size_t length=version==2 ? studio::PRESET_BYTES_V2 : version==3 ? studio::PRESET_BYTES_V3 : version==4 ? studio::PRESET_BYTES_V4 : studio::PRESET_BYTES;
            if (!check(gf::decode_engine_preset(bytes.data(),length,decoded) && decoded.engine==4,"tagged decoder preserves FM 4-OP versions")) return 1;
        }
    }
    constexpr uint8_t prophet = 2;
    const auto count = gf::factory_prophet_count();
    if (!check(count > 0, "factory bank empty")) return 1;
    for (uint8_t i = 0; i < count; ++i) {
        studio::Preset input, output;
        std::array<uint8_t, studio::PRESET_BYTES> bytes{};
        if (!check(gf::factory_prophet_preset(i, input), "factory load")) return 1;
        if (!check(studio::preset_encode(input, prophet, bytes.data(), bytes.size()) == bytes.size(), "encode")) return 1;
        if (!check(gf::decode_prophet_preset(bytes.data(), bytes.size(), output), "decode")) return 1;
        if (!check(std::strcmp(input.name, output.name) == 0 && input.cutoff_hz == output.cutoff_hz &&
                   input.patch.osc[0][0] == output.patch.osc[0][0], "factory roundtrip")) return 1;
        std::array<uint8_t, studio::PRESET_BYTES> encoded_again{};
        if (!check(studio::preset_encode(output, prophet, encoded_again.data(), encoded_again.size()) == encoded_again.size() &&
                   encoded_again == bytes, "factory bytes roundtrip")) return 1;
        for (const auto version : {uint8_t(2), uint8_t(3), uint8_t(4)}) {
            auto legacy = bytes;
            legacy[3] = version;
            const size_t size = version == 2 ? studio::PRESET_BYTES_V2 :
                                version == 3 ? studio::PRESET_BYTES_V3 : studio::PRESET_BYTES_V4;
            studio::Preset migrated;
            if (!check(gf::decode_prophet_preset(legacy.data(), size, migrated) &&
                       std::strcmp(input.name, migrated.name) == 0 && migrated.cutoff_hz == input.cutoff_hz,
                       "legacy version migration")) return 1;
        }
        const auto previous = output.cutoff_hz;
        if (!check(!gf::decode_prophet_preset(bytes.data(), bytes.size() - 1, output) && output.cutoff_hz == previous,
                   "truncated data must preserve state")) return 1;
        bytes[4] = 1;
        if (!check(!gf::decode_prophet_preset(bytes.data(), bytes.size(), output), "wrong engine")) return 1;
        bytes[4] = prophet;
        const float bad = std::numeric_limits<float>::quiet_NaN();
        std::memcpy(bytes.data() + 5 + studio::PRESET_NAME_LEN, &bad, sizeof(bad));
        if (!check(!gf::decode_prophet_preset(bytes.data(), bytes.size(), output) && output.cutoff_hz == previous,
                   "non-finite data must preserve state")) return 1;
    }
    for (uint8_t i = 0; i < studio::factory_count(1); ++i) {
        studio::Preset input, output;
        std::array<uint8_t, studio::PRESET_BYTES> bytes{};
        uint8_t decoded_engine = 99;
        if (!check(studio::factory_preset(1, i, &input) &&
                   studio::preset_encode(input, 1, bytes.data(), bytes.size()) == bytes.size() &&
                   gf::decode_supported_preset(bytes.data(), bytes.size(), output, decoded_engine) &&
                   decoded_engine == 1 && std::strcmp(input.name, output.name) == 0, "Juno GFP factory roundtrip")) return 1;
        auto malformed = bytes; malformed[3] = 0;
        if (!check(!gf::decode_supported_preset(malformed.data(), malformed.size(), output, decoded_engine) &&
                   decoded_engine == 1, "invalid Juno GFP preserves output")) return 1;
    }
    if (!check(studio::factory_count(3) > 0, "OB-6 factory bank exists")) return 1;
    if (!check(studio::factory_count(0) > 0, "Model D factory bank exists")) return 1;
    for (uint8_t i = 0; i < studio::factory_count(0); ++i) {
        studio::Preset input, output; uint8_t engine = 99;
        std::array<uint8_t, studio::PRESET_BYTES> bytes{};
        if (!check(studio::factory_preset(0, i, &input) && studio::preset_encode(input, 0, bytes.data(), bytes.size()) == bytes.size() &&
                   gf::decode_supported_preset(bytes.data(), bytes.size(), output, engine) && engine == 0 &&
                   std::strcmp(input.name, output.name) == 0, "Model D GFP factory roundtrip")) return 1;
    }
    for (uint8_t i = 0; i < studio::factory_count(3); ++i) {
        studio::Preset input, output; uint8_t engine = 0;
        std::array<uint8_t, studio::PRESET_BYTES> bytes{};
        if (!check(studio::factory_preset(3, i, &input) && studio::preset_encode(input, 3, bytes.data(), bytes.size()) == bytes.size() &&
                   gf::decode_supported_preset(bytes.data(), bytes.size(), output, engine) && engine == 3 &&
                   std::strcmp(input.name, output.name) == 0, "OB-6 GFP factory roundtrip")) return 1;
    }
    studio::Patch patch;
    patch.osc[0][0] = 12; patch.osc[0][1] = 35; patch.osc[0][2] = 1; patch.osc[0][3] = 1; patch.osc[0][4] = 1;
    patch.osc[1][0] = -12; patch.osc[1][1] = 7; patch.osc[1][2] = 60; patch.osc[1][3] = 1;
    patch.osc[1][4] = 1; patch.osc[1][5] = 1; patch.osc[1][6] = 1; patch.osc[1][7] = 1;
    patch.osc[2][0] = 80; patch.osc[2][1] = 40; patch.osc[2][2] = 10; patch.osc[2][3] = 30; patch.osc[2][4] = 2;
    patch.glide = 0.2f;
    patch.mod[0][0] = 25; patch.mod[0][1] = 50; patch.mod[0][2] = 1; patch.mod[0][4] = 1;
    patch.mod[1][0] = 75; patch.mod[1][1] = 1; patch.mod[1][4] = 1; patch.mod[1][5] = 1;
    Receiver r;
    studio::apply_prophet_osc_patch(r, patch);
    if (!check(near(r.af, 12) && near(r.apw, .35f) && r.asaw && r.apulse && r.sync &&
               near(r.bi, -12) && near(r.bf, 7) && near(r.bpw, .6f) && r.keyboard && r.bsaw && r.btri &&
               r.bpulse && r.low && near(r.a, .8f) && near(r.b, .4f) && near(r.noise, .1f) &&
               near(r.glide, 600) && near(r.tracking, 1) && near(r.pm_env, .25f) && near(r.pm_b, .5f) &&
               r.pm_freq && !r.pm_pw && r.pm_filter && near(r.wheel, .75f) && r.wf_a && !r.wf_b && !r.wp_a &&
               r.wp_b && r.wfilter, "Prophet OSC/MOD mapping")) return 1;
    return 0;
}
