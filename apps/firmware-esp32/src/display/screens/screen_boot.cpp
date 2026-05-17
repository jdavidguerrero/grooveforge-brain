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
#include <Arduino.h>

/* ── Duraciones de la secuencia (ms) ─────────────────────────────────────── */

static constexpr uint32_t ARC_ANIM_DURATION_MS    = 800;
static constexpr uint32_t TITLE_FADE_DELAY_MS      = 600;
static constexpr uint32_t TITLE_FADE_DURATION_MS   = 400;
static constexpr uint32_t SUBTITLE_FADE_DELAY_MS   = 1000;
static constexpr uint32_t SUBTITLE_FADE_DURATION_MS = 400;
static constexpr uint32_t DOT_FADE_DELAY_MS        = 1600;
static constexpr uint32_t DOT_FADE_DURATION_MS     = 300;
static constexpr uint32_t BOOT_TOTAL_MS            = 2200;

/*
 * Animacion del arc — sweep 360 grados desde 12 o'clock clockwise.
 *
 * Enfoque correcto en LVGL 8: usar lv_arc_set_value() con rango [0, 360]
 * en lugar de lv_arc_set_end_angle() con angulos > 360.
 * lv_arc_set_end_angle() con valores > 360 puede crashear en la draw pipeline
 * de LVGL 8 (el draw_arc espera angulos en [0, 360] y los pasa directo a
 * _lv_trigo_sin() que tiene tabla solo para [-360, 360]).
 *
 * Con lv_arc_set_rotation(arc, 270): el range [0,360] empieza en 12 o'clock.
 * lv_arc_set_value(arc, 0)   → arc vacio.
 * lv_arc_set_value(arc, 360) → arc completo (circulo entero).
 */

/**
 * exec_cb para animar la opacidad de un objeto (fade-in).
 * v va de LV_OPA_TRANSP (0) a LV_OPA_COVER (255).
 */
static void opa_anim_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

/**
 * exec_cb para animar el valor del arc (0=vacio, 360=completo).
 * Cast explicito a lv_arc_set_value signature: (lv_obj_t*, int32_t).
 */
static void arc_value_anim_cb(void* obj, int32_t v) {
    lv_arc_set_value(static_cast<lv_obj_t*>(obj), v);
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
    Serial.println("[boot] screen_boot_create — lv_scr_act()");
    lv_obj_t* scr = lv_scr_act();
    Serial.printf("[boot] scr ptr = %p\n", scr);

    /* Fondo near-black — sin esto el fondo es blanco por defecto del theme */
    lv_obj_set_style_bg_color(scr, BRAIN_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* ── Arc exterior: ring que se "llena" durante el boot ─────────────── */

    lv_obj_t* arc = lv_arc_create(scr);
    lv_obj_set_size(arc, BRAIN_RING_OBJ_SIZE, BRAIN_RING_OBJ_SIZE);
    lv_obj_center(arc);

    /* Rango 0-360: valor 0 = arc vacio, valor 360 = circulo completo.
     * rotation=270 hace que el range empiece en 12 o'clock (arriba). */
    lv_arc_set_range(arc, 0, 360);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_value(arc, 0);          /* empieza vacio */

    /* bg_angles: pista de fondo, circulo completo visible */
    lv_arc_set_bg_angles(arc, 0, 360);

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

    /* Animacion del arc: valor 0 → 360 en 800ms con ease_out.
     * lv_arc_set_value() maneja el mapping [0,360] → angulos internamente,
     * sin riesgo de angulos > 360 que causarian UB en _lv_trigo_sin(). */
    lv_anim_t a_arc;
    lv_anim_init(&a_arc);
    lv_anim_set_var(&a_arc, arc);
    lv_anim_set_exec_cb(&a_arc, arc_value_anim_cb);
    lv_anim_set_values(&a_arc, 0, 360);
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
