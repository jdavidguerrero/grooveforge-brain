#pragma once

/**
 * @file gf_fonts.h
 * @brief Declaraciones de las fuentes IBM Plex Mono — GrooveForge Brain
 *
 * Las fuentes se pre-rasterizan con lv-font-conv (LVGL no lee TTF en runtime).
 * Generacion (desde src/display/fonts/):
 *
 *   npx lv_font_conv --font IBMPlexMono-Regular.ttf --size N --bpp 4 \
 *       --no-compress --format lvgl --range 0x20-0x7F --range 0x00B7 \
 *       -o ibm_plex_mono_N.c
 *
 * El tamano 64 usa un set restringido (0-9 # A-G espacio) — la nota hero del
 * synth solo necesita esos caracteres, lo que reduce el flash ~5x.
 *
 * bpp 4 = 16 niveles de antialiasing. --no-compress es obligatorio porque
 * lv_conf.h tiene LV_USE_FONT_COMPRESSED 0.
 *
 * NOTA: los simbolos geometricos (● ▶ ◀ ▲ ▼ ✓ ✕) NO estan en IBM Plex Mono.
 * Los mocks los especifican como vector line-art → se dibujan en gf_glyphs.
 */

#include "lvgl.h"

LV_FONT_DECLARE(ibm_plex_mono_8);
LV_FONT_DECLARE(ibm_plex_mono_11);
LV_FONT_DECLARE(ibm_plex_mono_16);
LV_FONT_DECLARE(ibm_plex_mono_22);
LV_FONT_DECLARE(ibm_plex_mono_36);
LV_FONT_DECLARE(ibm_plex_mono_64);
