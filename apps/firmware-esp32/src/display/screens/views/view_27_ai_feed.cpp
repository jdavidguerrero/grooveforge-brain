/**
 * @file view_27_ai_feed.cpp
 * @brief Vista 27 — AI FEED: timeline de eventos AI (Sprint 34 Polish)
 *
 * Muestra los últimos 4 eventos del motor AI como log cronológico, con
 * opacidad decreciente según antigüedad. Los eventos se detectan localmente
 * comparando el estado actual contra el frame anterior.
 *
 * Tipos de eventos detectados:
 *   [KEY]  — key_idx cambió (nueva tonalidad detectada)
 *   [CHD]  — chord_root/qual cambió (nuevo acorde detectado)
 *   [BPM]  — BPM cambió > 5 BPM
 *   [SNA]  — Scale Lock corrigió una nota (SNAP event)
 *   [SCL]  — Modo manual/auto de scale lock cambió
 *
 * Layout (centro del display, 5 filas):
 *   Título "AI FEED" en arc superior (gf_arc_title)
 *
 *   ROW 0  [KEY]  C MAJ               ← más reciente, teal, opa=255
 *   ROW 1  [CHD]  Am7         · 5s    ← opa decrece con antigüedad
 *   ROW 2  [BPM]  127         · 12s
 *   ROW 3  [SNA]  C# > D      · 34s   ← más antiguo, gris tenue
 *
 *   Si no hay eventos: "Play notes to start" centrado
 *
 * Opacidad por edad:
 *   < 5s  → 255  (teal_plus)
 *   5-20s → 180  (teal)
 *   20-60s→ 120  (gray)
 *   > 60s →  60  (gray dim)
 *
 * Spec: apps/docs/sprints/34-camelot-hero-modes.md §"AI FEED redesign"
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <string.h>

/* ── Event ring buffer ────────────────────────────────────────────────────── */

struct FeedEvent {
    char     prefix[6];   ///< "[KEY]", "[CHD]", "[BPM]", "[SNA]", "[SCL]"
    char     text[22];    ///< valor legible, ej. "C MAJ", "Am7", "127 BPM"
    uint32_t timestamp;   ///< lv_tick_get() cuando se generó el evento
};

static FeedEvent s_feed[4];
static uint8_t   s_feed_count = 0;  ///< cuántos eventos hay en el buffer (0-4)

/** Inserta un nuevo evento en la posición 0 (más reciente), desplazando los demás. */
static void feed_push(const char* prefix, const char* text) {
    /* Shift down: [0]→[1], [1]→[2], [2]→[3] */
    for (int i = 3; i > 0; i--) s_feed[i] = s_feed[i - 1];
    strncpy(s_feed[0].prefix, prefix, sizeof(s_feed[0].prefix) - 1);
    s_feed[0].prefix[sizeof(s_feed[0].prefix) - 1] = '\0';
    strncpy(s_feed[0].text,   text,   sizeof(s_feed[0].text)   - 1);
    s_feed[0].text[sizeof(s_feed[0].text) - 1] = '\0';
    s_feed[0].timestamp = lv_tick_get();
    if (s_feed_count < 4) s_feed_count++;
}

/* ── LVGL objects ─────────────────────────────────────────────────────────── */

static lv_obj_t* s_row_prefix[4] = {};  ///< "[KEY]", "[CHD]", etc.
static lv_obj_t* s_row_text[4]   = {};  ///< "C MAJ · 2s", etc.
static lv_obj_t* s_empty_lbl     = nullptr;
static lv_timer_t* s_timer       = nullptr;

/* ── State snapshots para detección de cambios ────────────────────────────── */

static uint8_t  s_prev_key    = 0xFF;
static uint8_t  s_prev_chord  = 0xFF;
static uint8_t  s_prev_qual   = 0xFF;
static uint16_t s_prev_bpm    = 0;
static uint32_t s_prev_snap   = 0xFFFFFFFF;
static bool     s_prev_manual = false;

/* ── Nota por pitch class ─────────────────────────────────────────────────── */

static const char* const FEED_NOTES[12] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

/* ── Helper: cadena de edad ───────────────────────────────────────────────── */

static void age_str(uint32_t age_ms, char* out, size_t n) {
    uint32_t s = age_ms / 1000;
    if (s < 60)      snprintf(out, n, "%lus",  (unsigned long)s);
    else             snprintf(out, n, ">1m");
}

/* ── Helper: opacidad + color por edad ───────────────────────────────────── */

static lv_opa_t age_opa(uint32_t age_ms) {
    if (age_ms < 5000)  return 255;
    if (age_ms < 20000) return 180;
    if (age_ms < 60000) return 120;
    return 60;
}

static lv_color_t age_color(uint32_t age_ms) {
    if (age_ms < 5000)  return GF_COLOR_TEAL_PLUS;
    if (age_ms < 20000) return GF_COLOR_TEAL;
    return GF_COLOR_GRAY;
}

/* ── Timer 100ms ──────────────────────────────────────────────────────────── */

static void feed_timer_cb(lv_timer_t* /*t*/) {
    uint32_t now = lv_tick_get();

    /* ── 1. Detección de cambios → push de eventos ──────────────────────── */

    uint8_t  key_idx    = bridge_get_ai_key_idx();
    uint8_t  chord_root = bridge_get_ai_chord_root();
    uint8_t  chord_qual = bridge_get_ai_chord_quality();
    uint16_t bpm        = bridge_get_ai_bpm();
    bool     manual     = bridge_get_scale_manual_mode();

    /* KEY change */
    if (key_idx != 0xFF && key_idx != s_prev_key) {
        feed_push("[KEY]", bridge_get_ai_key_name());
    }
    s_prev_key = key_idx;

    /* CHORD change (root o quality) */
    if (chord_root != 0xFF &&
        (chord_root != s_prev_chord || chord_qual != s_prev_qual)) {
        feed_push("[CHD]", bridge_get_ai_chord_name());
    }
    s_prev_chord = chord_root;
    s_prev_qual  = chord_qual;

    /* BPM change > 5 */
    if (bpm != 0) {
        int16_t delta = (int16_t)bpm - (int16_t)s_prev_bpm;
        if (delta < 0) delta = -delta;
        if (delta > 5) {
            static char bpm_buf[12];
            snprintf(bpm_buf, sizeof(bpm_buf), "%u BPM", (unsigned)bpm);
            feed_push("[BPM]", bpm_buf);
        }
    }
    s_prev_bpm = bpm;

    /* SNAP event (edge: snap_age < 200ms y más reciente que la última vez) */
    {
        uint8_t snap_from, snap_to;
        uint32_t snap_age;
        bridge_get_last_snap(&snap_from, &snap_to, &snap_age);
        bool is_new_snap = (snap_age < 200) && (snap_age < s_prev_snap);
        s_prev_snap = snap_age;
        if (is_new_snap) {
            static char snap_buf[12];
            snprintf(snap_buf, sizeof(snap_buf), "%s > %s",
                     FEED_NOTES[snap_from % 12], FEED_NOTES[snap_to % 12]);
            feed_push("[SNA]", snap_buf);
        }
    }

    /* SCALE mode change */
    if (manual != s_prev_manual) {
        if (manual) {
            uint8_t k = bridge_get_scale_manual_key();
            /* Mismo encoding key_name: usar nota raíz + modo */
            static char scl_buf[12];
            const char* root = (k != 0xFF) ? FEED_NOTES[k % 12] : "?";
            snprintf(scl_buf, sizeof(scl_buf), "MANUAL %s%s",
                     root, (k < 12) ? " MAJ" : " MIN");
            feed_push("[SCL]", scl_buf);
        } else {
            feed_push("[SCL]", "AUTO");
        }
    }
    s_prev_manual = manual;

    /* ── 2. Render del buffer ────────────────────────────────────────────── */

    bool has_events = (s_feed_count > 0);

    /* Empty state label */
    if (s_empty_lbl) {
        lv_obj_set_style_opa(s_empty_lbl, has_events ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    }

    /* 4 event rows */
    for (int i = 0; i < 4; i++) {
        if (!s_row_prefix[i] || !s_row_text[i]) continue;

        if (i >= (int)s_feed_count) {
            /* Slot vacío — ocultar */
            lv_obj_set_style_opa(s_row_prefix[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_opa(s_row_text[i],   LV_OPA_TRANSP, 0);
            continue;
        }

        uint32_t age = now - s_feed[i].timestamp;
        lv_opa_t   opa   = age_opa(age);
        lv_color_t col   = age_color(age);

        /* Prefix */
        lv_label_set_text(s_row_prefix[i], s_feed[i].prefix);
        lv_obj_set_style_text_color(s_row_prefix[i], col, 0);
        lv_obj_set_style_opa(s_row_prefix[i], opa, 0);

        /* Text + age inline: "C MAJ · 5s" */
        static char row_buf[32];
        if (age > 2000) {
            static char age_buf[8];
            age_str(age, age_buf, sizeof(age_buf));
            snprintf(row_buf, sizeof(row_buf), "%s  %s", s_feed[i].text, age_buf);
        } else {
            snprintf(row_buf, sizeof(row_buf), "%s", s_feed[i].text);
        }
        lv_label_set_text(s_row_text[i], row_buf);
        lv_obj_set_style_text_color(s_row_text[i], col, 0);
        lv_obj_set_style_opa(s_row_text[i], opa, 0);
    }
}

/* ── API pública ─────────────────────────────────────────────────────────── */

void view_27_create(lv_obj_t* parent) {
    /* Resetear estado */
    for (int i = 0; i < 4; i++) {
        s_row_prefix[i] = nullptr;
        s_row_text[i]   = nullptr;
        s_feed[i]       = FeedEvent{};
    }
    s_feed_count  = 0;
    s_empty_lbl   = nullptr;
    s_timer       = nullptr;
    s_prev_key    = bridge_get_ai_key_idx();
    s_prev_chord  = bridge_get_ai_chord_root();
    s_prev_qual   = bridge_get_ai_chord_quality();
    s_prev_bpm    = bridge_get_ai_bpm();
    s_prev_snap   = 0xFFFFFFFF;
    s_prev_manual = bridge_get_scale_manual_mode();

    gf_screen_bg(parent);
    gf_arc_title(parent, "AI FEED", GF_COLOR_TEAL);

    /* ── 4 event rows ───────────────────────────────────────────────────── */
    /* y-offsets desde el centro: 4 filas de 18px de altura, separadas 20px */
    static const lv_coord_t ROW_DY[4] = { -35, -15, 5, 25 };

    for (int i = 0; i < 4; i++) {
        lv_coord_t dy = ROW_DY[i];

        /* Prefix label — "[KEY]", "[CHD]", etc. — 36px a la izquierda del centro */
        lv_obj_t* pre = lv_label_create(parent);
        lv_obj_set_style_text_font(pre, GF_FONT_MICRO, 0);
        lv_obj_set_style_text_color(pre, GF_COLOR_TEAL_PLUS, 0);
        lv_label_set_text(pre, "");
        lv_obj_align(pre, LV_ALIGN_CENTER, -46, dy);
        lv_obj_set_style_opa(pre, LV_OPA_TRANSP, 0);
        s_row_prefix[i] = pre;

        /* Text label — valor + age — centrado ligeramente a la derecha */
        lv_obj_t* txt = lv_label_create(parent);
        lv_obj_set_style_text_font(txt, GF_FONT_LABEL, 0);
        lv_obj_set_style_text_color(txt, GF_COLOR_WHITE, 0);
        lv_label_set_text(txt, "");
        lv_obj_align(txt, LV_ALIGN_CENTER, 16, dy);
        lv_obj_set_style_opa(txt, LV_OPA_TRANSP, 0);
        s_row_text[i] = txt;
    }

    /* ── Empty state ────────────────────────────────────────────────────── */
    s_empty_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(s_empty_lbl, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_empty_lbl, GF_COLOR_GRAY, 0);
    lv_obj_set_style_text_align(s_empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_empty_lbl, "Play notes to\nstart logging");
    lv_obj_align(s_empty_lbl, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_opa(s_empty_lbl, LV_OPA_COVER, 0);  /* visible hasta primer evento */

    /* Timer 100ms */
    s_timer = lv_timer_create(feed_timer_cb, 100, nullptr);
}

void view_27_destroy(void) {
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = nullptr;
    }
    for (int i = 0; i < 4; i++) {
        s_row_prefix[i] = nullptr;
        s_row_text[i]   = nullptr;
    }
    s_empty_lbl = nullptr;
    s_feed_count = 0;
}
