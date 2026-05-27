/**
 * @file view_10_ai_processing.cpp
 * @brief Vista 10 — AI PROCESSING + Overlay Siri-like durante hold ENC NAV
 *
 * ## Dos modos de operación
 *
 * ### Modo overlay (durante hold 0→4s)
 *   Activado por `view_10_overlay_start()` desde `carousel_ai_overlay_start()`.
 *   La vista anterior permanece en pantalla; este módulo crea encima:
 *     - Un dimmer negro 40% opacidad (→ vista anterior al 60% visualmente)
 *     - El arco de progreso (200px, 6px grosor) que se llena 0→360°
 *     - Un dot central que "respira" (12→18px, 800ms period)
 *     - Label "ANALYZING..." que hace fade-in a t=1s
 *   Si el usuario suelta antes de 4s → `view_10_overlay_cancel()` → fade-out
 *   200ms + delete; la vista anterior reaparece intacta.
 *   Si se completa → `view_10_overlay_destroy_immediate()` (el carousel hace
 *   flash blanco + carousel_goto(VIEW_IDX_AI_PROC) para el modo full).
 *
 * ### Modo full (vista completa post-hold)
 *   Construida por `view_10_create()` / destruida por `view_10_destroy()`.
 *   Muestra: arco 220px, 8 orbit dots, core breathing, textos, bypass pills.
 *   El timer de 50ms actualiza arco y pills en-place sin rebuild.
 *
 * Spec: apps/docs/sprints/30-navigation-rmx-style.md §3.6.3d
 */

#include "views.h"
#include "../../ui_theme.h"
#include "../../widgets/gf_widgets.h"
#include "../../../bridge/bridge_handlers.h"
#include <math.h>

/* ── Callbacks de animación compartidos (overlay + full) ──────────────────── */

/** exec_cb: opacidad de un lv_obj_t. */
static void opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(v), 0);
}

/**
 * exec_cb: tamaño del core (respiración) — recentra al cambiar de tamaño.
 * Funciona tanto si el padre es lv_scr_act() (modo full) como s_ov_dimmer
 * (modo overlay), porque lv_obj_center() centra dentro del padre.
 */
static void core_size_cb(void* obj, int32_t v) {
    lv_obj_t* d = static_cast<lv_obj_t*>(obj);
    lv_obj_set_size(d, v, v);
    lv_obj_set_style_radius(d, v / 2, 0);
    lv_obj_center(d);
}

/* ══════════════════════════════════════════════════════════════════════════
 * MODO OVERLAY — estado y funciones
 * ══════════════════════════════════════════════════════════════════════════ */

static lv_obj_t*   s_ov_dimmer      = nullptr;  ///< contenedor semi-opaco (40% negro)
static lv_obj_t*   s_ov_arc         = nullptr;  ///< arco de progreso 0→360°
static lv_obj_t*   s_ov_core        = nullptr;  ///< dot central que respira
static lv_obj_t*   s_ov_label       = nullptr;  ///< "ANALYZING..."
static lv_timer_t* s_ov_timer       = nullptr;  ///< timer 50ms actualiza arco
static lv_timer_t* s_ov_fadein_tmr  = nullptr;  ///< one-shot 1s para fade-in label

static void ov_progress_timer_cb(lv_timer_t* /*t*/) {
    if (!s_ov_arc) return;
    float p = bridge_get_ai_progress();
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;
    /* Arco desde 270° (top) en sentido horario: llena 0→360° con el progreso. */
    lv_arc_set_value(s_ov_arc, (int16_t)(p * 360.0f + 0.5f));
}

/**
 * @brief Inicia el overlay AI sobre la vista actual.
 *
 * Crea un dimmer + arco + dot + label sobre @p screen (lv_scr_act()).
 * No destruye ni toca los widgets de la vista anterior.
 *
 * @param screen  lv_scr_act() — los widgets del overlay serán hijos de él.
 */
void view_10_overlay_start(lv_obj_t* screen) {
    /* ── 1. Dimmer container ────────────────────────────────────────────── */
    /* 40% negro opaco = vista anterior percibida al ~60% de brillo.
     * No es lo mismo que LV_OPA_60 en la vista, pero el efecto visual es
     * equivalente sin necesidad de un container intermedio por vista. */
    s_ov_dimmer = lv_obj_create(screen);
    lv_obj_set_size(s_ov_dimmer, 240, 240);
    lv_obj_set_pos(s_ov_dimmer, 0, 0);
    lv_obj_set_style_bg_color(s_ov_dimmer, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ov_dimmer, LV_OPA_TRANSP, 0);   /* empieza transparente */
    lv_obj_set_style_border_width(s_ov_dimmer, 0, 0);
    lv_obj_set_style_radius(s_ov_dimmer, 0, 0);
    lv_obj_set_style_pad_all(s_ov_dimmer, 0, 0);
    lv_obj_clear_flag(s_ov_dimmer, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    /* Fade-in del dimmer en 100ms hasta 40% opacidad (102/255). */
    lv_anim_t da;
    lv_anim_init(&da);
    lv_anim_set_var(&da, s_ov_dimmer);
    lv_anim_set_exec_cb(&da, opa_cb);
    lv_anim_set_values(&da, 0, 102);    /* 0 → LV_OPA_40 */
    lv_anim_set_time(&da, 100);
    lv_anim_start(&da);

    /* ── 2. Arco de progreso (200×200, centrado, 6px grosor) ───────────── */
    /* 200px (radio 100) en vez de los 220px del full-view para que quepa con
     * margen en el dimmer 240×240. La diferencia visual es mínima. */
    s_ov_arc = lv_arc_create(s_ov_dimmer);
    lv_obj_set_size(s_ov_arc, 200, 200);
    lv_obj_center(s_ov_arc);
    lv_arc_set_rotation(s_ov_arc, 270);         /* empieza desde las 12h */
    lv_arc_set_bg_angles(s_ov_arc, 0, 360);
    lv_arc_set_range(s_ov_arc, 0, 360);
    lv_arc_set_value(s_ov_arc, 0);
    lv_obj_remove_style(s_ov_arc, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_color(s_ov_arc, GF_COLOR_TEAL_DIM,  LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_ov_arc, GF_COLOR_TEAL_PLUS, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_ov_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_ov_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_ov_arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(s_ov_arc, LV_OBJ_FLAG_CLICKABLE);

    /* ── 3. Dot central que respira (12→18px, 800ms) ───────────────────── */
    s_ov_core = gf_dot(s_ov_dimmer, 12, GF_COLOR_TEAL_PLUS);
    lv_obj_center(s_ov_core);

    lv_anim_t ca;
    lv_anim_init(&ca);
    lv_anim_set_var(&ca, s_ov_core);
    lv_anim_set_exec_cb(&ca, core_size_cb);
    lv_anim_set_values(&ca, 12, 18);
    lv_anim_set_time(&ca, 800);
    lv_anim_set_playback_time(&ca, 800);
    lv_anim_set_repeat_count(&ca, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&ca);

    /* ── 4. Label "ANALYZING..." — invisible, fade-in a t=1s ───────────── */
    s_ov_label = lv_label_create(s_ov_dimmer);
    lv_label_set_text(s_ov_label, "ANALYZING...");
    lv_obj_set_style_text_font(s_ov_label, GF_FONT_MICRO, 0);
    lv_obj_set_style_text_color(s_ov_label, GF_COLOR_TEAL_PLUS, 0);
    lv_obj_set_style_opa(s_ov_label, LV_OPA_TRANSP, 0);
    lv_obj_align(s_ov_label, LV_ALIGN_CENTER, 0, 28);

    s_ov_fadein_tmr = lv_timer_create([](lv_timer_t* t) {
        lv_timer_del(t);
        s_ov_fadein_tmr = nullptr;
        if (!s_ov_label) return;
        lv_anim_t la;
        lv_anim_init(&la);
        lv_anim_set_var(&la, s_ov_label);
        lv_anim_set_exec_cb(&la, opa_cb);
        lv_anim_set_values(&la, 0, LV_OPA_COVER);
        lv_anim_set_time(&la, 300);
        lv_anim_start(&la);
    }, 1000, nullptr);

    /* ── 5. Timer de actualización del arco (50ms = 20fps) ─────────────── */
    s_ov_timer = lv_timer_create(ov_progress_timer_cb, 50, nullptr);
}

/**
 * @brief Cancela el overlay (usuario soltó ENC NAV antes de 4s).
 *
 * Anima el dimmer a OPA_TRANSP en 200ms y luego lo elimina.
 * La vista anterior queda intacta y visible.
 */
void view_10_overlay_cancel(void) {
    if (!s_ov_dimmer) return;

    /* Detener timers activos del overlay. */
    if (s_ov_timer) {
        lv_timer_del(s_ov_timer);
        s_ov_timer = nullptr;
    }
    if (s_ov_fadein_tmr) {
        lv_timer_del(s_ov_fadein_tmr);
        s_ov_fadein_tmr = nullptr;
    }

    /* Cancelar animaciones pendientes sobre objetos del overlay. */
    if (s_ov_core)   lv_anim_del(s_ov_core,   core_size_cb);
    if (s_ov_label)  lv_anim_del(s_ov_label,   opa_cb);
    lv_anim_del(s_ov_dimmer, opa_cb);

    /* Guardar el puntero antes de nulificar — el ready_cb lo necesita. */
    lv_obj_t* dimmer = s_ov_dimmer;
    s_ov_dimmer = nullptr;
    s_ov_arc    = nullptr;
    s_ov_core   = nullptr;
    s_ov_label  = nullptr;

    /* Fade-out del dimmer (y todos sus hijos) en 200ms, luego delete. */
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, dimmer);
    lv_anim_set_exec_cb(&a, opa_cb);
    lv_anim_set_values(&a, 102, 0);
    lv_anim_set_time(&a, 200);
    lv_anim_set_ready_cb(&a, [](lv_anim_t* a) {
        lv_obj_del(static_cast<lv_obj_t*>(a->var));
    });
    lv_anim_start(&a);
}

/**
 * @brief Destruye el overlay inmediatamente sin animación de fade.
 *
 * Usado cuando carousel_goto() interrumpe el hold (ej. PANIC, timeout).
 * carousel_goto() llama lv_obj_clean() justo después — por eso es seguro
 * simplemente matar el timer y dejar que lv_obj_clean lo elimine,
 * pero hacemos lv_obj_del explícito para cancelar animaciones pendientes
 * antes de que el pool global las dispare contra un objeto eliminado.
 */
void view_10_overlay_destroy_immediate(void) {
    if (s_ov_timer) {
        lv_timer_del(s_ov_timer);
        s_ov_timer = nullptr;
    }
    if (s_ov_fadein_tmr) {
        lv_timer_del(s_ov_fadein_tmr);
        s_ov_fadein_tmr = nullptr;
    }
    if (s_ov_core)   lv_anim_del(s_ov_core,   core_size_cb);
    if (s_ov_label)  lv_anim_del(s_ov_label,   opa_cb);
    if (s_ov_dimmer) {
        lv_anim_del(s_ov_dimmer, opa_cb);
        lv_obj_del(s_ov_dimmer);
        s_ov_dimmer = nullptr;
    }
    s_ov_arc   = nullptr;
    s_ov_core  = nullptr;
    s_ov_label = nullptr;
}

/* ══════════════════════════════════════════════════════════════════════════
 * MODO FULL — estado y funciones
 * ══════════════════════════════════════════════════════════════════════════ */

static lv_obj_t*   s_prog_arc  = nullptr;  ///< arco exterior de progreso (220px)
static lv_timer_t* s_prog_timer = nullptr;  ///< timer de actualización 20fps

/* Indicadores de bypass de modelos AI (actualizados por el timer) */
static lv_obj_t*   s_sl_label  = nullptr;  ///< etiqueta "SCALE LOCK"
static lv_obj_t*   s_sl_status = nullptr;  ///< texto "ON" / "OFF"
static lv_obj_t*   s_bf_label  = nullptr;  ///< etiqueta "BEAT FOLLOW"
static lv_obj_t*   s_bf_status = nullptr;  ///< texto "ON" / "OFF"

static void ai_progress_timer_cb(lv_timer_t* /*t*/) {
    /* ── Arco de progreso ─────────────────────────────────────────────── */
    if (s_prog_arc) {
        float p = bridge_get_ai_progress();
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        lv_arc_set_value(s_prog_arc, (int16_t)(p * 360.0f + 0.5f));
    }

    /* ── Indicadores de bypass ────────────────────────────────────────── */
    /* 40% opacidad (102/255) cuando el modelo está bypassed:
     * el texto es legible pero claramente "apagado". */
    bool sl_off = bridge_get_scale_lock_bypass();
    bool bf_off = bridge_get_beat_follower_bypass();

    if (s_sl_label) {
        lv_obj_set_style_opa(s_sl_label, sl_off ? (lv_opa_t)102 : LV_OPA_COVER, 0);
    }
    if (s_sl_status) {
        lv_label_set_text(s_sl_status, sl_off ? "OFF" : "ON");
        lv_obj_set_style_text_color(s_sl_status,
            sl_off ? GF_COLOR_GRAY : GF_COLOR_TEAL_PLUS, 0);
        lv_obj_set_style_opa(s_sl_status, sl_off ? (lv_opa_t)102 : LV_OPA_COVER, 0);
    }
    if (s_bf_label) {
        lv_obj_set_style_opa(s_bf_label, bf_off ? (lv_opa_t)102 : LV_OPA_COVER, 0);
    }
    if (s_bf_status) {
        lv_label_set_text(s_bf_status, bf_off ? "OFF" : "ON");
        lv_obj_set_style_text_color(s_bf_status,
            bf_off ? GF_COLOR_GRAY : GF_COLOR_WHITE, 0);
        lv_obj_set_style_opa(s_bf_status, bf_off ? (lv_opa_t)102 : LV_OPA_COVER, 0);
    }
}

void view_10_create(lv_obj_t* parent) {
    /* Limpiar estado de un rebuild anterior. */
    s_prog_arc   = nullptr;
    s_prog_timer = nullptr;
    s_sl_label   = nullptr;
    s_sl_status  = nullptr;
    s_bf_label   = nullptr;
    s_bf_status  = nullptr;

    gf_screen_bg(parent);
    gf_arc_title(parent, "GROOVEFORGE AI", GF_COLOR_TEAL);

    /* ── Arco de progreso: radio 110, grosor 6px ────────────────────── */
    /* Muestra el progreso del arm completado (1.0) o resetea a 0 en la
     * próxima sesión. El timer lo actualiza cada 50ms. */
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 220, 220);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 270);
    lv_arc_set_bg_angles(arc, 0, 360);
    lv_arc_set_range(arc, 0, 360);
    lv_arc_set_value(arc, 360);            /* lleno: el arm ya completó */
    lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_DIM,  LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, GF_COLOR_TEAL_PLUS, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    s_prog_arc = arc;

    /* Timer 50ms para actualizar arco + bypass pills. */
    s_prog_timer = lv_timer_create(ai_progress_timer_cb, 50, nullptr);

    /* ── Glow detras del core ────────────────────────────────────────── */
    lv_obj_t* glow = gf_dot(parent, 32, GF_COLOR_TEAL_PLUS);
    lv_obj_center(glow);
    lv_obj_set_style_opa(glow, 45, 0);

    /* ── 8 orbit dots con chase escalonado ──────────────────────────── */
    for (int i = 0; i < 8; i++) {
        const float a = i * 45.0f * (float)M_PI / 180.0f;
        lv_obj_t* d = gf_dot(parent, 7, GF_COLOR_TEAL_PLUS);
        lv_obj_align(d, LV_ALIGN_CENTER,
                     (lv_coord_t)lroundf(50.0f * cosf(a)),
                     (lv_coord_t)lroundf(50.0f * sinf(a)));

        lv_anim_t an;
        lv_anim_init(&an);
        lv_anim_set_var(&an, d);
        lv_anim_set_exec_cb(&an, opa_cb);
        lv_anim_set_values(&an, 50, 255);
        lv_anim_set_time(&an, 400);
        lv_anim_set_playback_time(&an, 400);
        lv_anim_set_delay(&an, i * 100);    /* chase secuencial */
        lv_anim_set_repeat_count(&an, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&an);
    }

    /* ── Core que respira (14px base) ───────────────────────────────── */
    lv_obj_t* core = gf_dot(parent, 14, GF_COLOR_TEAL_PLUS);
    lv_obj_center(core);
    lv_anim_t ca;
    lv_anim_init(&ca);
    lv_anim_set_var(&ca, core);
    lv_anim_set_exec_cb(&ca, core_size_cb);
    lv_anim_set_values(&ca, 10, 18);
    lv_anim_set_time(&ca, 700);
    lv_anim_set_playback_time(&ca, 700);
    lv_anim_set_repeat_count(&ca, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&ca);

    /* ── Textos centrales ────────────────────────────────────────────── */
    gf_hero_label(parent, "AI THINKING",     GF_FONT_LABEL, GF_COLOR_TEAL_PLUS, 46);
    gf_hero_label(parent, "ANALYZING GROOVE", GF_FONT_MICRO, GF_COLOR_GRAY,      64);

    /* ── Bypass pills — estado actualizado por timer ─────────────────── */
    /* SCALE LOCK: ENC L click toggle (enviado por sketch Teensy via 0x00F2).
     * Timer actualiza opacidad (100% activo / 40% bypassed) y texto ON/OFF. */
    s_sl_label  = gf_label(parent, "SCALE LOCK",  GF_FONT_MICRO, GF_COLOR_TEAL_PLUS);
    if (s_sl_label)  lv_obj_align(s_sl_label,  LV_ALIGN_CENTER, -18, 78);

    s_sl_status = gf_label(parent, "ON", GF_FONT_MICRO, GF_COLOR_TEAL_PLUS);
    if (s_sl_status) lv_obj_align(s_sl_status, LV_ALIGN_CENTER,  46, 78);

    /* BEAT FOLLOW: ENC R click toggle (0x00F3). */
    s_bf_label  = gf_label(parent, "BEAT FOLLOW", GF_FONT_MICRO, GF_COLOR_WHITE);
    if (s_bf_label)  lv_obj_align(s_bf_label,  LV_ALIGN_CENTER, -18, 92);

    s_bf_status = gf_label(parent, "ON", GF_FONT_MICRO, GF_COLOR_WHITE);
    if (s_bf_status) lv_obj_align(s_bf_status, LV_ALIGN_CENTER,  46, 92);
}

void view_10_destroy(void) {
    /* Eliminar timer antes de que lv_obj_clean() destruya los widgets. */
    if (s_prog_timer) {
        lv_timer_del(s_prog_timer);
        s_prog_timer = nullptr;
    }
    s_prog_arc  = nullptr;
    s_sl_label  = nullptr;
    s_sl_status = nullptr;
    s_bf_label  = nullptr;
    s_bf_status = nullptr;
}
