#pragma once
#include "model_d_preview.h"
#include "ui/preset_bank.h"
namespace gf {
/** @brief Aplica el subconjunto GFP Model D admitido al motor portable. */
void apply_model_d_preset_preview(ModelDPreview& engine, const studio::Preset& preset);
}
