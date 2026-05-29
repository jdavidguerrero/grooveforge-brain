/**
 * @file view_28_ai_rack.cpp
 * @brief Vista 28 — AI RACK: control surface de los 6 features de AI (Sprint 45)
 *
 * Muestra y permite controlar los 6 features sound-changing del AI mode en
 * una sola pantalla. ENC NAV (en el Teensy) selecciona la fila activa; ENC L/R
 * ajustan el feature de esa fila. Esta vista es solo display — los encoders
 * mandan params al Teensy via Bridge, el Teensy ejecuta el audio.
 *
 * Filas (orden = s_row_names[]):
 *   0  SCALE LOCK  — ON/OFF (bridge_get_scale_lock_bypass(), invertido)
 *   1  HARMONIZE   — ON + intervalo ("3rd"/"6th")
 *   2  ARP         — modo + división ("SMART 1/8")
 *   3  GROOVE      — estilo + barra de amount
 *   4  BEAT FX     — ON/OFF (bridge_get_beat_follower_bypass(), invertido)
 *   5  VELOCITY    — "AUTO" estático (auto-calibra, sin control usuario)
 *
 * La fila activa se resalta en GF_COLOR_TEAL_PLUS con un ">" indicador.
 * Las demás filas aparecen en GF_COLOR_GRAY.
 *
 * Timer 50ms actualiza todos los status labels y el highlight en-place.
 *
 * Refs:
 *   apps/docs/sprints/45-ai-mode-consolidation.md
 *   apps/firmware-esp32/src/bridge/bridge_handlers.h (getters 0x00E9-0x00EE)
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <string.h>

/* ── Layout ──────────────────────────────────────────────────────────────── */

/* 6 filas equidistantes en pantalla round 240×240, centradas verticalmente. */
static constexpr lv_coord_t ROW_DY[6] = { -60, -36, -12, 12, 36, 60 };

/* Columnas horizontales (desde centro). */
static constexpr lv_coord_t DX_CURSOR =  -82;  ///< ">" indicador de fila activa
static constexpr lv_coord_t DX_DOT    =  -68;  ///< dot de estado
static constexpr lv_coord_t DX_NAME   =  -22;  ///< nombre del feature (izquierda)
static constexpr lv_coord_t DX_STATUS =   48;  ///< texto de status (derecha)

static const char* const s_row_names[6] = {
    "SCALE LOCK",
    "HARMONIZE",
    "ARP",
    "GROOVE",
    "BEAT FX",
    "VELOCITY",
};

/* ── Helpers de texto ─────────────────────────────────────────────────────── */

static const char* arp_mode_name(uint8_t m) {
    switch (m) {
        case 0:  return "UP";
        case 1:  return "DOWN";
        case 2:  return "U/D";
        case 3:  return "RND";
        default: return "SMART";
    }
}

static const char* arp_div_name(uint8_t d) {
    switch (d) {
        case 0:  return "1/4";
        case 2:  return "1/16";
        default: return "1/8";
    }
}

static const char* groove_style_name(uint8_t s) {
    switch (s) {
        case 0:  return "OFF";
        case 2:  return "SWING";
        default: return "HUMAN";
    }
}

/** Intervalo de armonía: 0=THIRD, 1=SIXTH (enum HarmonyInterval en Teensy). */
static const char* harm_interval_name(uint8_t i) {
    return (i == 1) ? "6th" : "3rd";
}

/* ── Estado de la vista ──────────────────────────────────────────────────── */

typedef struct {
    lv_obj_t* cursor_lbl;  ///< ">" cuando es la fila activa
    lv_obj_t* dot;         ///< indicador de estado (color = activo/inactivo)
    lv_obj_t* name_lbl;    ///< nombre del feature
    lv_obj_t* status_lbl;  ///< texto de status dinámico
} ai_rack_row_t;

static ai_rack_row_t s_rows[6]  = {};
static lv_timer_t*   s_timer    = nullptr;

/* ── Timer de actualización 50ms ─────────────────────────────────────────── */

static void rack_timer_cb(lv_timer_t* /*t*/) {
    uint8_t cursor = bridge_get_ai_rack_cursor();

    /* Buffer temporal para status strings */
    static char buf[24];

    for (uint8_t i = 0; i < 6; i++) {
        if (!s_rows[i].status_lbl) continue;

        bool is_active_row = (i == cursor);

        /* ── Color del nombre según fila activa ── */
        if (s_rows[i].name_lbl) {
            lv_obj_set_style_text_color(s_rows[i].name_lbl,
                is_active_row ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
        }

        /* ── Indicador ">" de fila activa ── */
        if (s_rows[i].cursor_lbl) {
            lv_label_set_text(s_rows[i].cursor_lbl, is_active_row ? ">" : " ");
            lv_obj_set_style_text_color(s_rows[i].cursor_lbl, GF_COLOR_TEAL_PLUS, 0);
        }

        /* ── Status y dot por fila ── */
        switch (i) {

        case 0: {   // SCALE LOCK — ON cuando bypass=false
            bool on = !bridge_get_scale_lock_bypass();
            if (s_rows[i].dot) {
                lv_obj_set_style_bg_color(s_rows[i].dot,
                    on ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
            }
            lv_label_set_text(s_rows[i].status_lbl, on ? "ON" : "OFF");
            lv_obj_set_style_text_color(s_rows[i].status_lbl,
                on ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
            break;
        }

        case 1: {   // HARMONIZE — enabled + intervalo
            bool on = bridge_get_auto_harm_enabled();
            if (s_rows[i].dot) {
                lv_obj_set_style_bg_color(s_rows[i].dot,
                    on ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
            }
            if (on) {
                snprintf(buf, sizeof(buf), "%s", harm_interval_name(bridge_get_auto_harm_interval()));
            } else {
                snprintf(buf, sizeof(buf), "OFF");
            }
            lv_label_set_text(s_rows[i].status_lbl, buf);
            lv_obj_set_style_text_color(s_rows[i].status_lbl,
                on ? GF_COLOR_WHITE : GF_COLOR_GRAY, 0);
            break;
        }

        case 2: {   // ARP — modo + división
            uint8_t mode = bridge_get_arp_mode();
            uint8_t div  = bridge_get_arp_division();
            snprintf(buf, sizeof(buf), "%s %s", arp_mode_name(mode), arp_div_name(div));
            lv_label_set_text(s_rows[i].status_lbl, buf);
            if (s_rows[i].dot) {
                lv_obj_set_style_bg_color(s_rows[i].dot, GF_COLOR_TEAL_PLUS, 0);
            }
            lv_obj_set_style_text_color(s_rows[i].status_lbl, GF_COLOR_WHITE, 0);
            break;
        }

        case 3: {   // GROOVE — estilo + barra de amount con caracteres de bloque
            uint8_t style  = bridge_get_groove_style();
            float   amount = bridge_get_groove_amount();
            bool    active = (style != 0);
            /* Mini-barra de 4 bloques: cada bloque = 0.25 */
            uint8_t filled = (uint8_t)(amount * 4.0f + 0.5f);
            if (filled > 4) filled = 4;
            /* Construir cadena "HUMAN |||." estilo ASCII */
            static const char* const BAR4[5] = { "____", "|___", "||__", "|||_", "||||" };
            if (active) {
                snprintf(buf, sizeof(buf), "%s %s", groove_style_name(style), BAR4[filled]);
            } else {
                snprintf(buf, sizeof(buf), "OFF");
            }
            lv_label_set_text(s_rows[i].status_lbl, buf);
            if (s_rows[i].dot) {
                lv_obj_set_style_bg_color(s_rows[i].dot,
                    active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
            }
            lv_obj_set_style_text_color(s_rows[i].status_lbl,
                active ? GF_COLOR_WHITE : GF_COLOR_GRAY, 0);
            break;
        }

        case 4: {   // BEAT FX — ON cuando bypass=false
            bool on = !bridge_get_beat_follower_bypass();
            if (s_rows[i].dot) {
                lv_obj_set_style_bg_color(s_rows[i].dot,
                    on ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
            }
            lv_label_set_text(s_rows[i].status_lbl, on ? "ON" : "OFF");
            lv_obj_set_style_text_color(s_rows[i].status_lbl,
                on ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
            break;
        }

        case 5: {   // VELOCITY — estático AUTO
            if (s_rows[i].dot) {
                lv_obj_set_style_bg_color(s_rows[i].dot, GF_COLOR_GRAY, 0);
            }
            lv_label_set_text(s_rows[i].status_lbl, "AUTO");
            lv_obj_set_style_text_color(s_rows[i].status_lbl, GF_COLOR_GRAY, 0);
            break;
        }

        } /* switch (i) */
    }
}

/* ── Helpers de construcción de fila ──────────────────────────────────────── */

/**
 * @brief Construye los widgets de una fila (cursor + dot + nombre + status).
 *
 * @param parent   Pantalla padre.
 * @param idx      Índice de fila (0-5).
 * @param name     Texto del nombre del feature.
 * @param status   Texto inicial del status.
 */
static void build_row(lv_obj_t* parent, uint8_t idx,
                      const char* name, const char* status) {
    const lv_coord_t dy = ROW_DY[idx];

    /* ">" cursor — visible solo en la fila activa */
    lv_obj_t* cur = gf_label(parent, " ", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS);
    if (cur) lv_obj_align(cur, LV_ALIGN_CENTER, DX_CURSOR, dy);
    s_rows[idx].cursor_lbl = cur;

    /* Dot de estado */
    lv_obj_t* dot = gf_dot(parent, 6, GF_COLOR_GRAY);
    lv_obj_align(dot, LV_ALIGN_CENTER, DX_DOT, dy);
    s_rows[idx].dot = dot;

    /* Nombre del feature */
    lv_obj_t* name_lbl = gf_label(parent, name, GF_FONT_MICRO, GF_COLOR_GRAY);
    if (name_lbl) lv_obj_align(name_lbl, LV_ALIGN_CENTER, DX_NAME, dy);
    s_rows[idx].name_lbl = name_lbl;

    /* Status dinámico */
    lv_obj_t* st = gf_label(parent, status, GF_FONT_MICRO, GF_COLOR_GRAY);
    if (st) lv_obj_align(st, LV_ALIGN_CENTER, DX_STATUS, dy);
    s_rows[idx].status_lbl = st;
}

/* ── API pública ──────────────────────────────────────────────────────────── */

void view_28_create(lv_obj_t* parent) {
    for (uint8_t i = 0; i < 6; i++) s_rows[i] = ai_rack_row_t{};
    s_timer = nullptr;

    gf_screen_bg(parent);
    gf_arc_title(parent, "AI RACK", GF_COLOR_TEAL);

    /* Fila 0 — SCALE LOCK */
    {
        bool on = !bridge_get_scale_lock_bypass();
        build_row(parent, 0, s_row_names[0], on ? "ON" : "OFF");
    }

    /* Fila 1 — HARMONIZE */
    {
        bool on = bridge_get_auto_harm_enabled();
        const char* st = on ? harm_interval_name(bridge_get_auto_harm_interval()) : "OFF";
        build_row(parent, 1, s_row_names[1], st);
    }

    /* Fila 2 — ARP */
    {
        static char buf[16];
        snprintf(buf, sizeof(buf), "%s %s",
                 arp_mode_name(bridge_get_arp_mode()),
                 arp_div_name(bridge_get_arp_division()));
        build_row(parent, 2, s_row_names[2], buf);
    }

    /* Fila 3 — GROOVE */
    {
        uint8_t style = bridge_get_groove_style();
        build_row(parent, 3, s_row_names[3], groove_style_name(style));
    }

    /* Fila 4 — BEAT FX */
    {
        bool on = !bridge_get_beat_follower_bypass();
        build_row(parent, 4, s_row_names[4], on ? "ON" : "OFF");
    }

    /* Fila 5 — VELOCITY */
    build_row(parent, 5, s_row_names[5], "AUTO");

    /* Page dots — 3 posiciones, activo en 1 (esta es la sub-vista del medio) */
    gf_page_dots(parent, 3, 1, GF_COLOR_TEAL);

    /* Mode pill */
    gf_mode_pill(parent, "AI", GF_COLOR_TEAL);

    /* Timer 50ms para actualizar estados desde bridge */
    s_timer = lv_timer_create(rack_timer_cb, 50, nullptr);
}

void view_28_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    for (uint8_t i = 0; i < 6; i++) s_rows[i] = ai_rack_row_t{};
}
