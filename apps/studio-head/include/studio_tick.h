#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Milisegundos monotónicos desde el arranque (fuente de tick de LVGL). */
uint32_t studio_tick_ms(void);

#ifdef __cplusplus
}
#endif
