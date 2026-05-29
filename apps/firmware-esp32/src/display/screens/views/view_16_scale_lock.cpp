/**
 * @file view_16_scale_lock.cpp
 * @brief Vista 16 — SCALE LOCK (Sprint 34 Batch J rewrite)
 *
 * Muestra en tiempo real la escala activa (AI o manual), el código Camelot,
 * un cromagram de 12 dots que resalta las notas diatónicas en color Camelot,
 * y el estado AUTO/MANUAL del override.
 *
 * Gesto: ENC L rotation = cambiar root (C…B) → MANUAL mode.
 *        ENC R rotation = toggle major/minor  → MANUAL mode.
 *        B1 HOME = resetear a AUTO.
 *
 * Sprint 34 Batch J — Sprint 34 (Camelot hero modes)
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>

// ── State interno ─────────────────────────────────────────────────────────

static lv_obj_t*   s_mode_badge   = nullptr;
static lv_obj_t*   s_scale_name   = nullptr;
static lv_obj_t*   s_camelot_code = nullptr;
static lv_obj_t*   s_dot[12]      = {};
static lv_obj_t*   s_dot_lbl[12]  = {};
static lv_obj_t*   s_hint         = nullptr;
static lv_obj_t*   s_stats        = nullptr;
static lv_obj_t*   s_state_pill   = nullptr;
static lv_timer_t* s_timer        = nullptr;
static uint32_t    s_create_ms    = 0;   // para hint fade

// ── Tablas ────────────────────────────────────────────────────────────────

static const char* const NOTES[12] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

// Mapeo pitch class → posición en Circle of Fifths (0=C en CoF).
// PC_TO_COF[pc] = cof_pos. Idéntico al usado en view_10.
static const uint8_t PC_TO_COF[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };

// Intervalos de escala mayor y menor natural desde la root (en semitonos).
static const uint8_t MAJOR_INTERVALS[7] = {0, 2, 4, 5, 7, 9, 11};
static const uint8_t MINOR_INTERVALS[7] = {0, 2, 3, 5, 7, 8, 10};

// ── Helpers ───────────────────────────────────────────────────────────────

/**
 * @brief Retorna true si el pitch class `pc` pertenece a la escala indicada por key_idx.
 * @param pc       Pitch class 0-11 (C=0…B=11)
 * @param key_idx  0-11 major, 12-23 minor. 0xFF → siempre false.
 */
static bool note_in_scale(uint8_t pc, uint8_t key_idx) {
    if (key_idx == 0xFF) return false;
    uint8_t root  = key_idx % 12;
    bool    minor = (key_idx >= 12);
    const uint8_t* iv = minor ? MINOR_INTERVALS : MAJOR_INTERVALS;
    for (uint8_t i = 0; i < 7; i++) {
        if ((root + iv[i]) % 12 == pc) return true;
    }
    return false;
}

// ── Timer callback (50ms ≈ 20fps) ─────────────────────────────────────────

static void timer_cb(lv_timer_t*) {
    // Determinar key efectivo: override manual tiene prioridad sobre AI.
    bool    manual  = bridge_get_scale_manual_mode();
    uint8_t key_idx = manual ? bridge_get_scale_manual_key()
                             : bridge_get_ai_key_idx();

    // ── Mode badge ────────────────────────────────────────────────────────
    if (s_mode_badge) {
        lv_label_set_text(s_mode_badge, manual ? "[MANUAL]" : "[AUTO]");
        lv_obj_set_style_text_color(s_mode_badge,
            manual ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
    }

    // ── Scale name hero + Camelot code ────────────────────────────────────
    if (key_idx == 0xFF) {
        if (s_scale_name)   lv_label_set_text(s_scale_name, "---");
        if (s_camelot_code) lv_label_set_text(s_camelot_code, "--");
    } else {
        uint8_t pc    = key_idx % 12;
        bool    minor = (key_idx >= 12);

        // Scale name: "F# minor" / "A major"
        char nbuf[20];
        snprintf(nbuf, sizeof(nbuf), "%s %s", NOTES[pc], minor ? "minor" : "major");
        if (s_scale_name) {
            lv_label_set_text(s_scale_name, nbuf);
            uint8_t cof_pos = PC_TO_COF[pc];
            lv_obj_set_style_text_color(s_scale_name, gf_camelot_color(cof_pos), 0);
        }

        // Camelot code: "2B" / "11A"
        if (s_camelot_code) {
            uint8_t cof_pos = PC_TO_COF[pc];
            char cbuf[8];
            snprintf(cbuf, sizeof(cbuf), "%u%c",
                     COF_TO_CAMELOT_NUM[cof_pos], minor ? 'A' : 'B');
            lv_label_set_text(s_camelot_code, cbuf);
            lv_obj_set_style_text_color(s_camelot_code, gf_camelot_color(cof_pos), 0);
        }
    }

    // ── Chromagram dots: diatónicas bright Camelot, cromáticas dim ────────
    for (uint8_t pc = 0; pc < 12; pc++) {
        bool    in_scale = note_in_scale(pc, key_idx);
        uint8_t cof_pos  = PC_TO_COF[pc];

        if (s_dot[pc]) {
            lv_obj_set_style_bg_color(s_dot[pc],
                in_scale ? gf_camelot_color(cof_pos) : GF_COLOR_GRAY, 0);
            lv_obj_set_style_bg_opa(s_dot[pc],
                in_scale ? LV_OPA_COVER : (lv_opa_t)60, 0);
        }
        if (s_dot_lbl[pc]) {
            lv_obj_set_style_text_color(s_dot_lbl[pc],
                in_scale ? GF_COLOR_WHITE : GF_COLOR_GRAY, 0);
        }
    }

    // ── Hint fade: visible 3s, fade 1s, luego invisible ──────────────────
    if (s_hint) {
        uint32_t age_ms = lv_tick_get() - s_create_ms;
        lv_opa_t opa;
        if (age_ms < 3000) {
            opa = 150;
        } else if (age_ms < 4000) {
            // fade lineal de 150 a 0 durante 1s
            opa = (lv_opa_t)(150u * (4000u - age_ms) / 1000u);
        } else {
            opa = LV_OPA_TRANSP;
        }
        lv_obj_set_style_opa(s_hint, opa, 0);
    }

    // ── Stats: notas snapped / total ─────────────────────────────────────
    if (s_stats) {
        uint16_t snapped, total;
        bridge_get_snap_stats(&snapped, &total);
        char sbuf[24];
        snprintf(sbuf, sizeof(sbuf), "snap %u/%u", snapped, total);
        lv_label_set_text(s_stats, sbuf);
    }

    // ── State pill: BYPASS vs ACTIVE ─────────────────────────────────────
    if (s_state_pill) {
        bool bypass = bridge_get_scale_lock_bypass();
        lv_label_set_text(s_state_pill, bypass ? "-- BYPASS --" : "-- ACTIVE --");
        lv_obj_set_style_text_color(s_state_pill,
            bypass ? GF_COLOR_GRAY : GF_COLOR_TEAL_PLUS, 0);
    }
}

// ── Create ────────────────────────────────────────────────────────────────

void view_16_create(lv_obj_t* parent) {
    gf_screen_bg(parent);
    gf_arc_title(parent, "SCALE LOCK", GF_COLOR_TEAL);

    s_create_ms = lv_tick_get();

    // Mode badge: "[AUTO]" / "[MANUAL]" — justo debajo del arc title
    s_mode_badge = lv_label_create(parent);
    lv_obj_set_style_text_font(s_mode_badge, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_mode_badge, GF_COLOR_GRAY, 0);
    lv_label_set_text(s_mode_badge, "[AUTO]");
    lv_obj_align(s_mode_badge, LV_ALIGN_CENTER, 0, -55);

    // Scale name hero: "A major" en color Camelot, font TITLE
    s_scale_name = lv_label_create(parent);
    lv_obj_set_style_text_font(s_scale_name, GF_FONT_TITLE, 0);
    lv_obj_set_style_text_color(s_scale_name, GF_COLOR_GRAY, 0);
    lv_label_set_text(s_scale_name, "---");
    lv_obj_align(s_scale_name, LV_ALIGN_CENTER, 0, -32);

    // Camelot code: "8B" / "11A" — font LABEL
    s_camelot_code = lv_label_create(parent);
    lv_obj_set_style_text_font(s_camelot_code, GF_FONT_LABEL, 0);
    lv_obj_set_style_text_color(s_camelot_code, GF_COLOR_GRAY, 0);
    lv_label_set_text(s_camelot_code, "--");
    lv_obj_align(s_camelot_code, LV_ALIGN_CENTER, 0, -10);

    // Chromagram: 12 dots centrados en y=+20
    // Total ancho = 12 * 14 = 168px → x offset base = -84 + 7 = -77 (centrado en dot)
    for (uint8_t pc = 0; pc < 12; pc++) {
        // Dot (6px diámetro)
        s_dot[pc] = lv_obj_create(parent);
        lv_obj_set_size(s_dot[pc], 6, 6);
        lv_obj_set_style_radius(s_dot[pc], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(s_dot[pc], GF_COLOR_GRAY, 0);
        lv_obj_set_style_bg_opa(s_dot[pc], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_dot[pc], 0, 0);
        lv_obj_set_style_pad_all(s_dot[pc], 0, 0);
        lv_obj_align(s_dot[pc], LV_ALIGN_CENTER,
                     -77 + (lv_coord_t)pc * 14, 20);

        // Label nota (font MICRO)
        s_dot_lbl[pc] = lv_label_create(parent);
        lv_obj_set_style_text_font(s_dot_lbl[pc], GF_FONT_MICRO, 0);
        lv_obj_set_style_text_color(s_dot_lbl[pc], GF_COLOR_GRAY, 0);
        lv_label_set_text(s_dot_lbl[pc], NOTES[pc]);
        // C# / D# / F# / G# / A# tienen '#' — ancho variable: alinear en el centro del dot
        lv_obj_align(s_dot_lbl[pc], LV_ALIGN_CENTER,
                     -77 + (lv_coord_t)pc * 14, 36);
    }

    // Hint "L=root  R=maj/min" — fade out a 4s
    s_hint = lv_label_create(parent);
    lv_obj_set_style_text_font(s_hint, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_hint, GF_COLOR_GRAY, 0);
    lv_label_set_text(s_hint, "L=root  R=maj/min");
    lv_obj_align(s_hint, LV_ALIGN_CENTER, 0, 58);

    // Stats "snap N/M"
    s_stats = lv_label_create(parent);
    lv_obj_set_style_text_font(s_stats, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_stats, GF_COLOR_GRAY, 0);
    lv_label_set_text(s_stats, "snap 0/0");
    lv_obj_align(s_stats, LV_ALIGN_CENTER, 0, 72);

    // State pill "-- ACTIVE --" / "-- BYPASS --"
    s_state_pill = lv_label_create(parent);
    lv_obj_set_style_text_font(s_state_pill, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_state_pill, GF_COLOR_TEAL_PLUS, 0);
    lv_label_set_text(s_state_pill, "-- ACTIVE --");
    lv_obj_align(s_state_pill, LV_ALIGN_CENTER, 0, 88);

    // Timer 50ms
    s_timer = lv_timer_create(timer_cb, 50, nullptr);
}

// ── Destroy ───────────────────────────────────────────────────────────────

void view_16_destroy(void) {
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = nullptr;
    }
    // Nulificar punteros — el carousel hace lv_obj_clean() del parent
    s_mode_badge   = nullptr;
    s_scale_name   = nullptr;
    s_camelot_code = nullptr;
    for (uint8_t i = 0; i < 12; i++) {
        s_dot[i]     = nullptr;
        s_dot_lbl[i] = nullptr;
    }
    s_hint       = nullptr;
    s_stats      = nullptr;
    s_state_pill = nullptr;
}
