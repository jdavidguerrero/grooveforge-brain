#pragma once

/**
 * @file ui_theme.h
 * @brief Design tokens — GroovePilot Design Language, Brain Edition
 *
 * GroovePilot (groovepilot.co) define el lenguaje visual del ecosistema:
 *   - Background near-black (#0B0B12): evoca estudio oscuro, foco en el audio
 *   - Violet principal (#7C3AED): identidad de marca, Tailwind violet-600
 *   - Teal (#06B6D4): brand mark, highlights activos, Tailwind cyan-500
 *   - Tipografia blanca/gris: jerarquia clara sin decoracion
 *
 * Adaptaciones para GC9A01 240x240 circular (RGB565):
 *   - Colores pasados por lv_color_hex() — LVGL convierte internamente a RGB565
 *   - Ring exterior a 116px de radio (4px de margen del borde fisico del display)
 *   - Fuentes maximas: Montserrat 24 para que el texto entre dentro del circulo util
 *
 * Referencia de diseno: Sprint 3.1 brief — GrooveForge Brain monorepo
 */

#include "lvgl.h"

/* ── Paleta principal ────────────────────────────────────────────────────── */

/** Near-black: fondo de todas las pantallas */
#define BRAIN_COLOR_BG          lv_color_hex(0x0B0B12)

/** Violet primario: ring de boot, ring decorativo screen_main, accents */
#define BRAIN_COLOR_PURPLE      lv_color_hex(0x7C3AED)

/** Violet oscuro: fondo del arc (parte no activa del ring) */
#define BRAIN_COLOR_PURPLE_DIM  lv_color_hex(0x5B21B6)

/** Teal: brand mark, status "connected", ready indicator, highlights */
#define BRAIN_COLOR_TEAL        lv_color_hex(0x06B6D4)

/** Blanco: texto primario (engine name, titulos) */
#define BRAIN_COLOR_WHITE       lv_color_hex(0xFFFFFF)

/** Gris medio: texto secundario (labels "ENGINE", "FX CHAIN") */
#define BRAIN_COLOR_GRAY        lv_color_hex(0x6B7280)

/** Gris oscuro: elementos inactivos, parte "vacia" del ring arc */
#define BRAIN_COLOR_GRAY_DIM    lv_color_hex(0x374151)

/** Verde: status connected / OK */
#define BRAIN_COLOR_GREEN       lv_color_hex(0x10B981)

/** Rojo: status error / disconnected */
#define BRAIN_COLOR_RED         lv_color_hex(0xEF4444)

/* ── Geometria del display circular ─────────────────────────────────────── */

/** Resolucion fisica del GC9A01 */
#define BRAIN_DISP_W    240
#define BRAIN_DISP_H    240

/** Centro geometrico del display circular */
#define BRAIN_DISP_CX   120
#define BRAIN_DISP_CY   120

/** Radio del display circular (distancia del centro al borde fisico) */
#define BRAIN_DISP_R    120

/* ── Ring arc decorativo ─────────────────────────────────────────────────── */

/** Ancho del stroke del ring en pixeles */
#define BRAIN_RING_WIDTH     8

/**
 * Radio exterior del ring: 116px deja 4px de margen al borde fisico del GC9A01.
 * El arc en LVGL se define por el tamano del objeto (232x232 para radio 116).
 */
#define BRAIN_RING_R_OUTER   116

/** Tamano del objeto arc que produce BRAIN_RING_R_OUTER: diameter = 2 * radio */
#define BRAIN_RING_OBJ_SIZE  (BRAIN_RING_R_OUTER * 2)

/* ── Tipografia ──────────────────────────────────────────────────────────── */

/**
 * Fuentes disponibles (habilitadas en lv_conf.h):
 *   BRAIN_FONT_SM  = Montserrat 12 — labels secundarios, status
 *   BRAIN_FONT_MD  = Montserrat 16 — labels principales, boot title
 *   BRAIN_FONT_LG  = Montserrat 24 — engine name (cabe en circulo util ~180px)
 *
 * Montserrat 32 no habilitado en Sprint 3.1 para conservar flash.
 */
#define BRAIN_FONT_SM   (&lv_font_montserrat_12)
#define BRAIN_FONT_MD   (&lv_font_montserrat_16)
#define BRAIN_FONT_LG   (&lv_font_montserrat_24)

/* ── Dot de status ───────────────────────────────────────────────────────── */

/** Tamano del cuadrado que contiene el dot circular de status en screen_main */
#define BRAIN_STATUS_DOT_SIZE   8
