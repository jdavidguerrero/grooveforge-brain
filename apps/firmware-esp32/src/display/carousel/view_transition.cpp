/**
 * @file view_transition.cpp
 * @brief Implementacion del radial-wipe — GrooveForge Brain
 */

#include "view_transition.h"
#include "../ui_theme.h"

/* Diagonal del display 240x240 ≈ 339; 340 garantiza cobertura total. */
static constexpr int32_t GF_WIPE_MAX = 340;

static lv_obj_t*          s_wipe = nullptr;
static gf_transition_cb_t s_pending_cb = nullptr;

/* exec_cb de la animacion: ajusta tamano + radio del circulo y lo recentra
 * (lv_obj_set_size fija la esquina sup-izq, hay que recentrar al crecer). */
static void wipe_size_cb(void* obj, int32_t v) {
    lv_obj_t* o = static_cast<lv_obj_t*>(obj);
    lv_obj_set_size(o, v, v);
    lv_obj_set_style_radius(o, v / 2, 0);
    lv_obj_center(o);
}

/*
 * El callback del usuario (que destruye vistas, llama lv_anim_del, lv_obj_clean)
 * se ejecuta en un lv_timer one-shot, NO directo en el ready_cb de la animacion.
 * Asi corre en el contexto limpio de lv_timer_handler, sin reentrancia con el
 * procesamiento de animaciones en curso.
 */
static void deferred_run_cb(lv_timer_t* /*t*/) {
    gf_transition_cb_t cb = s_pending_cb;
    s_pending_cb = nullptr;
    if (cb != nullptr) cb();
}

static void defer_pending(void) {
    lv_timer_t* t = lv_timer_create(deferred_run_cb, 1, nullptr);
    lv_timer_set_repeat_count(t, 1);   /* one-shot: corre una vez y se autodestruye */
}

static void out_ready_cb(lv_anim_t* /*a*/) {
    defer_pending();
}

static void in_ready_cb(lv_anim_t* /*a*/) {
    lv_obj_add_flag(s_wipe, LV_OBJ_FLAG_HIDDEN);   /* ocultar entre transiciones */
    defer_pending();
}

void gf_transition_init(void) {
    if (s_wipe != nullptr) return;

    /* El wipe vive en el top layer → no lo destruye lv_obj_clean(lv_scr_act()). */
    s_wipe = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_wipe, 0, 0);
    lv_obj_center(s_wipe);
    lv_obj_set_style_border_width(s_wipe, 0, 0);
    lv_obj_set_style_pad_all(s_wipe, 0, 0);
    lv_obj_set_style_bg_opa(s_wipe, LV_OPA_COVER, 0);
    /* LVGL 8.3 no tiene gradiente radial; un gradiente vertical purple→teal
     * aproxima el "radial grad #534AB7→#1D9E75" del mock 09. */
    lv_obj_set_style_bg_color(s_wipe, GF_COLOR_PURPLE, 0);
    lv_obj_set_style_bg_grad_color(s_wipe, GF_COLOR_TEAL, 0);
    lv_obj_set_style_bg_grad_dir(s_wipe, LV_GRAD_DIR_VER, 0);
    lv_obj_clear_flag(s_wipe, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(s_wipe, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_wipe, LV_OBJ_FLAG_HIDDEN);
}

void gf_transition_out(gf_transition_cb_t on_done) {
    s_pending_cb = on_done;
    lv_obj_clear_flag(s_wipe, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_wipe);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_wipe);
    lv_anim_set_exec_cb(&a, wipe_size_cb);
    lv_anim_set_values(&a, 0, GF_WIPE_MAX);
    lv_anim_set_time(&a, GF_TRANSITION_HALF_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&a, out_ready_cb);
    lv_anim_start(&a);
}

void gf_transition_in(gf_transition_cb_t on_done) {
    s_pending_cb = on_done;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_wipe);
    lv_anim_set_exec_cb(&a, wipe_size_cb);
    lv_anim_set_values(&a, GF_WIPE_MAX, 0);
    lv_anim_set_time(&a, GF_TRANSITION_HALF_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a, in_ready_cb);
    lv_anim_start(&a);
}
