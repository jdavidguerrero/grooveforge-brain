/**
 * @file screen_boot.cpp
 * @brief Pantalla de arranque animada — GrooveForge Brain (mock GFB-UI-001)
 *
 * Teoria — animaciones LVGL 8:
 *
 * lv_anim_t es un interpolador de entero que llama a un exec_cb() con el valor
 * interpolado entre start_value y end_value durante `time` ms. El path de
 * easing (ease_out, linear...) define la curva. lv_anim_set_delay() encadena
 * animaciones sin un state machine manual.
 *
 * Nota sobre arc en LVGL 8: usar lv_arc_set_value() con rango [0,360] en vez de
 * lv_arc_set_end_angle() con angulos > 360 (que rompe el draw pipeline).
 *
 * Esta pantalla corre una vez al arranque; tambien la reusa la vista 01 del
 * carrusel via screen_boot_create().
 */

#include "screen_boot.h"
#include "../ui_theme.h"
#include <lvgl.h>

/* ── Duraciones de la secuencia (ms) ─────────────────────────────────────── */

static constexpr uint32_t ARC_ANIM_DURATION_MS      = 1000;
static constexpr uint32_t TITLE_FADE_DELAY_MS       = 500;
static constexpr uint32_t TITLE_FADE_DURATION_MS    = 400;
static constexpr uint32_t SUBTITLE_FADE_DELAY_MS    = 900;
static constexpr uint32_t SUBTITLE_FADE_DURATION_MS = 400;
static constexpr uint32_t VERSION_FADE_DELAY_MS     = 1200;
static constexpr uint32_t VERSION_FADE_DURATION_MS  = 300;
static constexpr uint32_t BOOT_TOTAL_MS             = 2200;

/* Ring de carga fino — mock 01: 2px stroke, Ø 220 (objeto = diametro). */
static constexpr lv_coord_t BOOT_RING_SIZE  = 220;
static constexpr lv_coord_t BOOT_RING_WIDTH = 3;

/* ── Callbacks de animacion ──────────────────────────────────────────────── */

static void opa_anim_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

static void arc_value_anim_cb(void* obj, int32_t v) {
    lv_arc_set_value(static_cast<lv_obj_t*>(obj), v);
}

static void start_fade_in(lv_obj_t* obj, uint32_t delay, uint32_t duration) {
    lv_obj_set_style_opa(obj, LV_OPA_TRANSP, 0);   /* empieza invisible */

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, opa_anim_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_time(&a, duration);
    lv_anim_set_path_cb(&a, lv_anim_path_linear);
    lv_anim_start(&a);
}

/* ── Implementacion publica ──────────────────────────────────────────────── */

void screen_boot_create(void) {
    lv_obj_t* scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, GF_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    /* ── Loading ring: arc fino teal que se llena 0→360° ───────────────── */

    lv_obj_t* arc = lv_arc_create(scr);
    lv_obj_set_size(arc, BOOT_RING_SIZE, BOOT_RING_SIZE);
    lv_obj_center(arc);
    lv_arc_set_range(arc, 0, 360);
    lv_arc_set_rotation(arc, 270);          /* el rango empieza en 12 o'clock */
    lv_arc_set_value(arc, 0);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL,     LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, BOOT_RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, BOOT_RING_WIDTH, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    lv_anim_t a_arc;
    lv_anim_init(&a_arc);
    lv_anim_set_var(&a_arc, arc);
    lv_anim_set_exec_cb(&a_arc, arc_value_anim_cb);
    lv_anim_set_values(&a_arc, 0, 360);
    lv_anim_set_time(&a_arc, ARC_ANIM_DURATION_MS);
    lv_anim_set_path_cb(&a_arc, lv_anim_path_ease_out);
    lv_anim_start(&a_arc);

    /* ── Wordmark "GROOVEFORGE" — teal+ ─────────────────────────────────── */

    lv_obj_t* lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "GROOVEFORGE");
    lv_obj_set_style_text_color(lbl_title, GF_COLOR_TEAL_PLUS, 0);
    lv_obj_set_style_text_font(lbl_title, GF_FONT_TITLE, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, -10);
    start_fade_in(lbl_title, TITLE_FADE_DELAY_MS, TITLE_FADE_DURATION_MS);

    /* ── Subtitulo "BRAIN" — teal ───────────────────────────────────────── */

    lv_obj_t* lbl_sub = lv_label_create(scr);
    lv_label_set_text(lbl_sub, "BRAIN");
    lv_obj_set_style_text_color(lbl_sub, GF_COLOR_TEAL, 0);
    lv_obj_set_style_text_font(lbl_sub, GF_FONT_BODY, 0);
    lv_obj_align(lbl_sub, LV_ALIGN_CENTER, 0, 18);
    start_fade_in(lbl_sub, SUBTITLE_FADE_DELAY_MS, SUBTITLE_FADE_DURATION_MS);

    /* ── Version tag — gris, abajo ──────────────────────────────────────── */

    lv_obj_t* lbl_ver = lv_label_create(scr);
    lv_label_set_text(lbl_ver, "v1.0");
    lv_obj_set_style_text_color(lbl_ver, GF_COLOR_GRAY, 0);
    lv_obj_set_style_text_font(lbl_ver, GF_FONT_MICRO, 0);
    lv_obj_align(lbl_ver, LV_ALIGN_CENTER, 0, 64);
    start_fade_in(lbl_ver, VERSION_FADE_DELAY_MS, VERSION_FADE_DURATION_MS);
}

bool screen_boot_done(void) {
    /* lv_tick_get() retorna ms desde lv_init() — determinista. */
    return lv_tick_get() >= BOOT_TOTAL_MS;
}
