#pragma once

/**
 * @file gf_widgets.h
 * @brief Helpers de chrome compartido — GrooveForge Brain Display UI
 *
 * Elementos repetidos en muchas de las 23 vistas: fondo, tint de modo,
 * arc-title curvo, mode pill, page dots, labels y dots. Centralizarlos aqui
 * evita duplicar el mismo codigo en 23 archivos de vista.
 *
 * Todas las funciones crean los widgets sobre `parent` (normalmente la pantalla
 * activa). El carrusel los destruye con lv_obj_clean() al cambiar de vista.
 */

#include "lvgl.h"

/**
 * @brief Pinta el fondo antracita de una pantalla y desactiva scroll/padding.
 * @param scr Pantalla (lv_scr_act()).
 */
void gf_screen_bg(lv_obj_t* scr);

/**
 * @brief Overlay de tinte a pantalla completa (marcador de modo FX / error).
 * @param scr  Pantalla.
 * @param c    Color del tinte.
 * @param opa  Opacidad (0-255).
 * @return El objeto overlay creado.
 */
lv_obj_t* gf_tint(lv_obj_t* scr, lv_color_t c, lv_opa_t opa);

/**
 * @brief Crea un label con fuente y color dados (sin posicionar).
 */
lv_obj_t* gf_label(lv_obj_t* parent, const char* txt, const lv_font_t* font, lv_color_t color);

/**
 * @brief Label centrado en la pantalla con offset vertical.
 * @param dy Offset en y respecto al centro (negativo = arriba).
 */
lv_obj_t* gf_hero_label(lv_obj_t* parent, const char* txt, const lv_font_t* font,
                        lv_color_t color, lv_coord_t dy);

/**
 * @brief Dot circular solido (lv_obj con radius = diametro/2).
 * @param d Diametro en px.
 */
lv_obj_t* gf_dot(lv_obj_t* parent, lv_coord_t d, lv_color_t color);

/**
 * @brief Barra de progreso horizontal (lv_bar) con pista atenuada.
 * @param w,h   Tamano en px.
 * @param pct   Valor 0-100.
 * @param color Color del indicador.
 */
lv_obj_t* gf_bar(lv_obj_t* parent, lv_coord_t w, lv_coord_t h, uint8_t pct, lv_color_t color);

/**
 * @brief Arco de parametro (lv_arc) centrado, decorativo (sin interaccion).
 * @param size        Diametro del objeto arc en px.
 * @param width       Grosor del stroke.
 * @param bg_start    Angulo inicial de la pista (grados, 0°=derecha, CW).
 * @param bg_end      Angulo final de la pista (si < start, envuelve por 0°).
 * @param value       Valor 0-100 que llena el indicador desde bg_start.
 * @param ind_color   Color del indicador (parte activa).
 * @param track_color Color de la pista de fondo.
 */
lv_obj_t* gf_arc(lv_obj_t* parent, lv_coord_t size, lv_coord_t width,
                 uint16_t bg_start, uint16_t bg_end, uint8_t value,
                 lv_color_t ind_color, lv_color_t track_color);

/**
 * @brief Mode pill: capsula con borde + texto, centrada abajo (y=224).
 *        Usada como indicador SYNTH/FX.
 */
lv_obj_t* gf_mode_pill(lv_obj_t* parent, const char* txt, lv_color_t color);

/**
 * @brief Fila de page dots a y=198. El dot activo es mas grande y brillante.
 * @param count  Numero de dots.
 * @param active Indice del dot activo (0-based).
 * @param color  Color base.
 */
void gf_page_dots(lv_obj_t* parent, uint8_t count, uint8_t active, lv_color_t color);

/**
 * @brief Titulo curvo siguiendo el arco superior (span ~centrado en 12 o'clock).
 *
 * LVGL 8 no tiene texto-en-curva nativo; se posiciona cada glifo como un label
 * individual sobre el arco de radio GF_ARC_TITLE_R, sin rotar (la curvatura es
 * suave a 11px y los glifos verticales se leen bien).
 *
 * @param txt   Texto del titulo (se renderiza en mayusculas tal cual se pase).
 * @param color Color de los glifos.
 */
void gf_arc_title(lv_obj_t* parent, const char* txt, lv_color_t color);
