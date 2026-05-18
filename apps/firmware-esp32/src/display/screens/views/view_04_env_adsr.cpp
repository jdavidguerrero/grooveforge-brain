/**
 * @file view_04_env_adsr.cpp
 * @brief Vista 04 — ENV · ADSR (mock GFB-UI-004)
 *
 * Curva ADSR dibujada en canvas: 4 segmentos (Attack/Decay/Sustain/Release).
 * El segmento en edicion (Decay) se resalta brillante y mas grueso; los
 * demas van atenuados. Vertices marcados con dots. Labels de segmento abajo.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"

void view_04_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "ENV ADSR", GF_COLOR_TEAL);

    /* A. Label de segmento en edicion. */
    gf_hero_label(parent, "EDIT  DECAY", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS, -52);

    /* Curva ADSR sobre canvas. */
    const lv_coord_t W = 180, H = 80;
    lv_obj_t* cv = gf_canvas_create(parent, W, H);
    if (cv != nullptr) {
        lv_obj_align(cv, LV_ALIGN_CENTER, 0, 2);

        /* Vertices de la envolvente. */
        const lv_point_t p0 = {0, (lv_coord_t)(H - 2)};
        const lv_point_t pA = {44, 2};
        const lv_point_t pD = {86, 38};
        const lv_point_t pS = {132, 38};
        const lv_point_t pR = {(lv_coord_t)(W - 1), (lv_coord_t)(H - 2)};

        lv_draw_line_dsc_t in, act;
        lv_draw_line_dsc_init(&in);
        in.color = GF_COLOR_TEAL; in.width = 2; in.opa = 115;       /* ~0.45 */
        in.round_start = in.round_end = 1;
        lv_draw_line_dsc_init(&act);
        act.color = GF_COLOR_TEAL_PLUS; act.width = 3; act.opa = LV_OPA_COVER;
        act.round_start = act.round_end = 1;

        lv_point_t segA[2] = {p0, pA};
        lv_point_t segD[2] = {pA, pD};
        lv_point_t segS[2] = {pD, pS};
        lv_point_t segR[2] = {pS, pR};
        lv_canvas_draw_line(cv, segA, 2, &in);
        lv_canvas_draw_line(cv, segD, 2, &act);   /* segmento activo */
        lv_canvas_draw_line(cv, segS, 2, &in);
        lv_canvas_draw_line(cv, segR, 2, &in);

        /* Vertices. */
        lv_draw_rect_dsc_t vd;
        lv_draw_rect_dsc_init(&vd);
        vd.bg_color = GF_COLOR_TEAL_PLUS; vd.bg_opa = LV_OPA_COVER;
        vd.radius = LV_RADIUS_CIRCLE;
        const lv_point_t v[3] = {pA, pD, pS};
        for (uint8_t i = 0; i < 3; i++) {
            lv_canvas_draw_rect(cv, v[i].x - 2, v[i].y - 2, 4, 4, &vd);
        }
    }

    /* E. Labels de segmento (ms / nivel). */
    const char*      seg[4]   = {"A 5", "D 80", "S 62", "R 240"};
    const lv_coord_t seg_x[4] = {-66, -22, 24, 70};
    for (uint8_t i = 0; i < 4; i++) {
        const lv_color_t c = (i == 1) ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY;
        lv_obj_t* l = gf_label(parent, seg[i], GF_FONT_MICRO, c);
        lv_obj_align(l, LV_ALIGN_CENTER, seg_x[i], 56);
    }

    gf_page_dots(parent, 5, 2, GF_COLOR_TEAL);
    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);
}

void view_04_destroy(void) {}
