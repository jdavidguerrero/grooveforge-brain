/**
 * 26-encoder-test.cpp — Smoke test de los 3 encoders ALPS EC11.
 *
 * Sprint 5 / Hardware Integration — encoders cableados, validar antes de
 * integrar con engines.
 *
 * Qué hace:
 *   - Inicializa ENC L, ENC R, ENC NAV con sus pines (01-architecture.md §3.3)
 *   - En loop(), imprime por Serial cualquier delta de giro o evento de push
 *   - Nada de audio, nada de ML — solo UI pura
 *
 * Pines:
 *   ENC L   A=10  B=11  SW=12
 *   ENC R   A=13  B=14  SW=15
 *   ENC NAV A=16  B=17  SW=26
 *
 * Cómo flashear:
 *   cd apps/firmware-teensy
 *   pio run -e sketch26 -t upload
 *   pio device monitor -b 115200
 *
 * Salida esperada al girar ENC L un detent CW:
 *   [ENC L]  delta=+1  (total=1)
 * Al presionar ENC NAV:
 *   [ENC NAV] SW pressed
 *
 * Build reference: Sprint Hardware Integration (apps/docs/sprints/29-hardware-integration.md)
 */

#include <Arduino.h>

#include "ui/encoders.h"

static Encoders encoders;

// Acumuladores para mostrar posición absoluta
static int32_t total_l   = 0;
static int32_t total_r   = 0;
static int32_t total_nav = 0;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}  // espera USB CDC hasta 3s

    encoders.init();

    Serial.println("\n=== Sprint HW Integration — Encoder Test (sketch26) ===");
    Serial.println("Gira cualquier encoder o presiona su switch.");
    Serial.println("ENC L  : Cutoff  (A=10 B=11 SW=12)");
    Serial.println("ENC R  : Resonance (A=13 B=14 SW=15)");
    Serial.println("ENC NAV: Navegacion (A=16 B=17 SW=26)");
    Serial.println("------------------------------------------------------");
}

void loop() {
    encoders.update();

    // ── ENC L
    int32_t dl = encoders.read_enc_l();
    if (dl != 0) {
        total_l += dl;
        Serial.printf("[ENC L  ] delta=%+d  total=%d\n", (int)dl, (int)total_l);
    }
    if (encoders.sw_l_pressed()) {
        Serial.println("[ENC L  ] SW pressed  → filter bypass toggle");
    }

    // ── ENC R
    int32_t dr = encoders.read_enc_r();
    if (dr != 0) {
        total_r += dr;
        Serial.printf("[ENC R  ] delta=%+d  total=%d\n", (int)dr, (int)total_r);
    }
    if (encoders.sw_r_pressed()) {
        Serial.println("[ENC R  ] SW pressed  → resonance reset a 0");
    }

    // ── ENC NAV
    int32_t dn = encoders.read_enc_nav();
    if (dn != 0) {
        total_nav += dn;
        Serial.printf("[ENC NAV] delta=%+d  total=%d\n", (int)dn, (int)total_nav);
    }
    if (encoders.sw_nav_pressed()) {
        Serial.println("[ENC NAV] SW pressed  → confirm / AI·Action");
    }

    delay(5);  // 200Hz poll — suficiente para EC11 con Bounce2
}
