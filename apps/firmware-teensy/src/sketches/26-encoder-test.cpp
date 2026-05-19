/**
 * 26-encoder-test.cpp — Smoke test de los 3 encoders ALPS EC11 + 4 botones Kailh Choc.
 *
 * Sprint 5 / Hardware Integration — validar UI completa antes de integrar engines.
 *
 * Qué hace:
 *   - Inicializa ENC L, ENC R, ENC NAV y B1-B4
 *   - Imprime por Serial cualquier delta de giro, push de encoder o evento de botón
 *   - Detecta hold (>500ms) en botones
 *   - Nada de audio, nada de ML — solo UI pura
 *
 * Pines encoders (01-architecture.md §3.3):
 *   ENC L   A=10  B=11  SW=12
 *   ENC R   A=13  B=14  SW=15
 *   ENC NAV A=16  B=17  SW=26
 *
 * Pines botones:
 *   B1=GPIO2  B2=GPIO3  B3=GPIO4  B4=GPIO5  (Kailh Choc V2 Brown, INPUT_PULLUP)
 *
 * Cómo flashear:
 *   cd apps/firmware-teensy
 *   pio run -e sketch26 -t upload
 *   pio device monitor -b 115200
 *
 * Build reference: Sprint Hardware Integration (apps/docs/sprints/29-hardware-integration.md)
 */

#include <Arduino.h>

#include "ui/encoders.h"
#include "ui/buttons.h"

static Encoders encoders;
static Buttons  buttons;

// Acumuladores para mostrar posición absoluta de cada encoder
static int32_t total_l   = 0;
static int32_t total_r   = 0;
static int32_t total_nav = 0;

// Etiquetas de botones según modo SYNTH (01-architecture.md §3.4)
static const char* BTN_LABELS[4] = { "B1=OSC", "B2=ENV", "B3=LFO/MOD", "B4=PRESET" };

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}  // espera USB CDC hasta 3s

    encoders.init();
    buttons.init();

    Serial.println("\n=== Sprint HW Integration — Encoder + Button Test (sketch26) ===");
    Serial.println("Encoders:");
    Serial.println("  ENC L   : Cutoff     (A=10 B=11 SW=12)");
    Serial.println("  ENC R   : Resonance  (A=13 B=14 SW=15)");
    Serial.println("  ENC NAV : Navegacion (A=16 B=17 SW=26)");
    Serial.println("Botones (Kailh Choc V2 Brown, INPUT_PULLUP):");
    Serial.println("  B1=GPIO2  B2=GPIO3  B3=GPIO4  B4=GPIO5");
    Serial.println("  Hold >500ms reportado como [HELD]");
    Serial.println("----------------------------------------------------------------");
}

void loop() {
    encoders.update();
    buttons.update();

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

    // ── B1–B4
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        if (buttons.pressed(i)) {
            Serial.printf("[%s] pressed\n", BTN_LABELS[i]);
        }
        if (buttons.held(i)) {
            Serial.printf("[%s] HELD (>500ms)\n", BTN_LABELS[i]);
        }
    }

    delay(5);  // 200Hz poll — suficiente para EC11 + Bounce2
}
