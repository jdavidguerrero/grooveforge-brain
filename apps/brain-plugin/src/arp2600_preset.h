#pragma once
#include "arp2600_preview.h"
#include "ui/preset_bank.h"
namespace gf {
/** @brief Traduce al núcleo ARP la parte GFP implementada. */
Arp2600Patch make_arp2600_patch(const studio::Preset& preset);
}
