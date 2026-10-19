#include "page_params.h"

#include <math.h>

#include "studio_ui.h"

namespace studio {

namespace {

constexpr float AMT_STEP = 0.02f;            // 50 pasos de 0 a 1
constexpr float RATE_STEP = 1.06f;           // 6 % por paso: 0.05–20 Hz en ~100 pasos
constexpr float TIME_STEP = 0.12f;           // tiempos: 12 % por paso, mínimo 1 ms

float clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

float step_time(float ms, int8_t steps, GF_Range r) {
    for (int n = steps > 0 ? steps : -steps; n > 0; --n) {
        const float d = ms * TIME_STEP < 1.0f ? 1.0f : ms * TIME_STEP;
        ms += steps > 0 ? d : -d;
    }
    return clamp(ms, r.lo, r.hi);
}

/// Los 4 valores del envelope `env` (GF_ENV_AMP / GF_ENV_FILTER) en orden A, D, S, R.
float* env_field(Patch& p, uint8_t env, uint8_t idx) {
    float* amp[4] = {&p.attack_ms, &p.decay_ms, &p.sustain, &p.release_ms};
    float* flt[4] = {&p.f_attack_ms, &p.f_decay_ms, &p.f_sustain, &p.f_release_ms};
    return env == GF_ENV_FILTER ? flt[idx] : amp[idx];
}

ParamChange env_change(const Patch& p, uint8_t e, uint8_t env, uint8_t idx) {
    const float v = *env_field(const_cast<Patch&>(p), env, idx);
    const uint8_t id = uint8_t(env * 4 + idx);
    switch (idx) {
        case 0:  return {gf_param_id(e, GF_GROUP_ENV, id), gf_norm(GF_ATTACK_RANGE[e], v)};
        case 1:  return {gf_param_id(e, GF_GROUP_ENV, id), gf_norm(GF_DECAY_RANGE[e], v)};
        case 2:  return {gf_param_id(e, GF_GROUP_ENV, id), v};
        default: return {gf_param_id(e, GF_GROUP_ENV, id), gf_norm(GF_RELEASE_RANGE[e], v)};
    }
}

ParamChange osc_change(const Patch& p, uint8_t e, uint8_t stop, uint8_t slot) {
    const GF_OscSlot* s = &GF_OSC_SLOT[e][stop][slot];
    const float v = p.osc[s->global ? 0 : stop][slot];
    return {gf_osc_param_id(e, s, stop, slot), (v - s->lo) / (s->hi - s->lo)};
}

ParamChange mod_change(const Patch& p, uint8_t e, uint8_t stop, uint8_t slot) {
    const GF_OscSlot* s = &GF_MOD_SLOT[e][stop][slot];
    return {gf_mod_param_id(e, stop, slot), (p.mod[stop][slot] - s->lo) / (s->hi - s->lo)};
}

/// Un detent sobre un slot de plantilla: los discretos avanzan de a un valor, los continuos `step` por paso.
void step_slot(float& v, const GF_OscSlot* s, int8_t steps) {
    v = clamp(v + s->step * (s->count ? (steps > 0 ? 1 : -1) : steps), s->lo, s->hi);
}

ParamChange sfx_param_change(const Patch& p, uint8_t e, uint8_t fx, uint8_t k) {
    return {gf_param_id(e, GF_GROUP_SFX_P, uint8_t(fx * GF_SFX_PARAMS + k)), p.sfx_p[fx][k]};
}

/// Paso de un parámetro de la cadena: los discretos avanzan de a un valor, los continuos 2 % por paso.
float step_sfx(float v, uint8_t steps_count, int8_t steps) {
    if (steps_count == 0) return clamp(v + AMT_STEP * steps, 0.0f, 1.0f);
    const int i = int(gf_index_from_norm(v, steps_count)) + (steps > 0 ? 1 : -1);
    return gf_index_norm(uint8_t(i < 0 ? 0 : i >= steps_count ? steps_count - 1 : i), steps_count);
}

ParamChange amt_change(const Patch& p, uint8_t e, uint8_t dest) {
    return {gf_param_id(e, GF_GROUP_LFO, uint8_t(GF_LFO_AMT0 + dest)), p.lfo_amt[dest]};
}

/// Suma `steps` a la cantidad de un destino si el engine lo tiene.
uint8_t edit_amt(Patch& p, uint8_t e, uint8_t dest, int8_t steps, ParamChange* out) {
    if (dest == GF_DEST_NONE || !gf_lfo_dest_available(e, dest)) return 0;
    p.lfo_amt[dest] = clamp(p.lfo_amt[dest] + AMT_STEP * steps, 0.0f, 1.0f);
    out[0] = amt_change(p, e, dest);
    return 1;
}

}  // namespace

uint8_t edit_encoder(Patch& p, const GF_UiState& ui, uint8_t e, uint8_t enc, int8_t steps, ParamChange* out) {
    if (ui.mode != 0 || steps == 0 || e >= GF_ENGINE_COUNT) return 0;   // en FX los encoders de página son del FX (fase 2)
    const bool sh = ui.shift != 0;
    if (enc > 3) return 0;
    // Páginas de edición: los 4 encoders (P1 · P2 · ENC L · ENC R). La capa SHIFT existe en OSC y LFO / MOD
    // (plantillas de 8 slots); en ENV y SYNTH FX está vacía.
    if (sh && (ui.page == ENV || ui.page == SYNTH_FX)) return 0;

    switch (ui.page) {
        case PERFORM: case SPECTRUM:                                     // SPECTRUM = igual que PERFORM (UI v3)
            if (enc > 1) return 0;                                       // ENC L/R = filtro
            if (sh) {
                if (enc == 0) {
                    p.glide = clamp(p.glide + AMT_STEP * steps, 0.0f, 1.0f);
                    out[0] = {gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_GLIDE), p.glide};
                } else {
                    p.drive = clamp(p.drive + AMT_STEP * steps, 0.0f, 1.0f);
                    out[0] = {gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_DRIVE), p.drive};
                }
                return 1;
            }
            if (enc == 1) return edit_amt(p, e, GF_DEST_CUTOFF, steps, out);
            p.env_amt = clamp(p.env_amt + AMT_STEP * steps, 0.0f, 1.0f);
            out[0] = {gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_ENV_AMT), p.env_amt};
            return 1;

        case ENV: {                                                      // el cursor elige AMP o FILTER
            const uint8_t env = ui.cursor == GF_ENV_FILTER ? GF_ENV_FILTER : GF_ENV_AMP;
            const uint8_t idx = enc;                                     // A · D · S · R
            float& v = *env_field(p, env, idx);
            if (idx == 0) v = step_time(v, steps, GF_ATTACK_RANGE[e]);
            else if (idx == 1) v = step_time(v, steps, GF_DECAY_RANGE[e]);
            else if (idx == 2) v = clamp(v + AMT_STEP * steps, 0.0f, 1.0f);
            else v = step_time(v, steps, GF_RELEASE_RANGE[e]);
            out[0] = env_change(p, e, env, idx);
            return 1;
        }

        case OSC: {                                                      // el cursor es el oscilador
            const uint8_t stop = ui.cursor < GF_OSC_STOPS ? ui.cursor : 0;
            const uint8_t slot = uint8_t((sh ? 4 : 0) + enc);
            const GF_OscSlot* s = &GF_OSC_SLOT[e][stop][slot];
            if (!gf_osc_used(s)) return 0;
            step_slot(p.osc[s->global ? 0 : stop][slot], s, steps);
            out[0] = osc_change(p, e, stop, slot);
            return 1;
        }

        case LFO:
            if (ui.cursor > 0) {                                         // paradas de MOD del engine
                const uint8_t st = uint8_t(ui.cursor - 1);
                if (st >= GF_MOD_STOPS[e]) return 0;
                const uint8_t slot = uint8_t((sh ? 4 : 0) + enc);
                const GF_OscSlot* s = &GF_MOD_SLOT[e][st][slot];
                if (!gf_osc_used(s)) return 0;
                step_slot(p.mod[st][slot], s, steps);
                out[0] = mod_change(p, e, st, slot);
                return 1;
            }
            if (sh) return enc == 0 ? step_lfo_shape(p, e, steps > 0 ? 1 : -1, out) : 0;   // SHIFT + P1 = SHAPE
            if (enc >= 2) return edit_amt(p, e, enc == 2 ? GF_DEST_PITCH : GF_LFO_THIRD_DEST[e], steps, out);
            if (enc == 1) return edit_amt(p, e, GF_DEST_CUTOFF, steps, out);
            if (ui.flags & GF_UI_FLAG_LFO_SYNC) {
                const int d = int(p.lfo_div) + (steps > 0 ? 1 : -1);
                p.lfo_div = uint8_t(d < 0 ? 0 : d >= int(GF_LFO_DIVS) ? GF_LFO_DIVS - 1 : d);
                out[0] = {gf_param_id(e, GF_GROUP_LFO, GF_LFO_DIV), gf_index_norm(p.lfo_div, GF_LFO_DIVS)};
            } else {
                p.lfo_rate_hz = clamp(p.lfo_rate_hz * powf(RATE_STEP, steps), GF_LFO_RATE_MIN, GF_LFO_RATE_MAX);
                out[0] = {gf_param_id(e, GF_GROUP_LFO, GF_LFO_RATE), gf_lfo_rate_norm(p.lfo_rate_hz)};
            }
            return 1;

        case SYNTH_FX: {             // igual que el modo FX (05 §1.13): ENC L MIX · ENC R principal · P1 / P2 usuario
            const uint8_t fx = ui.cursor < GF_SFX_COUNT ? ui.cursor : 0;
            if (enc == 2) {
                p.sfx_mix[fx] = clamp(p.sfx_mix[fx] + AMT_STEP * steps, 0.0f, 1.0f);
                out[0] = {gf_param_id(e, GF_GROUP_SFX, uint8_t(GF_SFX_COUNT + fx)), p.sfx_mix[fx]};
                return 1;
            }
            const uint8_t k = enc == 3 ? 0 : uint8_t(enc + 1);          // ENC R → principal · P1 → 1 · P2 → 2
            p.sfx_p[fx][k] = step_sfx(p.sfx_p[fx][k], GF_SFX_STEPS[fx][k], steps);
            out[0] = sfx_param_change(p, e, fx, k);
            return 1;
        }

        default:
            return 0;                                                    // AI (ARP), ENGINE, PRESETS: fase 2
    }
}

uint8_t step_lfo_shape(Patch& p, uint8_t e, int8_t steps, ParamChange* out) {
    const int s = int(p.lfo_shape) + steps;
    p.lfo_shape = uint8_t(s < 0 ? 0 : s >= int(GF_LFO_SHAPES) ? GF_LFO_SHAPES - 1 : s);
    out[0] = {gf_param_id(e, GF_GROUP_LFO, GF_LFO_SHAPE), gf_index_norm(p.lfo_shape, GF_LFO_SHAPES)};
    return 1;
}

uint8_t toggle_sfx(Patch& p, uint8_t e, uint8_t slot, ParamChange* out) {
    if (slot >= GF_SFX_COUNT) return 0;
    p.sfx_on[slot] = !p.sfx_on[slot];
    out[0] = {gf_param_id(e, GF_GROUP_SFX, slot), p.sfx_on[slot] ? 1.0f : 0.0f};
    return 1;
}

void patch_init(Patch& p, uint8_t e) {
    float (&o)[GF_OSC_STOPS][GF_OSC_SLOTS] = p.osc;
    p.glide = 0.0f;
    p.drive = 0.2f;
    switch (e) {
        case 0:  // MODEL D: SAW / SAW / SQR, +7 CT, −12 ST, niveles 80 / 72 / 50, ruido 12, PW 50%, FINE 0 CT
            o[0][0] = 0; o[1][0] = 0; o[2][0] = 2; o[1][1] = 7; o[2][1] = -12;
            o[0][2] = 80; o[1][2] = 72; o[2][2] = 50; o[0][3] = 12;
            o[0][4] = 50; o[2][5] = 0;
            break;
        case 1:  // JUNO-106: PULSE (para que se oiga el PWM), SUB 60, NOISE 10, PW 50, CHORUS I (1), HPF FLAT (1)
            o[0][0] = 2; o[0][1] = 60; o[0][2] = 10; o[0][3] = 50; o[0][4] = 1; o[0][5] = 1;
            break;
        case 2:  // PROPHET-5: A saw, B pulse +7 ST, MIXER 60 / 40, WHEEL-MOD → FREQ A y B (como sale de fábrica)
            o[0][0] = 0; o[0][1] = 50; o[0][2] = 1; o[0][3] = 0; o[0][4] = 0;
            o[1][0] = 7; o[1][1] = 0; o[1][2] = 50; o[1][3] = 1; o[1][4] = 0; o[1][5] = 0; o[1][6] = 1; o[1][7] = 0;
            o[2][0] = 60; o[2][1] = 40; o[2][2] = 0; o[2][3] = 0; o[2][4] = 0;
            p.mod[1][0] = 0; p.mod[1][1] = 1; p.mod[1][2] = 1;
            break;
        case 3:  // OB-6: SAW / PULSE, SUB 40, +7 ST, niveles 80 / 70, ruido 0, PW 50/50, DETUNE +4 CT, MORPH 0 (LP), BP OFF
            o[0][0] = 0; o[1][0] = 3; o[0][1] = 40; o[1][1] = 7; o[0][2] = 80; o[1][2] = 70; o[0][3] = 0;
            o[0][4] = 50; o[1][4] = 50; o[1][5] = 4; o[0][6] = 0; o[0][7] = 0;
            break;
        case 5:  // ARP 2600: PULSE / SINE, PW 50%, ×1.50, RING 40, NOISE 5, VCO1 80%, VCO2 70%
            o[0][0] = 3; o[0][1] = 50; o[1][0] = 4; o[1][1] = 1.5f; o[0][2] = 40; o[0][3] = 5;
            o[0][4] = 80; o[1][4] = 70;
            break;
        case 4: {  // DX7: E.PIANO (algoritmo 5), igual que DX7Engine::begin()
            const float op[4][8] = {{1, 99, 5, 2, 0, 1800, 0, 400},
                                    {1, 72, 0, 0, 0, 900, 15, 300},
                                    {1, 88, 0, 0, 0, 2200, 0, 400},
                                    {14, 52, 0, 0, 0, 160, 0, 120}};
            for (int k = 0; k < 4; ++k)
                for (int s = 0; s < 8; ++s) o[k][s] = op[k][s];
            break;
        }
        default: break;
    }
}

uint8_t patch_params(const Patch& p, uint8_t e, ParamChange* out) {
    uint8_t n = 0;
    for (uint8_t env = 0; env < 2; ++env)
        for (uint8_t i = 0; i < 4; ++i) out[n++] = env_change(p, e, env, i);
    for (uint8_t st = 0; st < GF_OSC_STOPS; ++st)
        for (uint8_t sl = 0; sl < GF_OSC_SLOTS; ++sl) {
            const GF_OscSlot* s = &GF_OSC_SLOT[e][st][sl];
            if (gf_osc_used(s) && !(s->global && st > 0)) out[n++] = osc_change(p, e, st, sl);
        }
    for (uint8_t st = 0; st < GF_MOD_STOPS[e]; ++st)
        for (uint8_t sl = 0; sl < GF_OSC_SLOTS; ++sl)
            if (gf_osc_used(&GF_MOD_SLOT[e][st][sl])) out[n++] = mod_change(p, e, st, sl);
    out[n++] = {gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_ENV_AMT), p.env_amt};
    out[n++] = {gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_GLIDE), p.glide};
    out[n++] = {gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_DRIVE), p.drive};
    out[n++] = {gf_param_id(e, GF_GROUP_LFO, GF_LFO_RATE), gf_lfo_rate_norm(p.lfo_rate_hz)};
    out[n++] = {gf_param_id(e, GF_GROUP_LFO, GF_LFO_SHAPE), gf_index_norm(p.lfo_shape, GF_LFO_SHAPES)};
    out[n++] = {gf_param_id(e, GF_GROUP_LFO, GF_LFO_DIV), gf_index_norm(p.lfo_div, GF_LFO_DIVS)};
    for (uint8_t d = 0; d < GF_DEST_COUNT; ++d) out[n++] = amt_change(p, e, d);
    for (uint8_t fx = 0; fx < GF_SFX_COUNT; ++fx) {
        out[n++] = {gf_param_id(e, GF_GROUP_SFX, fx), p.sfx_on[fx] ? 1.0f : 0.0f};
        out[n++] = {gf_param_id(e, GF_GROUP_SFX, uint8_t(GF_SFX_COUNT + fx)), p.sfx_mix[fx]};
        for (uint8_t k = 0; k < GF_SFX_PARAMS; ++k) out[n++] = sfx_param_change(p, e, fx, k);
    }
    return n;
}

}  // namespace studio
