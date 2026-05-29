/**
 * @file view_02_synth_main.cpp
 * @brief Vista 02 — ENGINE SUB-HOME (Sprint 31 — oscilloscope redesign)
 *
 * Layout 240×240 circular:
 *   A. Nombre del engine — GF_FONT_TITLE teal, dy=-70
 *   B. Canvas osciloscopio 200×64px — onda animada ~30fps, dy=-5
 *      · Forma específica por engine: seno cálido (Moog), coro (Juno), sierra (Prophet)
 *      · Amplitud escala con cutoff [0,1], modulación de resonance
 *      · Fase y respiración basadas en lv_tick_get() → scroll temporal estable
 *   C. Readout cutoff + resonance — GF_FONT_LABEL, dy=+55
 *   D. Hint "NAV → PARAMS" — GF_FONT_MICRO, dy=+88
 *
 * Sin mode pill (eliminado Sprint 31), sin mini-arcs (reemplazados por canvas).
 *
 * Acceso bridge:
 *   bridge_get_engine_name()      — nombre del engine activo
 *   bridge_get_engine_id()        — 0x10-0x12 → engine_idx 0-2
 *   bridge_get_synth_cutoff()     — norm [0,1]
 *   bridge_get_synth_resonance()  — norm [0,1]
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"
#include <stdio.h>
#include <math.h>

/* ── Dimensiones del canvas ─────────────────────────────────────────────── */

#define OSC_W  200
#define OSC_H   64

/* ── Estado persistente ──────────────────────────────────────────────────── */

static lv_obj_t*   s_canvas    = nullptr;
static lv_obj_t*   s_eng_label = nullptr;
static lv_obj_t*   s_cut_label = nullptr;
static lv_obj_t*   s_res_label = nullptr;
static lv_timer_t* s_timer     = nullptr;

/**
 * Envelope de nota: sube al velocity en el tick del NOTE_ON (attack instantáneo),
 * decae exponencialmente tras NOTE_OFF (~450ms half-life a 30fps).
 * Cuando s_envelope ≈ 0, el osciloscopio muestra una onda muy pequeña
 * ("respiración") para que la vista no quede muerta entre notas.
 */
static float s_envelope  = 0.0f;

/**
 * Frecuencia visual actual del osciloscopio (ciclos visibles en el canvas).
 * Se interpola suavemente hacia la frecuencia correspondiente a la nota MIDI,
 * para evitar saltos bruscos cuando cambia la tecla.
 */
static float s_disp_freq = 2.5f;

/**
 * Buffer del canvas — static en BSS (~25KB DRAM ESP32-S3).
 * Permanece entre create/destroy para evitar malloc/free en cada vista.
 * Si DRAM es critica, añadir EXT_RAM_BSS_ATTR para redirigir a PSRAM.
 */
static lv_color_t  s_osc_buf[OSC_W * OSC_H];

/* Puntos de la polyline — static para evitar 800B en stack de la ISR LVGL */
static lv_point_t  s_pts[OSC_W];

/* ── Timer callback ~30fps ─────────────────────────────────────────────────── */

static void subhome_timer_cb(lv_timer_t* /*t*/) {
    if (!s_canvas) return;

    /* Actualizar nombre del engine (puede cambiar por ENGINE_CHANGED en vuelo) */
    if (s_eng_label) lv_label_set_text(s_eng_label, bridge_get_engine_name());

    const float cutoff = bridge_get_synth_cutoff();    /* norm [0,1] */
    const float res    = bridge_get_synth_resonance(); /* norm [0,1] */

    /* ── Canvas osciloscopio ────────────────────────────────────────────── */
    lv_canvas_fill_bg(s_canvas, lv_color_black(), LV_OPA_COVER);

    /* Fase absoluta (no acumuladora) → scroll suave sin deriva por jitter */
    const float t_sec = (float)lv_tick_get() * 0.001f;
    const float phase = fmodf(t_sec * 3.6f, 2.0f * (float)M_PI * 20.0f);

    /* Respiración suave: ±20% amplitud, período ~3s */
    const float breath = 0.80f + 0.20f * sinf(t_sec * 2.094f);

    /* ── Envelope de nota: attack instantáneo, decay exponencial ───────── */
    if (bridge_get_note_active()) {
        /* Attack: interpolación rápida al 60% por tick → ≤2 ticks (66ms) para llegar */
        s_envelope += (bridge_get_note_velocity() - s_envelope) * 0.6f;
    } else {
        /* Decay: ×0.95 por tick a 30fps → half-life ≈ 450ms */
        s_envelope *= 0.95f;
        if (s_envelope < 0.005f) s_envelope = 0.0f;
    }

    /* ── Frecuencia visual desde nota MIDI ──────────────────────────────── */
    /* 2.5 ciclos en canvas para C4 (MIDI 60); cada octava dobla / divide.
     * Rango visual [0.5, 6.0] ciclos — cubre C2 (grave) a D#5 (agudo). */
    float target_freq = 2.5f * powf(2.0f, ((float)bridge_get_note_midi() - 60.0f) / 12.0f);
    if (target_freq < 0.5f) target_freq = 0.5f;
    if (target_freq > 6.0f) target_freq = 6.0f;
    /* Interpolación 50%/tick → suave pero reactiva (~66ms para el 75%) */
    s_disp_freq += (target_freq - s_disp_freq) * 0.5f;

    /* ── Amplitud: base "viva" + envelope de nota ────────────────────────── */
    const float half_h  = (float)(OSC_H / 2) - 3.0f;
    const float amp_base = 0.12f * breath * half_h;             /* siempre animada */
    const float amp_note = s_envelope * (0.30f + cutoff * 0.45f) * half_h;
    const float amp      = amp_base + amp_note;

    /* Engine activo determina la forma de onda */
    const uint8_t eid = bridge_get_engine_id();
    const uint8_t eng = (eid >= 0x10) ? (uint8_t)(eid - 0x10) : 0;

    const int cy = OSC_H / 2;

    for (int x = 0; x < OSC_W; x++) {
        /* Argumento espacial: s_disp_freq ciclos a lo ancho del canvas
         * 2π × freq_ciclos abarca el ancho completo */
        const float t = (float)x / (float)(OSC_W - 1) * 2.0f * (float)M_PI * s_disp_freq + phase;
        float y_val;

        switch (eng) {
            case 0:
                /* Moog Model D — seno cálido con 2º armónico (0.15×) */
                y_val = (sinf(t) + 0.15f * sinf(2.0f * t + 0.5f)) * 0.87f;
                break;
            case 1:
                /* Juno-106 — chorus: doble seno ligeramente desafinado */
                y_val = (sinf(t) + 0.35f * sinf(1.97f * t + 0.25f)
                         + 0.10f * sinf(3.0f * t)) * 0.65f;
                break;
            case 2:
                /* Prophet-5 — sierra sintética: armónicos 1/n hasta el 4º */
                y_val = (sinf(t)
                         + 0.50f * sinf(2.0f * t)
                         + 0.33f * sinf(3.0f * t)
                         + 0.25f * sinf(4.0f * t)) * 0.44f;
                break;
            case 3:
                /* OB-6 — dois VCOs levemente desafinados + sub octave (som gordo) */
                y_val = (sinf(t)
                         + 0.60f * sinf(t + 0.05f)           /* VCO2 desafinado ~3 cents */
                         + 0.30f * sinf(0.5f * t)) * 0.56f;  /* sub osc 1 oct abajo */
                break;
            case 4:
                /* DX7 — FM carrier + modulator: seno com harmônicos FM */
                {
                    float mod = 0.55f * sinf(2.0f * t + phase * 0.5f);  /* modulador 2x */
                    y_val = sinf(t + mod);                               /* carrier FM */
                }
                break;
            case 5:
                /* ARP 2600 — ring mod: componentes soma e diferença de 2 oscs */
                y_val = sinf(t) * sinf(1.07f * t + phase * 0.1f);  /* ring mod clásico */
                break;
            default:
                y_val = sinf(t);
                break;
        }

        /* Resonance añade modulación de alta frecuencia (pico de resonancia) */
        y_val += res * 0.25f * sinf(t * (1.5f + res * 2.0f) + phase * 0.3f);

        int iy = cy - (int)(y_val * amp);
        if (iy < 1)          iy = 1;
        if (iy >= OSC_H - 1) iy = OSC_H - 2;

        s_pts[x].x = (lv_coord_t)x;
        s_pts[x].y = (lv_coord_t)iy;
    }

    lv_draw_line_dsc_t dsc;
    lv_draw_line_dsc_init(&dsc);
    dsc.color = GF_COLOR_TEAL_PLUS;
    dsc.width = 2;
    lv_canvas_draw_line(s_canvas, s_pts, OSC_W, &dsc);

    /* ── Readout: cutoff Hz y resonance Q ───────────────────────────────── */
    if (s_cut_label) {
        char buf[12];
        const float hz = 20.0f + cutoff * 17980.0f;
        if (hz < 1000.0f) snprintf(buf, sizeof(buf), "%.0f Hz", hz);
        else               snprintf(buf, sizeof(buf), "%.1f K",  hz / 1000.0f);
        lv_label_set_text(s_cut_label, buf);
    }
    if (s_res_label) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%.2f Q", 0.7f + res * 4.3f);
        lv_label_set_text(s_res_label, buf);
    }
}

/* ── API pública ─────────────────────────────────────────────────────────── */

void view_02_create(lv_obj_t* parent) {
    s_canvas = s_eng_label = s_cut_label = s_res_label = nullptr;
    s_timer    = nullptr;
    s_envelope = 0.0f;
    s_disp_freq = bridge_get_note_active()
                  ? fmaxf(0.5f, fminf(6.0f,
                      2.5f * powf(2.0f, ((float)bridge_get_note_midi() - 60.0f) / 12.0f)))
                  : 2.5f;

    gf_screen_bg(parent);
    gf_tint(parent, GF_COLOR_TEAL, 20);

    /* A. Nombre del engine */
    lv_obj_t* eng = gf_label(parent, bridge_get_engine_name(),
                             GF_FONT_TITLE, GF_COLOR_TEAL_PLUS);
    if (eng) {
        lv_obj_set_width(eng, 160);
        lv_label_set_long_mode(eng, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(eng, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(eng, LV_ALIGN_CENTER, 0, -70);
    }
    s_eng_label = eng;

    /* B. Canvas osciloscopio — tamaño explícito antes de alinear para que
     *    LVGL 8 no calcule la posición con el tamaño lazy pendiente. */
    lv_obj_t* cv = lv_canvas_create(parent);
    lv_obj_set_size(cv, OSC_W, OSC_H);
    lv_canvas_set_buffer(cv, s_osc_buf, OSC_W, OSC_H, LV_IMG_CF_TRUE_COLOR);
    lv_canvas_fill_bg(cv, lv_color_black(), LV_OPA_COVER);
    lv_obj_align(cv, LV_ALIGN_CENTER, 0, -10);
    s_canvas = cv;

    /* C. Nombres de los parámetros — micro, debajo del canvas */
    lv_obj_t* lc_name = gf_label(parent, "CUT", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS);
    if (lc_name) lv_obj_align(lc_name, LV_ALIGN_CENTER, -42, 46);

    lv_obj_t* lr_name = gf_label(parent, "RES", GF_FONT_MICRO, GF_COLOR_GRAY);
    if (lr_name) lv_obj_align(lr_name, LV_ALIGN_CENTER, +42, 46);

    /* C. Valores — label font, debajo del nombre del parámetro */
    lv_obj_t* cl = gf_label(parent, "---", GF_FONT_LABEL, GF_COLOR_TEAL_PLUS);
    if (cl) lv_obj_align(cl, LV_ALIGN_CENTER, -42, 58);
    s_cut_label = cl;

    lv_obj_t* rl = gf_label(parent, "---", GF_FONT_LABEL, GF_COLOR_GRAY);
    if (rl) lv_obj_align(rl, LV_ALIGN_CENTER, +42, 58);
    s_res_label = rl;

    /* D. Hint */
    lv_obj_t* hint = gf_label(parent, "NAV \xE2\x86\x92 PARAMS",
                              GF_FONT_MICRO, GF_COLOR_GRAY);
    if (hint) {
        lv_obj_set_style_opa(hint, 140, 0);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 92);
    }

    /* Timer ~30fps */
    s_timer = lv_timer_create(subhome_timer_cb, 33, nullptr);
}

void view_02_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    s_canvas = s_eng_label = s_cut_label = s_res_label = nullptr;
}
