/**
 * @file view_11_idle.cpp
 * @brief Vista 11 — IDLE / STANDBY (mock GFB-UI-011)
 *
 * Estado pasivo: nombre del engine atenuado, dot que respira, y un anillo de
 * ticks que rota lentamente (12s/vuelta). Timer de inactividad muy tenue.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

static void opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

static void arc_rot_cb(void* obj, int32_t v) {
    lv_arc_set_rotation(static_cast<lv_obj_t*>(obj), v);
}

void view_11_create(lv_obj_t* parent) {
    gf_screen_bg(parent);

    /* C. Anillo de ticks que rota 12s/vuelta. El hueco del arco viaja al
     *    rotar → sensacion de giro lento. */
    lv_obj_t* ring = lv_arc_create(parent);
    lv_obj_set_size(ring, 184, 184);
    lv_obj_center(ring);
    lv_arc_set_bg_angles(ring, 0, 320);          /* hueco de 40° */
    lv_arc_set_angles(ring, 0, 320);
    lv_obj_set_style_arc_color(ring, GF_COLOR_TEAL_DIM, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, GF_COLOR_TEAL_DIM, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(ring, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, 2, LV_PART_INDICATOR);
    lv_obj_remove_style(ring, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);

    lv_anim_t ra;
    lv_anim_init(&ra);
    lv_anim_set_var(&ra, ring);
    lv_anim_set_exec_cb(&ra, arc_rot_cb);
    lv_anim_set_values(&ra, 0, 360);
    lv_anim_set_time(&ra, 12000);
    lv_anim_set_repeat_count(&ra, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&ra);

    /* A. Nombre del engine — atenuado (~0.55). */
    lv_obj_t* en = gf_hero_label(parent, "MOOG MODEL D", GF_FONT_LABEL,
                                 GF_COLOR_TEAL, -14);
    lv_obj_set_style_opa(en, 140, 0);

    /* B. Dot que respira. */
    lv_obj_t* dot = gf_dot(parent, 12, GF_COLOR_TEAL);
    lv_obj_align(dot, LV_ALIGN_CENTER, 0, 16);
    lv_anim_t ba;
    lv_anim_init(&ba);
    lv_anim_set_var(&ba, dot);
    lv_anim_set_exec_cb(&ba, opa_cb);
    lv_anim_set_values(&ba, 70, 255);
    lv_anim_set_time(&ba, 700);
    lv_anim_set_playback_time(&ba, 700);
    lv_anim_set_repeat_count(&ba, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&ba);

    /* D. Timer de inactividad — muy tenue (~0.25). */
    lv_obj_t* tm = gf_hero_label(parent, "IDLE  0:42", GF_FONT_MICRO,
                                 GF_COLOR_GRAY, 52);
    lv_obj_set_style_opa(tm, 64, 0);
}

void view_11_destroy(void) {}
