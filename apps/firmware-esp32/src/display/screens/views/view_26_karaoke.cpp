/**
 * @file view_26_karaoke.cpp
 * @brief Vista 26 — KARAOKE: nota activa en tiempo real (Sprint 34 Batch G)
 *
 * Enfoque en el presente: muestra la nota que se está tocando en tamaño máximo,
 * con el contexto de la tonalidad y acorde debajo. Orientada a aprendizaje
 * y ejecución en vivo donde el usuario necesita confirmación visual inmediata
 * de qué nota acaba de sonar.
 *
 * Layout vertical centrado:
 *   Arc title    "PLAYING"         top arc, gray
 *   Note hero    "A"               GF_FONT_HERO 36px, color Camelot del pitch
 *   Octave       "4"               GF_FONT_LABEL, gray, y=+18
 *   Vel bar      12 dots 4px       y=+38, iluminados ∝ velocity
 *   Chord ctx    "Am7 · 11A"       GF_FONT_MICRO, gray, y=+60
 *   Key ctx      "in A major"      GF_FONT_MICRO, gray, y=+75
 *   BPM          "120"             GF_FONT_MICRO, gray, y=+95
 *
 * Fuentes de datos: bridge_get_note_midi(), bridge_get_note_active(),
 *                   bridge_get_note_velocity(), bridge_get_ai_chord_name(),
 *                   bridge_get_ai_key_name(), bridge_get_ai_bpm().
 *
 * Spec: apps/docs/sprints/34-camelot-hero-modes.md §"Batch G"
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>

/* ── Mapeo pitch class → posición en Circle of Fifths (para color Camelot) ── */

/** PC → CoF position. Mismo mapeo que view_10: PC_TO_COF_POS */
static const uint8_t K_PC_TO_COF[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };

/** Nombres de nota por pitch class (0=C ... 11=B) */
static const char* const K_NOTES[12] = {
    "C","C#","D","D#","E","F","F#","G","G#","A","A#","B"
};

/* ── Estado privado de la vista ──────────────────────────────────────────── */

static lv_obj_t*   s_note_lbl  = nullptr;   ///< nota activa HUGE (ej. "A")
static lv_obj_t*   s_octave_lbl = nullptr;  ///< octava (ej. "4")
static lv_obj_t*   s_vel_dot[12] = {};      ///< 12 dots de velocity bar
static lv_obj_t*   s_chord_ctx = nullptr;   ///< "Am7 · 11A"
static lv_obj_t*   s_key_ctx   = nullptr;   ///< "in A major"
static lv_obj_t*   s_bpm_lbl   = nullptr;   ///< "120"
static lv_timer_t* s_timer     = nullptr;

/* ── Timer de actualización 50ms ─────────────────────────────────────────── */

static void karaoke_timer_cb(lv_timer_t* /*t*/) {
    uint8_t midi   = bridge_get_note_midi();
    bool    active = bridge_get_note_active();
    float   vel    = bridge_get_note_velocity();

    uint8_t pc     = midi % 12;
    int     octave = (int)(midi / 12) - 1;  /* MIDI standard: octave(60)=4 */
    uint8_t cof    = K_PC_TO_COF[pc];

    /* ── Nota hero ──────────────────────────────────────────────────────── */
    if (s_note_lbl) {
        lv_label_set_text(s_note_lbl, K_NOTES[pc]);
        lv_color_t c = gf_camelot_color(cof);
        lv_obj_set_style_text_color(s_note_lbl, c, 0);
        /* Dim 50% cuando no hay nota activa */
        lv_obj_set_style_opa(s_note_lbl, active ? LV_OPA_COVER : (lv_opa_t)128, 0);
    }

    /* ── Octave ─────────────────────────────────────────────────────────── */
    if (s_octave_lbl) {
        static char oct_buf[4];
        snprintf(oct_buf, sizeof(oct_buf), "%d", octave);
        lv_label_set_text(s_octave_lbl, oct_buf);
        lv_obj_set_style_opa(s_octave_lbl, active ? LV_OPA_COVER : (lv_opa_t)128, 0);
    }

    /* ── Velocity bar: iluminar floor(vel*12) dots de izquierda a derecha ── */
    int lit = active ? (int)(vel * 12.0f) : 0;
    for (int i = 0; i < 12; i++) {
        if (!s_vel_dot[i]) continue;
        bool on = (i < lit);
        lv_obj_set_style_opa(s_vel_dot[i],
            on ? LV_OPA_COVER : (lv_opa_t)40, 0);
        /* Los dots encendidos toman el color Camelot de la nota */
        lv_obj_set_style_bg_color(s_vel_dot[i],
            on ? gf_camelot_color(cof) : GF_COLOR_GRAY, 0);
    }

    /* ── Chord + Camelot code context ───────────────────────────────────── */
    if (s_chord_ctx) {
        const char* chord   = bridge_get_ai_chord_name();
        uint8_t key_idx     = bridge_get_ai_key_idx();
        bool has_chord      = (chord[0] != '-');
        bool has_key        = (key_idx != 0xFF);

        if (has_chord || has_key) {
            static char ctx_buf[28];
            if (has_chord && has_key) {
                uint8_t k_pc    = key_idx % 12;
                uint8_t k_cof   = K_PC_TO_COF[k_pc];
                uint8_t cam_num = COF_TO_CAMELOT_NUM[k_cof];
                char    cam_let = (key_idx < 12) ? 'B' : 'A';
                snprintf(ctx_buf, sizeof(ctx_buf), "%s   %u%c", chord, (unsigned)cam_num, cam_let);
            } else if (has_chord) {
                snprintf(ctx_buf, sizeof(ctx_buf), "%s", chord);
            } else {
                /* solo key — mostrar código Camelot */
                uint8_t k_pc    = key_idx % 12;
                uint8_t k_cof   = K_PC_TO_COF[k_pc];
                uint8_t cam_num = COF_TO_CAMELOT_NUM[k_cof];
                char    cam_let = (key_idx < 12) ? 'B' : 'A';
                snprintf(ctx_buf, sizeof(ctx_buf), "%u%c", (unsigned)cam_num, cam_let);
            }
            lv_label_set_text(s_chord_ctx, ctx_buf);
        } else {
            lv_label_set_text(s_chord_ctx, "");
        }
    }

    /* ── Key context ────────────────────────────────────────────────────── */
    if (s_key_ctx) {
        const char* key = bridge_get_ai_key_name();
        if (key[0] != '-') {
            static char key_buf[24];
            snprintf(key_buf, sizeof(key_buf), "in %s", key);
            lv_label_set_text(s_key_ctx, key_buf);
        } else {
            lv_label_set_text(s_key_ctx, "");
        }
    }

    /* ── BPM ────────────────────────────────────────────────────────────── */
    if (s_bpm_lbl) {
        uint16_t bpm = bridge_get_ai_bpm();
        if (bpm > 0) {
            static char bpm_buf[8];
            snprintf(bpm_buf, sizeof(bpm_buf), "%u", (unsigned)bpm);
            lv_label_set_text(s_bpm_lbl, bpm_buf);
        } else {
            lv_label_set_text(s_bpm_lbl, "\xe2\x80\x94");  /* em dash */
        }
    }
}

/* ── API pública ─────────────────────────────────────────────────────────── */

/**
 * @brief Construye la vista KARAOKE.
 *
 * Layout fijo en el interior del display circular (240×240).
 * Los 12 velocity dots se calculan en coordenadas absolutas desde el centro
 * para que queden equidistantes: anchura total = 12*4 + 11*2 = 70px → x0=-35.
 */
void view_26_create(lv_obj_t* parent) {
    /* Resetear estado */
    s_note_lbl   = nullptr;
    s_octave_lbl = nullptr;
    for (int i = 0; i < 12; i++) s_vel_dot[i] = nullptr;
    s_chord_ctx  = nullptr;
    s_key_ctx    = nullptr;
    s_bpm_lbl    = nullptr;
    s_timer      = nullptr;

    gf_screen_bg(parent);
    gf_arc_title(parent, "PLAYING", GF_COLOR_GRAY);

    /* Note hero — GF_FONT_HERO 36px, centrado en y=−15 */
    s_note_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(s_note_lbl, GF_FONT_HERO, 0);
    lv_obj_set_style_text_color(s_note_lbl, GF_COLOR_WHITE, 0);
    lv_label_set_text(s_note_lbl, "A");
    lv_obj_align(s_note_lbl, LV_ALIGN_CENTER, 0, -15);

    /* Octave — GF_FONT_LABEL 11px, y=+18 */
    s_octave_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(s_octave_lbl, GF_FONT_LABEL, 0);
    lv_obj_set_style_text_color(s_octave_lbl, GF_COLOR_GRAY, 0);
    lv_label_set_text(s_octave_lbl, "4");
    lv_obj_align(s_octave_lbl, LV_ALIGN_CENTER, 0, 18);

    /* Velocity bar — 12 dots de 4×4px, separación 2px, total 70px
     * x0 del primer dot = −(70/2) = −35, cada dot ocupa 6px (4+2) */
    for (int i = 0; i < 12; i++) {
        lv_obj_t* dot = lv_obj_create(parent);
        lv_obj_set_size(dot, 4, 4);
        lv_obj_set_style_radius(dot, 2, 0);
        lv_obj_set_style_bg_color(dot, GF_COLOR_GRAY, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        /* Posición: centro del dot i → x = −35 + i*6 + 2 (mitad del dot) */
        lv_coord_t dx = (lv_coord_t)(-35 + i * 6 + 2);
        lv_obj_align(dot, LV_ALIGN_CENTER, dx, 38);
        lv_obj_set_style_opa(dot, (lv_opa_t)40, 0);
        s_vel_dot[i] = dot;
    }

    /* Chord + Camelot context — GF_FONT_MICRO, gray, y=+60 */
    s_chord_ctx = lv_label_create(parent);
    lv_obj_set_style_text_font(s_chord_ctx, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_chord_ctx, GF_COLOR_GRAY, 0);
    lv_label_set_text(s_chord_ctx, "");
    lv_obj_align(s_chord_ctx, LV_ALIGN_CENTER, 0, 60);

    /* Key context — GF_FONT_MICRO, gray, y=+75 */
    s_key_ctx = lv_label_create(parent);
    lv_obj_set_style_text_font(s_key_ctx, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_key_ctx, GF_COLOR_GRAY, 0);
    lv_label_set_text(s_key_ctx, "");
    lv_obj_align(s_key_ctx, LV_ALIGN_CENTER, 0, 75);

    /* BPM — GF_FONT_MICRO, gray, y=+95 */
    s_bpm_lbl = lv_label_create(parent);
    lv_obj_set_style_text_font(s_bpm_lbl, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_bpm_lbl, GF_COLOR_GRAY, 0);
    lv_label_set_text(s_bpm_lbl, "\xe2\x80\x94");
    lv_obj_align(s_bpm_lbl, LV_ALIGN_CENTER, 0, 95);

    /* Timer 50ms */
    s_timer = lv_timer_create(karaoke_timer_cb, 50, nullptr);
}

/**
 * @brief Destruye el timer de la vista KARAOKE y nulifica punteros.
 *        Los widgets se eliminan por lv_obj_clean() en el carousel.
 */
void view_26_destroy(void) {
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = nullptr;
    }
    s_note_lbl   = nullptr;
    s_octave_lbl = nullptr;
    for (int i = 0; i < 12; i++) s_vel_dot[i] = nullptr;
    s_chord_ctx  = nullptr;
    s_key_ctx    = nullptr;
    s_bpm_lbl    = nullptr;
}
