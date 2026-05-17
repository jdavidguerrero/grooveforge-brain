/**
 * @file screen_boot.cpp
 * @brief Pantalla de arranque animada — GrooveForge Brain
 *
 * Teoria — animaciones LVGL 8:
 *
 * lv_anim_t es un interpolador de entero que llama a un exec_cb() con el
 * valor interpolado entre start_value y end_value durante `time` ms.
 * El path (lv_anim_path_ease_out, lv_anim_path_linear, etc.) define la
 * curva de easing — ease_out da sensacion "inercial" al arc sweep.
 *
 * lv_anim_set_delay() permite encadenar animaciones sin callbacks manuales:
 * cada elemento aparece en su momento sin necesidad de un state machine.
 *
 * Nota sobre arc angles en LVGL 8:
 *   - 0° = derecha (3 o'clock), crece clockwise
 *   - 270° = arriba (12 o'clock)
 *   - Para sweep clockwise desde arriba: start=270, animar end de 270 a 270+360=630
 *   - LVGL normaliza automaticamente angulos > 360
 */

#include "screen_boot.h"
#include "../ui_theme.h"
#include <lvgl.h>

/* ── Duraciones de la secuencia (ms) ─────────────────────────────────────── */

static constexpr uint32_t ARC_ANIM_DURATION_MS    = 800;
static constexpr uint32_t TITLE_FADE_DELAY_MS      = 600;
static constexpr uint32_t TITLE_FADE_DURATION_MS   = 400;
static constexpr uint32_t SUBTITLE_FADE_DELAY_MS   = 1000;
static constexpr uint32_t SUBTITLE_FADE_DURATION_MS = 400;
static constexpr uint32_t DOT_FADE_DELAY_MS        = 1600;
static constexpr uint32_t DOT_FADE_DURATION_MS     = 300;
static constexpr uint32_t BOOT_TOTAL_MS            = 2200;

/* Angulos del arc — 270 = 12 o'clock, sweep clockwise 360 grados */
static constexpr int32_t ARC_START_ANGLE = 270;
static constexpr int32_t ARC_END_START   = 270;     /* inicio animacion: arc vacio */
static constexpr int32_t ARC_END_TARGET  = 270 + 360; /* fin animacion: arc completo */

/* ── Callbacks de animacion ──────────────────────────────────────────────── */

/**
 * exec_cb para animar el angulo final del arc.
 * LVGL llama esta funcion con v interpolado entre ARC_END_START y ARC_END_TARGET.
 */
static void arc_angle_anim_cb(void* obj, int32_t v) {
    lv_arc_set_end_angle(static_cast<lv_obj_t*>(obj), static_cast<uint16_t>(v));
}

/**
 * exec_cb para animar la opacidad de un objeto (fade-in).
 * v va de LV_OPA_TRANSP (0) a LV_OPA_COVER (255).
 */
static void opa_anim_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

/* ── Helpers de animacion ────────────────────────────────────────────────── */

/**
 * @brief Configura y lanza un fade-in sobre un objeto LVGL.
 *
 * @param obj     Objeto a animar
 * @param delay   Retardo antes de iniciar (ms)
 * @param duration Duracion del fade (ms)
 */
static void start_fade_in(lv_obj_t* obj, uint32_t delay, uint32_t duration) {
    lv_obj_set_style_opa(obj, LV_OPA_TRANSP, 0); /* empieza invisible */

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

    /* Fondo near-black — sin esto el fondo es blanco por defecto del theme */
    lv_obj_set_style_bg_color(scr, BRAIN_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ── Arc exterior: ring que se "llena" durante el boot ─────────────── */

    lv_obj_t* arc = lv_arc_create(scr);
    lv_obj_set_size(arc, BRAIN_RING_OBJ_SIZE, BRAIN_RING_OBJ_SIZE);
    lv_obj_center(arc);

    /* bg_angles: circulo completo visible como "pista" gris oscura */
    lv_arc_set_bg_angles(arc, 0, 360);

    /* arc activo: empieza desde 270 (12 o'clock), end = start = vacio */
    lv_arc_set_rotation(arc, 0);
    lv_arc_set_angles(arc, ARC_START_ANGLE, ARC_START_ANGLE);

    /* Colores: pista = gris oscuro, indicador (parte activa) = purple */
    lv_obj_set_style_arc_color(arc, BRAIN_COLOR_GRAY_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, BRAIN_COLOR_PURPLE,   LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, BRAIN_RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, BRAIN_RING_WIDTH, LV_PART_INDICATOR);

    /* Eliminar el knob (boton en el extremo del arc) — solo queremos el stroke */
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    lv_obj_set_style_size(arc, 0, LV_PART_KNOB);

    /* Deshabilitar interaccion — es decorativo, no interactivo */
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    /* Animacion del arc: sweep de 270 a 630 (270+360) en 800ms con ease_out */
    lv_anim_t a_arc;
    lv_anim_init(&a_arc);
    lv_anim_set_var(&a_arc, arc);
    lv_anim_set_exec_cb(&a_arc, arc_angle_anim_cb);
    lv_anim_set_values(&a_arc, ARC_END_START, ARC_END_TARGET);
    lv_anim_set_time(&a_arc, ARC_ANIM_DURATION_MS);
    lv_anim_set_path_cb(&a_arc, lv_anim_path_ease_out);
    lv_anim_start(&a_arc);

    /* ── Label "GROOVEFORGE" — fade-in a 600ms ──────────────────────────── */

    lv_obj_t* lbl_title = lv_label_create(scr);
    lv_label_set_text(lbl_title, "GROOVEFORGE");
    lv_obj_set_style_text_color(lbl_title, BRAIN_COLOR_WHITE, 0);
    lv_obj_set_style_text_font(lbl_title, BRAIN_FONT_MD, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 0, -15);
    start_fade_in(lbl_title, TITLE_FADE_DELAY_MS, TITLE_FADE_DURATION_MS);

    /* ── Label "BRAIN" — fade-in a 1000ms ───────────────────────────────── */

    lv_obj_t* lbl_sub = lv_label_create(scr);
    lv_label_set_text(lbl_sub, "BRAIN");
    lv_obj_set_style_text_color(lbl_sub, BRAIN_COLOR_GRAY, 0);
    lv_obj_set_style_text_font(lbl_sub, BRAIN_FONT_SM, 0);
    lv_obj_align(lbl_sub, LV_ALIGN_CENTER, 0, +10);
    start_fade_in(lbl_sub, SUBTITLE_FADE_DELAY_MS, SUBTITLE_FADE_DURATION_MS);

    /* ── Label "●" teal — ready indicator a 1600ms ──────────────────────── */

    lv_obj_t* lbl_dot = lv_label_create(scr);
    lv_label_set_text(lbl_dot, "\xE2\x97\x8F"); /* UTF-8: U+25CF BLACK CIRCLE */
    lv_obj_set_style_text_color(lbl_dot, BRAIN_COLOR_TEAL, 0);
    lv_obj_set_style_text_font(lbl_dot, BRAIN_FONT_SM, 0);
    lv_obj_align(lbl_dot, LV_ALIGN_CENTER, 0, +35);
    start_fade_in(lbl_dot, DOT_FADE_DELAY_MS, DOT_FADE_DURATION_MS);
}

bool screen_boot_done(void) {
    /* lv_tick_get() retorna ms desde lv_init() — determinista independiente del loop */
    return lv_tick_get() >= BOOT_TOTAL_MS;
}
