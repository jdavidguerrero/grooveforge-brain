/**
 * @file view_23_midi_learn.cpp
 * @brief Vista 23 — MIDI LEARN (mock GFB-UI-023)
 *
 * FSM de 3 estados: A WAITING → B DETECTED → C MAPPED. Como el carrusel no
 * tiene entrada, un lv_timer interno cicla los 3 estados dentro del slot de 5s
 * para mostrarlos todos. El chrome (titulo + indicador REC pulsante) persiste;
 * solo se reconstruye el contenedor de contenido en cada estado.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../widgets/gf_anim.h"

static lv_obj_t* s_ml_content = nullptr;
static uint8_t   s_ml_state   = 0;

static void opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

/* Reconstruye el contenedor de contenido para el estado dado. */
static void ml_build_state(uint8_t st) {
    if (s_ml_content == nullptr) return;
    lv_obj_clean(s_ml_content);
    lv_obj_t* c = s_ml_content;

    if (st == 0) {
        /* ── A — WAITING ─────────────────────────────────────────────── */
        lv_obj_t* pl = gf_label(c, "VOL", GF_FONT_MICRO, GF_COLOR_GRAY);
        lv_obj_set_style_opa(pl, 90, 0);
        lv_obj_align(pl, LV_ALIGN_CENTER, -72, -22);
        lv_obj_t* pc = gf_label(c, "CUTOFF", GF_FONT_BODY, GF_COLOR_WHITE);
        lv_obj_align(pc, LV_ALIGN_CENTER, 0, -22);
        lv_obj_t* pr = gf_label(c, "RES", GF_FONT_MICRO, GF_COLOR_GRAY);
        lv_obj_set_style_opa(pr, 90, 0);
        lv_obj_align(pr, LV_ALIGN_CENTER, 72, -22);

        lv_obj_t* nm = gf_label(c, "NO MAPPING", GF_FONT_MICRO, GF_COLOR_GRAY);
        lv_obj_set_style_opa(nm, 140, 0);
        lv_obj_align(nm, LV_ALIGN_CENTER, 0, 4);

        lv_obj_t* mv = gf_label(c, "MOVE A KNOB", GF_FONT_LABEL, GF_COLOR_TEAL);
        lv_obj_align(mv, LV_ALIGN_CENTER, 0, 26);

        for (uint8_t i = 0; i < 3; i++) {
            lv_obj_t* d = gf_dot(c, 4, GF_COLOR_TEAL);
            lv_obj_align(d, LV_ALIGN_CENTER, -12 + i * 12, 48);
        }
    } else if (st == 1) {
        /* ── B — DETECTED ────────────────────────────────────────────── */
        gf_arc(c, 96, 2, 0, 360, 100, GF_COLOR_TEAL_PLUS, GF_COLOR_TEAL_PLUS);

        lv_obj_t* cc = gf_label(c, "CC 74 \xC2\xB7 CH 1", GF_FONT_BODY, GF_COLOR_TEAL_PLUS);
        lv_obj_align(cc, LV_ALIGN_CENTER, 0, -16);

        lv_obj_t* rx = gf_label(c, "CC 74 RECEIVED", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS);
        lv_obj_align(rx, LV_ALIGN_CENTER, 0, 8);

        lv_obj_t* sr = gf_label(c, "FROM KEYBOARD USB-A", GF_FONT_MICRO, GF_COLOR_GRAY);
        lv_obj_set_style_opa(sr, 140, 0);
        lv_obj_align(sr, LV_ALIGN_CENTER, 0, 26);

        lv_obj_t* sv = gf_label(c, "PRESS TO SAVE", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS);
        lv_obj_align(sv, LV_ALIGN_CENTER, 0, 50);
    } else {
        /* ── C — MAPPED ──────────────────────────────────────────────── */
        lv_obj_t* ring = lv_obj_create(c);
        lv_obj_set_size(ring, 48, 48);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(ring, GF_COLOR_TEAL_PLUS, 0);
        lv_obj_set_style_bg_opa(ring, 30, 0);
        lv_obj_set_style_border_color(ring, GF_COLOR_TEAL_PLUS, 0);
        lv_obj_set_style_border_width(ring, 2, 0);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(ring, LV_ALIGN_CENTER, 0, -20);
        lv_obj_t* chk = gf_glyph_check(c, 26, GF_COLOR_TEAL_PLUS);
        if (chk != nullptr) lv_obj_align(chk, LV_ALIGN_CENTER, 0, -20);

        lv_obj_t* mp = gf_label(c, "MAPPED", GF_FONT_BODY, GF_COLOR_TEAL_PLUS);
        lv_obj_align(mp, LV_ALIGN_CENTER, 0, 18);

        lv_obj_t* cd = gf_label(c, "CC 74 \xC2\xB7 CH 1", GF_FONT_MICRO, GF_COLOR_GRAY);
        lv_obj_align(cd, LV_ALIGN_CENTER, 0, 38);

        lv_obj_t* nx = gf_label(c, "NEXT PARAM IN 0.5s", GF_FONT_MICRO, GF_COLOR_GRAY);
        lv_obj_set_style_opa(nx, 130, 0);
        lv_obj_align(nx, LV_ALIGN_CENTER, 0, 54);
    }
}

/* Avanza la FSM A→B→C→A cada disparo del timer. */
static void ml_tick(lv_timer_t* /*t*/) {
    s_ml_state = (s_ml_state + 1) % 3;
    ml_build_state(s_ml_state);
}

void view_23_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "MIDI LEARN", GF_COLOR_TEAL);

    /* Indicador REC — dot rojo pulsante + label. */
    lv_obj_t* rec = gf_dot(parent, 8, GF_COLOR_RED);
    lv_obj_align(rec, LV_ALIGN_CENTER, -16, -62);
    lv_obj_t* rl = gf_label(parent, "REC", GF_FONT_MICRO, GF_COLOR_RED);
    lv_obj_align(rl, LV_ALIGN_CENTER, 8, -62);

    lv_anim_t pa;
    lv_anim_init(&pa);
    lv_anim_set_var(&pa, rec);
    lv_anim_set_exec_cb(&pa, opa_cb);
    lv_anim_set_values(&pa, 90, 255);
    lv_anim_set_time(&pa, 400);
    lv_anim_set_playback_time(&pa, 400);
    lv_anim_set_repeat_count(&pa, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&pa);

    /* Contenedor de contenido — se reconstruye en cada estado de la FSM. */
    s_ml_content = lv_obj_create(parent);
    lv_obj_set_size(s_ml_content, 220, 132);
    lv_obj_set_style_bg_opa(s_ml_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_ml_content, 0, 0);
    lv_obj_set_style_pad_all(s_ml_content, 0, 0);
    lv_obj_clear_flag(s_ml_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_ml_content, LV_ALIGN_CENTER, 0, 18);

    s_ml_state = 0;
    ml_build_state(0);

    /* Timer de la FSM — cicla los 3 estados dentro del slot de 5s. */
    lv_timer_t* t = lv_timer_create(ml_tick, 1600, nullptr);
    gf_anim_register_timer(t);
}

void view_23_destroy(void) {
    s_ml_content = nullptr;
}
