/**
 * @file gf_glyphs.cpp
 * @brief Implementacion de los glifos line-art — GrooveForge Brain
 */

#include "gf_glyphs.h"
#include "../ui_theme.h"
#include <esp_heap_caps.h>
#include <math.h>

/* ── Gestion del buffer de canvas ────────────────────────────────────────── */

/* Callback LV_EVENT_DELETE: libera el buffer del canvas cuando el objeto muere
 * (p.ej. al hacer lv_obj_clean() en un cambio de vista). */
static void gf_canvas_del_cb(lv_event_t* e) {
    void* buf = lv_event_get_user_data(e);
    if (buf != nullptr) free(buf);
}

lv_obj_t* gf_canvas_create(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
    const uint32_t sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR_ALPHA(w, h);

    /* Buffer en PSRAM para no consumir el pool de 96KB de LVGL.
     * Fallback a SRAM interna si la PSRAM no esta disponible. */
    void* buf = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
    if (buf == nullptr) buf = heap_caps_malloc(sz, MALLOC_CAP_8BIT);
    if (buf == nullptr) return nullptr;

    lv_obj_t* cv = lv_canvas_create(parent);
    lv_canvas_set_buffer(cv, buf, w, h, LV_IMG_CF_TRUE_COLOR_ALPHA);
    lv_canvas_fill_bg(cv, lv_color_black(), LV_OPA_TRANSP);
    lv_obj_add_event_cb(cv, gf_canvas_del_cb, LV_EVENT_DELETE, buf);
    return cv;
}

/* ── Helpers de dibujo ───────────────────────────────────────────────────── */

static void line_dsc(lv_draw_line_dsc_t* d, lv_color_t color, lv_coord_t width) {
    lv_draw_line_dsc_init(d);
    d->color = color;
    d->width = width;
    d->opa = LV_OPA_COVER;
    d->round_start = 1;
    d->round_end = 1;
}

/* Dibuja un circulo (outline) como polilinea de 24 segmentos. */
static void draw_circle(lv_obj_t* cv, lv_coord_t cx, lv_coord_t cy, lv_coord_t r,
                        lv_color_t color, lv_coord_t width) {
    lv_point_t p[25];
    for (int i = 0; i <= 24; i++) {
        const float a = (float)i / 24.0f * 2.0f * (float)M_PI;
        p[i].x = cx + (lv_coord_t)lroundf(r * cosf(a));
        p[i].y = cy + (lv_coord_t)lroundf(r * sinf(a));
    }
    lv_draw_line_dsc_t d;
    line_dsc(&d, color, width);
    lv_canvas_draw_line(cv, p, 25, &d);
}

/* ── Waveforms ───────────────────────────────────────────────────────────── */

lv_obj_t* gf_glyph_wave(lv_obj_t* parent, gf_wave_t wave, lv_coord_t w, lv_coord_t h,
                        lv_color_t color) {
    lv_obj_t* cv = gf_canvas_create(parent, w, h);
    if (cv == nullptr) return nullptr;

    const int N = 48;
    lv_point_t pts[48];
    const float mid = (h - 1) / 2.0f;
    const float amp = (h - 1) / 2.0f - 1.0f;

    for (int i = 0; i < N; i++) {
        const float t = (float)i / (N - 1);   /* 0..1 — un periodo */
        float v;                               /* -1..1 */
        switch (wave) {
            case GF_WAVE_SINE:   v = sinf(t * 2.0f * (float)M_PI); break;
            case GF_WAVE_TRI:    v = (t < 0.5f) ? (-1.0f + 4.0f * t) : (3.0f - 4.0f * t); break;
            case GF_WAVE_SAW:    v = -1.0f + 2.0f * t; break;
            case GF_WAVE_SQUARE: v = (t < 0.5f) ? -1.0f : 1.0f; break;
            case GF_WAVE_PULSE:  v = (t < 0.25f) ? 1.0f : -1.0f; break;
            default:             v = 0.0f; break;
        }
        pts[i].x = (lv_coord_t)lroundf(t * (w - 1));
        pts[i].y = (lv_coord_t)lroundf(mid - v * amp);
    }

    lv_draw_line_dsc_t d;
    line_dsc(&d, color, 2);
    lv_canvas_draw_line(cv, pts, N, &d);
    return cv;
}

/* ── Flecha (triangulo solido) ───────────────────────────────────────────── */

lv_obj_t* gf_glyph_arrow(lv_obj_t* parent, lv_coord_t size, lv_color_t color,
                         gf_arrow_dir_t dir) {
    lv_obj_t* cv = gf_canvas_create(parent, size, size);
    if (cv == nullptr) return nullptr;

    const lv_coord_t s = size - 1;
    lv_point_t p[3];
    switch (dir) {
        case GF_ARROW_RIGHT: p[0] = {0, 0}; p[1] = {s, s / 2}; p[2] = {0, s}; break;
        case GF_ARROW_LEFT:  p[0] = {s, 0}; p[1] = {0, s / 2}; p[2] = {s, s}; break;
        case GF_ARROW_UP:    p[0] = {0, s}; p[1] = {s / 2, 0}; p[2] = {s, s}; break;
        case GF_ARROW_DOWN:  p[0] = {0, 0}; p[1] = {s / 2, s}; p[2] = {s, 0}; break;
        default:             p[0] = {0, 0}; p[1] = {s, s / 2}; p[2] = {0, s}; break;
    }

    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_color = color;
    d.bg_opa = LV_OPA_COVER;
    lv_canvas_draw_polygon(cv, p, 3, &d);
    return cv;
}

/* ── Checkmark ───────────────────────────────────────────────────────────── */

lv_obj_t* gf_glyph_check(lv_obj_t* parent, lv_coord_t size, lv_color_t color) {
    lv_obj_t* cv = gf_canvas_create(parent, size, size);
    if (cv == nullptr) return nullptr;

    lv_point_t p[3];
    p[0] = {(lv_coord_t)(size * 0.18f), (lv_coord_t)(size * 0.55f)};
    p[1] = {(lv_coord_t)(size * 0.40f), (lv_coord_t)(size * 0.78f)};
    p[2] = {(lv_coord_t)(size * 0.82f), (lv_coord_t)(size * 0.24f)};

    lv_draw_line_dsc_t d;
    line_dsc(&d, color, 3);
    lv_canvas_draw_line(cv, p, 3, &d);
    return cv;
}

/* ── Triangulo de warning ────────────────────────────────────────────────── */

lv_obj_t* gf_glyph_warning(lv_obj_t* parent, lv_coord_t size, lv_color_t color) {
    lv_obj_t* cv = gf_canvas_create(parent, size, size);
    if (cv == nullptr) return nullptr;

    const lv_coord_t s = size - 1;
    lv_draw_line_dsc_t d;
    line_dsc(&d, color, 2);

    /* Contorno del triangulo (cerrado). */
    lv_point_t tri[4] = {
        {(lv_coord_t)(s / 2), 1}, {s, s}, {0, s}, {(lv_coord_t)(s / 2), 1}
    };
    lv_canvas_draw_line(cv, tri, 4, &d);

    /* Barra del signo de exclamacion. */
    lv_point_t bar[2] = {
        {(lv_coord_t)(s / 2), (lv_coord_t)(size * 0.38f)},
        {(lv_coord_t)(s / 2), (lv_coord_t)(size * 0.66f)}
    };
    lv_canvas_draw_line(cv, bar, 2, &d);

    /* Punto del signo de exclamacion (segmento corto). */
    lv_point_t dot[2] = {
        {(lv_coord_t)(s / 2), (lv_coord_t)(size * 0.80f)},
        {(lv_coord_t)(s / 2), (lv_coord_t)(size * 0.82f)}
    };
    lv_canvas_draw_line(cv, dot, 2, &d);
    return cv;
}

/* ── Glifos FX (12) ──────────────────────────────────────────────────────── */

/*
 * Marca line-art distinta por FX. Orden segun 01-architecture.md §3.4:
 * 0 Cymatic · 1 Granular · 2 Ghost Echo · 3 Spectral Smear · 4 Tape Saturate ·
 * 5 Bit Sculpt · 6 Modal Reverb · 7 Phase Chorus · 8 Pitch Mosaic ·
 * 9 Spring+Plate · 10 Glitch Stutter · 11 Sub Genesis.
 */
lv_obj_t* gf_glyph_fx(lv_obj_t* parent, uint8_t fx_index, lv_coord_t size, lv_color_t color) {
    lv_obj_t* cv = gf_canvas_create(parent, size, size);
    if (cv == nullptr) return nullptr;

    const lv_coord_t s = size - 1;
    const lv_coord_t c = size / 2;
    lv_draw_line_dsc_t d;
    line_dsc(&d, color, 2);

    switch (fx_index) {
        case 0: /* Cymatic — circulos concentricos */
            draw_circle(cv, c, c, (lv_coord_t)(s * 0.42f), color, 2);
            draw_circle(cv, c, c, (lv_coord_t)(s * 0.20f), color, 2);
            break;
        case 1: { /* Granular — nube de puntos */
            const float gx[6] = {0.25f, 0.55f, 0.40f, 0.70f, 0.30f, 0.65f};
            const float gy[6] = {0.30f, 0.25f, 0.55f, 0.50f, 0.72f, 0.75f};
            lv_draw_rect_dsc_t r;
            lv_draw_rect_dsc_init(&r);
            r.bg_color = color; r.bg_opa = LV_OPA_COVER; r.radius = LV_RADIUS_CIRCLE;
            for (int i = 0; i < 6; i++) {
                lv_canvas_draw_rect(cv, (lv_coord_t)(s * gx[i]), (lv_coord_t)(s * gy[i]),
                                    3, 3, &r);
            }
            break;
        }
        case 2: { /* Ghost Echo — taps decrecientes */
            for (int i = 0; i < 4; i++) {
                const lv_coord_t x = (lv_coord_t)(s * (0.15f + i * 0.23f));
                const lv_coord_t hh = (lv_coord_t)(s * (0.62f - i * 0.14f));
                lv_point_t l[2] = {{x, s}, {x, (lv_coord_t)(s - hh)}};
                lv_canvas_draw_line(cv, l, 2, &d);
            }
            break;
        }
        case 3: { /* Spectral Smear — lineas horizontales difusas */
            for (int i = 0; i < 4; i++) {
                const lv_coord_t y = (lv_coord_t)(s * (0.25f + i * 0.17f));
                lv_point_t l[2] = {{(lv_coord_t)(s * 0.12f), y},
                                   {(lv_coord_t)(s * (0.55f + i * 0.10f)), y}};
                lv_canvas_draw_line(cv, l, 2, &d);
            }
            break;
        }
        case 4: { /* Tape Saturate — curva en S */
            lv_point_t p[7];
            for (int i = 0; i < 7; i++) {
                const float t = (float)i / 6.0f;
                p[i].x = (lv_coord_t)(s * t);
                p[i].y = (lv_coord_t)(c - (s * 0.42f) * tanhf((t - 0.5f) * 4.0f));
            }
            lv_canvas_draw_line(cv, p, 7, &d);
            break;
        }
        case 5: { /* Bit Sculpt — escalera */
            lv_point_t p[9];
            for (int i = 0; i < 9; i++) {
                p[i].x = (lv_coord_t)(s * i / 8.0f);
                p[i].y = (lv_coord_t)(s - s * ((i / 2) / 4.0f));
            }
            lv_canvas_draw_line(cv, p, 9, &d);
            break;
        }
        case 6: /* Modal Reverb — arcos expandiendo desde una esquina */
            draw_circle(cv, 2, s, (lv_coord_t)(s * 0.45f), color, 2);
            draw_circle(cv, 2, s, (lv_coord_t)(s * 0.80f), color, 2);
            break;
        case 7: { /* Phase Chorus — dos senos desfasados */
            lv_point_t a[24], b[24];
            for (int i = 0; i < 24; i++) {
                const float t = (float)i / 23.0f;
                a[i].x = (lv_coord_t)(s * t);
                a[i].y = (lv_coord_t)(c - (s * 0.30f) * sinf(t * 2.0f * (float)M_PI));
                b[i].x = a[i].x;
                b[i].y = (lv_coord_t)(c - (s * 0.30f) * sinf(t * 2.0f * (float)M_PI + 1.2f));
            }
            lv_canvas_draw_line(cv, a, 24, &d);
            lv_canvas_draw_line(cv, b, 24, &d);
            break;
        }
        case 8: { /* Pitch Mosaic — rejilla de cuadros */
            lv_draw_rect_dsc_t r;
            lv_draw_rect_dsc_init(&r);
            r.bg_opa = LV_OPA_TRANSP;
            r.border_color = color; r.border_width = 2; r.border_opa = LV_OPA_COVER;
            const lv_coord_t q = (lv_coord_t)(s * 0.28f);
            for (int gy = 0; gy < 2; gy++)
                for (int gx = 0; gx < 2; gx++)
                    lv_canvas_draw_rect(cv, (lv_coord_t)(s * 0.12f + gx * (q + 4)),
                                        (lv_coord_t)(s * 0.12f + gy * (q + 4)), q, q, &r);
            break;
        }
        case 9: { /* Spring + Plate — espiral en zigzag */
            lv_point_t p[13];
            for (int i = 0; i < 13; i++) {
                p[i].x = (lv_coord_t)(s * i / 12.0f);
                p[i].y = (lv_coord_t)((i % 2) ? (s * 0.30f) : (s * 0.70f));
            }
            lv_canvas_draw_line(cv, p, 13, &d);
            break;
        }
        case 10: { /* Glitch Stutter — segmentos rotos desplazados */
            for (int i = 0; i < 4; i++) {
                const lv_coord_t y = (lv_coord_t)(s * (0.25f + i * 0.17f));
                const lv_coord_t x0 = (lv_coord_t)(s * ((i % 2) ? 0.30f : 0.08f));
                lv_point_t l[2] = {{x0, y}, {(lv_coord_t)(x0 + s * 0.50f), y}};
                lv_canvas_draw_line(cv, l, 2, &d);
            }
            break;
        }
        case 11: { /* Sub Genesis — onda grave de gran amplitud */
            lv_point_t p[24];
            for (int i = 0; i < 24; i++) {
                const float t = (float)i / 23.0f;
                p[i].x = (lv_coord_t)(s * t);
                p[i].y = (lv_coord_t)(c - (s * 0.46f) * sinf(t * (float)M_PI));
            }
            lv_canvas_draw_line(cv, p, 24, &d);
            break;
        }
        default:
            draw_circle(cv, c, c, (lv_coord_t)(s * 0.40f), color, 2);
            break;
    }
    return cv;
}
