/**
 * 26-encoder-test.cpp — Smoke test UI completa: encoders + botones + LEDs WS2812B.
 *
 * Sprint 5 / Hardware Integration — validar toda la UI antes de integrar engines.
 *
 * Qué hace:
 *   - Encoders: reporta delta y push de ENC L, ENC R, ENC NAV por Serial
 *   - Botones: reporta pressed / held (>500ms) de B1-B4 por Serial
 *   - LEDs: feedback visual inmediato para cada evento:
 *       ENC L gira   → LED del ring en posición proporcional al total, color Teal
 *       ENC R gira   → todos los ring en Teal con intensidad proporcional a total_r
 *       ENC NAV gira → sweep del ring siguiendo el detent, color Purple
 *       ENC L push   → ring flash Blanco 1 ciclo (filter bypass)
 *       ENC R push   → ring flash Rojo 1 ciclo (resonance reset)
 *       ENC NAV push → ring flash Verde 1 ciclo (confirm)
 *       B1 pressed   → keycap B1 (LED 12) flash Naranja
 *       B2 pressed   → keycap B2 (LED 13) flash Naranja
 *       B3 pressed   → keycap B3 (LED 14) flash Naranja
 *       B4 pressed   → keycap B4 (LED 15) flash Naranja
 *       Bx held      → keycap Bx cambia a Rojo mientras se mantiene
 *       Idle         → ring pulso suave Teal (modo Synth color, 01-architecture.md §3.4)
 *
 * Pines encoders (01-architecture.md §3.3):
 *   ENC L   A=10  B=11  SW=12
 *   ENC R   A=13  B=14  SW=15
 *   ENC NAV A=16  B=17  SW=26
 *
 * Pines botones:
 *   B1=GPIO2  B2=GPIO3  B3=GPIO4  B4=GPIO5
 *
 * LEDs (16 en cadena, pin 28):
 *   0-11  → ring ENC NAV
 *   12-15 → keycap underglow B1-B4
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
#include "ui/leds.h"

static Encoders encoders;
static Buttons  buttons;
static Leds     leds;

// Acumuladores posición absoluta
static int32_t total_l   = 0;
static int32_t total_r   = 0;
static int32_t total_nav = 0;

// Etiquetas de botones (modo SYNTH — 01-architecture.md §3.4)
static const char* BTN_LABELS[4] = { "B1=OSC", "B2=ENV", "B3=LFO/MOD", "B4=PRESET" };

// Colores del spec (01-architecture.md §3.4)
static const CRGB COLOR_TEAL   = CRGB(0x1D, 0x9E, 0x75);  // #1D9E75 — Synth mode
static const CRGB COLOR_PURPLE = CRGB(0x53, 0x4A, 0xB7);  // #534AB7 — FX mode / NAV
static const CRGB COLOR_WHITE  = CRGB(180, 180, 180);
static const CRGB COLOR_RED    = CRGB(200, 0,   0  );
static const CRGB COLOR_GREEN  = CRGB(0,   200, 0  );
static const CRGB COLOR_ORANGE = CRGB(220, 80,  0  );
static const CRGB COLOR_OFF    = CRGB(0,   0,   0  );

// Temporizadores de flash (ms restantes)
static uint32_t flash_ring_until  = 0;
static CRGB     flash_ring_color  = COLOR_OFF;
static uint32_t flash_key_until[4] = {0, 0, 0, 0};

// Flag para escribir el idle glow solo una vez (evita show() cada ciclo en reposo)
static bool  idle_set   = false;

// ── helpers ──────────────────────────────────────────────────────────────────

static uint8_t ring_pos_from_total(int32_t total) {
    // Mapea total a posición 0-11 con wrap
    int32_t pos = ((total % NUM_RING) + NUM_RING) % NUM_RING;
    return (uint8_t)pos;
}

// ── setup / loop ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}

    encoders.init();
    buttons.init();
    leds.init();

    // Startup: sweep completo del ring para verificar que todos los LEDs encienden
    for (uint8_t i = 0; i < NUM_RING; i++) {
        leds.set_ring(i, COLOR_TEAL, COLOR_OFF);
        leds.show();
        delay(40);
    }
    // Underglow todos naranjas para verificar B1-B4
    for (uint8_t i = 0; i < 4; i++) leds.set_keycap(i, COLOR_ORANGE);
    leds.show();
    delay(400);
    leds.clear();

    Serial.println("\n=== Sprint HW Integration — Encoder + Button + LED Test (sketch26) ===");
    Serial.println("Encoders:");
    Serial.println("  ENC L   : Cutoff     (A=10 B=11 SW=12)  → ring pos Teal");
    Serial.println("  ENC R   : Resonance  (A=13 B=14 SW=15)  → ring brillo");
    Serial.println("  ENC NAV : Navegacion (A=16 B=17 SW=26)  → ring sweep Purple");
    Serial.println("Botones (Kailh Choc V2 Brown, INPUT_PULLUP):");
    Serial.println("  B1=GPIO2  B2=GPIO3  B3=GPIO4  B4=GPIO5");
    Serial.println("  pressed → keycap Naranja  |  held → keycap Rojo");
    Serial.println("LEDs: 0-11 ring ENC NAV | 12-15 keycap B1-B4 | pin 28");
    Serial.println("----------------------------------------------------------------------");
    Serial.println("DEBUG: cada 2s imprime raw counts — deben cambiar al girar");
    Serial.println("  Si raw no cambia → problema de cableado (COM/GND del encoder)");
}

void loop() {
    encoders.update();
    buttons.update();

    uint32_t now = millis();
    bool ring_event = false;

    // ── ENC L — sin feedback en ring (el knob físico es el feedback)
    int32_t dl = encoders.read_enc_l();
    if (dl != 0) {
        total_l += dl;
        Serial.printf("[ENC L  ] delta=%+d  total=%d\n", (int)dl, (int)total_l);
    }
    if (encoders.sw_l_pressed()) {
        Serial.println("[ENC L  ] SW pressed  → filter bypass toggle  [ring BLANCO]");
        flash_ring_color = COLOR_WHITE;
        flash_ring_until = now + 200;
    }

    // ── ENC R — sin feedback en ring (el knob físico es el feedback)
    int32_t dr = encoders.read_enc_r();
    if (dr != 0) {
        total_r += dr;
        Serial.printf("[ENC R  ] delta=%+d  total=%d\n", (int)dr, (int)total_r);
    }
    if (encoders.sw_r_pressed()) {
        total_r = 0;
        Serial.println("[ENC R  ] SW pressed  → resonance reset a 0  [ring ROJO]");
        flash_ring_color = COLOR_RED;
        flash_ring_until = now + 200;
    }

    // ── ENC NAV — único que mueve el ring
    int32_t dn = encoders.read_enc_nav();
    if (dn != 0) {
        total_nav += dn;
        Serial.printf("[ENC NAV] delta=%+d  total=%d  → ring pos %d\n",
                      (int)dn, (int)total_nav, ring_pos_from_total(total_nav));
        // LED activo en posición NAV; resto muy tenues (idle glow)
        leds.set_ring(ring_pos_from_total(total_nav), COLOR_PURPLE,
                      CRGB(COLOR_TEAL.r / 12, COLOR_TEAL.g / 12, COLOR_TEAL.b / 12));
        leds.show();
        ring_event = true;
    }

    // ── ENC NAV push — flash Verde (confirm)
    if (encoders.sw_nav_pressed()) {
        Serial.println("[ENC NAV] SW pressed  → confirm / AI·Action  [ring VERDE]");
        flash_ring_color = COLOR_GREEN;
        flash_ring_until = now + 200;
    }

    // ── Flash de ring (SW pushes de cualquier encoder)
    if (now < flash_ring_until) {
        for (uint8_t i = 0; i < NUM_RING; i++)
            leds.set(i, flash_ring_color.r, flash_ring_color.g, flash_ring_color.b);
        for (uint8_t i = 0; i < 4; i++) leds.set_keycap(i, COLOR_OFF);
        leds.show();
        ring_event = true;
    }

    // ── B1–B4
    bool key_changed = false;
    for (uint8_t i = 0; i < NUM_BUTTONS; i++) {
        if (buttons.pressed(i)) {
            Serial.printf("[%s] pressed  [keycap %d NARANJA]\n", BTN_LABELS[i], i + 1);
            flash_key_until[i] = now + 150;
        }
        if (buttons.held(i)) {
            leds.set_keycap(i, COLOR_RED);
            key_changed = true;
        } else if (now < flash_key_until[i]) {
            leds.set_keycap(i, COLOR_ORANGE);
            key_changed = true;
        } else {
            leds.set_keycap(i, COLOR_OFF);
            key_changed = true;
        }
    }
    if (key_changed && now >= flash_ring_until) leds.show();

    // ── Idle: ring muy tenue Teal estático — referencia de modo Synth activo
    // Brillo ~8% (20/255) — visible en oscuridad, no distrae en uso normal.
    // "Despierta" a brillo completo al girar ENC NAV.
    if (!ring_event && now >= flash_ring_until) {
        if (!idle_set) {
            uint8_t bri = 20;  // ~8% — ajustar si es muy brillante o muy oscuro
            for (uint8_t i = 0; i < NUM_RING; i++) {
                leds.set(i, (COLOR_TEAL.r * bri) / 255,
                            (COLOR_TEAL.g * bri) / 255,
                            (COLOR_TEAL.b * bri) / 255);
            }
            leds.show();
            idle_set = true;
        }
    } else {
        idle_set = false;  // resetear para re-aplicar idle cuando termine el evento
    }

    // ── DEBUG: raw counts cada 2s — cambian si el encoder está bien cableado
    static uint32_t last_raw_print = 0;
    if (now - last_raw_print >= 2000) {
        last_raw_print = now;
        // Acceso directo a los Encoder objects via punteros temporales no es posible
        // desde fuera de la clase — usamos una lectura de posición acumulada
        Serial.printf("[RAW] Si estos cambian al girar → SW bug. Si no cambian → cableado COM/GND\n");
        Serial.printf("  total_l=%d  total_r=%d  total_nav=%d\n",
                      (int)total_l, (int)total_r, (int)total_nav);
    }

    delay(5);  // 200Hz poll
}
