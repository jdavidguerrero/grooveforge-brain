/**
 * @file view_09_mode_switch.cpp
 * @brief Vista 09 — MODE SWITCH (mock GFB-UI-009)
 *
 * Muestra dinámicamente el modo de origen y el modo destino según
 * bridge_get_top_mode(). Dura 1s (timer en bridge_handlers) antes de
 * navegar al modo destino. Gradiente purple→teal + anillo LED.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../widgets/gf_glyphs.h"
#include "../../../bridge/bridge_handlers.h"

void view_09_create(lv_obj_t* parent) {
    uint8_t dest = bridge_get_top_mode();   // modo destino

    /* Gradiente y borde — siempre igual. */
    lv_obj_set_style_bg_color(parent, GF_COLOR_PURPLE, 0);
    lv_obj_set_style_bg_grad_color(parent, GF_COLOR_TEAL, 0);
    lv_obj_set_style_bg_grad_dir(parent, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    gf_arc(parent, 228, 3, 0, 360, 100, GF_COLOR_TEAL_PLUS, GF_COLOR_TEAL_PLUS);
    gf_arc_title(parent, "MODE SWITCH", GF_COLOR_WHITE);

    /* Modo destino arriba (grande, color del modo) y nombre abajo. */
    static const char* const MODE_NAMES[] = { "FX", "SYNTH", "AI" };
    static const char* const MODE_SUBS[]  = { "EFFECTS ENGINE", "SYNTHESIZER", "GROOVEFORGE AI" };
    const char* dest_name = (dest < 3) ? MODE_NAMES[dest] : "?";
    const char* dest_sub  = (dest < 3) ? MODE_SUBS[dest]  : "";

    lv_color_t dest_color = (dest == 1) ? GF_COLOR_TEAL_PLUS :
                            (dest == 2) ? GF_COLOR_TEAL      : GF_COLOR_PURPLE_BRIGHT;

    gf_hero_label(parent, dest_name, GF_FONT_HERO, dest_color, -18);
    gf_hero_label(parent, dest_sub,  GF_FONT_MICRO, GF_COLOR_GRAY, 22);

    /* Mode pill refleja el destino. */
    gf_mode_pill(parent, dest_name, dest_color);
}

void view_09_destroy(void) {}
