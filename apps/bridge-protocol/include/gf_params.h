#pragma once

/**
 * @file gf_params.h
 * @brief Ids y normalización de los parámetros del synth que viajan en PARAM_CHANGED (0x14).
 *
 * param_id = ((0x10 + engine) << 8) | (grupo << 4) | índice, valor normalizado 0–1 (esquema del sketch 28).
 * Las conversiones están acá para que el Teensy y la cabeza usen exactamente las mismas.
 * Teoría: apps/docs/theory/lfo-modulacion.md §7.
 */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#define GF_ENGINE_COUNT 6U

#define GF_GROUP_OSC    0U   /* oscilador · 4 + slot (ver GF_OSC_SLOT) */
#define GF_GROUP_ENV    1U   /* 0–3 A D S R del amplificador · 4–7 A D S R del filtro */
#define GF_GROUP_FILTER 2U   /* 0 cutoff · 1 resonancia · 2 FLT ENV AMT · 3 GLIDE · 4 DRIVE */
#define GF_GROUP_LFO    3U   /* ver GF_LFO_* */
#define GF_GROUP_SFX    4U   /* cadena de FX del synth: 0–3 encendido · 4–7 MIX */
#define GF_GROUP_SFX_P  5U   /* fx · 3 + k (k: 0 principal · 1 SHIFT P1 · 2 SHIFT P2) */

#define GF_FILTER_CUTOFF  0U
#define GF_FILTER_RESO    1U
#define GF_FILTER_ENV_AMT 2U
#define GF_FILTER_GLIDE   3U
#define GF_FILTER_DRIVE   4U

#define GF_LFO_RATE  0U
#define GF_LFO_SHAPE 1U
#define GF_LFO_DIV   2U
#define GF_LFO_AMT0  3U      /* + GF_DEST_* */

/** Destinos del LFO (una cantidad por destino). */
#define GF_DEST_CUTOFF 0U
#define GF_DEST_PITCH  1U
#define GF_DEST_AMP    2U
#define GF_DEST_PWM    3U
#define GF_DEST_COUNT  4U
#define GF_DEST_NONE   0xFFU

#define GF_LFO_SHAPES 5U     /* SINE · TRI · SAW · SQR · S&H */
#define GF_LFO_DIVS   4U     /* 1/4 · 1/8 · 1/16 · 1/8T */

#define GF_LFO_RATE_MIN 0.05f
#define GF_LFO_RATE_MAX 20.0f
#define GF_FLT_ENV_OCT  5.0f   /* FLT ENV AMT = 1 → +5 octavas */
#define GF_LFO_CUT_OCT  4.0f   /* cantidad CUTOFF = 1 → ±4 octavas */
#define GF_LFO_PITCH_ST 12.0f  /* cantidad PITCH = 1 → ±12 semitonos (curva cuadrática) */

typedef struct { float lo, hi; } GF_Range;

/** Rangos por engine (descriptores del sketch 28): cutoff en Hz, tiempos del envelope en ms. */
static const GF_Range GF_CUTOFF_RANGE[GF_ENGINE_COUNT] = {
    {20, 8000}, {20, 8000}, {20, 8000}, {20, 20000}, {500, 20000}, {20, 20000}};
static const GF_Range GF_ATTACK_RANGE[GF_ENGINE_COUNT] = {
    {0, 5000}, {0, 5000}, {0, 5000}, {0, 5000}, {0, 5000}, {0, 5000}};
static const GF_Range GF_DECAY_RANGE[GF_ENGINE_COUNT] = {
    {10, 5000}, {10, 5000}, {10, 5000}, {10, 5000}, {5, 3000}, {10, 5000}};
static const GF_Range GF_RELEASE_RANGE[GF_ENGINE_COUNT] = {
    {10, 5000}, {10, 5000}, {10, 5000}, {10, 5000}, {5, 5000}, {10, 5000}};

/** Tercer destino del LFO de cada engine (theory §3.1); CUTOFF y PITCH se ven aparte. */
static const uint8_t GF_LFO_THIRD_DEST[GF_ENGINE_COUNT] = {
    GF_DEST_NONE, GF_DEST_PWM, GF_DEST_PWM, GF_DEST_PWM, GF_DEST_AMP, GF_DEST_PWM};

/** Beats por ciclo de cada división de SYNC. */
static const float GF_LFO_DIV_BEATS[GF_LFO_DIVS] = {1.0f, 0.5f, 0.25f, 1.0f / 3.0f};

static inline uint16_t gf_param_id(uint8_t engine, uint8_t group, uint8_t idx) {
    return (uint16_t)(((0x10u + engine) << 8) | ((group & 0xFu) << 4) | (idx & 0xFu));
}

/** @return true si el engine tiene ese destino de LFO (el DX7 no tiene filtro en el original). */
static inline bool gf_lfo_dest_available(uint8_t engine, uint8_t dest) {
    if (engine >= GF_ENGINE_COUNT) return false;
    if (dest == GF_DEST_CUTOFF) return engine != 4u;
    if (dest == GF_DEST_PITCH) return true;
    return GF_LFO_THIRD_DEST[engine] == dest;
}

static inline float gf_clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }
static inline float gf_norm(GF_Range r, float v) { return gf_clamp01((v - r.lo) / (r.hi - r.lo)); }
static inline float gf_denorm(GF_Range r, float n) { return r.lo + gf_clamp01(n) * (r.hi - r.lo); }

/** RATE libre 0.05–20 Hz en escala log. */
static inline float gf_lfo_rate_norm(float hz) {
    return gf_clamp01(logf(hz / GF_LFO_RATE_MIN) / logf(GF_LFO_RATE_MAX / GF_LFO_RATE_MIN));
}
static inline float gf_lfo_rate_hz(float norm) {
    return GF_LFO_RATE_MIN * powf(GF_LFO_RATE_MAX / GF_LFO_RATE_MIN, gf_clamp01(norm));
}

/** Índices discretos (forma, división) ↔ 0–1, con redondeo al más cercano al volver. */
static inline float gf_index_norm(uint8_t i, uint8_t count) { return count > 1 ? (float)i / (float)(count - 1) : 0.0f; }
static inline uint8_t gf_index_from_norm(float n, uint8_t count) {
    return (uint8_t)(gf_clamp01(n) * (float)(count - 1) + 0.5f);
}

/* ── Páginas ENV y OSC (theory/lfo-modulacion.md §6; plantilla OSC = osc_catalog.cpp de la cabeza) ── */

#define GF_ENV_AMP    0U     /* cursor de la página ENV */
#define GF_ENV_FILTER 1U
#define GF_OSC_STOPS  4U
#define GF_OSC_SLOTS  8U     /* P1 · P2 · ENC L · ENC R, y los mismos con SHIFT (theory/04 de Studio §3.1) */
#define GF_GROUP_OSC_SHIFT 6U   /* slots 4–7 de OSC: parada · 4 + (slot − 4) */
#define GF_GROUP_MOD       7U   /* paradas de MOD de la página LFO / MOD: parada · 8 + slot */
#define GF_MOD_STOPS_MAX   2U

/** Un slot de las páginas OSC y MOD en el firmware: rango en unidades de la UI, paso de un detent y si es del engine. */
typedef struct {
    float lo, hi;            /* lo == hi: slot sin uso */
    float step;              /* discretos: 1 (índice) */
    uint8_t count;           /* > 0: discreto con `count` valores (forma de onda, interruptor, FILTER KBD) */
    bool global;             /* el valor es del engine: se guarda en el oscilador 0 */
} GF_OscSlot;

#define GF_OSC_NONE          {0, 0, 0, 0, false}
#define GF_OSC_WAVE(n)       {0, (float)((n) - 1), 1, (n), false}
#define GF_OSC_TOGGLE        {0, 1, 1, 2, false}
#define GF_OSC_TOGGLE_GLOBAL {0, 1, 1, 2, true}
#define GF_OSC_KBD3          {0, 2, 1, 3, false}
#define GF_OSC_ENUM4_GLOBAL  {0, 3, 1, 4, true}
#define GF_OSC_DETUNE        {-50, 50, 1, 0, false}
#define GF_OSC_INTERVAL      {-24, 24, 1, 0, false}
#define GF_OSC_LEVEL         {0, 100, 2, 0, false}
#define GF_OSC_PW            {5, 95, 2, 0, false}
#define GF_OSC_PW_GLOBAL     {5, 95, 2, 0, true}
#define GF_OSC_GLOBAL100     {0, 100, 2, 0, true}
#define GF_OSC_RATIO         {0.5f, 8, 0.05f, 0, false}
#define GF_OSC_NONE4         GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE
/* DX7 de 4 operadores (sprints/46-dx7-4-operadores.md §4): RATIO · LEVEL · ALGORITHM · FEEDBACK | A · D · S · R */
#define GF_FM_OP {{0.5f, 16, 0.5f, 0, false}, {0, 99, 1, 0, false}, {1, 8, 1, 8, true}, {0, 7, 1, 8, true}, \
                  {0, 3000, 20, 0, false}, {0, 5000, 20, 0, false}, {0, 100, 2, 0, false}, {0, 5000, 20, 0, false}}
#define GF_OSC_EMPTY     {GF_OSC_NONE4, GF_OSC_NONE4}
/* DX7, paradas de MOD (fidelidad §11.1): W1–W8 y DETUNE por operador | KBD LEVEL · KBD RATE · D2 RATE */
#define GF_FM_WAVE   {1, 8, 1, 8, false}
#define GF_FM_DETUNE {-3, 3, 1, 7, false}

/** Plantilla por engine / parada / slot, la misma que osc_catalog.cpp (el DX7 tiene página de operadores). */
static const GF_OscSlot GF_OSC_SLOT[GF_ENGINE_COUNT][GF_OSC_STOPS][GF_OSC_SLOTS] = {
    /* MODEL D: WAVE · — / DETUNE / INTERVAL · LEVEL | SHIFT: PW (engine) · FINE (VCO 3)
       NOISE (4.ª parada, el ruido del MIXER del original): LEVEL · WHITE / PINK */
    {{GF_OSC_WAVE(4), GF_OSC_NONE, GF_OSC_LEVEL, GF_OSC_NONE,
      GF_OSC_PW_GLOBAL, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE},
     {GF_OSC_WAVE(4), GF_OSC_DETUNE, GF_OSC_LEVEL, GF_OSC_NONE,
      GF_OSC_PW_GLOBAL, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE},
     {GF_OSC_WAVE(4), GF_OSC_INTERVAL, GF_OSC_LEVEL, GF_OSC_NONE,
      GF_OSC_PW_GLOBAL, GF_OSC_DETUNE, GF_OSC_NONE, GF_OSC_NONE},
     {GF_OSC_LEVEL, GF_OSC_TOGGLE, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE4}},
    /* JUNO-106: WAVE (SAW/SQR/PULSE/SAW+PLS) · SUB · NOISE · PW | SHIFT: CHORUS (OFF/I/II/I+II) · HPF (0..3) ·
       RANGE (16'/8'/4') · VCA (ENV/GATE) (fidelidad §11.5) */
    {{GF_OSC_WAVE(4), GF_OSC_GLOBAL100, GF_OSC_GLOBAL100, GF_OSC_PW,
      GF_OSC_ENUM4_GLOBAL, GF_OSC_ENUM4_GLOBAL, {0, 2, 1, 3, true}, GF_OSC_TOGGLE_GLOBAL},
     GF_OSC_EMPTY, GF_OSC_EMPTY, GF_OSC_EMPTY},
    /* PROPHET-5 (theory/prophet-5-completo.md §3.1):
       OSC A: FREQ · PW · SAW · PULSE | SYNC
       OSC B: FREQ · FINE · PW · KEYBOARD | SAW · TRI · PULSE · LO FREQ
       MIXER: OSC A · OSC B · NOISE · GLIDE | FILTER KBD */
    {{GF_OSC_INTERVAL, GF_OSC_PW, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE},
     {GF_OSC_INTERVAL, GF_OSC_DETUNE, GF_OSC_PW, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_TOGGLE,
      GF_OSC_TOGGLE},
     {GF_OSC_LEVEL, GF_OSC_LEVEL, GF_OSC_LEVEL, GF_OSC_LEVEL, GF_OSC_KBD3, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE},
     GF_OSC_EMPTY},
    /* OB-6: SHAPE (0 saw → 100 pulse, continua: fidelidad §11.3) · SUB / INTERVAL · LEVEL · NOISE |
       SHIFT: PW · — / DETUNE · FLT MORPH (global) · BP MODE (global) */
    {{GF_OSC_LEVEL, GF_OSC_GLOBAL100, GF_OSC_LEVEL, GF_OSC_GLOBAL100,
      GF_OSC_PW, GF_OSC_NONE, GF_OSC_GLOBAL100, GF_OSC_TOGGLE_GLOBAL},
     {GF_OSC_LEVEL, GF_OSC_INTERVAL, GF_OSC_LEVEL, GF_OSC_GLOBAL100,
      GF_OSC_PW, GF_OSC_DETUNE, GF_OSC_GLOBAL100, GF_OSC_TOGGLE_GLOBAL},
     GF_OSC_EMPTY, GF_OSC_EMPTY},
    /* DX7: OP 1–4 (ADR-005); ALGORITHM y FEEDBACK son del engine (se guardan en OP 1) */
    {GF_FM_OP, GF_FM_OP, GF_FM_OP, GF_FM_OP},
    /* ARP 2600: WAVE · PW / RATIO · RING MIX · NOISE | SHIFT: VCO 1/2 LEVEL */
    {{GF_OSC_WAVE(5), GF_OSC_PW, GF_OSC_GLOBAL100, GF_OSC_GLOBAL100,
      GF_OSC_LEVEL, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE},
     {GF_OSC_WAVE(5), GF_OSC_RATIO, GF_OSC_GLOBAL100, GF_OSC_GLOBAL100,
      GF_OSC_LEVEL, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE},
     GF_OSC_EMPTY, GF_OSC_EMPTY}};

/** Paradas de OSC que recorre NAV en cada engine. */
static const uint8_t GF_OSC_STOP_COUNT[GF_ENGINE_COUNT] = {4, 1, 3, 2, 4, 2};

/** Paradas de MOD (después de LFO) en la página LFO / MOD de cada engine. */
static const uint8_t GF_MOD_STOPS[GF_ENGINE_COUNT] = {1, 1, 2, 1, 2, 1};

/** Plantilla de MOD (theory/fidelidad-engines.md §11): las secciones que no caben en la página OSC. */
static const GF_OscSlot GF_MOD_SLOT[GF_ENGINE_COUNT][GF_MOD_STOPS_MAX][GF_OSC_SLOTS] = {
    {/* MODEL D MODULATION (fidelidad §11.2): MOD MIX (VCO 3 ↔ NOISE) · OSC MOD · FILTER MOD · — |
        VCO3 KBD · VCO3 LO · OVERDRIVE · FILTER KBD (OFF / 1/3 / 2/3 / FULL). WHITE / PINK: parada NOISE de OSC */
     {GF_OSC_LEVEL, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_NONE,
      GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_LEVEL, {0, 3, 1, 4, false}},
     GF_OSC_EMPTY},
    {/* JUNO-106 LFO: DELAY (ms, fidelidad §11.5) */
     {{0, 3000, 20, 0, false}, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE4},
     GF_OSC_EMPTY},
    {/* POLY-MOD: FILT ENV · OSC B · FREQ A · PW A | FILTER */
     {GF_OSC_LEVEL, GF_OSC_LEVEL, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_NONE, GF_OSC_NONE, GF_OSC_NONE},
     /* WHEEL-MOD: SOURCE LFO↔NOISE · FREQ A · FREQ B · PW A | PW B · FILTER */
     {GF_OSC_LEVEL, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_NONE,
      GF_OSC_NONE}},
    {/* OB-6 X-MOD: FILT ENV · VCO 2 · FREQ 1 · FLT MODE | PW 1 · SYNC (VCO 2 → VCO 1, §11.3) */
     {GF_OSC_LEVEL, GF_OSC_LEVEL, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_TOGGLE, GF_OSC_NONE, GF_OSC_NONE},
     GF_OSC_EMPTY},
    {/* DX7 WAVE / DETUNE: WAVE OP1–OP4 | DETUNE OP1–OP4 */
     {GF_FM_WAVE, GF_FM_WAVE, GF_FM_WAVE, GF_FM_WAVE, GF_FM_DETUNE, GF_FM_DETUNE, GF_FM_DETUNE, GF_FM_DETUNE},
     /* DX7 KBD / EG: KBD LEVEL · KBD RATE · D2 RATE (ms para 60 dB; 0 = sostiene) */
     {{0, 99, 1, 0, false}, {0, 3, 1, 4, false}, {0, 5000, 20, 0, false}, GF_OSC_NONE, GF_OSC_NONE4}},
    {/* ARP 2600 PATCH (fidelidad §11.4): MONO · SYNC · VCO3 RATE (Hz) · VCO3 → PITCH | VCO3 → CUTOFF · NOISE COLOR */
     {GF_OSC_TOGGLE, GF_OSC_TOGGLE, {0.1f, 30, 0.1f, 0, false}, GF_OSC_LEVEL,
      GF_OSC_LEVEL, GF_OSC_LEVEL, GF_OSC_NONE, GF_OSC_NONE},
     GF_OSC_EMPTY}};

static inline bool gf_osc_used(const GF_OscSlot* s) { return s->hi > s->lo; }

/** param_id de un slot de OSC: slots 0–3 en el grupo OSC, 4–7 en OSC_SHIFT; los globales en el oscilador 0. */
static inline uint16_t gf_osc_param_id(uint8_t engine, const GF_OscSlot* s, uint8_t stop, uint8_t slot) {
    const uint8_t st = s->global ? 0u : stop;
    return slot < 4u ? gf_param_id(engine, GF_GROUP_OSC, (uint8_t)(st * 4u + slot))
                     : gf_param_id(engine, GF_GROUP_OSC_SHIFT, (uint8_t)(st * 4u + slot - 4u));
}

/** param_id de un slot de MOD. */
static inline uint16_t gf_mod_param_id(uint8_t engine, uint8_t stop, uint8_t slot) {
    return gf_param_id(engine, GF_GROUP_MOD, (uint8_t)(stop * GF_OSC_SLOTS + slot));
}

/* ── Cadena de FX del synth (variants/studio/theory/05-studio-synth-fx.md) ── */

#define GF_SFX_COUNT 4U      /* orden fijo: BIT · TAPE · CHORUS · MODAL */
#define GF_SFX_BIT    0U
#define GF_SFX_TAPE   1U
#define GF_SFX_CHORUS 2U
#define GF_SFX_MODAL  3U
#define GF_SFX_PARAMS 3U     /* principal + 2 de usuario por FX */

/** Cantidad de pasos de cada parámetro de la cadena: 0 = continuo, n = discreto con n valores. */
static const uint8_t GF_SFX_STEPS[GF_SFX_COUNT][GF_SFX_PARAMS] = {
    {16, 0, 0},   /* BITS 16–1 · SAMPLE RATE · SCULPT */
    {0, 0, 0},    /* DRIVE · WOW · FLUTTER */
    {0, 0, 0},    /* DEPTH · RATE · DRIFT */
    {0, 4, 3}};   /* DECAY · MATERIAL · SIZE */

static inline uint8_t gf_sfx_bits(float n) { return (uint8_t)(16 - gf_index_from_norm(n, 16)); }
static inline float gf_sfx_sample_rate(float n) { return 48000.0f * powf(1000.0f / 48000.0f, gf_clamp01(n)); }
static inline float gf_sfx_drive(float n) { return 1.0f + 9.0f * gf_clamp01(n); }
static inline float gf_sfx_chorus_rate(float n) { return 0.1f * powf(100.0f, gf_clamp01(n)); }
static inline float gf_sfx_decay(float n) { return 0.2f * powf(20.0f, gf_clamp01(n)); }

/** Profundidad de vibrato en semitonos para una cantidad PITCH (cuadrática: valores chicos = vibrato fino). */
static inline float gf_lfo_pitch_st(float amt) { return GF_LFO_PITCH_ST * amt * amt; }
