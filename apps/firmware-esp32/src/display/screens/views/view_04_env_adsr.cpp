/**
 * @file view_04_env_adsr.cpp
 * @brief Vista 04 — ENV · ADSR (Sprint 31 Batch D — dinamico)
 *
 * Curva ADSR dibujada en canvas: 4 segmentos (Attack/Decay/Sustain/Release).
 * El segmento activo (bridge_get_synth_param()) se resalta brillante y mas
 * grueso; los demas van atenuados. Vertices marcados con dots.
 *
 * Estrategia: el canvas se crea una vez. draw_adsr() limpia y redibuja el
 * canvas completo cada vez que cambia el segmento activo. El redibujado solo
 * ocurre si el segmento cambio — la funcion es idempotente cuando no hay cambio.
 *
 * Timer a 100ms: lee bridge_get_synth_param() y llama draw_adsr() si diffiere
 * de s_last_seg.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../../bridge/bridge_handlers.h"

/* ── Estado persistente ───────────────────────────────────────────────────── */

static lv_obj_t*   s_canvas     = nullptr;
static lv_obj_t*   s_seg_lbl[4] = {};
static lv_timer_t* s_timer      = nullptr;
static uint8_t     s_last_seg   = 0xFF;

/* ── Puntos fijos de la envolvente en el canvas 180×80 ─────────────────────── */
/* Definidos como constexpr para que el compilador los inlinee.
 * Los valores x/y reflejan una forma ADSR convencional que llena el canvas. */

static const lv_point_t P0 = {  0, 78 };   ///< inicio (silencio)
static const lv_point_t PA = { 44,  2 };   ///< peak de ataque
static const lv_point_t PD = { 86, 38 };   ///< fin del decay → nivel sustain
static const lv_point_t PS = {132, 38 };   ///< fin del sustain → inicio release
static const lv_point_t PR = {179, 78 };   ///< fin del release (silencio)

/* ── Redibujado del canvas ───────────────────────────────────────────────── */

static void draw_adsr(uint8_t seg) {
    if (!s_canvas) return;

    lv_canvas_fill_bg(s_canvas, lv_color_black(), LV_OPA_TRANSP);

    /* Descriptores de linea: dim para inactivos, bright para el segmento activo. */
    lv_draw_line_dsc_t dim, bright;
    lv_draw_line_dsc_init(&dim);
    dim.color       = GF_COLOR_TEAL;
    dim.width       = 2;
    dim.opa         = 115;   /* ~45% — segmentos inactivos */
    dim.round_start = dim.round_end = 1;

    lv_draw_line_dsc_init(&bright);
    bright.color       = GF_COLOR_TEAL_PLUS;
    bright.width       = 3;
    bright.opa         = LV_OPA_COVER;
    bright.round_start = bright.round_end = 1;

    /* Los 4 segmentos en orden A→D→S→R. */
    const lv_point_t segs[4][2] = {
        { P0, PA },
        { PA, PD },
        { PD, PS },
        { PS, PR },
    };

    for (uint8_t i = 0; i < 4; i++) {
        /* lv_canvas_draw_line espera un arreglo no-const — copia local. */
        lv_point_t pair[2] = { segs[i][0], segs[i][1] };
        lv_canvas_draw_line(s_canvas, pair, 2, (i == seg) ? &bright : &dim);
    }

    /* Vertices de transicion (pA, pD, pS) — siempre teal_plus. */
    lv_draw_rect_dsc_t vd;
    lv_draw_rect_dsc_init(&vd);
    vd.bg_color = GF_COLOR_TEAL_PLUS;
    vd.bg_opa   = LV_OPA_COVER;
    vd.radius   = LV_RADIUS_CIRCLE;
    const lv_point_t verts[3] = { PA, PD, PS };
    for (uint8_t i = 0; i < 3; i++) {
        lv_canvas_draw_rect(s_canvas, verts[i].x - 2, verts[i].y - 2, 4, 4, &vd);
    }

    /* Actualiza color de labels de segmento. */
    for (uint8_t i = 0; i < 4; i++) {
        if (s_seg_lbl[i]) {
            lv_obj_set_style_text_color(s_seg_lbl[i],
                (i == seg) ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
        }
    }
}

/* ── Timer callback ───────────────────────────────────────────────────────── */

static void env_timer_cb(lv_timer_t* /*t*/) {
    uint8_t seg = bridge_get_synth_param();
    if (seg > 3) seg = 0;
    if (seg == s_last_seg) return;
    s_last_seg = seg;
    draw_adsr(seg);
}

/* ── Creacion ─────────────────────────────────────────────────────────────── */

void view_04_create(lv_obj_t* parent) {
    s_canvas   = nullptr;
    s_timer    = nullptr;
    s_last_seg = 0xFF;
    for (uint8_t i = 0; i < 4; i++) s_seg_lbl[i] = nullptr;

    gf_screen_bg(parent);
    gf_arc_title(parent, "ENV ADSR", GF_COLOR_TEAL);

    /* Canvas de la curva ADSR — posicionado ligeramente arriba del centro. */
    s_canvas = gf_canvas_create(parent, 180, 80);
    if (s_canvas) lv_obj_align(s_canvas, LV_ALIGN_CENTER, 0, 2);

    /* Labels de segmento debajo del canvas.
     * x relativo al centro del canvas: x_canvas=-90, puntos medios de cada segmento. */
    static const char* const SEG_NAME[4] = { "A", "D", "S", "R" };
    /* x offset absoluto respecto al centro de pantalla — ajustado a los midpoints del canvas */
    static const lv_coord_t SEG_X[4] = { -68, -24, 22, 70 };

    uint8_t init_seg = bridge_get_synth_param();
    if (init_seg > 3) init_seg = 0;

    for (uint8_t i = 0; i < 4; i++) {
        lv_color_t c = (i == init_seg) ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY;
        lv_obj_t* l = gf_label(parent, SEG_NAME[i], GF_FONT_MICRO, c);
        if (l) lv_obj_align(l, LV_ALIGN_CENTER, SEG_X[i], 56);
        s_seg_lbl[i] = l;
    }

    /* Dibuja el estado inicial con el segmento activo segun el bridge. */
    s_last_seg = init_seg;
    draw_adsr(init_seg);

    gf_page_dots(parent, 5, 2, GF_COLOR_TEAL);
    gf_mode_pill(parent, "SYNTH", GF_COLOR_TEAL);

    s_timer = lv_timer_create(env_timer_cb, 100, nullptr);
}

void view_04_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    s_canvas   = nullptr;
    s_last_seg = 0xFF;
    for (uint8_t i = 0; i < 4; i++) s_seg_lbl[i] = nullptr;
}
