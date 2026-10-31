#include "preset_codec.h"

#include <cmath>
#include <initializer_list>

namespace gf {
namespace {
constexpr uint8_t PROPHET_ENGINE = 2;

bool finite_preset(const studio::Preset& p) {
    const auto& t = p.patch;
    const float scalar[] = {p.cutoff_hz, p.reso_norm, t.attack_ms, t.decay_ms, t.sustain, t.release_ms,
        t.f_attack_ms, t.f_decay_ms, t.f_sustain, t.f_release_ms, t.env_amt, t.vel_amp, t.vel_cut,
        t.glide, t.drive, t.lfo_rate_hz};
    for (float x : scalar) if (!std::isfinite(x)) return false;
    for (float x : t.lfo_amt) if (!std::isfinite(x)) return false;
    for (const auto& row : t.osc) for (float x : row) if (!std::isfinite(x)) return false;
    for (const auto& row : t.mod) for (float x : row) if (!std::isfinite(x)) return false;
    for (float x : t.sfx_mix) if (!std::isfinite(x)) return false;
    for (const auto& row : t.sfx_p) for (float x : row) if (!std::isfinite(x)) return false;
    for (const auto& row : t.groove) for (float x : row) if (!std::isfinite(x)) return false;
    return true;
}
}

bool decode_prophet_preset(const uint8_t* data, size_t size, studio::Preset& out) {
    if (!data) return false;
    studio::Preset candidate;
    if (!studio::preset_decode(data, size, PROPHET_ENGINE, &candidate) || !finite_preset(candidate)) return false;
    out = candidate;
    return true;
}

bool decode_supported_preset(const uint8_t* data, size_t size, studio::Preset& out, uint8_t& engine) {
    if (!data) return false;
    for (uint8_t candidate_engine : {uint8_t(0), uint8_t(1), uint8_t(2), uint8_t(3), uint8_t(4)}) {
        studio::Preset candidate;
        if (studio::preset_decode(data, size, candidate_engine, &candidate) && finite_preset(candidate)) {
            out = candidate; engine = candidate_engine; return true;
        }
    }
    return false;
}

bool factory_prophet_preset(uint8_t index, studio::Preset& out) {
    studio::Preset candidate;
    if (!studio::factory_preset(PROPHET_ENGINE, index, &candidate) || !finite_preset(candidate)) return false;
    out = candidate;
    return true;
}

uint8_t factory_prophet_count() { return studio::factory_count(PROPHET_ENGINE); }

}  // namespace gf
