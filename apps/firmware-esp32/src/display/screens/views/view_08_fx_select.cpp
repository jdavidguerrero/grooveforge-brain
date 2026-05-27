/**
 * @file view_08_fx_select.cpp
 * @brief Vista 08 — FX SELECT (Sprint 30, RMX-style)
 *
 * Lista vertical de 9 FX, 5 visibles. La fila central (row 2) es la activa:
 * texto blanco en GF_FONT_LABEL enmarcado por un bracket. Las otras van en
 * GF_FONT_MICRO atenuadas. La lista es INFINITE-SCROLL: usa módulo %NUM_FX
 * para que SUB GENESIS → GHOST ECHO sin huecos.
 *
 * Actualización in-place via timer 50ms — sin carousel_goto en cada tick.
 * Solo se navega a esta vista si aún no estamos aquí (bridge_handlers).
 *
 * Layout:
 *   row 0  cursor-2  dy=-56  micro/gray
 *   row 1  cursor-1  dy=-28  micro/gray
 *   row 2  cursor    dy=  2  label/white + bracket
 *   row 3  cursor+1  dy= 32  micro/gray
 *   row 4  cursor+2  dy= 60  micro/gray
 *
 * ENC NAV push → confirma FX y navega a view_07 FX MAIN (manejado en Teensy,
 * que envía ENGINE_CHANGED al ESP32).
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>

/* ── Tabla de FX (mismo orden que fx_desc[] en sketch 27) ──────────────── */

static const char* const FX_NAMES[] = {
    "GHOST ECHO",
    "MODAL REV",
    "PHASE CHORUS",
    "BIT SCULPT",
    "TAPE SAT",
    "CYMATIC RES",
    "GRANULAR",
    "SPRING PLATE",
    "SUB GENESIS",
};
static constexpr uint8_t NUM_FX = 9;

/* ── Offsets verticales de las 5 filas (centradas en fila 2) ───────────── */

static constexpr lv_coord_t DY[5] = { -56, -28, 2, 32, 60 };

/* ── Estado persistente ──────────────────────────────────────────────────── */

static lv_obj_t*   s_names[5]  = {};       ///< label de nombre por fila
static lv_obj_t*   s_glyphs[5] = {};       ///< glifo por fila
static lv_obj_t*   s_bracket   = nullptr;  ///< bracket de selección (fija en row 2)
static lv_obj_t*   s_pos_label = nullptr;  ///< "3 / 9"
static lv_timer_t* s_timer     = nullptr;
static uint8_t     s_last_cursor = 0xFF;   ///< cursor anterior (detectar cambio)

/* ── Helpers de estilo por fila ─────────────────────────────────────────── */

static void style_row(lv_obj_t* name, lv_obj_t* glyph, bool is_active) {
    if (!name) return;
    lv_obj_set_style_text_font(name,
                               is_active ? GF_FONT_LABEL : GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(name,
                                is_active ? GF_COLOR_WHITE : GF_COLOR_GRAY, 0);
    lv_obj_set_style_opa(name, is_active ? LV_OPA_COVER : (lv_opa_t)100, 0);
    if (glyph) {
        /* El glifo toma el tamaño del texto de la fila. */
        lv_obj_set_style_opa(glyph, is_active ? LV_OPA_COVER : (lv_opa_t)100, 0);
    }
}

/* ── Timer callback: actualiza texto y estilos en-place ─────────────────── */

static void fx_select_timer_cb(lv_timer_t* /*t*/) {
    uint8_t cur = bridge_get_fx_cursor();
    if (cur == s_last_cursor) return;   /* sin cambio — nada que hacer */
    s_last_cursor = cur;

    /* Actualizar las 5 filas con wrap-around modular. */
    for (int8_t row = 0; row < 5; row++) {
        /* Módulo seguro para índices negativos: +NUM_FX*10 evita negativo. */
        uint8_t fx_i = (uint8_t)((cur + row - 2 + (int)NUM_FX * 10) % NUM_FX);
        bool    is_active = (row == 2);

        if (s_names[row])  lv_label_set_text(s_names[row],  FX_NAMES[fx_i]);
        style_row(s_names[row], s_glyphs[row], is_active);
    }

    /* Contador de posición. */
    if (s_pos_label) {
        static char ps[8];
        snprintf(ps, sizeof(ps), "%u / %u", cur + 1, NUM_FX);
        lv_label_set_text(s_pos_label, ps);
    }
}

/* ── Entrada principal ──────────────────────────────────────────────────── */

void view_08_create(lv_obj_t* parent) {
    /* Limpiar estado del build anterior. */
    for (int i = 0; i < 5; i++) s_names[i] = s_glyphs[i] = nullptr;
    s_bracket = s_pos_label = nullptr;
    s_timer       = nullptr;
    s_last_cursor = 0xFF;   /* forzar primera actualización del timer */

    gf_screen_bg(parent);
    gf_tint(parent, GF_COLOR_PURPLE, 30);
    gf_arc_title(parent, "FX SELECT", GF_COLOR_PURPLE_BRIGHT);

    uint8_t cur = bridge_get_fx_cursor();

    /* ── Bracket de selección (fijo en row 2 — el centro) ─────────────── */
    lv_obj_t* br = lv_obj_create(parent);
    lv_obj_set_size(br, 164, 26);
    lv_obj_set_style_radius(br, 4, 0);
    lv_obj_set_style_bg_opa(br, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(br, GF_COLOR_PURPLE_BRIGHT, 0);
    lv_obj_set_style_border_width(br, 1, 0);
    lv_obj_clear_flag(br, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(br, LV_ALIGN_CENTER, 0, DY[2]);
    s_bracket = br;

    /* ── 5 filas con wrap-around ────────────────────────────────────────── */
    for (int8_t row = 0; row < 5; row++) {
        uint8_t fx_i = (uint8_t)((cur + row - 2 + (int)NUM_FX * 10) % NUM_FX);
        bool    is_active = (row == 2);

        /* Glifo. */
        lv_obj_t* gl = gf_glyph_fx(parent, fx_i,
                                    is_active ? 22 : 14,
                                    GF_COLOR_PURPLE_BRIGHT);
        if (gl) lv_obj_align(gl, LV_ALIGN_CENTER, -54, DY[row]);
        s_glyphs[row] = gl;

        /* Nombre. */
        lv_obj_t* nm = gf_label(parent, FX_NAMES[fx_i],
                                is_active ? GF_FONT_LABEL : GF_FONT_MICRO,
                                is_active ? GF_COLOR_WHITE : GF_COLOR_GRAY);
        if (nm) {
            lv_obj_set_style_opa(nm, is_active ? LV_OPA_COVER : (lv_opa_t)100, 0);
            lv_obj_align(nm, LV_ALIGN_CENTER, 14, DY[row]);
        }
        s_names[row] = nm;
    }

    /* ── Contador de posición ───────────────────────────────────────────── */
    static char pos_str[8];
    snprintf(pos_str, sizeof(pos_str), "%u / %u", cur + 1, NUM_FX);
    lv_obj_t* pos = gf_label(parent, pos_str, GF_FONT_MICRO, GF_COLOR_PURPLE_BRIGHT);
    if (pos) lv_obj_align(pos, LV_ALIGN_CENTER, 0, 98);
    s_pos_label = pos;

    gf_mode_pill(parent, "FX", GF_COLOR_PURPLE_BRIGHT);

    /* ── Timer 50ms: actualiza texto/estilos en-place ──────────────────── */
    s_last_cursor = cur;   /* sincronizar — no redibujar en el primer tick */
    s_timer = lv_timer_create(fx_select_timer_cb, 50, nullptr);
}

void view_08_destroy(void) {
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = nullptr;
    }
    for (int i = 0; i < 5; i++) s_names[i] = s_glyphs[i] = nullptr;
    s_bracket = s_pos_label = nullptr;
}
