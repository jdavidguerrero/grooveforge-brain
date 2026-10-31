#pragma once
#include "ob6_preview.h"
#include "ui/preset_bank.h"
namespace gf {
/** @brief Aplica el subconjunto OB-6 admitido de un GFP validado. */
void apply_ob6_preset_preview(Ob6Preview& engine, const studio::Preset& preset);
}
