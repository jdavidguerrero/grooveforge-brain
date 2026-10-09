/**
 * @file lv_drv_conf.h
 * @brief Configuración de lv_drivers (v8.3) para el Studio head.
 *
 * USE_SDL / USE_FBDEV / USE_EVDEV los define CMake según STUDIO_DISPLAY.
 */
#ifndef LV_DRV_CONF_H
#define LV_DRV_CONF_H

#include "lv_conf.h"

#ifndef USE_SDL
#define USE_SDL 0
#endif
#ifndef USE_FBDEV
#define USE_FBDEV 0
#endif
#ifndef USE_EVDEV
#define USE_EVDEV 0
#endif

/* ── SDL (simulador en la Mac) ─────────────────────────────────────────────── */
#define SDL_HOR_RES          800
#define SDL_VER_RES          480
#define SDL_ZOOM             1
#define SDL_DOUBLE_BUFFERED  0
#define SDL_INCLUDE_PATH     <SDL.h>
#define SDL_DUAL_DISPLAY     0

/* ── Framebuffer (Raspberry Pi) ────────────────────────────────────────────── */
#define FBDEV_PATH           "/dev/fb0"

/* ── Touch evdev (Raspberry Pi) ────────────────────────────────────────────────
 * El path real se pasa por línea de comandos (--touch); este es solo el default.
 * Ver theory doc §8.3: regla udev que crea /dev/studio-touch. */
#define EVDEV_NAME           "/dev/studio-touch"
#define EVDEV_SWAP_AXES      0
#define EVDEV_CALIBRATE      0

#endif /* LV_DRV_CONF_H */
