#pragma once

/**
 * @brief Inicializa pantalla e input según el backend elegido en CMake (STUDIO_DISPLAY).
 *
 * - SDL:   ventana de 800×480 en la Mac; el mouse hace de touch.
 * - FBDEV: /dev/fb0 en la Raspberry Pi + touch por evdev.
 *
 * @param touch_dev Dispositivo evdev del touch (solo FBDEV). nullptr = default de lv_drv_conf.h.
 * @return false si no se pudo abrir la pantalla.
 */
bool hal_init(const char* touch_dev);
