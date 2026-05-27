/**
 * @file gf_param_slider.cpp
 * @brief Implementación del slider horizontal hero+snap.
 *
 * Modelo: N páginas hijo del contenedor, cada una de 240px ancho. Solo una
 * está visible (x=0); las demás están offscreen a izquierda o derecha. Al
 * cambiar página, las animaciones de x se disparan para todas las páginas
 * que cambien de posición (300ms ease-out).
 *
 * Estado interno: guardado en lv_obj_set_user_data(slider, *state). El state
 * se libera en el event LV_EVENT_DELETE para evitar fugas.
 */

#include "gf_param_slider.h"
#include "../ui_theme.h"
#include "gf_anim.h"

#define PSLIDER_W           240
#define PSLIDER_H           210     // 240 menos 30 para los dots
#define PSLIDER_ANIM_MS     300

typedef struct {
    uint8_t num_pages;
    uint8_t active;
} pslider_state_t;

/* Anim exec callback: set x del objeto sin path overrides. */
static void anim_set_x(void* obj, int32_t v) {
    lv_obj_set_x((lv_obj_t*)obj, (lv_coord_t)v);
}

/* Libera el state al destruir el slider (event handler). */
static void pslider_delete_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    pslider_state_t* st = (pslider_state_t*)lv_obj_get_user_data(slider);
    if (st) {
        lv_mem_free(st);
        lv_obj_set_user_data(slider, nullptr);
    }
}

lv_obj_t* gf_pslider_create(lv_obj_t* parent, uint8_t num_pages) {
    if (num_pages == 0) num_pages = 1;
    if (num_pages > 8) num_pages = 8;

    lv_obj_t* slider = lv_obj_create(parent);
    lv_obj_set_size(slider, PSLIDER_W, PSLIDER_H);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 15);   // 15px desde top → centra vertical
    lv_obj_set_style_bg_opa(slider, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(slider, 0, 0);
    lv_obj_set_style_pad_all(slider, 0, 0);
    lv_obj_clear_flag(slider, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(slider, false, 0);

    pslider_state_t* st = (pslider_state_t*)lv_mem_alloc(sizeof(pslider_state_t));
    st->num_pages = num_pages;
    st->active    = 0;
    lv_obj_set_user_data(slider, st);
    lv_obj_add_event_cb(slider, pslider_delete_cb, LV_EVENT_DELETE, nullptr);

    /* Crear las N páginas — todas inactivas (offscreen derecha) excepto la 0. */
    for (uint8_t i = 0; i < num_pages; i++) {
        lv_obj_t* page = lv_obj_create(slider);
        lv_obj_set_size(page, PSLIDER_W, PSLIDER_H);
        lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_set_style_pad_all(page, 0, 0);
        lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_x(page, (i == 0) ? 0 : PSLIDER_W);
        lv_obj_set_y(page, 0);
    }

    return slider;
}

lv_obj_t* gf_pslider_get_page(lv_obj_t* slider, uint8_t idx) {
    if (!slider) return nullptr;
    pslider_state_t* st = (pslider_state_t*)lv_obj_get_user_data(slider);
    if (!st || idx >= st->num_pages) return nullptr;
    return lv_obj_get_child(slider, idx);
}

void gf_pslider_set_active(lv_obj_t* slider, uint8_t idx, bool animate) {
    if (!slider) return;
    pslider_state_t* st = (pslider_state_t*)lv_obj_get_user_data(slider);
    if (!st || idx >= st->num_pages || idx == st->active) return;

    int8_t   direction = (idx > st->active) ? +1 : -1;  // +1=avanzar (entra desde derecha)
    uint8_t  prev      = st->active;
    st->active         = idx;

    /* Cada página tiene 3 estados posibles: visible (x=0), offscreen izquierda
     * (x=-W), offscreen derecha (x=+W). Anima de su x actual al target. */
    for (uint8_t i = 0; i < st->num_pages; i++) {
        lv_obj_t* page = lv_obj_get_child(slider, i);
        if (!page) continue;

        lv_coord_t target_x;
        if (i == idx)        target_x = 0;
        else if (i == prev)  target_x = (lv_coord_t)(-direction * PSLIDER_W);  // se va al lado opuesto
        else                 target_x = (lv_coord_t)(((int32_t)i < (int32_t)idx) ? -PSLIDER_W : PSLIDER_W);

        if (animate) {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, page);
            lv_anim_set_values(&a, lv_obj_get_x(page), target_x);
            lv_anim_set_time(&a, PSLIDER_ANIM_MS);
            lv_anim_set_exec_cb(&a, anim_set_x);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        } else {
            lv_obj_set_x(page, target_x);
        }
    }
}

uint8_t gf_pslider_get_active(lv_obj_t* slider) {
    if (!slider) return 0;
    pslider_state_t* st = (pslider_state_t*)lv_obj_get_user_data(slider);
    return st ? st->active : 0;
}

uint8_t gf_pslider_get_count(lv_obj_t* slider) {
    if (!slider) return 0;
    pslider_state_t* st = (pslider_state_t*)lv_obj_get_user_data(slider);
    return st ? st->num_pages : 0;
}
