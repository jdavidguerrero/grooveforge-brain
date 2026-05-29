/**
 * @file view_25_ai_models.cpp
 * @brief Vista 25 — AI MODELS (Sprint 32)
 *
 * Muestra el estado de los 4 modelos de ML en modo AI y permite activar/
 * desactivar los modelos que soportan bypass:
 *
 *   KEY DETECT    → ACTIVE / IDLE  (status, no toggleable desde display)
 *   CHORD DETECT  → ACTIVE / IDLE  (status, no toggleable desde display)
 *   BEAT FOLLOW   → ON / OFF       (toggle via Bridge param_id 0x00F3)
 *   SCALE LOCK    → ON / OFF       (toggle via Bridge param_id 0x00F2)
 *
 * Layout (4 filas equidistantes):
 *   [dot 6px]  [NOMBRE MODELO]  [STATUS]
 *
 * El timer de 50ms lee bridge_get_ai_* y bridge_get_*_bypass() para
 * actualizar opacidad y texto de cada fila en-place sin rebuild.
 *
 * Ref: apps/docs/sprints/32-ml-engine-integration.md
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"

/* ── Layout ──────────────────────────────────────────────────────────────── */

/* Desplazamientos verticales de cada fila (dy desde centro de pantalla). */
static constexpr lv_coord_t ROW_DY[4] = { -52, -26, 0, 26 };

/* Desplazamientos horizontales de los tres elementos de cada fila. */
static constexpr lv_coord_t DX_DOT    = -68;  ///< centro del dot indicator
static constexpr lv_coord_t DX_NAME   = -18;  ///< centro del label de nombre
static constexpr lv_coord_t DX_STATUS =  55;  ///< centro del label de estado

/* Nombres de los modelos (orden coincide con ROW_DY). */
static const char* const MODEL_NAMES[4] = {
    "KEY DETECT",
    "CHORD DETECT",
    "BEAT FOLLOW",
    "SCALE LOCK",
};

/* ── Estado de la vista ──────────────────────────────────────────────────── */

typedef struct {
    lv_obj_t* dot;     ///< indicador de punto izquierdo
    lv_obj_t* status;  ///< texto ON/OFF/ACTIVE/IDLE
} ai_model_row_t;

static ai_model_row_t s_rows[4]  = {};
static lv_timer_t*    s_timer    = nullptr;

/* ── Timer de actualización 50ms ─────────────────────────────────────────── */

static void models_timer_cb(lv_timer_t* /*t*/) {
    /* Fila 0 — KEY DETECT: activo cuando ya se recibió una tonalidad */
    {
        bool active = (bridge_get_ai_key_name()[0] != '-');
        lv_opa_t opa = active ? LV_OPA_COVER : (lv_opa_t)90;
        if (s_rows[0].dot)    lv_obj_set_style_opa(s_rows[0].dot,    opa, 0);
        if (s_rows[0].status) {
            lv_label_set_text(s_rows[0].status, active ? "ACTIVE" : "IDLE");
            lv_obj_set_style_text_color(s_rows[0].status,
                active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
            lv_obj_set_style_opa(s_rows[0].status, opa, 0);
        }
    }

    /* Fila 1 — CHORD DETECT: activo cuando ya se recibió un acorde */
    {
        bool active = (bridge_get_ai_chord_name()[0] != '-');
        lv_opa_t opa = active ? LV_OPA_COVER : (lv_opa_t)90;
        if (s_rows[1].dot)    lv_obj_set_style_opa(s_rows[1].dot,    opa, 0);
        if (s_rows[1].status) {
            lv_label_set_text(s_rows[1].status, active ? "ACTIVE" : "IDLE");
            lv_obj_set_style_text_color(s_rows[1].status,
                active ? GF_COLOR_TEAL_PLUS : GF_COLOR_GRAY, 0);
            lv_obj_set_style_opa(s_rows[1].status, opa, 0);
        }
    }

    /* Fila 2 — BEAT FOLLOW: ON cuando bypass=false */
    {
        bool off = bridge_get_beat_follower_bypass();
        lv_opa_t opa = off ? (lv_opa_t)90 : LV_OPA_COVER;
        if (s_rows[2].dot)    lv_obj_set_style_opa(s_rows[2].dot,    opa, 0);
        if (s_rows[2].status) {
            lv_label_set_text(s_rows[2].status, off ? "OFF" : "ON");
            lv_obj_set_style_text_color(s_rows[2].status,
                off ? GF_COLOR_GRAY : GF_COLOR_WHITE, 0);
            lv_obj_set_style_opa(s_rows[2].status, opa, 0);
        }
    }

    /* Fila 3 — SCALE LOCK: ON cuando bypass=false */
    {
        bool off = bridge_get_scale_lock_bypass();
        lv_opa_t opa = off ? (lv_opa_t)90 : LV_OPA_COVER;
        if (s_rows[3].dot)    lv_obj_set_style_opa(s_rows[3].dot,    opa, 0);
        if (s_rows[3].status) {
            lv_label_set_text(s_rows[3].status, off ? "OFF" : "ON");
            lv_obj_set_style_text_color(s_rows[3].status,
                off ? GF_COLOR_GRAY : GF_COLOR_TEAL_PLUS, 0);
            lv_obj_set_style_opa(s_rows[3].status, opa, 0);
        }
    }
}

/* ── Helpers de construcción de fila ──────────────────────────────────────── */

/**
 * @brief Construye los tres widgets de una fila (dot + nombre + status).
 *
 * @param parent  Pantalla padre.
 * @param idx     Índice de fila (0-3).
 * @param name    Texto del nombre del modelo.
 * @param status_txt  Texto inicial del status.
 * @param active  Estado inicial (true=activo/ON).
 * @param color   Color del texto del nombre (TEAL_PLUS para key/chord, WHITE para beat/scale).
 */
static void build_row(lv_obj_t* parent, uint8_t idx,
                      const char* name, const char* status_txt,
                      bool active, lv_color_t color) {
    const lv_coord_t dy = ROW_DY[idx];
    const lv_opa_t   opa = active ? LV_OPA_COVER : (lv_opa_t)90;

    /* Dot indicador */
    lv_obj_t* dot = gf_dot(parent, 6, color);
    lv_obj_align(dot, LV_ALIGN_CENTER, DX_DOT, dy);
    lv_obj_set_style_opa(dot, opa, 0);
    s_rows[idx].dot = dot;

    /* Nombre del modelo */
    lv_obj_t* lbl = gf_label(parent, name, GF_FONT_MICRO, color);
    if (lbl) lv_obj_align(lbl, LV_ALIGN_CENTER, DX_NAME, dy);

    /* Status (ON/OFF/ACTIVE/IDLE) */
    lv_color_t sc = active ? color : GF_COLOR_GRAY;
    lv_obj_t* st  = gf_label(parent, status_txt, GF_FONT_MICRO, sc);
    if (st) {
        lv_obj_align(st, LV_ALIGN_CENTER, DX_STATUS, dy);
        lv_obj_set_style_opa(st, opa, 0);
    }
    s_rows[idx].status = st;
}

/* ── API pública ──────────────────────────────────────────────────────────── */

void view_25_create(lv_obj_t* parent) {
    for (uint8_t i = 0; i < 4; i++) s_rows[i] = ai_model_row_t{};
    s_timer = nullptr;

    gf_screen_bg(parent);
    gf_arc_title(parent, "AI MODELS", GF_COLOR_TEAL);

    /* Fila 0 — KEY DETECT */
    {
        bool active = (bridge_get_ai_key_name()[0] != '-');
        build_row(parent, 0, MODEL_NAMES[0],
                  active ? "ACTIVE" : "IDLE", active, GF_COLOR_TEAL_PLUS);
    }

    /* Fila 1 — CHORD DETECT */
    {
        bool active = (bridge_get_ai_chord_name()[0] != '-');
        build_row(parent, 1, MODEL_NAMES[1],
                  active ? "ACTIVE" : "IDLE", active, GF_COLOR_TEAL_PLUS);
    }

    /* Fila 2 — BEAT FOLLOW (toggleable) */
    {
        bool on = !bridge_get_beat_follower_bypass();
        build_row(parent, 2, MODEL_NAMES[2],
                  on ? "ON" : "OFF", on, GF_COLOR_WHITE);
    }

    /* Fila 3 — SCALE LOCK (toggleable) */
    {
        bool on = !bridge_get_scale_lock_bypass();
        build_row(parent, 3, MODEL_NAMES[3],
                  on ? "ON" : "OFF", on, GF_COLOR_TEAL_PLUS);
    }

    /* Mode pill */
    gf_mode_pill(parent, "MODELS", GF_COLOR_TEAL);

    /* Timer 50ms para actualizar estados desde bridge */
    s_timer = lv_timer_create(models_timer_cb, 50, nullptr);
}

void view_25_destroy(void) {
    if (s_timer) { lv_timer_del(s_timer); s_timer = nullptr; }
    for (uint8_t i = 0; i < 4; i++) s_rows[i] = ai_model_row_t{};
}
