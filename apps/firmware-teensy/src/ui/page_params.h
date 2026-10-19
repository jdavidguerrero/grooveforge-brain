#pragma once

#include <stdint.h>

#include <gf_params.h>
#include <protocol.h>

/**
 * @file page_params.h
 * @brief Qué editan P1 / P2 (y SHIFT + P1 / P2) en la página visible, y cómo se avisa a la cabeza.
 *
 * Lógica pura, sin hardware: recibe el giro de un encoder y el estado de UI que decide StudioUi, cambia el
 * patch del engine y devuelve el PARAM_CHANGED que hay que mandar. Mapeo: theory/lfo-modulacion.md §6.
 */

namespace studio {

/// Parámetros que editan los encoders de página. Uno por engine (al cambiar de engine vuelve su patch).
struct Patch {
    float attack_ms = 5.0f, decay_ms = 300.0f, sustain = 0.7f, release_ms = 300.0f;     ///< envelope del amplificador
    float f_attack_ms = 5.0f, f_decay_ms = 400.0f, f_sustain = 0.3f, f_release_ms = 300.0f;  ///< envelope del filtro
    float env_amt = 0.3f;                    ///< FLT ENV AMT, 0–1 = 0 a +5 oct
    float glide = 0.0f;                      ///< GLIDE (SHIFT+P1 en PERFORM/SPECTRUM), 0–1
    float drive = 0.2f;                      ///< DRIVE del filtro (SHIFT+P2 en PERFORM/SPECTRUM), 0–1
    float lfo_rate_hz = 2.0f;                ///< RATE libre
    uint8_t lfo_shape = 1;                   ///< 0 SINE · 1 TRI · 2 SAW · 3 SQR · 4 S&H
    uint8_t lfo_div = 1;                     ///< 0 1/4 · 1 1/8 · 2 1/16 · 3 1/8T (con SYNC)
    float lfo_amt[GF_DEST_COUNT] = {};       ///< cantidad por destino, 0–1 (GF_DEST_*)

    /// Página OSC en unidades de la UI (GF_OSC_SLOT): [parada][slot 0–7]; los globales, en la parada 0.
    float osc[GF_OSC_STOPS][GF_OSC_SLOTS] = {};
    /// Paradas de MOD de la página LFO / MOD (GF_MOD_SLOT), p. ej. POLY-MOD y WHEEL-MOD del Prophet.
    float mod[GF_MOD_STOPS_MAX][GF_OSC_SLOTS] = {};

    /// Cadena de FX del synth (BIT · TAPE · CHORUS · MODAL), todo normalizado 0–1 (gf_params.h).
    bool sfx_on[GF_SFX_COUNT] = {};
    float sfx_mix[GF_SFX_COUNT] = {0.5f, 0.6f, 0.5f, 0.3f};
    float sfx_p[GF_SFX_COUNT][GF_SFX_PARAMS] = {
        {0.4f, 0.3f, 0.3f},                  ///< 10 bits · ~15 kHz · sculpt
        {0.25f, 0.2f, 0.2f},                 ///< drive 3.25 · wow · flutter
        {0.5f, 0.35f, 0.2f},                 ///< depth · ~0.5 Hz · drift
        {0.45f, 1.0f / 3.0f, 0.5f}};         ///< decay ×0.8 · GUADUA · MEDIUM
};

struct ParamChange {
    uint16_t id;
    float norm;
};

/// 8 ENV + 3 FILTER (ENV AMT, GLIDE, DRIVE) + RATE, SHAPE, DIV + 4 cantidades + cadena (4 + 4 + 12) + hasta 32 de OSC + 16 de MOD
constexpr uint8_t PATCH_PARAM_COUNT = 86;

/// Valores de arranque de la página OSC de cada engine (los del diseño v3, osc_catalog.cpp de la cabeza).
void patch_init(Patch& p, uint8_t engine);

/**
 * @brief Un encoder de página giró `steps` pasos: `enc` 0 = P1, 1 = P2, 2 = ENC L, 3 = ENC R (theory/04 §3.1).
 *        En las vistas, ENC L / R son el filtro y no pasan por acá.
 * @return cuántos cambios escribió en `out` (0 si en esa página / engine el control no hace nada, o 1).
 */
uint8_t edit_encoder(Patch& p, const GF_UiState& ui, uint8_t engine, uint8_t enc, int8_t steps, ParamChange* out);

/// NAV en la página LFO: siguiente / anterior forma (sin dar la vuelta).
uint8_t step_lfo_shape(Patch& p, uint8_t engine, int8_t steps, ParamChange* out);

/// NAV push en SYNTH FX: enciende / apaga el FX `slot` de la cadena.
uint8_t toggle_sfx(Patch& p, uint8_t engine, uint8_t slot, ParamChange* out);

/// Todo el patch, para mandarlo al conectar o al cambiar de engine. `out` necesita PATCH_PARAM_COUNT lugares.
uint8_t patch_params(const Patch& p, uint8_t engine, ParamChange* out);

}  // namespace studio
