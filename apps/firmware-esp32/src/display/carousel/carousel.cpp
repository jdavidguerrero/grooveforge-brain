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
};
static const uint8_t GF_VIEW_COUNT = sizeof(s_views) / sizeof(s_views[0]);

static uint8_t     s_idx   = 0;
static lv_timer_t* s_dwell = nullptr;   /* timer de permanencia de la vista */

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

/* Expira el dwell → arrancar la transicion de salida. */
static void dwell_cb(lv_timer_t* /*t*/) {
    lv_timer_pause(s_dwell);
    gf_transition_out(on_wipe_out_done);
}

void carousel_start(void) {
    gf_transition_init();
    s_idx = 0;
    build_current();
    s_dwell = lv_timer_create(dwell_cb, GF_CAROUSEL_DWELL_MS, nullptr);
}
