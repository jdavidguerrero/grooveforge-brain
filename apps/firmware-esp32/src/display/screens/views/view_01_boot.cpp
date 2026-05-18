/**
 * @file view_01_boot.cpp
 * @brief Vista 01 — BOOT SEQUENCE (mock GFB-UI-001)
 *
 * El carrusel reusa la animacion de arranque: ring teal llenandose +
 * fade-in del wordmark/subtitulo/version. screen_boot_create() construye
 * todo sobre lv_scr_act() (que es el `parent` que recibe la vista).
 */

#include "views.h"
#include "../screen_boot.h"

void view_01_create(lv_obj_t* /*parent*/) {
    screen_boot_create();
}

void view_01_destroy(void) {}
