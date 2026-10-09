/**
 * @file lv_conf.h
 * @brief Configuración de LVGL 8 para el Studio head (800×480, render por software).
 *
 * Solo se definen los valores que difieren de los defaults de LVGL (lv_conf_internal.h).
 * Restricciones de render: theory doc §5 (RGB565, refresco parcial, 30 fps).
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Color: RGB565 (la mitad de ancho de banda que 32 bits; los assets del diseño son RGB565). */
#define LV_COLOR_DEPTH          16
#define LV_COLOR_16_SWAP        0

/* Memoria: malloc del sistema (en Linux sobra RAM; evita dimensionar un pool fijo). */
#define LV_MEM_CUSTOM           1

/* Tick provisto por la app (CLOCK_MONOTONIC). */
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  "studio_tick.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (studio_tick_ms())

/* 30 fps: refresco de pantalla y lectura de input. */
#define LV_DISP_DEF_REFR_PERIOD 33
#define LV_INDEV_DEF_READ_PERIOD 16

/* 5" 800×480 ≈ 187 PPI. */
#define LV_DPI_DEF              187

/* Monitor de fps/CPU en pantalla: se activa con -DSTUDIO_PERF=1 al configurar. */
#ifdef STUDIO_PERF
#define LV_USE_PERF_MONITOR     1
#define LV_USE_PERF_MONITOR_POS LV_ALIGN_TOP_LEFT
#endif

#define LV_USE_LOG              1
#define LV_LOG_LEVEL            LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF           1

/* Fuente por defecto de LVGL (fallback); la UI usa las IBM Plex Mono de src/fonts. */
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/* Captura de pantalla a archivo (--snapshot): documentación, contenido y pruebas visuales. */
#define LV_USE_SNAPSHOT         1

/* Sin tema por defecto: la UI aplica sus propios estilos (rellenos sólidos, sin sombras). */
#define LV_USE_THEME_DEFAULT    0
#define LV_USE_THEME_BASIC      1

#endif /* LV_CONF_H */
