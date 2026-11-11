#pragma once
#include <stdint.h>

/** @brief Precompensate NeoPixel brightness63 for an already capped linear RGB7 level. */
constexpr uint8_t consola_led_raw(uint8_t rgb7) {
    return uint8_t(uint32_t(rgb7>31?31:rgb7)*255u*256u/(127u*64u));
}
