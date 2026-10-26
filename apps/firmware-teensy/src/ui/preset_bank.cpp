#include "preset_bank.h"

#include <stdio.h>
#include <string.h>

namespace studio {

namespace {

constexpr uint8_t MAGIC[3] = {'G', 'F', 'P'};

/// Recorre los campos del preset en el orden del archivo; el mismo código escribe y lee (sin desfasajes).
template <typename F, typename B>
void visit(Preset& p, uint8_t format, F&& f32, B&& u8) {
    Patch& t = p.patch;
    float* const scalars[12] = {&t.attack_ms, &t.decay_ms,   &t.sustain,   &t.release_ms, &t.f_attack_ms,
                                &t.f_decay_ms, &t.f_sustain, &t.f_release_ms, &t.env_amt,   &t.glide,
                                &t.drive,     &t.lfo_rate_hz};
    for (float* s : scalars) f32(*s);
    for (float& a : t.lfo_amt) f32(a);
    for (auto& st : t.osc)
        for (float& v : st) f32(v);
    for (auto& st : t.mod)
        for (float& v : st) f32(v);
    for (float& m : t.sfx_mix) f32(m);
    for (auto& fx : t.sfx_p)
        for (float& v : fx) f32(v);
    f32(p.cutoff_hz);
    f32(p.reso_norm);
    u8(t.lfo_shape);
    u8(t.lfo_div);
    for (bool& on : t.sfx_on) {
        uint8_t b = on;
        u8(b);
        on = b != 0;
    }
    if (format < 3) return;                     // formato 2: GROOVE queda en los valores neutros de Patch
    for (auto& st : t.groove)
        for (float& v : st) f32(v);
    for (bool& on : t.groove_on) {
        uint8_t b = on;
        u8(b);
        on = b != 0;
    }
    if (format < 4) return;                     // formato 3: velocity por defecto del engine (preset_decode)
    f32(t.vel_amp);
    f32(t.vel_cut);
}

void set_name(char* dst, const char* src) {
    strncpy(dst, src, PRESET_NAME_LEN);
    dst[PRESET_NAME_LEN] = '\0';
}

/// Un patch de fábrica: lo que cambia respecto de patch_init() del engine.
struct Factory {
    const char* name;
    float a, d, s, r;            ///< ENV AMP (ms, ms, 0–1, ms)
    float fa, fd, fs, fr;        ///< ENV FILTER
    float env_amt, cutoff_hz, reso, glide;
    float lfo_hz;
    uint8_t lfo_shape;
    float amt_cut, amt_pitch, amt_third;
    void (*osc)(Patch&);         ///< página OSC / MOD (nullptr = la de patch_init)
};

using O = float (&)[GF_OSC_STOPS][GF_OSC_SLOTS];

// theory/06 §8. Waves: MODEL D 0 SAW 1 TRI 2 SQR 3 PULSE · OB-6 SHAPE 0 (saw) – 100 (pulse) · JUNO 0 SAW 1 SQR 2 PULSE 3 SAW+PLS ·
// ARP 0 SAW 1 TRI 2 SQR 3 PULSE 4 SINE. DX7 por operador: RATIO · LEVEL · ALG · FB | A · D · S % · R.
const Factory MODEL_D[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 800, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"FAT BASS", 2, 400, 0.6f, 120, 2, 350, 0.15f, 150, 0.45f, 300, 0.25f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[2][0] = 2; o[1][1] = 6; o[2][1] = -12;
                    o[0][2] = 90; o[1][2] = 80; o[2][2] = 70; o[3][0] = 0; }},
    {"70S LEAD", 3, 500, 0.8f, 250, 3, 600, 0.5f, 300, 0.35f, 1200, 0.4f, 0.35f, 5.5f, 0, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[2][0] = 2; o[1][1] = 9; o[2][1] = 12;
                    o[0][2] = 80; o[1][2] = 75; o[2][2] = 40; o[3][0] = 0; }},
    {"BRASS", 60, 600, 0.7f, 250, 80, 700, 0.4f, 250, 0.5f, 500, 0.1f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[2][0] = 0; o[1][1] = 7; o[2][1] = 12;
                    o[0][2] = 80; o[1][2] = 80; o[2][2] = 50; o[3][0] = 0; }},
    {"FLUTE", 80, 200, 0.9f, 200, 60, 300, 0.7f, 200, 0.1f, 1500, 0.1f, 0, 5, 0, 0, 0.12f, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 1; o[0][2] = 80; o[1][2] = 0; o[2][2] = 0; o[3][0] = 15; o[3][1] = 1; }},   // ruido rosa
    {"PLUCK", 1, 250, 0.0f, 200, 1, 200, 0.0f, 200, 0.6f, 250, 0.35f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 2; o[1][0] = 0; o[1][1] = 4; o[0][2] = 80; o[1][2] = 70; o[2][2] = 0;
                    o[3][0] = 0; }},
    // Presets del track (variants/studio/presets-track-run.md): mitad A de GF_Run_FunkBass
    {"JD RUN FUNKBASS", 0, 220, 0.15f, 80, 0, 220, 0.15f, 80, 0.7f, 160, 0.25f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[2][0] = 2; o[1][1] = 6; o[2][1] = -12;
                    o[0][2] = 90; o[1][2] = 85; o[2][2] = 70; o[3][0] = 0; p.drive = 0.35f;
                    p.vel_amp = 60; p.vel_cut = 80; }},   // ghost notes: murmullo a 45–53, mordida a 74–81
    // Pilotos Signature (caracter-sonoro §9): velocity propia + POCKET
    {"GF POCKET BASS", 1, 300, 0.3f, 120, 1, 250, 0.1f, 120, 0.6f, 250, 0.3f, 0.1f, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[2][0] = 2; o[1][1] = 5; o[2][1] = -12;
                    o[0][2] = 90; o[1][2] = 80; o[2][2] = 60; o[3][0] = 0;
                    p.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET] = 60; p.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 15;
                    p.vel_amp = 30; p.vel_cut = 50; }},
};

const Factory JUNO[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 1000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"STRINGS", 300, 800, 0.85f, 900, 300, 800, 0.6f, 900, 0.1f, 2500, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[0][1] = 0; o[0][2] = 0; o[0][4] = 2; o[0][5] = 2; }},
    {"PWM PAD", 400, 1000, 0.8f, 1200, 400, 1000, 0.5f, 1200, 0.15f, 1800, 0.15f, 0, 0.6f, 1, 0, 0, 0.6f,
     [](Patch& p) { O o = p.osc; o[0][0] = 2; o[0][1] = 20; o[0][2] = 0; o[0][3] = 50; o[0][4] = 1; o[0][5] = 1; }},
    {"SUB BASS", 1, 300, 0.7f, 80, 1, 250, 0.2f, 100, 0.35f, 400, 0.2f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 1; o[0][1] = 100; o[0][2] = 0; o[0][4] = 0; o[0][5] = 0; }},
    {"BRASS STAB", 15, 300, 0.5f, 200, 20, 300, 0.3f, 200, 0.55f, 600, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 3; o[0][1] = 30; o[0][2] = 0; o[0][3] = 40; o[0][4] = 1; o[0][5] = 1; }},
    {"PLUCK KEYS", 1, 600, 0.0f, 300, 1, 400, 0.0f, 300, 0.55f, 500, 0.2f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[0][1] = 30; o[0][2] = 0; o[0][4] = 1; o[0][5] = 1; }},
    // Presets del track: mitad B de GF_Horizon_Pedal, mitad A de GF_Disco_RapStab y de GF_Vocal_VelvetPad
    {"JD HORIZON JUNO", 120, 1000, 0.85f, 600, 120, 1000, 0.6f, 600, 0.15f, 1500, 0.1f, 0, 0.6f, 1, 0, 0, 0.5f,
     [](Patch& p) { O o = p.osc; o[0][0] = 2; o[0][1] = 0; o[0][2] = 0; o[0][3] = 50; o[0][4] = 2; o[0][5] = 1; }},
    {"JD RAP STAB JUNO", 2, 220, 0.0f, 110, 12, 160, 0.0f, 110, 0.8f, 450, 0.35f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 3; o[0][1] = 0; o[0][2] = 0; o[0][3] = 40; o[0][4] = 1; o[0][5] = 1;
                    p.vel_amp = 40; p.vel_cut = 50; }},
    {"JD VELVET JUNO", 60, 800, 0.7f, 450, 60, 800, 0.5f, 450, 0.3f, 650, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[0][1] = 20; o[0][2] = 0; o[0][4] = 2; o[0][5] = 1;
                    p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 60; }},
    // Piloto Signature: pad PWM bombeando con el reloj (PUMP KICK 60 %, 1/4)
    {"GF PUMP PAD", 300, 1000, 0.85f, 900, 300, 1000, 0.6f, 900, 0.15f, 1600, 0.1f, 0, 0.5f, 1, 0, 0, 0.5f,
     [](Patch& p) { O o = p.osc; o[0][0] = 2; o[0][1] = 30; o[0][2] = 0; o[0][3] = 50; o[0][4] = 2; o[0][5] = 1;
                    float* pu = p.groove[GF_GROOVE_PUMP];
                    pu[GF_PUMP_DEPTH] = 60; pu[GF_PUMP_SHAPE] = 2; pu[GF_PUMP_RATE] = 0; pu[GF_PUMP_RELEASE] = 220;
                    p.groove_on[GF_GROOVE_PUMP] = true; p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 30; }},
};

const Factory PROPHET[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 1000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"SYNC LEAD", 2, 500, 0.8f, 300, 2, 600, 0.5f, 300, 0.35f, 2000, 0.2f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 12; o[0][2] = 1; o[0][3] = 0; o[0][4] = 1;
                    o[1][0] = 0; o[1][4] = 1; o[1][6] = 0; o[2][0] = 80; o[2][1] = 20;
                    p.mod[0][0] = 40; p.mod[0][2] = 1; }},
    {"POLY BRASS", 40, 700, 0.75f, 300, 50, 800, 0.4f, 300, 0.45f, 450, 0.1f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][2] = 1; o[0][3] = 0; o[1][0] = 0; o[1][1] = 6; o[1][4] = 1; o[1][6] = 0;
                    o[2][0] = 70; o[2][1] = 70; o[2][4] = 1; }},
    {"POLYMOD BELL", 1, 1500, 0.0f, 1200, 1, 1200, 0.0f, 1000, 0.3f, 3000, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][2] = 0; o[0][3] = 1; o[0][1] = 50; o[1][0] = 19; o[1][4] = 0; o[1][5] = 1;
                    o[1][6] = 0; o[2][0] = 80; o[2][1] = 0; o[2][4] = 2; p.mod[0][1] = 60; p.mod[0][2] = 1; }},
    {"STRING PAD", 500, 1000, 0.85f, 1200, 500, 1000, 0.6f, 1200, 0.1f, 2200, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][2] = 1; o[0][3] = 0; o[1][0] = 0; o[1][1] = 8; o[1][4] = 1; o[1][6] = 0;
                    o[2][0] = 60; o[2][1] = 60; }},
    {"WARM KEYS", 2, 900, 0.4f, 400, 2, 600, 0.2f, 400, 0.4f, 700, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][1] = 40; o[0][2] = 0; o[0][3] = 1; o[1][0] = -12; o[1][4] = 1; o[1][6] = 0;
                    o[2][0] = 60; o[2][1] = 40; o[2][4] = 1; }},
    // Presets del track: mitad B de GF_Run_HybridLead y de GF_Disco_RapStab
    {"JD RUN LEAD BODY", 5, 350, 0.6f, 180, 5, 350, 0.4f, 180, 0.55f, 1200, 0.3f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 2; o[0][2] = 1; o[0][3] = 0; o[0][4] = 1;   // A +2 ST, esclava de B
                    o[1][0] = 0; o[1][4] = 1; o[1][6] = 0; o[2][0] = 70; o[2][1] = 50;
                    p.mod[0][0] = 15; p.mod[0][2] = 1; }},                             // POLY-MOD FILT ENV → FREQ A
    {"JD RAP STAB P5", 2, 220, 0.0f, 110, 12, 160, 0.0f, 110, 0.8f, 450, 0.35f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][2] = 1; o[0][3] = 0; o[1][0] = 0; o[1][1] = 6; o[1][4] = 1; o[1][6] = 0;
                    o[2][0] = 70; o[2][1] = 70; }},
    // Piloto Signature: arpegio SMART 1/16 con swing, 2 octavas
    {"GF ORGANIC ARP", 1, 300, 0.0f, 250, 1, 250, 0.0f, 250, 0.45f, 1400, 0.25f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][2] = 1; o[0][3] = 0; o[1][0] = 0; o[1][1] = 4; o[1][4] = 0; o[1][5] = 1;
                    o[1][6] = 0; o[2][0] = 70; o[2][1] = 50;
                    float* a = p.groove[GF_GROOVE_ARP];
                    a[GF_ARP_MODE] = 4; a[GF_ARP_DIV] = 2; a[GF_ARP_GATE] = 50; a[GF_ARP_OCT] = 2; a[GF_ARP_SWING] = 56;
                    p.groove_on[GF_GROOVE_ARP] = true; p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 30; }},
};

const Factory OB6[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 2000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"JUMP BRASS", 10, 400, 0.8f, 300, 15, 500, 0.5f, 300, 0.4f, 900, 0.1f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[1][1] = 0; o[1][5] = 6; o[0][1] = 0;
                    o[0][2] = 80; o[1][2] = 80; }},
    {"BP PAD", 600, 1200, 0.9f, 1500, 600, 1200, 0.6f, 1500, 0.15f, 1500, 0.3f, 0, 0.4f, 0, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 100; o[1][1] = 0; o[1][5] = 7; o[0][1] = 0; o[0][7] = 1; }},
    {"XMOD LEAD", 2, 500, 0.8f, 250, 2, 500, 0.5f, 250, 0.3f, 2500, 0.25f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 30; o[1][1] = 7; o[0][1] = 0; o[0][2] = 80; o[1][2] = 40;
                    p.mod[0][1] = 30; p.mod[0][2] = 1; p.mod[0][5] = 1; }},   // + SYNC
    {"SAW STRINGS", 400, 800, 0.85f, 1000, 400, 800, 0.7f, 1000, 0.1f, 3000, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[1][1] = 0; o[1][5] = 9; o[0][1] = 0; }},
    {"PULSE BASS", 1, 300, 0.6f, 100, 1, 250, 0.1f, 100, 0.5f, 300, 0.2f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 100; o[1][0] = 100; o[1][1] = -12; o[0][4] = 30; o[1][4] = 30;
                    o[0][1] = 60; }},
    // Presets del track: mitad A de GF_Horizon_Pedal (MORPH 50 = notch)
    {"JD HORIZON NOTCH", 120, 1000, 0.85f, 600, 120, 1000, 0.6f, 600, 0.15f, 900, 0.2f, 0, 0.15f, 0, 0.35f, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[1][1] = 0; o[1][5] = 16; o[0][1] = 0; o[0][3] = 0;
                    o[0][2] = 80; o[1][2] = 80; o[0][6] = 50; o[0][7] = 0; }},
    // Piloto Signature: lead UNISON ×3 (18 ¢) con DRIFT y un poco de GRIT
    {"GF MELODIC LEAD", 5, 600, 0.7f, 400, 5, 600, 0.5f, 400, 0.35f, 2500, 0.25f, 0.05f, 5.5f, 0, 0, 0.08f, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[1][1] = 0; o[1][5] = 6; o[0][1] = 0; o[0][3] = 0;
                    o[0][2] = 80; o[1][2] = 70;
                    p.groove[GF_GROOVE_UNISON][GF_UNISON_VOICES] = 2; p.groove[GF_GROOVE_UNISON][GF_UNISON_DETUNE] = 18;
                    p.groove_on[GF_GROOVE_UNISON] = true;
                    p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 25; p.groove[GF_GROOVE_MACRO][GF_MACRO_GRIT] = 20; }},
};

void dx7_ops(Patch& p, const float (&op)[4][8]) {
    for (int k = 0; k < 4; ++k)
        for (int s = 0; s < 8; ++s) p.osc[k][s] = op[k][s];
}

const Factory DX7[] = {
    {"E.PIANO", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"FM BASS", 0, 600, 0.5f, 100, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 1, 5, 0, 800, 40, 100}, {1, 80, 0, 0, 0, 400, 20, 80},
                                {1, 60, 0, 0, 0, 300, 0, 80}, {1, 50, 0, 0, 0, 200, 0, 60}}); }},
    {"BELLS", 0, 3000, 0.0f, 2500, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 5, 0, 0, 3000, 0, 2500}, {3.5f, 70, 0, 0, 0, 2000, 0, 2000},
                                {1, 90, 0, 0, 0, 2500, 0, 2000}, {14, 50, 0, 0, 0, 600, 0, 500}}); }},
    {"BRASS FM", 60, 800, 0.85f, 300, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 3, 6, 60, 800, 85, 300}, {1, 70, 0, 0, 80, 900, 70, 300},
                                {1, 55, 0, 0, 60, 600, 60, 300}, {1, 60, 0, 0, 50, 400, 50, 300}}); }},
    {"MARIMBA", 0, 500, 0.0f, 300, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 5, 0, 0, 500, 0, 300}, {4, 75, 0, 0, 0, 80, 0, 80},
                                {4, 60, 0, 0, 0, 300, 0, 200}, {10, 50, 0, 0, 0, 40, 0, 40}}); }},
    {"ORGAN", 5, 100, 1.0f, 80, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 6, 0, 0, 0.06f, 0,
     [](Patch& p) { dx7_ops(p, {{0.5f, 85, 8, 0, 5, 100, 100, 80}, {1, 99, 0, 0, 5, 100, 100, 80},
                                {2, 80, 0, 0, 5, 100, 100, 80}, {3, 70, 0, 0, 5, 100, 100, 80}}); }},
    // Presets del track: mitad A de GF_Run_HybridLead, B de GF_Vocal_VelvetPad y de GF_Tribal_ZapPerc.
    // Algoritmo 1 (4 → 3 → 2 → 1) con OP3 y OP4 en 0 = par de 2 operadores
    {"JD RUN PLUCK", 0, 120, 1.0f, 120, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 1, 0, 0, 90, 0, 90}, {3, 75, 0, 0, 0, 45, 0, 45},
                                {1, 0, 0, 0, 0, 100, 0, 100}, {1, 0, 0, 0, 0, 100, 0, 100}}); }},
    {"JD VELVET RHODES", 60, 800, 0.7f, 450, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 5, 0, 60, 800, 70, 450}, {1, 40, 0, 0, 60, 600, 40, 400},
                                {2, 75, 0, 0, 60, 800, 60, 450}, {1, 35, 0, 0, 60, 500, 30, 400}});
                    p.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT] = 60; }},
    {"JD ZAP PERC WOOD", 0, 100, 1.0f, 60, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 1, 0, 0, 55, 0, 55}, {3.14f, 70, 0, 0, 0, 40, 0, 40},
                                {1, 0, 0, 0, 0, 100, 0, 100}, {1, 0, 0, 0, 0, 100, 0, 100}}); }},
    // Piloto Signature: stab FM con CHORD en tonalidad (A menor, 1.ª inversión, strum 15 ms) y PUMP suave
    {"GF FM DISCO STAB", 0, 500, 1.0f, 250, 5, 400, 0.3f, 300, 0.3f, 15000, 0.15f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { dx7_ops(p, {{1, 99, 5, 2, 0, 400, 0, 250}, {1, 70, 0, 0, 0, 200, 0, 150},
                                {2, 85, 0, 0, 0, 350, 0, 250}, {3, 55, 0, 0, 0, 120, 0, 100}});
                    float* c = p.groove[GF_GROOVE_CHORD];
                    c[GF_CHORD_TYPE] = 1; c[GF_CHORD_INV] = 1; c[GF_CHORD_STRUM] = 15; c[GF_CHORD_KEY] = 21;
                    p.groove_on[GF_GROOVE_CHORD] = true;
                    p.groove[GF_GROOVE_PUMP][GF_PUMP_DEPTH] = 35; p.groove_on[GF_GROOVE_PUMP] = true; }},
};

const Factory ARP[] = {
    {"INIT", 5, 300, 0.7f, 300, 5, 400, 0.3f, 300, 0.3f, 2000, 0.15f, 0, 2, 1, 0, 0, 0, nullptr},
    {"RING BELL", 1, 1200, 0.0f, 900, 1, 1000, 0.0f, 900, 0.2f, 6000, 0.05f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 4; o[1][0] = 4; o[1][1] = 2.75f; o[0][2] = 90; o[0][3] = 0;
                    o[0][4] = 20; o[1][4] = 10; p.mod[0][0] = 0; }},   // POLY
    {"SQUARE BASS", 1, 300, 0.6f, 100, 1, 200, 0.1f, 100, 0.5f, 350, 0.3f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 2; o[1][0] = 0; o[1][1] = 0.5f; o[0][2] = 0; o[0][3] = 0;
                    o[0][4] = 80; o[1][4] = 60; }},
    {"SEQ LEAD", 2, 400, 0.7f, 200, 2, 400, 0.4f, 200, 0.35f, 1500, 0.45f, 0.25f, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 3; o[1][1] = 2; o[0][2] = 0; o[0][3] = 0;
                    o[0][4] = 80; o[1][4] = 50; }},
    {"S&H BURBLE", 2, 300, 0.8f, 300, 2, 300, 0.5f, 300, 0.2f, 800, 0.55f, 0, 6, 4, 0.35f, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 0; o[1][0] = 0; o[1][1] = 1; o[0][2] = 0; o[0][3] = 0;
                    o[0][4] = 80; o[1][4] = 0; }},
    {"PERC BLIP", 0, 120, 0.0f, 120, 0, 100, 0.0f, 100, 0.7f, 300, 0.4f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 1; o[1][0] = 4; o[1][1] = 1.5f; o[0][2] = 30; o[0][3] = 0;
                    o[0][4] = 70; o[1][4] = 50; }},
    // Presets del track: mitad B de GF_Run_FunkBass (la mordida en medios) y A de GF_Tribal_ZapPerc
    {"JD RUN BASS BITE", 0, 220, 0.15f, 80, 0, 220, 0.15f, 80, 0.5f, 1200, 0.25f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 3; o[0][1] = 30; o[0][2] = 0; o[0][3] = 0; o[0][4] = 80; o[1][4] = 0; }},
    {"JD ZAP PERC ARP", 0, 95, 0.0f, 60, 0, 95, 0.0f, 60, 0.85f, 1800, 0.75f, 0, 2, 1, 0, 0, 0,
     [](Patch& p) { O o = p.osc; o[0][0] = 4; o[0][2] = 0; o[0][3] = 60; o[0][4] = 70; o[1][4] = 0;
                    p.mod[0][5] = 0; }},                                                 // ruido blanco
};

struct Bank {
    const Factory* v;
    uint8_t n;
};
#define GF_BANK(v) {v, uint8_t(sizeof(v) / sizeof(v[0]))}
const Bank BANKS[GF_ENGINE_COUNT] = {GF_BANK(MODEL_D), GF_BANK(JUNO), GF_BANK(PROPHET), GF_BANK(OB6), GF_BANK(DX7), GF_BANK(ARP)};
#undef GF_BANK

}  // namespace

size_t preset_encode(const Preset& src, uint8_t engine, uint8_t* buf, size_t cap) {
    if (cap < PRESET_BYTES) return 0;
    Preset p = src;
    size_t n = 0;
    memcpy(buf, MAGIC, 3);
    buf[3] = PRESET_FORMAT;
    buf[4] = engine;
    memset(buf + 5, 0, PRESET_NAME_LEN);
    memcpy(buf + 5, p.name, strnlen(p.name, PRESET_NAME_LEN));
    n = 5 + PRESET_NAME_LEN;
    visit(p, PRESET_FORMAT, [&](float& v) { memcpy(buf + n, &v, 4); n += 4; }, [&](uint8_t& b) { buf[n++] = b; });
    return n;
}

bool preset_decode(const uint8_t* buf, size_t len, uint8_t engine, Preset* out) {
    if (len < 5 || memcmp(buf, MAGIC, 3) != 0 || buf[4] != engine) return false;
    const uint8_t format = buf[3];
    const size_t expect = format == 4 ? PRESET_BYTES : format == 3 ? PRESET_BYTES_V3 : format == 2 ? PRESET_BYTES_V2 : 0;
    if (format < PRESET_FORMAT_MIN || format > PRESET_FORMAT || len != expect) return false;
    Preset p;
    patch_vel_defaults(p.patch, engine);        // lo que no traen los formatos 2 y 3
    memcpy(p.name, buf + 5, PRESET_NAME_LEN);
    p.name[PRESET_NAME_LEN] = '\0';
    size_t n = 5 + PRESET_NAME_LEN;
    visit(p, format, [&](float& v) { memcpy(&v, buf + n, 4); n += 4; }, [&](uint8_t& b) { b = buf[n++]; });
    p.patch.lfo_shape = p.patch.lfo_shape < GF_LFO_SHAPES ? p.patch.lfo_shape : 0;
    p.patch.lfo_div = p.patch.lfo_div < GF_LFO_DIVS ? p.patch.lfo_div : 0;
    *out = p;
    return true;
}

uint8_t factory_presets(uint8_t e, Preset* out, uint8_t cap) {
    if (e >= GF_ENGINE_COUNT) return 0;
    const Bank& b = BANKS[e];
    uint8_t n = 0;
    for (; n < b.n && n < cap; ++n) {
        const Factory& f = b.v[n];
        Preset& p = out[n];
        p = Preset{};
        patch_init(p.patch, e);
        set_name(p.name, f.name);
        Patch& t = p.patch;
        t.attack_ms = f.a; t.decay_ms = f.d; t.sustain = f.s; t.release_ms = f.r;
        t.f_attack_ms = f.fa; t.f_decay_ms = f.fd; t.f_sustain = f.fs; t.f_release_ms = f.fr;
        t.env_amt = f.env_amt;
        t.glide = f.glide;
        t.lfo_rate_hz = f.lfo_hz;
        t.lfo_shape = f.lfo_shape;
        t.lfo_amt[GF_DEST_CUTOFF] = f.amt_cut;
        t.lfo_amt[GF_DEST_PITCH] = f.amt_pitch;
        if (GF_LFO_THIRD_DEST[e] != GF_DEST_NONE) t.lfo_amt[GF_LFO_THIRD_DEST[e]] = f.amt_third;
        p.cutoff_hz = f.cutoff_hz;
        p.reso_norm = f.reso;
        if (f.osc) f.osc(t);
    }
    return n;
}

const char* PresetBank::name(uint8_t e, uint8_t i) const {
    return e < GF_ENGINE_COUNT && i < count_[e] ? names_[e][i] : "";
}

void PresetBank::scan(uint8_t e) {
    if (e >= GF_ENGINE_COUNT) return;
    uint8_t buf[PRESET_BYTES];
    Preset p;
    count_[e] = 0;
    // El banco es contiguo (save_new agrega al final): el primer hueco o archivo inválido lo termina
    for (uint8_t i = 0; i < PRESET_MAX; ++i) {
        const size_t len = storage_.read(e, i, buf, sizeof(buf));
        if (!preset_decode(buf, len, e, &p)) break;
        set_name(names_[e][i], p.name);
        count_[e] = uint8_t(i + 1);
    }
    // Banco vacío: el de fábrica entero. Banco con presets: solo los de fábrica que falten, al final (un firmware nuevo
    // trae presets nuevos sin borrar lo guardado)
    Preset fac[PRESET_MAX];
    const uint8_t n = factory_presets(e, fac, PRESET_MAX);
    // Presets de fábrica ya ofrecidos en este banco: nombres de PRESET_NAME_LEN bytes seguidos
    const size_t seen_len = storage_.read(e, FACTORY_SLOT, buf, sizeof(buf));
    auto offered = [&](const char* name) {
        for (size_t k = 0; k + PRESET_NAME_LEN <= seen_len; k += PRESET_NAME_LEN)
            if (strncmp(reinterpret_cast<const char*>(buf + k), name, PRESET_NAME_LEN) == 0) return true;
        return false;
    };
    bool seen[PRESET_MAX] = {};
    for (uint8_t f = 0; f < n; ++f) seen[f] = offered(fac[f].name);
    for (uint8_t f = 0; f < n && count_[e] < PRESET_MAX; ++f) {
        bool have = seen[f];
        for (uint8_t i = 0; i < count_[e] && !have; ++i) have = strcmp(names_[e][i], fac[f].name) == 0;
        if (have) continue;
        const uint8_t i = count_[e];
        if (preset_encode(fac[f], e, buf, sizeof(buf)) == 0 || !storage_.write(e, i, buf, PRESET_BYTES)) break;
        set_name(names_[e][i], fac[f].name);
        count_[e] = uint8_t(i + 1);
    }
    // Anota todos los de fábrica de este firmware como ofrecidos (solo si cambió algo)
    bool all_seen = true;
    for (uint8_t f = 0; f < n; ++f) all_seen &= seen[f];
    if (all_seen) return;
    uint8_t mark[PRESET_MAX * PRESET_NAME_LEN] = {};
    for (uint8_t f = 0; f < n; ++f) strncpy(reinterpret_cast<char*>(mark + f * PRESET_NAME_LEN), fac[f].name, PRESET_NAME_LEN);
    storage_.write(e, FACTORY_SLOT, mark, size_t(n) * PRESET_NAME_LEN);
}

bool PresetBank::load(uint8_t e, uint8_t i, Preset* out) {
    if (e >= GF_ENGINE_COUNT || i >= count_[e]) return false;
    uint8_t buf[PRESET_BYTES];
    return preset_decode(buf, storage_.read(e, i, buf, sizeof(buf)), e, out);
}

PutResult PresetBank::put(uint8_t e, uint8_t i, const uint8_t* buf, size_t len) {
    if (e >= GF_ENGINE_COUNT) return PutResult::Engine;
    if (i > count_[e] || i >= PRESET_MAX) return PutResult::Index;   // reemplaza o agrega, nunca deja hueco
    Preset p;
    if (!preset_decode(buf, len, e, &p)) return PutResult::Format;
    uint8_t out[PRESET_BYTES];
    if (preset_encode(p, e, out, sizeof(out)) == 0 || !storage_.write(e, i, out, PRESET_BYTES)) return PutResult::Write;
    set_name(names_[e][i], p.name);
    if (i == count_[e]) count_[e] = uint8_t(i + 1);
    return PutResult::Ok;
}

bool PresetBank::truncate(uint8_t e, uint8_t n) {
    if (e >= GF_ENGINE_COUNT) return false;
    bool ok = true;
    for (uint8_t i = n; i < count_[e]; ++i) ok &= storage_.remove(e, i);
    if (n < count_[e]) count_[e] = n;
    return ok;
}

int PresetBank::save_new(uint8_t e, Preset p) {
    if (e >= GF_ENGINE_COUNT || count_[e] >= PRESET_MAX) return -1;
    const uint8_t i = count_[e];
    snprintf(p.name, sizeof(p.name), "USER %02u", unsigned(i + 1));
    uint8_t buf[PRESET_BYTES];
    if (preset_encode(p, e, buf, sizeof(buf)) == 0 || !storage_.write(e, i, buf, PRESET_BYTES)) return -1;
    set_name(names_[e][i], p.name);
    count_[e] = uint8_t(i + 1);
    return i;
}

}  // namespace studio
