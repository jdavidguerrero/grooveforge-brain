/**
 * @file view_19_daw_connected.cpp
 * @brief Vista 19 — DAW CONNECTED (mock GFB-UI-019)
 *
 * Nivel 4 (DAW/VST3). Contexto de sesion: BPM, tonalidad/seccion, mix score
 * con banda de conflicto, link dot y pill MIX-AWARE.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

void view_19_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "DAW LINK", GF_COLOR_PURPLE_BRIGHT);

    /* A. BPM hero. */
    gf_hero_label(parent, "128", GF_FONT_HERO, GF_COLOR_WHITE, -34);
    gf_hero_label(parent, "BPM", GF_FONT_MICRO, GF_COLOR_GRAY, -6);

    /* B. Tonalidad / seccion. */
    gf_hero_label(parent, "F MIN \xC2\xB7 CHORUS", GF_FONT_LABEL, GF_COLOR_WHITE, 20);

    /* C. Mix score — barra purpura. */
    lv_obj_t* mix = gf_bar(parent, 130, 5, 82, GF_COLOR_PURPLE_BRIGHT);
    lv_obj_align(mix, LV_ALIGN_CENTER, 0, 42);
    /* D. Banda de conflicto — segmento rojo sobre la barra. */
    lv_obj_t* conf = lv_obj_create(parent);
    lv_obj_set_size(conf, 20, 5);
    lv_obj_set_style_bg_color(conf, GF_COLOR_RED, 0);
    lv_obj_set_style_bg_opa(conf, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(conf, 0, 0);
    lv_obj_set_style_radius(conf, 0, 0);
    lv_obj_clear_flag(conf, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(conf, LV_ALIGN_CENTER, 40, 42);

    lv_obj_t* ml = gf_label(parent, "MIX 82", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_align(ml, LV_ALIGN_CENTER, 0, 58);

    /* E. Link dot — esquina superior derecha. */
    lv_obj_t* link = gf_dot(parent, 6, GF_COLOR_PURPLE_BRIGHT);
    lv_obj_align(link, LV_ALIGN_TOP_RIGHT, -40, 34);

    /* F. Pill MIX-AWARE. */
    gf_mode_pill(parent, "MIX-AWARE", GF_COLOR_PURPLE_BRIGHT);
}

void view_19_destroy(void) {}
