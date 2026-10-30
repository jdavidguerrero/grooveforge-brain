#pragma once

#include "preset_codec.h"
#include "prophet_preview.h"

namespace gf {

/** @brief Etapas del preset que el piloto todavía no reproduce. */
struct PresetPreviewLimitations {
    bool synth_fx = false;
    bool groove = false;
    bool drift = false;
};
/** @brief Configura los efectos SYNTH comunes de un GFP validado. */
void apply_synth_fx_preview(ProphetPreview& engine, const studio::Preset& preset, uint8_t engine_id);
/** @brief Configura el reloj y las funciones GROOVE de un GFP validado. */
void apply_groove_preview(ProphetPreview& engine, const studio::Preset& preset);

/**
 * @brief Aplica un preset GFP decodificado al piloto ya preparado.
 * @return Etapas solicitadas por el preset que aún no están implementadas.
 * @note Es un preview; no certifica equivalencia con el Brain físico.
 */
PresetPreviewLimitations apply_prophet_preset_preview(ProphetPreview& engine, const studio::Preset& preset);

}  // namespace gf
