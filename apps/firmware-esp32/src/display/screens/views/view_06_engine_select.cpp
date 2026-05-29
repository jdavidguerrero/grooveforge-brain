/**
 * @file view_06_engine_select.cpp
 * @brief Vista 06 — ENGINE SELECT (Sprint 31 — card carousel redesign)
 *
 * Carrusel vertical de 3 tarjetas sobre display circular 240×240:
 *
 *   ┌──────────────────────────────┐
 *   │      ENGINE                  │ ← label teal dy=-98
 *   │  ┌──────────────────────┐    │
 *   │  │ JUNO-106 (dim 40%)   │    │ ← slot 0  dy=-56  160×44
 *   │  └──────────────────────┘    │
 *   │  ┌───────────────────────┐   │
 *   │  │  MOOG MODEL D  ←─────────── slot 1 (activo)  dy=0  205×62
 *   │  │  BASS / LEADS / MONO  │   │
 *   │  └───────────────────────┘   │
 *   │  ┌──────────────────────┐    │
 *   │  │ PROPHET-5 (dim 40%)  │    │ ← slot 2  dy=+58  160×44
 *   │  └──────────────────────┘    │
 *   │      NAV TO SELECT           │ ← hint  dy=+92
 *   └──────────────────────────────┘
 *
 * Solo engines seleccionables (0-2). Cursor llega de bridge_get_engine_cursor().
 * Sin code boxes, sin scrollbar, sin mode pill.
 *
 * Timer 100ms: actualiza los 3 slots cuando el cursor del Teensy cambia.
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"

/* ── Datos de los engines ────────────────────────────────────────────────── */

static constexpr uint8_t NUM_SEL = 6;

static const char* ENG_NAMES[NUM_SEL] = {
    "MOOG MODEL D",
    "JUNO-106",
    "PROPHET-5",
    "OB-6",
    "DX7",
    "ARP 2600",
};

/**
 * Sugerencias de estilo por engine — aparecen solo en la tarjeta activa (slot 1).
 * Separador "/" para garantizar compatibilidad con el charset de ibm_plex_mono_8.
 */
static const char* ENG_TAGS[NUM_SEL] = {
    "BASS / LEADS / MONO",
    "PADS / STRINGS / CHORUS",
    "CHORDS / POLY / LUSH",
    "CHORDS / AI / POLY",
    "FM / BELLS / DIGITAL",
    "RING MOD / SEMI / RAW",
};

/* ── Geometría de slots ──────────────────────────────────────────────────── */

/** dy desde el centro de pantalla de cada slot (top, center, bottom) */
static constexpr lv_coord_t SLOT_DY[3] = { -56, 0, 58 };
/** Ancho de la tarjeta de cada slot */
static constexpr lv_coord_t SLOT_W[3]  = { 160, 205, 160 };
/** Alto de la tarjeta de cada slot */
static constexpr lv_coord_t SLOT_H[3]  = {  44,  62,  44 };
/** Opacidad de los slots adyacentes (0=transparente, 255=sólido) */
static constexpr lv_opa_t   ADJ_OPA    = 100;   ///< ~39% de opacidad

/* ── Estado persistente ──────────────────────────────────────────────────── */

static lv_obj_t*   s_slot_name[3] = {};  ///< label del nombre en cada slot
static lv_obj_t*   s_slot_tags    = nullptr;  ///< tags de estilo, solo slot 1
static lv_timer_t* s_timer        = nullptr;
static uint8_t     s_last_cursor  = 0xFF;

/* ── Helper: asignar engine a cada slot según cursor activo ──────────────── */

static void update_cards(uint8_t cursor) {
    /* Slot 0 (top)    → engine anterior en el ciclo */
    /* Slot 1 (center) → engine activo */
    /* Slot 2 (bottom) → engine siguiente en el ciclo */
    const uint8_t e[3] = {
        (uint8_t)((cursor + NUM_SEL - 1) % NUM_SEL),  /* top    = prev */
        cursor,                                         /* center = active */
        (uint8_t)((cursor + 1)           % NUM_SEL),  /* bottom = next */
    };

    for (uint8_t s = 0; s < 3; s++) {
        if (s_slot_name[s]) lv_label_set_text(s_slot_name[s], ENG_NAMES[e[s]]);
    }
    if (s_slot_tags) lv_label_set_text(s_slot_tags, ENG_TAGS[e[1]]);
}

/* ── Timer callback 100ms ────────────────────────────────────────────────── */

static void engine_timer_cb(lv_timer_t* /*t*/) {
    uint8_t cursor = bridge_get_engine_cursor();
    if (cursor >= NUM_SEL) cursor = 0;
    if (cursor == s_last_cursor) return;
    s_last_cursor = cursor;
    update_cards(cursor);
}

/* ── Entrada principal ───────────────────────────────────────────────────── */

void view_06_create(lv_obj_t* parent) {
    for (uint8_t i = 0; i < 3; i++) s_slot_name[i] = nullptr;
    s_slot_tags   = nullptr;
    s_timer       = nullptr;
    s_last_cursor = 0xFF;

    gf_screen_bg(parent);
    gf_tint(parent, GF_COLOR_TEAL, 18);

    /* ── Título "ENGINE" — simple, centrado arriba ───────────────────────── */
    lv_obj_t* title = gf_label(parent, "ENGINE", GF_FONT_LABEL, GF_COLOR_TEAL);
    if (title) lv_obj_align(title, LV_ALIGN_CENTER, 0, -98);

    /* ── 3 tarjetas (slots) ──────────────────────────────────────────────── */
    uint8_t cursor = bridge_get_engine_cursor();
    if (cursor >= NUM_SEL) cursor = 0;

    for (uint8_t s = 0; s < 3; s++) {
        const bool is_center = (s == 1);

        /* Contenedor de la tarjeta */
        lv_obj_t* card = lv_obj_create(parent);
        lv_obj_set_size(card, SLOT_W[s], SLOT_H[s]);
        lv_obj_align(card, LV_ALIGN_CENTER, 0, SLOT_DY[s]);
        lv_obj_set_style_radius(card, 8, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

        /* Fondo y borde — solo la tarjeta central tiene acento teal */
        if (is_center) {
            lv_obj_set_style_bg_color(card, GF_COLOR_TEAL, 0);
            lv_obj_set_style_bg_opa(card, 22, 0);        /* sutil: ~9% */
            lv_obj_set_style_border_color(card, GF_COLOR_TEAL, 0);
            lv_obj_set_style_border_width(card, 1, 0);
            lv_obj_set_style_border_opa(card, LV_OPA_COVER, 0);
        } else {
            lv_obj_set_style_bg_opa(card, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(card, 0, 0);
            /* Atenuar toda la tarjeta (incluye labels hijos) */
            lv_obj_set_style_opa(card, ADJ_OPA, 0);
        }

        /* Nombre del engine — ancho fijo para que el centrado no varíe
         * al cambiar el texto en el timer (label_long_clip + text_align_center). */
        const lv_coord_t name_w = SLOT_W[s] - 10;
        lv_obj_t* nm = gf_label(card,
                                 is_center ? "---" : "---",
                                 is_center ? GF_FONT_BODY : GF_FONT_LABEL,
                                 is_center ? GF_COLOR_WHITE : GF_COLOR_GRAY);
        lv_obj_set_width(nm, name_w);
        lv_label_set_long_mode(nm, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(nm, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(nm, LV_ALIGN_CENTER, 0, is_center ? -10 : 0);
        s_slot_name[s] = nm;

        /* Tags de estilo — solo en la tarjeta central */
        if (is_center) {
            lv_obj_t* tags = gf_label(card, "---", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS);
            lv_obj_set_width(tags, name_w);
            lv_label_set_long_mode(tags, LV_LABEL_LONG_CLIP);
            lv_obj_set_style_text_align(tags, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_align(tags, LV_ALIGN_CENTER, 0, +18);
            s_slot_tags = tags;
        }
    }

    /* Rellenar contenido inicial */
    update_cards(cursor);
    s_last_cursor = cursor;

    /* ── Hint ────────────────────────────────────────────────────────────── */
    lv_obj_t* hint = gf_label(parent, "NAV TO SELECT", GF_FONT_MICRO, GF_COLOR_GRAY);
    if (hint) {
        lv_obj_set_style_opa(hint, 140, 0);
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 92);
    }

    /* Timer 100ms: actualiza highlight de la tarjeta activa */
    s_timer = lv_timer_create(engine_timer_cb, 100, nullptr);
}

void view_06_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    for (uint8_t i = 0; i < 3; i++) s_slot_name[i] = nullptr;
    s_slot_tags = nullptr;
}
