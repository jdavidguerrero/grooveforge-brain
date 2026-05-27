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
 * Timer a 100ms:
 *   - Actualiza el highlight de la fila activa (s_last_param).
 *   - Detecta cambio de param_id (bridge_get_last_real_param_id) y actualiza
 *     el value label de la fila correspondiente con el nuevo valor.
 *
 * Los value labels se crean con los valores cacheados del bridge
 * (bridge_get_synth_param_cached) para que muestren el estado real al entrar.
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
static lv_obj_t*   s_row_val[4]  = {};   ///< labels de valor por fila (live)
static lv_timer_t* s_timer       = nullptr;
static uint8_t     s_last_param  = 0xFF;
static uint16_t    s_last_pid    = 0xFFFF;  ///< último param_id recibido para detectar cambios

/* ── Nombres y posiciones de las 4 filas ─────────────────────────────────── */

static const char*      ROW_NAME[4]  = { "VCO WAVE", "VCO OCT", "VCO2 WAVE", "DETUNE" };
static const lv_coord_t ROW_DY[4]   = { -48, -16, 16, 48 };

/* ── Helper: valor de display por fila dado un valor normalizado explícito ── */

static const char* row_value_str(uint8_t row, float val, char* buf, size_t buf_sz) {
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

/* ── Timer: actualiza highlight Y value labels ────────────────────────────── */

static void osc_timer_cb(lv_timer_t* /*t*/) {
    /* ── 1. Actualizar highlight si el cursor cambió ─────────────────────── */
    uint8_t cur = bridge_get_synth_param();
    if (cur > 3) cur = 0;

    if (cur != s_last_param) {
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
            if (s_row_val[i]) {
                lv_obj_set_style_text_color(s_row_val[i],
                    active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
            }
        }
    }

    /* ── 2. Actualizar value label si llegó un PARAM_CHANGED nuevo ──────── */
    /* param_id encoding para OSC (engine 0, grupo 0):
     *   byte-alto = 0x10, byte-bajo = (0 << 4) | pidx → 0x1000..0x1003      */
    uint16_t pid = bridge_get_last_real_param_id();
    if (pid != s_last_pid) {
        s_last_pid = pid;
        /* ¿Es un param del grupo OSC del engine activo (Moog=0x10, grp=0)?   */
        uint8_t byte_hi = (uint8_t)(pid >> 8);
        uint8_t byte_lo = (uint8_t)(pid & 0xFF);
        uint8_t grp     = (byte_lo >> 4) & 0x0F;
        uint8_t pidx    = byte_lo & 0x0F;
        /* Moog = 0x10, cualquier engine >= 0x10 sirve; grp 0 = OSC */
        if (byte_hi >= 0x10 && byte_hi <= 0x12 && grp == 0 && pidx < 4) {
            float val = bridge_get_param_value();
            char buf[16];
            const char* vstr = row_value_str(pidx, val, buf, sizeof(buf));
            if (s_row_val[pidx]) lv_label_set_text(s_row_val[pidx], vstr);
        }
    }
}

/* ── Creacion ─────────────────────────────────────────────────────────────── */

void view_03_create(lv_obj_t* parent) {
    for (uint8_t i = 0; i < 4; i++) {
        s_row_bg[i]   = nullptr;
        s_row_name[i] = nullptr;
        s_row_val[i]  = nullptr;
    }
    s_timer      = nullptr;
    s_last_param = 0xFF;
    s_last_pid   = 0xFFFF;

    gf_screen_bg(parent);
    gf_arc_title(parent, "OSC", GF_COLOR_TEAL);

    uint8_t init_param = bridge_get_synth_param();
    if (init_param > 3) init_param = 0;

    /* Engine activo en este sprint: siempre engine 0 (Moog). */
    const uint8_t ENGINE = 0;

    for (uint8_t i = 0; i < 4; i++) {
        bool active = (i == init_param);

        /* Fondo de resaltado */
        lv_obj_t* bg = lv_obj_create(parent);
        lv_obj_set_size(bg, 200, 28);
        lv_obj_set_style_radius(bg, 6, 0);
        lv_obj_set_style_bg_color(bg, GF_COLOR_TEAL, 0);
        lv_obj_set_style_bg_opa(bg, active ? 56 : 0, 0);
        lv_obj_set_style_border_width(bg, 0, 0);
        lv_obj_clear_flag(bg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(bg, LV_ALIGN_CENTER, 0, ROW_DY[i]);
        s_row_bg[i] = bg;

        /* Glifo de onda para filas 0 y 2 (VCO_WAVE / VCO2_WAVE) */
        if (i == 0 || i == 2) {
            lv_color_t wc = active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY;
            lv_obj_t* wv = gf_glyph_wave(parent, GF_WAVE_SAW, 28, 14, wc);
            if (wv) lv_obj_align(wv, LV_ALIGN_CENTER, -72, ROW_DY[i]);
        }

        /* Nombre del parámetro */
        lv_color_t nc = active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY;
        lv_obj_t* nm = gf_label(parent, ROW_NAME[i], GF_FONT_MICRO, nc);
        if (nm) lv_obj_align(nm, LV_ALIGN_CENTER, -20, ROW_DY[i]);
        s_row_name[i] = nm;

        /* Value label — inicializado con el valor cacheado del bridge */
        float cached_val = bridge_get_synth_param_cached(ENGINE, 0 /* OSC */, i);
        char buf[16];
        const char* vstr = row_value_str(i, cached_val, buf, sizeof(buf));
        lv_obj_t* vl = gf_label(parent, vstr, GF_FONT_MICRO,
                                 active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY);
        if (vl) lv_obj_align(vl, LV_ALIGN_CENTER, 68, ROW_DY[i]);
        s_row_val[i] = vl;   /* guardamos el puntero para actualizaciones live */
    }

    gf_page_dots(parent, 5, 1, GF_COLOR_TEAL);
    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);

    s_last_param = init_param;
    s_last_pid   = bridge_get_last_real_param_id();   /* no disparar update falso en primer tick */

    s_timer = lv_timer_create(osc_timer_cb, 100, nullptr);
}

void view_03_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    for (uint8_t i = 0; i < 4; i++) {
        s_row_bg[i]   = nullptr;
        s_row_name[i] = nullptr;
        s_row_val[i]  = nullptr;
    }
    s_last_param = 0xFF;
    s_last_pid   = 0xFFFF;
}
