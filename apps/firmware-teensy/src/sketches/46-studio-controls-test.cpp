/**
 * 46-studio-controls-test.cpp — Prueba del panel de Studio: 5 encoders + 6 botones + 18 LEDs.
 *
 * Valida el mapa de pines del ADR-004 con cables, antes de fijarlo en el PCB. Es independiente de
 * ui/encoders y ui/buttons (que todavía manejan 3 encoders y 4 botones): usa directamente
 * Encoder.h y Bounce2 (oficiales de Teensyduino) y FastLED.
 *
 * Qué hace:
 *   - Al arrancar revisa que cada pin de push y de botón lea HIGH en reposo (pull-up interno).
 *     Un pin en LOW sin tocar nada = corto a GND o cable cruzado.
 *   - Encoders: imprime delta y total por detent; el LED del ring marca la posición con el color
 *     de cada encoder. Push: flash del ring entero.
 *   - Botones: imprime pressed / released (B6 es momentáneo: HOLD en FX, SHIFT en SYNTH) y
 *     enciende su keycap mientras está apretado.
 *
 * Pines (01-architecture.md §3.3 + ADR-004):
 *   ENC L   A=10  B=11  SW=12        P1  A=30  B=31  SW=32
 *   ENC R   A=13  B=14  SW=15        P2  A=34  B=35  SW=9
 *   ENC NAV A=16  B=17  SW=26
 *   B1=2  B2=3  B3=4  B4=5  B5 MODE=40  B6 HOLD/SHIFT=41
 *
 * LEDs WS2812B (18 en cadena, pin 28): 0–11 ring NAV · 12–15 keycaps B1–B4 · 16 B5 · 17 B6.
 *
 * Cableado: A y B a sus pines y el común del encoder a GND; push y botones entre el pin y GND.
 * La Teensy 4.1 NO tolera 5 V: módulos con pull-ups propios (KY-040) se alimentan con 3.3 V.
 *
 * Cómo flashear:
 *   cd apps/firmware-teensy
 *   pio run -e sketch46 -t upload
 *   pio device monitor -b 115200
 */

#include <Arduino.h>
#include <Bounce2.h>
#include <Encoder.h>
#include <FastLED.h>

namespace {

constexpr int32_t PULSES_PER_DETENT = 4;   // EC11: 4 pulsos de cuadratura por click
constexpr uint8_t LED_PIN = 28;
constexpr uint8_t NUM_LEDS = 18;
constexpr uint8_t NUM_RING = 12;
constexpr uint8_t FIRST_KEYCAP = 12;

// Encoder guarda la dirección de su estado para la interrupción: no se puede copiar, así que
// cada uno es un objeto propio y la tabla guarda punteros.
Encoder enc_l(10, 11), enc_r(13, 14), enc_nav(16, 17), enc_p1(30, 31), enc_p2(34, 35);

struct EncDef {
    const char* name;
    Encoder* enc;
    uint8_t sw_pin;
    CRGB color;
    Bounce sw;
    int32_t last_raw;
    int32_t total;
};

EncDef encs[] = {
    {"ENC L  ", &enc_l, 12, CRGB(0x1D, 0x9E, 0x75), Bounce(), 0, 0},   // teal
    {"ENC R  ", &enc_r, 15, CRGB(0x5D, 0xCA, 0xA5), Bounce(), 0, 0},   // teal claro
    {"ENC NAV", &enc_nav, 26, CRGB(0x53, 0x4A, 0xB7), Bounce(), 0, 0}, // púrpura
    {"P1     ", &enc_p1, 32, CRGB(0xE8, 0xB8, 0x4A), Bounce(), 0, 0},  // ámbar
    {"P2     ", &enc_p2, 9, CRGB(0xFF, 0x6A, 0x35), Bounce(), 0, 0},   // naranja
};

struct BtnDef {
    const char* name;
    uint8_t pin;
    Bounce b;
};

BtnDef btns[] = {
    {"B1 OSC/INSERT", 2, Bounce()},     {"B2 ENV/SEND", 3, Bounce()},
    {"B3 LFO/MASTER", 4, Bounce()},     {"B4 PRESET/FAV", 5, Bounce()},
    {"B5 MODE", 40, Bounce()},          {"B6 HOLD/SHIFT", 41, Bounce()},
};

CRGB leds[NUM_LEDS];
uint32_t flash_until = 0;
CRGB flash_color;
bool dirty = true;

void check_idle_pin(const char* name, uint8_t pin) {
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(50);
    if (digitalRead(pin) == LOW)
        Serial.printf("  ⚠ %s (pin %d) lee LOW en reposo: corto a GND o cable cruzado\n", name, pin);
}

}  // namespace

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}

    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setBrightness(60);

    Serial.println("\n=== sketch46 — Panel de Studio: 5 encoders + 6 botones + 18 LEDs ===");
    Serial.println("Revisión de pines en reposo (todos deberían leer HIGH):");
    for (EncDef& e : encs) check_idle_pin(e.name, e.sw_pin);
    for (BtnDef& b : btns) check_idle_pin(b.name, b.pin);
    Serial.println("  (sin avisos = ok)");

    for (EncDef& e : encs) {
        e.sw.attach(e.sw_pin, INPUT_PULLUP);
        e.sw.interval(5);
        e.last_raw = e.enc->read();
    }
    for (BtnDef& b : btns) {
        b.b.attach(b.pin, INPUT_PULLUP);
        b.b.interval(5);
    }

    // Barrido de los 18 LEDs para ver que la cadena completa enciende.
    for (uint8_t i = 0; i < NUM_LEDS; ++i) {
        leds[i] = CRGB(0x1D, 0x9E, 0x75);
        FastLED.show();
        delay(30);
    }
    delay(300);
    FastLED.clear(true);
    Serial.println("Girá y apretá cada control. El ring muestra la posición con el color del encoder.");
    Serial.println("----------------------------------------------------------------------");
}

void loop() {
    const uint32_t now = millis();

    for (EncDef& e : encs) {
        e.sw.update();
        const int32_t raw = e.enc->read();
        const int32_t detents = (raw - e.last_raw) / PULSES_PER_DETENT;
        if (detents != 0) {
            e.last_raw += detents * PULSES_PER_DETENT;  // solo se consumen detents enteros
            e.total -= detents;                         // mismo convenio que ui/encoders: CW = +
            Serial.printf("[%s] delta=%+ld total=%ld\n", e.name, long(-detents), long(e.total));
            fill_solid(leds, NUM_RING, CRGB::Black);
            leds[((e.total % NUM_RING) + NUM_RING) % NUM_RING] = e.color;
            dirty = true;
        }
        if (e.sw.fell()) {
            Serial.printf("[%s] push\n", e.name);
            flash_color = e.color;
            flash_until = now + 150;
        }
    }

    for (uint8_t i = 0; i < 6; ++i) {
        BtnDef& b = btns[i];
        b.b.update();
        if (b.b.fell()) Serial.printf("[%s] pressed (pin %d)\n", b.name, b.pin);
        if (b.b.rose()) Serial.printf("[%s] released\n", b.name);
        const CRGB c = b.b.read() == LOW ? CRGB(0xF2, 0xF2, 0xF2) : CRGB::Black;
        if (leds[FIRST_KEYCAP + i] != c) {
            leds[FIRST_KEYCAP + i] = c;
            dirty = true;
        }
    }

    static bool flashing = false;
    if (now < flash_until) {
        fill_solid(leds, NUM_RING, flash_color);
        flashing = dirty = true;
    } else if (flashing) {
        fill_solid(leds, NUM_RING, CRGB::Black);
        flashing = false;
        dirty = true;
    }

    if (dirty) {
        FastLED.show();
        dirty = false;
    }
    delay(2);
}
