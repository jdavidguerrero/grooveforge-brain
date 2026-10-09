#include "ui/screen_boot.h"

#include "ui/theme.h"

using namespace gf;

lv_obj_t* screen_boot_create(const char* subtitle) {
    lv_obj_t* scr = lv_obj_create(nullptr);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(BG), 0);

    lv_obj_t* title = label(scr, T28, WHITE, "GROOVEFORGE BRAIN");
    lv_obj_set_style_text_letter_space(title, 2, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -24);

    lv_obj_t* studio = label(scr, T16, TEAL_PLUS, "STUDIO");
    lv_obj_set_style_text_letter_space(studio, 6, 0);
    lv_obj_align(studio, LV_ALIGN_CENTER, 0, 14);

    rect(scr, 400 - 48, 260, 96, 2, TEAL_DIM);

    lv_obj_t* sub = label(scr, T12, GRAY, subtitle);
    lv_obj_set_style_text_letter_space(sub, 2, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 52);
    return scr;
}
