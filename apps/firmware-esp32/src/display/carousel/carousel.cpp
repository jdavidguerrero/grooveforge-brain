/**
 * @file carousel.cpp
 * @brief Implementacion del carrusel de vistas — GrooveForge Brain
 *
 * Ciclo por vista:
 *   dwell 5s → wipe-out 200ms → destroy()+anim_kill+clean → create(next)
 *            → wipe-in 200ms → (reanuda dwell)
 *
 * Solo una vista viva a la vez: lv_obj_clean() destruye la saliente antes de
 * crear la entrante, asi la memoria pico es max(vista), no la suma.
 */

#include "carousel.h"
#include "view_transition.h"
#include "../ui_theme.h"
#include "../widgets/gf_anim.h"
#include "../screens/views/views.h"
#include "lvgl.h"
#include <Arduino.h>


/* Descriptor de una vista del carrusel. */
typedef struct {
    void        (*create)(lv_obj_t* parent);
    void        (*destroy)(void);
    const char* name;
} gf_view_desc_t;

/* Tabla ordenada 01→23 — el orden del carrusel sigue la numeracion de mocks. */
static const gf_view_desc_t s_views[] = {
    { view_01_create, view_01_destroy, "BOOT"        },
    { view_02_create, view_02_destroy, "SYNTH MAIN"  },
    { view_03_create, view_03_destroy, "OSC"         },
    { view_04_create, view_04_destroy, "ENV ADSR"    },
    { view_05_create, view_05_destroy, "LFO MOD"     },
    { view_06_create, view_06_destroy, "ENGINE SEL"  },
    { view_07_create, view_07_destroy, "FX MAIN"     },
    { view_08_create, view_08_destroy, "FX SELECT"   },
    { view_09_create, view_09_destroy, "MODE SWITCH" },
    { view_10_create, view_10_destroy, "AI PROC"     },
    { view_11_create, view_11_destroy, "IDLE"        },
    { view_12_create, view_12_destroy, "PRESET"      },
    { view_13_create, view_13_destroy, "INSERT"      },
    { view_14_create, view_14_destroy, "SEND"        },
    { view_15_create, view_15_destroy, "MASTER"      },
    { view_16_create, view_16_destroy, "SCALE LOCK"  },
    { view_17_create, view_17_destroy, "WIFI"        },
    { view_18_create, view_18_destroy, "AI SUGGEST"  },
    { view_19_create, view_19_destroy, "DAW"         },
    { view_20_create, view_20_destroy, "OTA"         },
    { view_21_create, view_21_destroy, "ERROR"       },
    { view_22_create, view_22_destroy, "VOLUME"      },
    { view_23_create, view_23_destroy, "MIDI LEARN"  },
    { view_24_create, view_24_destroy, "FILTER"      },   /* idx 23 = VIEW_IDX_SYNTH_FILTER */
    { view_25_create, view_25_destroy, "AI MODELS"   },   /* idx 24 = VIEW_IDX_AI_MODELS */
    { view_26_create, view_26_destroy, "KARAOKE"     },   /* idx 25 = VIEW_IDX_AI_KARAOKE */
    { view_27_create, view_27_destroy, "AI FEED"     },   /* idx 26 = VIEW_IDX_AI_FEED */
    { view_28_create, view_28_destroy, "AI RACK"     },   /* idx 27 = VIEW_IDX_AI_RACK (Sprint 45) */
};
static const uint8_t GF_VIEW_COUNT = sizeof(s_views) / sizeof(s_views[0]);

static uint8_t     s_idx   = 0;
static lv_timer_t* s_dwell = nullptr;   /* timer de permanencia de la vista */
static bool        s_ai_overlay_active = false;  /* true mientras overlay AI esté sobre la vista */

/* Construye la vista actual sobre la pantalla activa. */
static void build_current(void) {
    s_views[s_idx].create(lv_scr_act());
    Serial.printf("[carousel] view %02u %-12s  heap=%u\n",
                  (unsigned)(s_idx + 1), s_views[s_idx].name,
                  (unsigned)ESP.getFreeHeap());
    Serial.flush();
}

/* Fin del wipe-in: la nueva vista quedo visible → reanudar el dwell. */
static void on_wipe_in_done(void) {
    lv_timer_reset(s_dwell);
    lv_timer_resume(s_dwell);
}

/* Fin del wipe-out: pantalla cubierta → momento seguro para el swap de vista. */
static void on_wipe_out_done(void) {
    /* 1. timers recurrentes de la vista saliente */
    if (s_views[s_idx].destroy != nullptr) s_views[s_idx].destroy();
    /* 2. todas las animaciones + timers registrados */
    gf_anim_kill_all();
    /* 3. destruir los widgets de la vista saliente */
    lv_obj_clean(lv_scr_act());
    /* 4. construir la siguiente */
    s_idx = (s_idx + 1) % GF_VIEW_COUNT;
    build_current();
    /* 5. revelar */
    gf_transition_in(on_wipe_in_done);
}

/* Expira el dwell → arrancar la transicion de salida.
 * Solo se usa para la transicion splash→FX (one-shot). */
static void dwell_cb(lv_timer_t* /*t*/) {
    if (s_dwell != nullptr) lv_timer_pause(s_dwell);
    gf_transition_out(on_wipe_out_done);
}

/* Callback one-shot: fin del splash → modo FX por defecto. */
static void on_splash_done(lv_timer_t* t) {
    lv_timer_del(t);
    s_dwell = nullptr;   // evitar que goto/pause lo reanuden
    carousel_goto(VIEW_IDX_FX_MAIN);
}

void carousel_start(void) {
    gf_transition_init();
    s_idx  = 0;
    s_dwell = nullptr;  // sin avance periodico — la UI es bridge-driven
    build_current();
    /* Mostrar splash 2s, luego saltar a FX MAIN. */
    lv_timer_create(on_splash_done, 2000, nullptr);
}

uint8_t carousel_get_current(void) { return s_idx; }

void carousel_pause(void) {
    if (s_dwell != nullptr) lv_timer_pause(s_dwell);
}

void carousel_resume(void) {
    if (s_dwell != nullptr) {
        lv_timer_reset(s_dwell);
        lv_timer_resume(s_dwell);
    }
}

void carousel_goto(uint8_t idx) {
    if (idx >= GF_VIEW_COUNT) return;

    /* Pausar el dwell para que no dispare otra transicion mientras hacemos el swap. */
    if (s_dwell != nullptr) lv_timer_pause(s_dwell);

    /* Si el overlay AI estaba activo, destruirlo inmediatamente (sin fade) antes
     * de que lv_obj_clean() libere los widgets — esto evita que animaciones
     * pendientes del overlay disparen contra punteros ya liberados. */
    if (s_ai_overlay_active) {
        view_10_overlay_destroy_immediate();
        s_ai_overlay_active = false;
    }

    /* Destruir la vista activa y limpiar estado LVGL. */
    if (s_views[s_idx].destroy != nullptr) s_views[s_idx].destroy();
    gf_anim_kill_all();
    lv_obj_clean(lv_scr_act());

    /* Saltar al indice solicitado y construir. */
    s_idx = idx;
    build_current();

    /* Reanudar el dwell desde cero para que la nueva vista tenga 5s completos. */
    if (s_dwell != nullptr) {
        lv_timer_reset(s_dwell);
        lv_timer_resume(s_dwell);
    }
}

/* ── AI Overlay API ─────────────────────────────────────────────────────── */

void carousel_ai_overlay_start(void) {
    if (s_ai_overlay_active) return;          /* ya activo — ignorar duplicados */
    s_ai_overlay_active = true;
    /* Pausar el dwell para que no avance sola mientras el hold ocurre. */
    if (s_dwell != nullptr) lv_timer_pause(s_dwell);
    view_10_overlay_start(lv_scr_act());
    Serial.printf("[carousel] AI overlay start  heap=%u\n", (unsigned)ESP.getFreeHeap());
    Serial.flush();
}

void carousel_ai_overlay_cancel(void) {
    if (!s_ai_overlay_active) return;
    s_ai_overlay_active = false;
    view_10_overlay_cancel();                 /* fade-out 200ms, la vista anterior reaparece */
    Serial.println("[carousel] AI overlay cancelled — view restored");
    Serial.flush();
}

void carousel_ai_overlay_complete(void) {
    if (!s_ai_overlay_active) return;
    s_ai_overlay_active = false;

    /* Flash blanco 100ms sobre todo lo que hay en pantalla. */
    lv_obj_t* flash = lv_obj_create(lv_scr_act());
    lv_obj_set_size(flash, 240, 240);
    lv_obj_set_pos(flash, 0, 0);
    lv_obj_set_style_bg_color(flash, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(flash, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(flash, 0, 0);
    lv_obj_set_style_radius(flash, 0, 0);
    lv_obj_clear_flag(flash, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    /* Después de 100ms: goto full view_10 (lv_obj_clean eliminará flash + overlay
     * + vista anterior; view_10_create construye la vista completa). */
    lv_timer_create([](lv_timer_t* t) {
        lv_timer_del(t);
        carousel_goto(VIEW_IDX_AI_PROC);
    }, 100, nullptr);

    Serial.println("[carousel] AI overlay complete — flash → view_10 full");
    Serial.flush();
}

bool carousel_ai_overlay_active(void) { return s_ai_overlay_active; }
