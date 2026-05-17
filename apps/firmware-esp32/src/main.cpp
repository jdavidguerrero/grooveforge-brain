/**
 * @file main.cpp
 * @brief Entry point — GrooveForge Brain ESP32-S3
 *
 * Sprint 3.1: Setup LVGL 8.3 + GC9A01 240x240, boot animation, pantalla principal.
 *
 * Secuencia de boot:
 *   setup()  → lv_port_disp_init() → screen_boot_create()
 *   loop()   → lv_task_handler() [LVGL tick via millis() custom]
 *              → screen_boot_done()? → screen_main_create()
 *
 * Nota sobre lv_tick_inc():
 *   No se llama en loop() porque lv_conf.h tiene LV_TICK_CUSTOM=1 con millis().
 *   LVGL consulta millis() directamente — mas preciso que incrementos manuales.
 *   Ver lv_conf.h para la configuracion.
 *
 * Sprint 3.2 agregara:
 *   - Bridge Protocol slave (UART RX desde Teensy)
 *   - screen_main_set_engine() / set_fx() / set_status() con datos reales
 *   - WiFi manager init
 */

#include <Arduino.h>
#include <lvgl.h>
#include "display/lv_port_disp.h"
#include "display/screens/screen_boot.h"
#include "display/screens/screen_main.h"

static bool s_main_created = false;

void setup() {
    Serial.begin(115200);
    /* 2s de espera para que USB CDC enumere en el host antes de cualquier print.
     * Sin esto los primeros prints se pierden si el monitor no esta listo. */
    delay(2000);

    Serial.println("=== GrooveForge Brain — ESP32-S3 Boot ==="); Serial.flush();
    Serial.printf("Sprint 3.1 | Display: GC9A01 240x240 | LVGL 8.x\n");

    /* Inicializar LVGL + driver GC9A01 + buffers de rendering */
    lv_port_disp_init();

    /* Crear pantalla de boot y lanzar animaciones */
    screen_boot_create();

    Serial.println("Boot screen active — animating...");
}

void loop() {
    /* lv_task_handler() procesa:
     *   - Animaciones pendientes (arc sweep, fade-ins del boot)
     *   - Invalidaciones de pantalla (redraws)
     *   - Timers y eventos LVGL
     *
     * Sin este call, LVGL no renderiza nada.
     * Frecuencia minima recomendada: cada 5ms (~200Hz cap).
     * LVGL internamente respeta LV_DISP_DEF_REFR_PERIOD (17ms = ~60fps). */
    lv_task_handler();

    /* Transicion boot → main: una sola vez cuando la animacion termina */
    if (!s_main_created && screen_boot_done()) {
        s_main_created = true;

        /* Limpiar widgets del boot screen antes de crear el main */
        lv_obj_clean(lv_scr_act());

        screen_main_create();
        Serial.println("Boot done — main screen active");
    }

    delay(5); /* cap ~200fps, LVGL throttlea internamente a LV_DISP_DEF_REFR_PERIOD */
}
