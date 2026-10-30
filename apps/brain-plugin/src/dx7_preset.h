#pragma once
#include "dx7_preview.h"
#include "preset_codec.h"
namespace gf {
/** @brief Construye los parámetros FM efectivos de un GFP, con macros aplicadas. */
Dx7Patch make_dx7_operator_patch(const studio::Preset& preset);
/** @brief Aplica la parte DX7 implementada de un GFP validado. */
void apply_dx7_preset_preview(Dx7Preview& engine, const studio::Preset& preset);
}
