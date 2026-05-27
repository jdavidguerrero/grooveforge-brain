/**
 * @file view_03_osc.cpp
 * @brief Vista 03 — OSC GROUP (Sprint 31 Batch D — dinamico)
 *
 * Muestra 4 parametros del grupo OSC del Moog Model D en lista vertical.
 * La fila activa (segun bridge_get_synth_param()) se resalta en teal.
 *
 * Layout (4 filas, dy = -48/-16/+16/+48 del centro):
 *   Fila 0: VCO WAVE  — categorico SINE/SAW/SQ/TRI
 *   Fila 1: VCO OCT   — categorico -2/-1/0/+1/+2
 *   Fila 2: VCO2 WAVE — categorico SINE/SAW/SQ/TRI
 *   Fila 3: DETUNE    — continuo, cents -25..+25
 *
 * Timer a 100ms: solo actualiza highlight si cambio el param activo.
 * Los objetos se crean una sola vez en view_03_create(); el timer no rebuild.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <math.h>

/* ── Estado persistente ───────────────────────────────────────────────────── */

static lv_obj_t*   s_row_bg[4]   = {};   ///< fondos de resaltado por fila
static lv_obj_t*   s_row_name[4] = {};   ///< labels de nombre del param
static lv_timer_t* s_timer       = nullptr;
static uint8_t     s_last_param  = 0xFF;

/* ── Nombres y posiciones de las 4 filas ─────────────────────────────────── */

static const char*      ROW_NAME[4]  = { "VCO WAVE", "VCO OCT", "VCO2 WAVE", "DETUNE" };
static const lv_coord_t ROW_DY[4]   = { -48, -16, 16, 48 };

/* ── Helper: valor de display por fila ───────────────────────────────────── */

static const char* row_value_str(uint8_t row, char* buf, size_t buf_sz) {
    float val = bridge_get_param_value();

    if (row == 0 || row == 2) {
        /* VCO WAVE / VCO2 WAVE: 4 formas */
        static const char* waves[4] = { "SINE", "SAW", "SQ", "TRI" };
        int idx = (int)lroundf(val * 3.0f);
        if (idx < 0) idx = 0;
        if (idx > 3) idx = 3;
        return waves[idx];
    }
    if (row == 1) {
        /* VCO OCT: 5 pasos -2..+2 */
        int oct = (int)lroundf(val * 4.0f) - 2;
        snprintf(buf, buf_sz, "%+d", oct);
        return buf;
    }
    /* row == 3: DETUNE en cents, rango -25..+25 */
    int cents = (int)lroundf((val - 0.5f) * 50.0f);
    snprintf(buf, buf_sz, "%+d ct", cents);
    return buf;
}

/* ── Timer: actualiza highlight sin rebuild ───────────────────────────────── */

static void osc_timer_cb(lv_timer_t* /*t*/) {
    uint8_t cur = bridge_get_synth_param();
    /* Clampeamos a 3 para no indexar fuera del arreglo si el bridge manda
     * un param inesperado — el grupo OSC tiene exactamente 4 params (0-3). */
    if (cur > 3) cur = 0;
    if (cur == s_last_param) return;
    s_last_param = cur;

    for (uint8_t i = 0; i < 4; i++) {
        bool active = (i == cur);
        if (s_row_bg[i]) {
            lv_obj_set_style_bg_opa(s_row_bg[i], active ? 56 : 0, 0);
        }
        if (s_row_name[i]) {
            lv_obj_set_style_text_color(s_row_name[i],
                active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
        }
    }
}

/* ── Creacion ─────────────────────────────────────────────────────────────── */

void view_03_create(lv_obj_t* parent) {
    /* Limpia estado residual de una creacion previa — el carrusel reutiliza
     * el mismo screen en algunas rutas de navegacion. */
    for (uint8_t i = 0; i < 4; i++) { s_row_bg[i] = nullptr; s_row_name[i] = nullptr; }
    s_timer      = nullptr;
    s_last_param = 0xFF;

    gf_screen_bg(parent);
    gf_arc_title(parent, "OSC", GF_COLOR_TEAL);

    uint8_t init_param = bridge_get_synth_param();
    if (init_param > 3) init_param = 0;

    for (uint8_t i = 0; i < 4; i++) {
        bool active = (i == init_param);

        /* Fondo de resaltado — siempre creado, opa=0 cuando inactivo. */
        lv_obj_t* bg = lv_obj_create(parent);
        lv_obj_set_size(bg, 200, 28);
        lv_obj_set_style_radius(bg, 6, 0);
        lv_obj_set_style_bg_color(bg, GF_COLOR_TEAL, 0);
        lv_obj_set_style_bg_opa(bg, active ? 56 : 0, 0);
        lv_obj_set_style_border_width(bg, 0, 0);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(bg, LV_ALIGN_CENTER, 0, ROW_DY[i]);
        s_row_bg[i] = bg;

        /* Glifo de onda para filas 0 y 2 (VCO_WAVE / VCO2_WAVE). */
        if (i == 0 || i == 2) {
            lv_color_t wc = active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY;
            lv_obj_t* wv = gf_glyph_wave(parent, GF_WAVE_SAW, 28, 14, wc);
            if (wv) lv_obj_align(wv, LV_ALIGN_CENTER, -72, ROW_DY[i]);
        }

        /* Nombre del parametro. */
        lv_color_t nc = active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY;
        lv_obj_t* nm = gf_label(parent, ROW_NAME[i], GF_FONT_MICRO, nc);
        if (nm) lv_obj_align(nm, LV_ALIGN_CENTER, -20, ROW_DY[i]);
        s_row_name[i] = nm;

        /* Valor estatico inicial (cambiara cuando el bridge envie PARAM_CHANGED). */
        char buf[16];
        const char* vstr = row_value_str(i, buf, sizeof(buf));
        lv_obj_t* vl = gf_label(parent, vstr, GF_FONT_MICRO,
                                 active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY);
        if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, 68, ROW_DY[i]);
    }

    gf_page_dots(parent, 5, 1, GF_COLOR_TEAL);
    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);

    /* Timer de 100ms — solo actualiza highlight, sin rebuild de objetos. */
    s_timer = lv_timer_create(osc_timer_cb, 100, nullptr);
}

void view_03_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    for (uint8_t i = 0; i < 4; i++) { s_row_bg[i] = nullptr; s_row_name[i] = nullptr; }
    s_last_param = 0xFF;
}
