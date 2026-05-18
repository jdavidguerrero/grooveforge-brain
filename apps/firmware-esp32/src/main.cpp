/**
 * @file main.cpp
 * @brief Entry point — GrooveForge Brain ESP32-S3
 *
 * Sprint 3.4: 23 vistas de UI en modo carrusel (una cada 5s, bucle infinito).
 *
 * Secuencia de boot:
 *   setup() → lv_port_disp_init() → screen_boot_create()
 *   loop()  → lv_task_handler() + lv_tick_inc(5)
 *           → screen_boot_done()? → carousel_start()
 *
 * El boot anima una sola vez; despues el carrusel toma el control y cicla las
 * 23 vistas indefinidamente. Ver apps/docs/sprints/18-display-ui-carousel.md.
 *
 * Nota sobre el tick: lv_conf.h tiene LV_TICK_CUSTOM=0, por eso loop() llama
 * lv_tick_inc(5) manualmente (patron validado de groove_drum).
 */

#include <Arduino.h>
#include <lvgl.h>
#include "display/lv_port_disp.h"
#include "display/screens/screen_boot.h"
#include "display/carousel/carousel.h"
#include "bridge/bridge_slave.h"
#include "bridge/bridge_handlers.h"

static bool        s_carousel_started = false;
static BridgeSlave s_bridge;

void setup() {
    Serial.begin(115200);
    /* 2s para que el USB CDC enumere antes de los primeros prints. */
    delay(2000);

    Serial.println("=== GrooveForge Brain — ESP32-S3 ==="); Serial.flush();
    Serial.println("Sprint 3.4 | Display UI carousel | 23 views | LVGL 8.x");

    /* Bridge Protocol UART slave — init antes del display para que los logs
     * de bridge aparezcan en orden cronológico en el monitor serial. */
    bridge_handlers_init(s_bridge);
    s_bridge.init();

    /* LVGL + driver GC9A01 + buffers de rendering */
    lv_port_disp_init();

    /* Pantalla de boot animada (corre una sola vez) */
    screen_boot_create();
    Serial.println("Boot screen active — animating...");
}

void loop() {
    /* lv_task_handler() procesa animaciones, invalidaciones, timers y eventos.
     * lv_tick_inc(5) avanza el reloj interno de LVGL (LV_TICK_CUSTOM=0). */
    /* Bridge poll: procesa bytes UART disponibles sin bloquear el loop */
    s_bridge.poll();

    lv_task_handler();
    lv_tick_inc(5);

    /* Transicion boot → carrusel: una sola vez al terminar la animacion. */
    if (!s_carousel_started && screen_boot_done()) {
        s_carousel_started = true;
        lv_obj_clean(lv_scr_act());
        carousel_start();
        Serial.println("Boot done — carousel active");
    }

    delay(5);
}
