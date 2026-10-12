#include "state/fx_catalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
const char* const MATERIALS[] = {"BELL", "GUADUA", "GLASS", "WOOD"};
const char* const ALGORITHMS[] = {"SPRING", "PLATE", "BLEND"};
}  // namespace

const FxInfo FX_CATALOG[FX_SLOTS] = {
    {"CYMATIC RESONATOR", true, FxRoute::Insert, FxSig::Cymatic,
     {{"RESON", "%", 0, 100, 70, 0}, {"DENSITY", "MODES", 1, 4, 4, 0}, {"TUNE", "", 1, 4, 1.59f, 2}}},
    {"GRANULAR CLOUD", true, FxRoute::Insert, FxSig::Granular,
     {{"SIZE", "MS", 5, 500, 120, 0}, {"DENSITY", "/S", 1, 50, 40, 0}, {"PITCH SPRD", "ST", 0, 12, 7, 0}}},
    {"GHOST ECHO", true, FxRoute::Send, FxSig::Ghost,
     {{"FEEDBK", "%", 0, 95, 58, 0}, {"TIME", "MS", 10, 2000, 375, 0}, {"GHOST", "%", 0, 100, 24, 0}}},
    {"SPECTRAL SMEAR", false, FxRoute::Insert, FxSig::None,
     {{"SMEAR", "MS", 50, 2000, 400, 0}, {"RESOLUTION", "BND", 8, 64, 32, 0}, {"TILT", "%", -100, 100, 0, 0}}},
    {"TAPE SATURATE", true, FxRoute::Master, FxSig::Tape,
     {{"DRIVE", "DB", 0, 12, 7.4f, 1}, {"WOW", "%", 0, 100, 40, 0}, {"FLUTTER", "%", 0, 100, 50, 0}}},
    {"BIT SCULPT", true, FxRoute::Insert, FxSig::Bit,
     {{"BITS", "", 1, 16, 4, 0}, {"RATE", "KHZ", 1, 48, 12, 0}, {"SCULPT", "%", 0, 100, 30, 0}}},
    {"MODAL REVERB", true, FxRoute::Send, FxSig::Modal,
     {{"DECAY", "S", 0.2f, 8, 2.4f, 1}, {"MATERIAL", "", 0, 3, 1, 0, MATERIALS}, {"SIZE", "%", 0, 100, 50, 0}}},
    {"PHASE CHORUS", true, FxRoute::Insert, FxSig::Chorus,
     {{"DEPTH", "%", 0, 100, 50, 0}, {"RATE", "HZ", 0.1f, 10, 0.4f, 1}, {"DRIFT", "%", 0, 100, 20, 0}}},
    {"PITCH MOSAIC", false, FxRoute::Insert, FxSig::None,
     {{"INT 1", "ST", -24, 24, 7, 0}, {"INTERVAL 2", "ST", -24, 24, 12, 0}, {"SCALE LOCK", "", 0, 1, 1, 0}}},
    {"SPRING + PLATE", true, FxRoute::Master, FxSig::Spring,
     {{"DECAY", "S", 0.5f, 10, 1.8f, 1}, {"ALGORITHM", "", 0, 2, 2, 0, ALGORITHMS}, {"DAMPING", "%", 0, 100, 40, 0}}},
    {"GLITCH STUTTER", false, FxRoute::Insert, FxSig::None,
     {{"PATTERN", "1/N", 4, 32, 16, 0}, {"PROBABILITY", "%", 0, 100, 50, 0}, {"PITCH", "ST", -12, 12, 0, 0}}},
    {"SUB GENESIS", true, FxRoute::Master, FxSig::Sub,
     {{"SUB LVL", "DB", 0, 12, 6, 1}, {"OCTAVES", "", 1, 2, 2, 0}, {"DRIVE", "%", 0, 100, 30, 0}}},
};

void fx_load_defaults(StudioState& s) {
    static const float DRY_WET[FX_SLOTS] = {55, 60, 42, 50, 100, 70, 35, 50, 50, 40, 100, 100};
    for (int i = 0; i < FX_SLOTS; ++i) {
        s.fx_val[i][0] = DRY_WET[i];
        for (int k = 0; k < 3; ++k) s.fx_val[i][k + 1] = FX_CATALOG[i].p[k].def;
        s.fx_route[i] = FX_CATALOG[i].layer;
    }
}

float fx_frac(const FxParam& p, float v) {
    return std::clamp((v - p.lo) / (p.hi - p.lo), 0.0f, 1.0f);
}

void fx_format(const FxParam& p, float v, char* out, size_t n) {
    if (p.labels) {
        const int i = std::clamp(int(std::lround(v)), int(p.lo), int(p.hi));
        std::snprintf(out, n, "%s", p.labels[i]);
        return;
    }
    std::snprintf(out, n, "%.*f", int(p.decimals), double(v));
}

uint8_t fx_next_slot(uint8_t slot, int dir) {
    int i = slot;
    for (int k = 0; k < FX_SLOTS; ++k) {
        i = (i + dir + FX_SLOTS) % FX_SLOTS;
        if (FX_CATALOG[i].available) return uint8_t(i);
    }
    return slot;
}
