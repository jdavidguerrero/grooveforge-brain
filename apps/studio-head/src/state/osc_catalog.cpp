#include "state/osc_catalog.h"

#include <gf_params.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

// Slots reutilizables: mismo nombre y rango en varios engines.
const OscSlot WAVE      = {"WAVE", "", SlotKind::WaveSel, 0, 0, 0, false, false};
const OscSlot DETUNE    = {"DETUNE", "CT", SlotKind::Number, -50, 50, 0, true, false};
const OscSlot INTERVAL  = {"INTERVAL", "ST", SlotKind::Number, -24, 24, 0, true, false};
const OscSlot LEVEL     = {"LEVEL", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot NOISE     = {"NOISE", "%", SlotKind::Number, 0, 100, 0, false, true};
const OscSlot SUB       = {"SUB", "%", SlotKind::Number, 0, 100, 0, false, true};
const OscSlot RATIO     = {"RATIO", "×", SlotKind::Ratio, 0.5f, 8, 2, false, false};
const OscSlot RING      = {"RING MIX", "%", SlotKind::Number, 0, 100, 0, false, true};
// Prophet-5 completo + controles extendidos de Model D, Juno-106, OB-6 y ARP 2600
const OscSlot FREQ      = {"FREQ", "ST", SlotKind::Number, -24, 24, 0, true, false};
const OscSlot FINE      = {"FINE", "CT", SlotKind::Number, -50, 50, 0, true, false};
const OscSlot PW        = {"PW", "%", SlotKind::Number, 5, 95, 0, false, false};
const OscSlot PW_G      = {"PW", "%", SlotKind::Number, 5, 95, 0, false, true};
const OscSlot CHORUS_M  = {"CHORUS", "", SlotKind::ChorusMode, 0, 3, 0, false, true};
const OscSlot HPF_M     = {"HPF", "", SlotKind::HpfMode, 0, 3, 0, false, true};
const OscSlot FLT_MORPH = {"FLT MORPH", "%", SlotKind::Number, 0, 100, 0, false, true};
const OscSlot BP_MODE   = {"BP MODE", "", SlotKind::Toggle, 0, 1, 0, false, true};
const OscSlot SAW_T     = {"SAW", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot PULSE_T   = {"PULSE", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot TRI_T     = {"TRI", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot SYNC_T    = {"SYNC", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot KBD_T     = {"KEYBOARD", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot LOFREQ_T  = {"LO FREQ", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot LVL_A     = {"OSC A", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot LVL_B     = {"OSC B", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot LVL_N     = {"NOISE", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot GLIDE     = {"GLIDE", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot FLT_KBD   = {"FILTER KBD", "", SlotKind::KbdTrack, 0, 2, 0, false, false};
const OscSlot PM_ENV    = {"FILT ENV", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot PM_B      = {"OSC B", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot XM_VCO2   = {"VCO 2", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot D_FREQA   = {"FREQ A", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot D_FREQB   = {"FREQ B", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot D_PWA     = {"PW A", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot D_PWB     = {"PW B", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot D_FILTER  = {"FILTER", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot D_FREQ1   = {"FREQ 1", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot D_FMODE   = {"FLT MODE", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot D_PW1     = {"PW 1", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot WM_SRC    = {"LFO/NOISE", "%", SlotKind::Number, 0, 100, 0, false, false};
// Tercera vuelta (theory/fidelidad-engines.md §11), sincronizado con GF_MOD_SLOT / GF_OSC_SLOT
const char* const L_FM_WAVE[8] = {"W1", "W2", "W3", "W4", "W5", "W6", "W7", "W8"};
const char* const L_KRATE[4] = {"0", "1", "2", "3"};
const char* const L_FKBD4[4] = {"OFF", "1/3", "2/3", "FULL"};
const char* const L_NOISE[2] = {"WHITE", "PINK"};
const char* const L_RANGE[3] = {"16'", "8'", "4'"};
const char* const L_VCA[2] = {"ENV", "GATE"};
const OscSlot FM_W1     = {"WAVE OP1", "", SlotKind::Choice, 1, 8, 0, false, false, L_FM_WAVE};
const OscSlot FM_W2     = {"WAVE OP2", "", SlotKind::Choice, 1, 8, 0, false, false, L_FM_WAVE};
const OscSlot FM_W3     = {"WAVE OP3", "", SlotKind::Choice, 1, 8, 0, false, false, L_FM_WAVE};
const OscSlot FM_W4     = {"WAVE OP4", "", SlotKind::Choice, 1, 8, 0, false, false, L_FM_WAVE};
const OscSlot FM_D1     = {"DETUNE 1", "", SlotKind::Number, -3, 3, 0, true, false};
const OscSlot FM_D2     = {"DETUNE 2", "", SlotKind::Number, -3, 3, 0, true, false};
const OscSlot FM_D3     = {"DETUNE 3", "", SlotKind::Number, -3, 3, 0, true, false};
const OscSlot FM_D4     = {"DETUNE 4", "", SlotKind::Number, -3, 3, 0, true, false};
const OscSlot FM_KLVL   = {"KBD LEVEL", "", SlotKind::Number, 0, 99, 0, false, false};
const OscSlot FM_KRATE  = {"KBD RATE", "", SlotKind::Choice, 0, 3, 0, false, false, L_KRATE};
const OscSlot FM_D2R    = {"D2 RATE", "MS", SlotKind::Number, 0, 5000, 0, false, false};
const OscSlot MD_MIX    = {"MOD MIX", "%", SlotKind::MixAB, 0, 100, 0, false, false};
const OscSlot MD_OSC    = {"OSC MOD", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot MD_FLT    = {"FILTER MOD", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot MD_NOISE  = {"NOISE", "", SlotKind::Choice, 0, 1, 0, false, false, L_NOISE};
const OscSlot MD_V3KBD  = {"VCO3 KBD", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot MD_V3LO   = {"VCO3 LO", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot MD_ODRIVE = {"OVERDRIVE", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot MD_FKBD   = {"FILTER KBD", "", SlotKind::Choice, 0, 3, 0, false, false, L_FKBD4};
const OscSlot JU_DELAY  = {"LFO DELAY", "MS", SlotKind::Number, 0, 3000, 0, false, false};
const OscSlot JU_RANGE  = {"RANGE", "", SlotKind::Choice, 0, 2, 0, false, true, L_RANGE};
const OscSlot JU_VCA    = {"VCA", "", SlotKind::Choice, 0, 1, 0, false, true, L_VCA};
const OscSlot OB_SHAPE  = {"SHAPE", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot SYNC_D    = {"SYNC", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot AR_MONO   = {"MONO", "", SlotKind::Toggle, 0, 1, 0, false, false};
const OscSlot AR_V3RATE = {"VCO3 RATE", "HZ", SlotKind::Ratio, 0.1f, 30, 1, false, false};
const OscSlot AR_V3PIT  = {"VCO3>PITCH", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot AR_V3CUT  = {"VCO3>CUTOFF", "%", SlotKind::Number, 0, 100, 0, false, false};
const OscSlot AR_NCOLOR = {"NOISE COLOR", "%", SlotKind::Number, 0, 100, 0, false, false};

const ModStop PROPHET_MOD[2] = {
    {"POLY-MOD", {&PM_ENV, &PM_B, &D_FREQA, &D_PWA, &D_FILTER, nullptr, nullptr, nullptr}},
    {"WHEEL-MOD", {&WM_SRC, &D_FREQA, &D_FREQB, &D_PWA, &D_PWB, &D_FILTER, nullptr, nullptr}},
};
const ModStop OB6_MOD[1] = {
    {"X-MOD", {&PM_ENV, &XM_VCO2, &D_FREQ1, &D_FMODE, &D_PW1, &SYNC_D, nullptr, nullptr}},
};
const ModStop MOOG_MOD[1] = {
    {"MODULATION", {&MD_MIX, &MD_OSC, &MD_FLT, &MD_NOISE, &MD_V3KBD, &MD_V3LO, &MD_ODRIVE, &MD_FKBD}},
};
const ModStop JUNO_MOD[1] = {
    {"LFO", {&JU_DELAY, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr}},
};
const ModStop DX7_MOD[2] = {
    {"WAVE / DETUNE", {&FM_W1, &FM_W2, &FM_W3, &FM_W4, &FM_D1, &FM_D2, &FM_D3, &FM_D4}},
    {"KBD / EG", {&FM_KLVL, &FM_KRATE, &FM_D2R, nullptr, nullptr, nullptr, nullptr, nullptr}},
};
const ModStop ARP_MOD[1] = {
    {"PATCH", {&AR_MONO, &SYNC_D, &AR_V3RATE, &AR_V3PIT, &AR_V3CUT, &AR_NCOLOR, nullptr, nullptr}},
};
const ModStop NO_MOD = {"", {}};

// Tabla "OSC SLOTS PER ENGINE · FROM FIRMWARE" sincronizada con GF_OSC_SLOT en gf_params.h.
const OscEngine ENGINES[6] = {
    // MODEL D: 3 VCO + NOISE. SHIFT: PW global y FINE en VCO 3.
    {3, 4, {{"VCO 1", true}, {"VCO 2", true}, {"VCO 3", true}, {"NOISE", false}},
     {{&WAVE, nullptr, &LEVEL, &NOISE, &PW_G, nullptr, nullptr, nullptr},
      {&WAVE, &DETUNE, &LEVEL, &NOISE, &PW_G, nullptr, nullptr, nullptr},
      {&WAVE, &INTERVAL, &LEVEL, &NOISE, &PW_G, &FINE, nullptr, nullptr}},
     {Wave::Saw, Wave::Tri, Wave::Sqr, Wave::Pulse}, 4, false},
    // JUNO-106: 1 DCO (SAW/SQR/PULSE/SAW+PLS) + SUB + NOISE + PW | SHIFT: CHORUS (OFF/I/II/I+II) · HPF (0..3).
    {1, 3, {{"DCO", true}, {"SUB", false}, {"NOISE", false}},
     {{&WAVE, &SUB, &NOISE, &PW, &CHORUS_M, &HPF_M, &JU_RANGE, &JU_VCA}},
     {Wave::Saw, Wave::Sqr, Wave::Pulse, Wave::SawPulse}, 4, false},
    // PROPHET-5 completo: OSC A · OSC B · MIXER, 8 slots por parada (capa SHIFT: SYNC, formas de B, FILTER KBD)
    {3, 3, {{"OSC A", true}, {"OSC B", true}, {"MIXER", true}},
     {{&FREQ, &PW, &SAW_T, &PULSE_T, &SYNC_T, nullptr, nullptr, nullptr},
      {&FREQ, &FINE, &PW, &KBD_T, &SAW_T, &TRI_T, &PULSE_T, &LOFREQ_T},
      {&LVL_A, &LVL_B, &LVL_N, &GLIDE, &FLT_KBD, nullptr, nullptr, nullptr}},
     {Wave::Saw, Wave::Tri, Wave::Pulse}, 3, false},
    // OB-6: VCO 1 · 2 + SUB + NOISE | SHIFT: PW · — / DETUNE · FLT MORPH (LP↔N↔HP) · BP MODE.
    {2, 4, {{"VCO 1", true}, {"VCO 2", true}, {"SUB", false}, {"NOISE", false}},
     {{&OB_SHAPE, &SUB, &LEVEL, &NOISE, &PW, nullptr, &FLT_MORPH, &BP_MODE},
      {&OB_SHAPE, &INTERVAL, &LEVEL, &NOISE, &PW, &DETUNE, &FLT_MORPH, &BP_MODE}},
     {Wave::Saw, Wave::Tri, Wave::Sqr, Wave::Pulse}, 4, false},
    // DX7: FM de 4 operadores (ADR-005). La página dibuja operadores y algoritmo, no esta tabla.
    {4, 4, {{"OP 1", true}, {"OP 2", true}, {"OP 3", true}, {"OP 4", true}},
     {},
     {Wave::Sine}, 1, true},
    // ARP 2600: VCO 1 · 2 + RING MOD + NOISE | P2: PW en VCO 1, RATIO en VCO 2 | SHIFT: LEVEL directo por VCO.
    {2, 4, {{"VCO 1", true}, {"VCO 2", true}, {"RING", false}, {"NOISE", false}},
     {{&WAVE, &PW, &RING, &NOISE, &LEVEL, nullptr, nullptr, nullptr},
      {&WAVE, &RATIO, &RING, &NOISE, &LEVEL, nullptr, nullptr, nullptr}},
     {Wave::Saw, Wave::Tri, Wave::Sqr, Wave::Pulse, Wave::Sine}, 5, false},
};

}  // namespace

uint8_t mod_stop_count(uint8_t engine_id) {
    return engine_id < GF_ENGINE_COUNT ? GF_MOD_STOPS[engine_id] : 0;
}

const ModStop& mod_stop(uint8_t engine_id, int stop) {
    if (stop < 0 || stop >= int(mod_stop_count(engine_id))) return NO_MOD;
    switch (engine_id) {
        case 0: return MOOG_MOD[0];
        case 1: return JUNO_MOD[0];
        case 2: return PROPHET_MOD[stop];
        case 3: return OB6_MOD[0];
        case 4: return DX7_MOD[stop];
        case 5: return ARP_MOD[0];
        default: return NO_MOD;
    }
}

const char* wave_name(Wave w) {
    static const char* const N[] = {"SAW", "TRI", "SQR", "PULSE", "SINE", "S&H", "NOISE", "SAW+PLS"};
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
    // MODEL D: SAW / SAW / SQR, +7 CT, −12 ST, niveles 80 / 72 / 50, ruido 12, PW 50%, FINE 0 CT
    set(0, 0, 0, 0); set(0, 1, 0, 0); set(0, 2, 0, 2);
    set(0, 1, 1, 7); set(0, 2, 1, -12);
    set(0, 0, 2, 80); set(0, 1, 2, 72); set(0, 2, 2, 50); set(0, 0, 3, 12);
    set(0, 0, 4, 50); set(0, 2, 5, 0);
    s.mod_val[0][0][4] = 1;                                   // MODULATION: VCO3 KBD ON (= patch_init)
    // JUNO-106: SAW, SUB 60, NOISE 10, PW 50, CHORUS I (1), HPF FLAT (1)
    set(1, 0, 0, 0); set(1, 0, 1, 60); set(1, 0, 2, 10); set(1, 0, 3, 50); set(1, 0, 4, 1); set(1, 0, 5, 1);
    set(1, 0, 6, 1); set(1, 0, 7, 0);                         // RANGE 8' · VCA ENV
    // PROPHET-5: A saw, B pulse +7 ST, MIXER 60 / 40 (= patch_init del firmware); WHEEL-MOD → FREQ A y B
    set(2, 0, 1, 50); set(2, 0, 2, 1); set(2, 1, 0, 7); set(2, 1, 2, 50); set(2, 1, 3, 1); set(2, 1, 6, 1);
    set(2, 2, 0, 60); set(2, 2, 1, 40);
    s.mod_val[2][1][1] = 1; s.mod_val[2][1][2] = 1;
    // OB-6: SAW / SAW, SUB 40, +7 ST, niveles 80 / 70, ruido 0, PW 50/50, DETUNE +4 CT, MORPH 0, BP OFF
    set(3, 0, 0, 0); set(3, 1, 0, 100); set(3, 0, 1, 40); set(3, 1, 1, 7);   // SHAPE: saw / pulse
    set(3, 0, 2, 80); set(3, 1, 2, 70); set(3, 0, 3, 0);
    set(3, 0, 4, 50); set(3, 1, 4, 50); set(3, 1, 5, 4); set(3, 0, 6, 0); set(3, 0, 7, 0);
    // ARP 2600: SAW / SINE, PW 50%, ×1.50, RING 40, NOISE 5, VCO1 80%, VCO2 70%
    set(5, 0, 0, 0); set(5, 0, 1, 50); set(5, 1, 0, 4); set(5, 1, 1, 1.5f); set(5, 0, 2, 40); set(5, 0, 3, 5);
    set(5, 0, 4, 80); set(5, 1, 4, 70);
    s.mod_val[5][0][0] = 1; s.mod_val[5][0][2] = 5;           // PATCH: MONO, VCO3 5 Hz
    for (int k = 0; k < 4; ++k) s.mod_val[4][0][k] = 1;      // DX7: W1 en los 4 operadores
    s.mod_val[4][0][4] = 1; s.mod_val[4][0][6] = -1;
}

void osc_format(const OscEngine& e, const OscSlot& sl, float v, char* out, size_t n) {
    switch (sl.kind) {
        case SlotKind::WaveSel:
            std::snprintf(out, n, "%s", wave_name(e.waves[std::clamp(int(v), 0, e.wave_count - 1)]));
            break;
        case SlotKind::Ratio:
            std::snprintf(out, n, "%.*f", int(sl.decimals), double(v));
            break;
        case SlotKind::Toggle:
            std::snprintf(out, n, "%s", v > 0.5f ? "ON" : "OFF");
            break;
        case SlotKind::KbdTrack: {
            static const char* const K[3] = {"OFF", "HALF", "FULL"};
            std::snprintf(out, n, "%s", K[std::clamp(int(std::lround(v)), 0, 2)]);
            break;
        }
        case SlotKind::ChorusMode: {
            static const char* const C[4] = {"OFF", "I", "II", "I+II"};
            std::snprintf(out, n, "%s", C[std::clamp(int(std::lround(v)), 0, 3)]);
            break;
        }
        case SlotKind::HpfMode: {
            static const char* const H[4] = {"BST", "FLAT", "225", "720"};
            std::snprintf(out, n, "%s", H[std::clamp(int(std::lround(v)), 0, 3)]);
            break;
        }
        case SlotKind::Choice: {
            const int i = int(std::lround(v - sl.lo)), n_lab = int(std::lround(sl.hi - sl.lo)) + 1;
            std::snprintf(out, n, "%s", sl.labels ? sl.labels[std::clamp(i, 0, n_lab - 1)] : "?");
            break;
        }
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
