/**
 * @file view_17_wifi_setup.cpp
 * @brief Vista 17 — WIFI SETUP (mock GFB-UI-017)
 *
 * Nivel 3 (WiFi/Cloud). Tres arcos WiFi concentricos en cascada, texto de
 * estado, SSID, progress dots y hint del portal de configuracion.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"

void view_17_create(lv_obj_t* parent) {
    gf_screen_bg(parent);

    /* A. Tres arcos WiFi en cascada (abanico superior). */
    const lv_coord_t sz[3]  = {78, 116, 154};
    const lv_color_t col[3] = {GF_COLOR_TEAL_PLUS, GF_COLOR_TEAL, GF_COLOR_TEAL};
    for (uint8_t i = 0; i < 3; i++) {
        lv_obj_t* a = gf_arc(parent, sz[i], 5, 234, 306, 100, col[i], GF_COLOR_TEAL_DIM);
        /* Subir el grupo de arcos a la zona alta del display. */
        lv_obj_align(a, LV_ALIGN_CENTER, 0, 18);
    }

    /* B. Texto de estado. */
    gf_hero_label(parent, "CONNECTING", GF_FONT_BODY, GF_COLOR_WHITE, -2);

    /* C. SSID encontrado. */
    gf_hero_label(parent, "GROOVE_NET", GF_FONT_LABEL, GF_COLOR_TEAL_PLUS, 22);

    /* D. Progress dots — 5, las 3 primeras activas. */
    for (uint8_t i = 0; i < 5; i++) {
        const bool on = (i < 3);
        lv_obj_t* dot = gf_dot(parent, on ? 6 : 4,
                               on ? GF_COLOR_TEAL_PLUS : GF_COLOR_TEAL_DIM);
        lv_obj_align(dot, LV_ALIGN_CENTER, -24 + i * 12, 48);
    }

    /* E. Hint del portal de configuracion. */
    lv_obj_t* hint = gf_label(parent, "groovebrain.local", GF_FONT_MICRO, GF_COLOR_GRAY);
    lv_obj_set_style_opa(hint, 130, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 78);
}

void view_17_destroy(void) {}
