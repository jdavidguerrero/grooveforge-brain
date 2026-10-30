#pragma once
#include "juno_preview.h"
#include "ui/preset_bank.h"
namespace gf {
/** @brief Aplica al piloto los campos Juno admitidos de un GFP decodificado. */
void apply_juno_preset_preview(JunoPreview& engine, const studio::Preset& preset);
}
