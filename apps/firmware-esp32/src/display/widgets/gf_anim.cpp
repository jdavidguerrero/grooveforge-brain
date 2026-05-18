/**
 * @file gf_anim.cpp
 * @brief Implementacion del registro de animaciones/timers — GrooveForge Brain
 */

#include "gf_anim.h"

/* Capacidad del registro. Ninguna vista usa mas de ~4 timers recurrentes;
 * 12 da margen sin desperdiciar RAM. */
static constexpr uint8_t GF_ANIM_MAX_TIMERS = 12;

static lv_timer_t* s_timers[GF_ANIM_MAX_TIMERS] = { nullptr };
static uint8_t     s_timer_count = 0;

void gf_anim_register_timer(lv_timer_t* t) {
    if (t == nullptr) return;
    if (s_timer_count >= GF_ANIM_MAX_TIMERS) return;  /* overflow: se ignora */
    s_timers[s_timer_count++] = t;
}

void gf_anim_kill_all(void) {
    /* lv_anim_del(NULL, NULL): el NULL en var Y exec_cb borra TODA animacion
     * en curso, sin importar a que objeto apunte. */
    lv_anim_del(NULL, NULL);

    /* Borrar cada timer recurrente registrado por las vistas. */
    for (uint8_t i = 0; i < s_timer_count; i++) {
        if (s_timers[i] != nullptr) {
            lv_timer_del(s_timers[i]);
            s_timers[i] = nullptr;
        }
    }
    s_timer_count = 0;
}
