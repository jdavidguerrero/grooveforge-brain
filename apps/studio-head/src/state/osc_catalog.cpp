#include "state/osc_catalog.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// Slots reutilizables: mismo nombre y rango en varios engines.
const OscSlot WAVE     = {"WAVE", "", SlotKind::WaveSel, 0, 0, 0, false, false};
const OscSlot DETUNE   = {"DETUNE", "CT", SlotKind::Number, -50, 50, 0, true, false};
const OscSlot INTERVAL = {"INTERVAL", "ST", SlotKind::Number, -24, 24, 0, true, false};
const OscSlot LEVEL    = {"LEVEL", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot NOISE    = {"NOISE", "%", SlotKind::Number, 0, 100, 0, false, true};
const OscSlot SUB      = {"SUB", "%", SlotKind::Number, 0, 100, 0, false, true};
const OscSlot XMOD     = {"CROSSMOD", "%", SlotKind::Number, 0, 100, 0, false, true};
const OscSlot MIX_AB   = {"MIX A/B", "", SlotKind::MixAB, 0, 100, 0, false, true};
const OscSlot RATIO    = {"RATIO", "×", SlotKind::Ratio, 0.5f, 8, 2, false, false};
const OscSlot RING     = {"RING MIX", "%", SlotKind::Number, 0, 100, 0, false, true};

// Tabla "OSC SLOTS PER ENGINE · FROM FIRMWARE" del diseño v3 (2026-10-12), en orden de engine_id.
const OscEngine ENGINES[6] = {
    // MODEL D: 3 VCO + NOISE. P2 = — en VCO 1 (referencia), DETUNE en VCO 2, INTERVAL en VCO 3.
    {3, 4, {{"VCO 1", true}, {"VCO 2", true}, {"VCO 3", true}, {"NOISE", false}},
     {{&WAVE, nullptr, &LEVEL, &NOISE}, {&WAVE, &DETUNE, &LEVEL, &NOISE}, {&WAVE, &INTERVAL, &LEVEL, &NOISE}},
     {Wave::Saw, Wave::Tri, Wave::Sqr, Wave::Pulse}, 4, false},
    // JUNO-106: 1 DCO + SUB (square −1 oct fijo) + NOISE. NAV sin función.
    {1, 3, {{"DCO", true}, {"SUB", false}, {"NOISE", false}},
     {{&WAVE, &SUB, &NOISE, nullptr}},
     {Wave::Saw, Wave::Sqr, Wave::Pulse}, 3, false},
    // PROPHET-5: OSC A · B. INTERVAL solo en B; CROSSMOD y MIX son del engine.
    {2, 2, {{"OSC A", true}, {"OSC B", true}},
     {{&WAVE, nullptr, &XMOD, &MIX_AB}, {&WAVE, &INTERVAL, &XMOD, &MIX_AB}},
     {Wave::Saw, Wave::Tri, Wave::Pulse}, 3, false},
    // OB-6: VCO 1 · 2 + SUB + NOISE. Slot P2 por parada: SUB en VCO 1, INTERVAL en VCO 2 (abierto en el diseño).
    {2, 4, {{"VCO 1", true}, {"VCO 2", true}, {"SUB", false}, {"NOISE", false}},
     {{&WAVE, &SUB, &LEVEL, &NOISE}, {&WAVE, &INTERVAL, &LEVEL, &NOISE}},
     {Wave::Saw, Wave::Tri, Wave::Sqr, Wave::Pulse}, 4, false},
    // DX7: FM de 4 operadores (ADR-005). La página dibuja operadores y algoritmo, no esta tabla.
    {4, 4, {{"OP 1", true}, {"OP 2", true}, {"OP 3", true}, {"OP 4", true}},
     {},
     {Wave::Sine}, 1, true},
    // ARP 2600: VCO 1 · 2 + RING MOD + NOISE. RATIO solo en VCO 2.
    {2, 4, {{"VCO 1", true}, {"VCO 2", true}, {"RING", false}, {"NOISE", false}},
     {{&WAVE, nullptr, &RING, &NOISE}, {&WAVE, &RATIO, &RING, &NOISE}},
     {Wave::Saw, Wave::Tri, Wave::Sqr, Wave::Pulse, Wave::Sine}, 5, false},
};

}  // namespace

const char* wave_name(Wave w) {
    static const char* const N[] = {"SAW", "TRI", "SQR", "PULSE", "SINE", "S&H", "NOISE"};
    return N[int(w)];
}

const OscEngine& osc_engine(uint8_t engine_id) {
    return ENGINES[engine_id < 6 ? engine_id : 0];
}

float& osc_value(StudioState& s, uint8_t engine, int stop, int slot) {
    const OscSlot* sl = osc_engine(engine).slot[stop][slot];
    return s.osc_val[engine][sl && sl->global ? 0 : stop][slot];
}

float osc_value(const StudioState& s, uint8_t engine, int stop, int slot) {
    return osc_value(const_cast<StudioState&>(s), engine, stop, slot);
}

void osc_load_defaults(StudioState& s) {
    // Valores de los ejemplos del diseño (11a, 11d, 11e) y plausibles para el resto.
    auto set = [&](uint8_t e, int stop, int slot, float v) { osc_value(s, e, stop, slot) = v; };
    // MODEL D: SAW / SAW / SQR, +7 CT, −12 ST, niveles 80 / 72 / 50, ruido 12
    set(0, 0, 0, 0); set(0, 1, 0, 0); set(0, 2, 0, 2);
    set(0, 1, 1, 7); set(0, 2, 1, -12);
    set(0, 0, 2, 80); set(0, 1, 2, 72); set(0, 2, 2, 50); set(0, 0, 3, 12);
    // JUNO-106: SAW, SUB 60, NOISE 10
    set(1, 0, 0, 0); set(1, 0, 1, 60); set(1, 0, 2, 10);
    // PROPHET-5: SAW / PULSE, +7 ST, CROSSMOD 30, MIX 60:40
    set(2, 0, 0, 0); set(2, 1, 0, 2); set(2, 1, 1, 7); set(2, 0, 2, 30); set(2, 0, 3, 60);
    // OB-6: SAW / SAW, SUB 40, +7 ST, niveles 80 / 70, ruido 0
    set(3, 0, 0, 0); set(3, 1, 0, 0); set(3, 0, 1, 40); set(3, 1, 1, 7);
    set(3, 0, 2, 80); set(3, 1, 2, 70); set(3, 0, 3, 0);
    // ARP 2600: SAW / SINE, ×1.50, RING 40, NOISE 5
    set(5, 0, 0, 0); set(5, 1, 0, 4); set(5, 1, 1, 1.5f); set(5, 0, 2, 40); set(5, 0, 3, 5);
}

void osc_format(const OscEngine& e, const OscSlot& sl, float v, char* out, size_t n) {
    switch (sl.kind) {
        case SlotKind::WaveSel:
            std::snprintf(out, n, "%s", wave_name(e.waves[std::clamp(int(v), 0, e.wave_count - 1)]));
            break;
        case SlotKind::Ratio:
            std::snprintf(out, n, "%.*f", int(sl.decimals), double(v));
            break;
        case SlotKind::MixAB: {
            const int a = int(std::lround(v));
            std::snprintf(out, n, "%d:%d", a, 100 - a);
            break;
        }
        case SlotKind::Number: {
            const int iv = int(std::lround(v));
            if (sl.signed_value && iv > 0) std::snprintf(out, n, "+%d", iv);
            else if (sl.signed_value && iv < 0) std::snprintf(out, n, "−%d", -iv);
            else std::snprintf(out, n, "%d", iv);
            break;
        }
    }
}

float osc_frac(const OscEngine& e, const OscSlot& sl, float v) {
    if (sl.kind == SlotKind::WaveSel) return e.wave_count > 1 ? std::clamp(v / (e.wave_count - 1), 0.0f, 1.0f) : 0.0f;
    return std::clamp((v - sl.lo) / (sl.hi - sl.lo), 0.0f, 1.0f);
}
