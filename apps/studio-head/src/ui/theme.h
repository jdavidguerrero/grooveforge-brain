#pragma once

#include <cstdint>

#include "lvgl.h"

/**
 * @file theme.h
 * @brief Tokens de la UI de Studio v3 (claude-design-2026-10-09): 12 hex de la base + 4 de Studio,
 *        IBM Plex Mono en 7 tamaños y 2 pesos, grilla de 8 px.
 *
 * Mismos hex que apps/firmware-esp32/src/display/ui_theme.h.
 */

namespace gf {

// ── Colores ──────────────────────────────────────────────────────────────────
constexpr uint32_t BG            = 0x0A0A0A;
constexpr uint32_t TEAL          = 0x1D9E75;
constexpr uint32_t TEAL_PLUS     = 0x5DCAA5;
constexpr uint32_t TEAL_DIM      = 0x0E5040;
constexpr uint32_t PURPLE        = 0x534AB7;
constexpr uint32_t PURPLE_BRIGHT = 0x7A6FE0;
constexpr uint32_t GRAY          = 0x9FE1CB;
constexpr uint32_t WHITE         = 0xF2F2F2;
constexpr uint32_t MON           = 0x5A5A6E;
constexpr uint32_t RED           = 0xE84A4A;
constexpr uint32_t AMBER         = 0xE8B84A;
constexpr uint32_t ORANGE        = 0xFF6A35;
// Extensiones de Studio
constexpr uint32_t SURFACE       = 0x141414;
constexpr uint32_t SURFACE_2     = 0x1C1C1C;
constexpr uint32_t LINE          = 0x242424;
constexpr uint32_t PURPLE_DIM    = 0x2B2758;

/// Color por número Camelot (1–12); A y B comparten color.
uint32_t camelot(uint8_t n);

/// Color principal / brillante / apagado según el modo (SYNTH teal, FX púrpura).
struct ModeColors { uint32_t mc, hi, dim; };
ModeColors mode_colors(bool fx);

// ── Tipografía ───────────────────────────────────────────────────────────────
extern const lv_font_t* const T12;     ///< 12/16 · 400 — labels, unidades
extern const lv_font_t* const T12B;    ///< 12 · 600 — tags
extern const lv_font_t* const T16;     ///< 16 · 600 — soft-keys, títulos de página
extern const lv_font_t* const T16R;    ///< 16 · 400 — filas inactivas
extern const lv_font_t* const T20;     ///< 20 · 600 — valores de encoder, nombre de engine
extern const lv_font_t* const T28;     ///< 28/32 · 600 — títulos de contenido
extern const lv_font_t* const T40;     ///< 40 · 600 — valores secundarios
extern const lv_font_t* const T64;     ///< 64 · 600 — cutoff, resonance, tonalidad
extern const lv_font_t* const T96;     ///< 96 · 600 — dry/wet hero, banner de modo

// ── Geometría de pantalla (UI v3) ────────────────────────────────────────────
constexpr int SCREEN_W   = 800;
constexpr int SCREEN_H   = 480;
constexpr int TOPBAR_H   = 40;
constexpr int CONTENT_X  = 16;
constexpr int CONTENT_Y  = 48;
constexpr int CONTENT_W  = 768;
constexpr int CONTENT_H  = 328;
constexpr int BOTTOM_Y   = 384;
constexpr int BOTTOM_H   = 96;

// ── Helpers: objetos de relleno sólido, sin bordes redondeados, sombras ni scroll ─
lv_obj_t* rect(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg);
lv_obj_t* box(lv_obj_t* parent, int x, int y, int w, int h, uint32_t bg, uint32_t border);
lv_obj_t* label(lv_obj_t* parent, const lv_font_t* font, uint32_t color, const char* text);
void set_bg(lv_obj_t* obj, uint32_t bg);
void set_text_color(lv_obj_t* obj, uint32_t color);

}  // namespace gf
