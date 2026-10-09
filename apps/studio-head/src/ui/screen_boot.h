#pragma once

#include "lvgl.h"

/**
 * @brief Splash de arranque (UI v3, 08e): se muestra hasta el handshake con el motor de audio.
 * @param subtitle "CONNECTING AUDIO ENGINE" o "DEMO MODE".
 */
lv_obj_t* screen_boot_create(const char* subtitle);
