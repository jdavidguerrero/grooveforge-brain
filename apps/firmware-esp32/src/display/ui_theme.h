#pragma once

/**
 * @file ui_theme.h
 * @brief Design tokens — GrooveForge Brain Display UI (Sprint 3.4)
 *
 * Paleta, tipografia y geometria del set de 23 vistas de los mocks
 * (apps/docs/ui-mocks/). Reemplaza la paleta de Sprint 3.1.
 *
 * Lenguaje visual (mocks GFB-UI-001/002/023):
 *   - Fondo antracita (#0A0A0A): estudio oscuro, foco en el contenido
 *   - Teal #1D9E75 primario / #5DCAA5 activo: identidad, modo SYNTH
 *   - Purple #534AB7 / #7A6FE0: chrome del modo FX
 *   - IBM Plex Mono: tipografia tecnica monoespaciada en toda la UI
 *
 * Geometria GC9A01 240x240 circular (RGB565):
 *   - Zona segura Ø 220px (10px de margen al borde fisico)
 *   - Arc-title en el top, span 30°-150°, baseline a 90px de radio
 *   - Mode pill siempre abajo (y=224), page dots a y=198
 *
 * Ver apps/docs/sprints/18-display-ui-carousel.md para la teoria completa.
 */

#include "lvgl.h"
#include "fonts/gf_fonts.h"

/* ── Paleta (10 colores — mocks GFB-UI) ──────────────────────────────────── */

/** Fondo antracita: todas las pantallas */
#define GF_COLOR_BG             lv_color_hex(0x0A0A0A)

/** Teal primario: arcs, titulos, acento del modo SYNTH */
#define GF_COLOR_TEAL           lv_color_hex(0x1D9E75)

/** Teal brillante: elementos activos, animaciones, highlights */
#define GF_COLOR_TEAL_PLUS      lv_color_hex(0x5DCAA5)

/** Teal apagado: anillo idle, divisores, pista de fondo de arcs */
#define GF_COLOR_TEAL_DIM       lv_color_hex(0x0E5040)

/** Purple: chrome del modo FX (tint, mode pill) */
#define GF_COLOR_PURPLE         lv_color_hex(0x534AB7)

/** Purple brillante: arcs FX, FX activo */
#define GF_COLOR_PURPLE_BRIGHT  lv_color_hex(0x7A6FE0)

/** Gris (con tinte menta): labels secundarios, texto micro */
#define GF_COLOR_GRAY           lv_color_hex(0x9FE1CB)

/** Rojo: error, clip de audio */
#define GF_COLOR_RED            lv_color_hex(0xE84A4A)

/** Ambar: warnings, "DO NOT POWER OFF" del OTA */
#define GF_COLOR_AMBER          lv_color_hex(0xE8B84A)

/** Blanco: texto hero (notas, valores grandes) */
#define GF_COLOR_WHITE          lv_color_hex(0xFFFFFF)

/** Blanco roto: hero values en sprint 30 (menos fatigante que #FFF puro en OLED). */
#define GF_COLOR_HERO_WHITE     lv_color_hex(0xF2F2F2)

/** Naranja bypass: overlay BYPASS activo en view_07 FX MAIN. */
#define GF_COLOR_BYPASS_ORANGE  lv_color_hex(0xFF6A35)

/** Gris monitor: cutoff monitor permanente en view_02 SYNTH (no compite con HERO). */
#define GF_COLOR_MONITOR_GRAY   lv_color_hex(0x5A5A6E)

/* ── Tipografia — IBM Plex Mono, 6 tamanos ───────────────────────────────── */

/**
 * Set reducido de 6 tamanos (snap de los ~23 tamanos de los mocks).
 * En un display de 240px la diferencia entre tamanos contiguos es
 * imperceptible. Las fuentes se generan con lv-font-conv (ver fonts/).
 */
#define GF_FONT_MICRO   (&ibm_plex_mono_8)    /* 6-9px:  hints, ETA, micro labels */
#define GF_FONT_LABEL   (&ibm_plex_mono_11)   /* 10-12px: arc titles, params, pill */
#define GF_FONT_BODY    (&ibm_plex_mono_16)   /* 13-18px: state text, FX name */
#define GF_FONT_TITLE   (&ibm_plex_mono_22)   /* 20-26px: FX hero, scale name */
#define GF_FONT_HERO    (&ibm_plex_mono_36)   /* 28-42px: BPM, dB, OTA % */
#define GF_FONT_GIANT   (&ibm_plex_mono_64)   /* 48-64px: nota hero del synth */

/* ── Aliases sprint 30 (nav RMX-style + UI hierarchy) ───────────────────── */

/**
 * HERO_XL (64px): wet/dry central en FX MAIN, valor SYNTH en HERO. Alias de
 * GF_FONT_GIANT — no requiere nueva fuente generada.
 */
#define GF_FONT_HERO_XL  GF_FONT_GIANT

/**
 * MAJOR (36px): sub-params FX (TIME / FEEDBACK) y valores secundarios. Alias
 * de GF_FONT_HERO — el spec pide 32px; 36px es el snap más cercano sin
 * recompilar fuente. Si la diferencia importa, generar ibm_plex_mono_32 con
 * lv-font-conv y reemplazar este alias.
 */
#define GF_FONT_MAJOR    GF_FONT_HERO

/* ── Geometria del display circular ──────────────────────────────────────── */

#define GF_DISP_W       240
#define GF_DISP_H       240
#define GF_DISP_CX      120
#define GF_DISP_CY      120

/** Radio de la zona segura: Ø 220px deja 10px de margen al borde fisico */
#define GF_SAFE_R       110

/* ── Arc-title (titulo curvo en el top) ──────────────────────────────────── */

/** Radio baseline donde se posicionan los glifos del arc-title */
#define GF_ARC_TITLE_R      90
/** Span angular del arc-title (grados, 0°=derecha, crece clockwise) */
#define GF_ARC_TITLE_DEG_LO 30
#define GF_ARC_TITLE_DEG_HI 150

/* ── Mode pill (indicador SYNTH/FX, siempre abajo) ───────────────────────── */

#define GF_MODE_PILL_Y  224
#define GF_MODE_PILL_W  36
#define GF_MODE_PILL_H  12

/* ── Page dots (indicador de pagina) ─────────────────────────────────────── */

#define GF_PAGE_DOTS_Y      198
#define GF_PAGE_DOTS_GAP    12
#define GF_DOT_R_ACTIVE     3   /* mock 2.4r → 3px en grid de pixeles */
#define GF_DOT_R_IDLE       2   /* mock 1.6r → 2px */

/* ── Ring decorativo ─────────────────────────────────────────────────────── */

#define GF_RING_WIDTH       8
#define GF_RING_R_OUTER     116
#define GF_RING_OBJ_SIZE    (GF_RING_R_OUTER * 2)

/* ── Overlays ────────────────────────────────────────────────────────────── */

/** Opacidad del scrim oscuro de overlays (volume, error) — mock ~0.55 */
#define GF_OVERLAY_SCRIM_OPA    140

/* ── Transicion entre vistas ─────────────────────────────────────────────── */

/** Duracion de cada mitad (out/in) del radial-wipe — mock 09 */
#define GF_TRANSITION_HALF_MS   200

/** Dwell por defecto de cada vista del carrusel */
#define GF_CAROUSEL_DWELL_MS    5000

/* ── Alias transitorios (Sprint 3.1 → 3.4) ───────────────────────────────── */

/*
 * screen_boot.cpp y screen_main.cpp aun usan los tokens BRAIN_* de Sprint 3.1.
 * Estos alias permiten que compilen mientras se migran a GF_* en Batch B.
 * BORRAR este bloque cuando ambos archivos esten migrados.
 */
#define BRAIN_COLOR_BG          GF_COLOR_BG
#define BRAIN_COLOR_PURPLE      GF_COLOR_PURPLE
#define BRAIN_COLOR_PURPLE_DIM  GF_COLOR_PURPLE
#define BRAIN_COLOR_TEAL        GF_COLOR_TEAL
#define BRAIN_COLOR_WHITE       GF_COLOR_WHITE
#define BRAIN_COLOR_GRAY        GF_COLOR_GRAY
#define BRAIN_COLOR_GRAY_DIM    GF_COLOR_TEAL_DIM
#define BRAIN_COLOR_GREEN       GF_COLOR_TEAL_PLUS
#define BRAIN_COLOR_RED         GF_COLOR_RED

#define BRAIN_FONT_SM           GF_FONT_LABEL
#define BRAIN_FONT_MD           GF_FONT_BODY
#define BRAIN_FONT_LG           GF_FONT_TITLE

#define BRAIN_DISP_W            GF_DISP_W
#define BRAIN_DISP_H            GF_DISP_H
#define BRAIN_DISP_CX           GF_DISP_CX
#define BRAIN_DISP_CY           GF_DISP_CY
#define BRAIN_DISP_R            GF_SAFE_R

#define BRAIN_RING_WIDTH        GF_RING_WIDTH
#define BRAIN_RING_R_OUTER      GF_RING_R_OUTER
#define BRAIN_RING_OBJ_SIZE     GF_RING_OBJ_SIZE
#define BRAIN_STATUS_DOT_SIZE   8
