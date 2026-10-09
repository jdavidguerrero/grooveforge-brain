#pragma once

#include "lvgl.h"

/**
 * @file fb.h
 * @brief Volcado de LVGL al framebuffer de Linux (/dev/fb0), con conversión de formato.
 *
 * Reemplaza a display/fbdev.c de lv_drivers 8.3, que copia con memcpy y asume que
 * LV_COLOR_DEPTH es igual a la profundidad del framebuffer. La UI renderiza en RGB565
 * (theory doc §5.1) y el framebuffer DSI de la Pi 5 es de 32 bpp: con memcpy, cada fila de
 * pantalla recibía dos filas de la UI y se veían dos copias con colores corridos.
 *
 * Soporta framebuffers de 16 bpp (copia directa) y 32 bpp (XRGB8888 o XBGR8888).
 */

/// Abre y mapea el framebuffer. Devuelve false si no existe o la profundidad no es 16 ni 32.
bool fb_open(const char* path);

/// flush_cb de LVGL: copia el área al framebuffer convirtiendo al formato real.
void fb_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p);
