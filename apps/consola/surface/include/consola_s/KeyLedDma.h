#pragma once
#include <stdint.h>

namespace key_led_dma {
/** @brief Configure pin13/QTIMER4/XBAR/three DMA channels, with no LED IRQ. */
void begin();
/** @brief Submit one limited 26-pixel RGB8 frame; false when busy, never waits. */
bool submit(const uint8_t* rgb,uint32_t now_us);
}
