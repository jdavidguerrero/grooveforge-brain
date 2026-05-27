/**
 * @file view_05_lfo_mod.cpp
 * @brief Vista 05 — LFO · MOD (Sprint 31 Batch D — rate y depth desde bridge)
 *
 * Onda LFO animada: lv_timer a ~30fps redibuja la senoidal con desfase creciente.
 * Sprint 31 agrega:
 *   - Velocidad de scroll proporcional al rate actual del bridge
 *   - Labels RATE y DEPTH actualizados dinamicamente segun bridge_get_param_value()
 *   - Cursor activo (rate=0 / depth=1) resaltado en teal+
 *
 * El incremento de fase por tick se mapea linealmente:
 *   rate_norm [0,1] → incremento [2,20] grados/tick @ 33ms ≈ [0.17,1.67 Hz]
 * Esto da scrolling perceptible a rate bajo y rapido a rate alto, sin saltos.
 *
 * El timer se registra con gf_anim_register_timer() para limpieza automatica.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../widgets/gf_anim.h"
#include "../../../bridge/bridge_handlers.h"
#include <math.h>
#include <stdio.h>

static constexpr lv_coord_t LFO_W  = 184;
static constexpr lv_coord_t LFO_H  = 60;
static constexpr lv_coord_t LFO_DY = -8;     ///< offset y del canvas respecto al centro

/* ── Estado persistente ───────────────────────────────────────────────────── */

static lv_obj_t* s_wave_cv   = nullptr;
static lv_obj_t* s_playhead  = nullptr;
static lv_obj_t* s_rate_lbl  = nullptr;   ///< label "RATE  x.x HZ"
static lv_obj_t* s_depth_lbl = nullptr;   ///< label "DEPTH  xx%"
static uint32_t  s_phase     = 0;

/* Ultimo rate y depth almacenados para que al mover el cursor a DEPTH no se
 * pierda el ultimo rate recibido (y viceversa). */
static float s_last_rate  = 0.3f;   ///< normalizado 0-1
static float s_last_depth = 0.6f;   ///< normalizado 0-1

/* ── Evaluacion de onda ───────────────────────────────────────────────────── */

static float lfo_value(float t) {
    const float ph = s_phase * (float)M_PI / 180.0f;
    return sinf(t * 2.0f * 2.0f * (float)M_PI + ph);   /* 2 ciclos visibles */
}

/* ── Tick de animacion — 33ms (~30fps) ───────────────────────────────────── */

static void lfo_tick(lv_timer_t* /*t*/) {
    /* Lee el param activo y el valor actual del bridge. */
    uint8_t cur_param = bridge_get_synth_param();
    float   cur_val   = bridge_get_param_value();

    /* Actualiza la cache segun cual parametro esta activo. */
    if (cur_param == 0) {
        s_last_rate = cur_val;
    } else {
        /* cur_param == 1 (depth) u otro — el rate no cambio. */
        s_last_depth = cur_val;
    }

    /* Incremento de fase proporcional al rate actual.
     * Rate 0.0 → 2°/tick, rate 1.0 → 20°/tick. */
    uint32_t inc = (uint32_t)(2.0f + s_last_rate * 18.0f);
    s_phase = (s_phase + inc) % 360;

    /* Redibuja la onda. */
    if (s_wave_cv) {
        lv_canvas_fill_bg(s_wave_cv, lv_color_black(), LV_OPA_TRANSP);

        const float mid = (LFO_H - 1) / 2.0f;
        const float amp = (LFO_H - 1) / 2.0f - 3.0f;

        lv_point_t pts[48];
        for (int i = 0; i < 48; i++) {
            const float t = (float)i / 47.0f;
            pts[i].x = (lv_coord_t)lroundf(t * (LFO_W - 1));
            pts[i].y = (lv_coord_t)lroundf(mid - lfo_value(t) * amp);
        }
        lv_draw_line_dsc_t d;
        lv_draw_line_dsc_init(&d);
        d.color       = GF_COLOR_TEAL_PLUS;
        d.width       = 2;
        d.opa         = LV_OPA_COVER;
        d.round_start = d.round_end = 1;
        lv_canvas_draw_line(s_wave_cv, pts, 48, &d);

        /* Playhead fijo en x=centro, su y sigue la onda. */
        if (s_playhead) {
            const float py = mid - lfo_value(0.5f) * amp;
            lv_obj_align(s_playhead, LV_ALIGN_CENTER, 0,
                         LFO_DY - LFO_H / 2 + (lv_coord_t)lroundf(py));
        }
    }

    /* Actualiza los labels de rate y depth con los valores actuales.
     * Resaltamos el label del param activo en teal+, el otro en blanco. */
    if (s_rate_lbl) {
        static char rbuf[24];
        /* rate_norm [0,1] → rango musical 0.1–10.0 Hz */
        float hz = 0.1f + s_last_rate * 9.9f;
        snprintf(rbuf, sizeof(rbuf), "RATE  %.1f HZ", (double)hz);
        lv_label_set_text(s_rate_lbl, rbuf);
        lv_obj_set_style_text_color(s_rate_lbl,
            (cur_param == 0) ? GF_COLOR_TEAL_PLUS : GF_COLOR_WHITE, 0);
    }

    if (s_depth_lbl) {
        static char dbuf[20];
        snprintf(dbuf, sizeof(dbuf), "DEPTH  %d%%", (int)(s_last_depth * 100.0f + 0.5f));
        lv_label_set_text(s_depth_lbl, dbuf);
        lv_obj_set_style_text_color(s_depth_lbl,
            (cur_param == 1) ? GF_COLOR_TEAL_PLUS : GF_COLOR_WHITE, 0);
    }
}

/* ── Creacion ─────────────────────────────────────────────────────────────── */

void view_05_create(lv_obj_t* parent) {
    s_wave_cv   = nullptr;
    s_playhead  = nullptr;
    s_rate_lbl  = nullptr;
    s_depth_lbl = nullptr;
    s_phase     = 0;
    /* Inicializa con el valor actual del bridge para el primer frame. */
    s_last_rate  = bridge_get_param_value();
    s_last_depth = 0.6f;

    gf_screen_bg(parent);
    gf_arc_title(parent, "LFO MOD", GF_COLOR_TEAL);

    /* Eje central (linea fina atenuada) — detras del canvas. */
    lv_obj_t* axis = lv_obj_create(parent);
    lv_obj_set_size(axis, LFO_W, 1);
    lv_obj_set_style_bg_color(axis, GF_COLOR_GRAY, 0);
    lv_obj_set_style_bg_opa(axis, 55, 0);
    lv_obj_set_style_border_width(axis, 0, 0);
    lv_obj_clear_flag(axis, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(axis, LV_ALIGN_CENTER, 0, LFO_DY);

    /* Canvas de la onda LFO. */
    s_wave_cv = gf_canvas_create(parent, LFO_W, LFO_H);
    if (s_wave_cv) lv_obj_align(s_wave_cv, LV_ALIGN_CENTER, 0, LFO_DY);

    /* Playhead — dot blanco en x=centro, y sigue la onda. */
    s_playhead = gf_dot(parent, 5, GF_COLOR_WHITE);

    /* Labels dinamicos RATE / DEPTH. */
    s_rate_lbl  = gf_hero_label(parent, "RATE  ?", GF_FONT_LABEL, GF_COLOR_WHITE, 40);
    s_depth_lbl = gf_hero_label(parent, "DEPTH  ?", GF_FONT_LABEL, GF_COLOR_WHITE, 58);

    /* Hint de target — estatico. */
    gf_hero_label(parent, "TARGET CUTOFF", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS, -52);

    /* Timer ~30fps registrado para limpieza automatica al cambiar de vista. */
    lv_timer_t* t = lv_timer_create(lfo_tick, 33, nullptr);
    gf_anim_register_timer(t);
    lfo_tick(nullptr);   /* primer frame inmediato */

    gf_page_dots(parent, 5, 3, GF_COLOR_TEAL);
    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);
}

void view_05_destroy(void) {
    /* El timer lo destruye gf_anim_kill_all(); aqui solo se sueltan punteros. */
    s_wave_cv   = nullptr;
    s_playhead  = nullptr;
    s_rate_lbl  = nullptr;
    s_depth_lbl = nullptr;
}
