/**
 * @file view_18_ai_suggestion.cpp
 * @brief Vista 18 — AI SUGGESTION (mock GFB-UI-018)
 *
 * Nivel 3 (Cloud AI). Caja de sugerencia con borde teal y fill tenue (fade-in
 * al entrar), texto hero + sub, fuente de la sugerencia, prompts APPLY/DISMISS.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

static void opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

void view_18_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "AI SUGGESTION", GF_COLOR_TEAL);

    /* B. Caja de sugerencia. */
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_set_size(box, 178, 92);
    lv_obj_set_style_radius(box, 8, 0);
    lv_obj_set_style_bg_color(box, GF_COLOR_TEAL_PLUS, 0);
    lv_obj_set_style_bg_opa(box, 16, 0);              /* ~6% fill */
    lv_obj_set_style_border_color(box, GF_COLOR_TEAL_PLUS, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_pad_all(box, 10, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, -6);

    /* Fade-in de la caja al entrar (240ms, one-shot). */
    lv_obj_set_style_opa(box, LV_OPA_TRANSP, 0);
    lv_anim_t fa;
    lv_anim_init(&fa);
    lv_anim_set_var(&fa, box);
    lv_anim_set_exec_cb(&fa, opa_cb);
    lv_anim_set_values(&fa, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&fa, 240);
    lv_anim_start(&fa);

    /* C. Texto de la sugerencia dentro de la caja. */
    lv_obj_t* hero = gf_label(box, "ADD TAPE\nSATURATE", GF_FONT_LABEL, GF_COLOR_WHITE);
    lv_obj_set_style_text_align(hero, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hero, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_t* sub = gf_label(box, "warmer low end", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* D. Fuente de la sugerencia. */
    lv_obj_t* src = gf_label(parent, "FROM GROOVEPILOT", GF_FONT_MICRO, GF_COLOR_TEAL);
    lv_obj_set_style_opa(src, 165, 0);
    lv_obj_align(src, LV_ALIGN_CENTER, 0, 48);

    /* E. Prompt APPLY (dot teal+ + label). */
    lv_obj_t* ad = gf_dot(parent, 6, GF_COLOR_TEAL_PLUS);
    lv_obj_align(ad, LV_ALIGN_CENTER, -54, 70);
    lv_obj_t* al = gf_label(parent, "APPLY", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS);
    lv_obj_align(al, LV_ALIGN_CENTER, -26, 70);

    /* F. Prompt DISMISS. */
    lv_obj_t* dl = gf_label(parent, "x DISMISS", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_align(dl, LV_ALIGN_CENTER, 44, 70);
}

void view_18_destroy(void) {}
