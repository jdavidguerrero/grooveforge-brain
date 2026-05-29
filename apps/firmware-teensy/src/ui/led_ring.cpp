/**
 * @file led_ring.cpp
 * @brief WS2812B LED ring — GrooveForge Brain (Sprint 35)
 *
 * ## Mapeo del ring (12 LEDs, GC9A01 y CoF en sync)
 *
 * En AI mode, el ring físico es un espejo de la rueda Camelot del display:
 *   LED 0 = C  (Camelot 8B, cyan)    — top del ring (12h)
 *   LED 1 = G  (9B, azul-cyan)
 *   LED 2 = D  (10B, azul)
 *   LED 3 = A  (11B, indigo)
 *   LED 4 = E  (12B, púrpura)
 *   LED 5 = B  (1B,  rojo)
 *   LED 6 = F# (2B,  naranja-rojo)
 *   LED 7 = C# (3B,  naranja)
 *   LED 8 = G# (4B,  amarillo)
 *   LED 9 = D# (5B,  verde-limón)
 *   LED 10= A# (6B,  verde)
 *   LED 11= F  (7B,  verde-teal)
 *
 * El brillo de cada LED = pitch_activity del pitch class correspondiente (0-255).
 * Si el ML está activo y el usuario toca en C Major, los LEDs C/E/G/B/D/A/F
 * brillarán más que los cromáticos (F#/C#/G#/D#/A#).
 *
 * ## Mapeo SYNTH — 4 quadrants de 3 LEDs:
 *   OSC    = LEDs 0-2   (verde claro)
 *   ENV    = LEDs 3-5   (teal)
 *   FILTER = LEDs 6-8   (cyan)
 *   LFO    = LEDs 9-11  (azul-teal)
 *
 * ## Mapeo SYNTH ENGINE_LIST — 3 engines en posiciones equidistantes:
 *   Moog    = LED 0, 1, 2, 3   (posición 0 → 90°)
 *   Juno    = LED 4, 5, 6, 7   (posición 4 → 120°)
 *   Prophet = LED 8, 9, 10, 11 (posición 8 → 240°)
 *   Engine activo → bright teal, otros → very dim teal
 *
 * Spec: apps/docs/sprints/29-hardware-integration.md §"LED ring"
 */

#include "led_ring.h"

/* ── Tabla de colores Camelot (CoF pos 0-11) ─────────────────────────────── */
/* Coinciden con ui_theme.h GF_COLOR_CAMELOT_0..11 (mismos hex) */
const CRGB LedRing::CAMELOT[12] = {
    CRGB(0x1F, 0xB6, 0xBB),  // 0: C  — 8B  cyan
    CRGB(0x1D, 0x86, 0xC4),  // 1: G  — 9B  azul-cyan
    CRGB(0x33, 0x58, 0xC0),  // 2: D  — 10B azul
    CRGB(0x55, 0x40, 0xB8),  // 3: A  — 11B indigo
    CRGB(0x8A, 0x2D, 0xB5),  // 4: E  — 12B púrpura
    CRGB(0xB6, 0x1F, 0x32),  // 5: B  — 1B  rojo
    CRGB(0xDD, 0x4D, 0x28),  // 6: F# — 2B  naranja-rojo
    CRGB(0xDD, 0x90, 0x2C),  // 7: C# — 3B  naranja
    CRGB(0xDD, 0xB4, 0x2A),  // 8: G# — 4B  amarillo
    CRGB(0xB0, 0xCC, 0x30),  // 9: D# — 5B  verde-limón
    CRGB(0x74, 0xC4, 0x43),  // 10: A# — 6B verde
    CRGB(0x2E, 0xBB, 0x87),  // 11: F  — 7B verde-teal
};

/** CoF position p → pitch class. Igual que view_10: {C,G,D,A,E,B,F#,C#,G#,D#,A#,F} */
const uint8_t LedRing::COF_TO_PC[12] = { 0, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10, 5 };

/* ── Instancia global ─────────────────────────────────────────────────────── */
LedRing led_ring;

/* ── Colores internos ─────────────────────────────────────────────────────── */

/* Teal para SYNTH mode — versiones: bright, medium, dim */
static constexpr CRGB TEAL_BRIGHT  = CRGB(0x1D, 0x9E, 0x75);  ///< GF_COLOR_TEAL
static constexpr CRGB TEAL_MEDIUM  = CRGB(0x0D, 0x4A, 0x38);  ///< ~50% teal
static constexpr CRGB TEAL_DIM     = CRGB(0x05, 0x18, 0x12);  ///< ~15% teal

/* Purple para FX mode */
static constexpr CRGB PURPLE_BRIGHT = CRGB(0x53, 0x4A, 0xB7);  ///< GF_COLOR_PURPLE
static constexpr CRGB PURPLE_DIM    = CRGB(0x15, 0x12, 0x2E);  ///< ~25% purple

/* Colores keycap fijos */
static constexpr CRGB WHITE_DIM     = CRGB(0x30, 0x30, 0x30);  ///< B1 HOME
static constexpr CRGB RED_DIM       = CRGB(0x40, 0x05, 0x05);  ///< B4 PANIC
static constexpr CRGB CYAN_DIM      = CRGB(0x08, 0x25, 0x28);  ///< B3 TAP TEMPO idle

/* Colores quadrant SYNTH por grupo:
 *   OSC    = verde-teal claro
 *   ENV    = teal puro
 *   FILTER = cyan
 *   LFO    = azul-teal
 */
static constexpr CRGB QUAD_COLORS[4] = {
    CRGB(0x1D, 0x9E, 0x75),  // OSC    — teal
    CRGB(0x20, 0x80, 0x60),  // ENV    — teal oscuro
    CRGB(0x1F, 0xB6, 0xBB),  // FILTER — cyan (= camelot C)
    CRGB(0x1D, 0x86, 0xC4),  // LFO    — azul-cyan (= camelot G)
};

/* ── Helpers privados ─────────────────────────────────────────────────────── */

void LedRing::_set_keycaps_fx() {
    _leds[LED_B1] = WHITE_DIM;
    _leds[LED_B2] = PURPLE_DIM;   /* mode color */
    _leds[LED_B3] = CYAN_DIM;
    _leds[LED_B4] = RED_DIM;
}

void LedRing::_set_keycaps_synth() {
    _leds[LED_B1] = WHITE_DIM;
    _leds[LED_B2] = TEAL_DIM;     /* mode color */
    _leds[LED_B3] = CYAN_DIM;
    _leds[LED_B4] = RED_DIM;
}

void LedRing::_set_keycaps_ai() {
    _leds[LED_B1] = WHITE_DIM;
    _leds[LED_B2] = TEAL_MEDIUM;  /* AI mode — ligeramente más brillante */
    _leds[LED_B3] = CYAN_DIM;
    _leds[LED_B4] = RED_DIM;
}

/* ── Implementación pública ───────────────────────────────────────────────── */

void LedRing::init() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(_leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear(true);  /* apaga todos, envía el frame vacío */
    Serial.println("[LED] WS2812B init OK — 16 LEDs @ GPIO 28, brightness=80");
}

/* ─── FX MODE ────────────────────────────────────────────────────────────── */

void LedRing::enter_fx_mode() {
    _mode = 0;
    update_fx();
}

void LedRing::update_fx() {
    /* Ring: todos dim purple — el cursor del FX está en el ESP32 */
    for (uint8_t i = 0; i < RING_COUNT; i++) _leds[i] = PURPLE_DIM;
    _set_keycaps_fx();
    show();
}

void LedRing::update_fx_wet(float wet, bool bypass) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;

    /* Número de LEDs encendidos (0-12). wet=1.0 → 12 LEDs, wet=0.0 → 0 LEDs.
     * Se usa roundf para que 50% lit = exactamente 6 LEDs (no 5 ni 7). */
    uint8_t lit = (uint8_t)(wet * (float)RING_COUNT + 0.5f);
    if (lit > RING_COUNT) lit = RING_COUNT;

    /* bypass OFF: alto contraste BRIGHT vs DIM — feedback inmediato del nivel.
     * bypass ON:  bajo contraste DIM vs casi-off — preset acumulado pero mute claro.
     * La diferencia perceptual en el anillo comunica el estado sin label. */
    CRGB col_lit  = bypass ? PURPLE_DIM                  : PURPLE_BRIGHT;
    CRGB col_dark = bypass ? CRGB(0x04, 0x03, 0x0A)      : PURPLE_DIM;

    for (uint8_t i = 0; i < RING_COUNT; i++) {
        _leds[i] = (i < lit) ? col_lit : col_dark;
    }

    /* Keycap B2 indica bypass: apagado en bypass ON, purple dim en bypass OFF. */
    _leds[LED_B1] = WHITE_DIM;
    _leds[LED_B2] = bypass ? CRGB(0x04, 0x03, 0x0A) : PURPLE_DIM;
    _leds[LED_B3] = CYAN_DIM;
    _leds[LED_B4] = RED_DIM;

    show();
}

/* ─── SYNTH MODE ─────────────────────────────────────────────────────────── */

void LedRing::enter_synth_mode(uint8_t synth_level,
                                uint8_t active_engine,
                                uint8_t active_group) {
    _mode = 1;
    update_synth(synth_level, active_engine, active_group);
}

void LedRing::update_synth(uint8_t synth_level,
                             uint8_t active_engine,
                             uint8_t active_group) {
    if (synth_level == 0) {
        /* ── ENGINE_LIST: 3 engines × 4 LEDs, activo = bright ───────────── */
        /* Moog=0-3, Juno=4-7, Prophet=8-11 */
        for (uint8_t e = 0; e < 3; e++) {
            CRGB c = (e == active_engine) ? TEAL_BRIGHT : TEAL_DIM;
            for (uint8_t i = 0; i < 4; i++) _leds[e * 4 + i] = c;
        }
        /* En ENGINE_LIST B2 keycap ligeramente más brillante (llamada a acción) */
        _leds[LED_B1] = WHITE_DIM;
        _leds[LED_B2] = TEAL_MEDIUM;
        _leds[LED_B3] = CYAN_DIM;
        _leds[LED_B4] = RED_DIM;
    } else if (synth_level == 1) {
        /* ── ENGINE_SUBHOME: todos los LEDs en teal suave, "en espera" ─── */
        for (uint8_t i = 0; i < RING_COUNT; i++) _leds[i] = TEAL_DIM;
        _set_keycaps_synth();
    } else {
        /* ── GROUP_VIEW: 4 quadrants, grupo activo = bright ────────────── */
        /* Cada grupo tiene 3 LEDs consecutivos. Activo = color del grupo bright.
         * Los demás = dim version del mismo color del grupo (indica que existen). */
        for (uint8_t g = 0; g < 4; g++) {
            CRGB base = QUAD_COLORS[g];
            bool is_active = (g == active_group);
            /* bright vs 25% dim — calculado manualmente para evitar fadeTo() */
            CRGB col = is_active ? base : CRGB(base.r >> 2, base.g >> 2, base.b >> 2);
            for (uint8_t i = 0; i < 3; i++) _leds[g * 3 + i] = col;
        }
        _set_keycaps_synth();
    }
    show();
}

/* ─── AI MODE ────────────────────────────────────────────────────────────── */

void LedRing::enter_ai_mode() {
    _mode = 2;
    /* El ring arrancará con todos los LEDs en el color Camelot base dim (opa=0 act) */
    uint8_t zero[12] = {};
    update_ai(zero);
}

void LedRing::update_ai(const uint8_t pitch_act[12]) {
    /* LED p (CoF position) = Camelot color de esa posición,
     * brillo = pitch_activity del pitch class en esa posición.
     *
     * La escala 0-255 de pitch_act ya es perceptualmente adecuada:
     *   act=0   → LED prácticamente apagado (mínimo dim para ver el color)
     *   act=255 → LED a full color (limitado por BRIGHTNESS=80 global)
     *
     * Mínimo dim = 15/255 para que el anillo siempre sea visible como "marco".
     */
    for (uint8_t p = 0; p < RING_COUNT; p++) {
        uint8_t pc  = COF_TO_PC[p];
        uint8_t act = pitch_act[pc];

        /* Interpolar entre 15% (idle) y 100% (full activity) del color Camelot */
        uint8_t scale = (uint8_t)(15u + (uint32_t)act * 241u / 255u);
        _leds[p] = CRGB(
            (uint8_t)((uint32_t)CAMELOT[p].r * scale / 255u),
            (uint8_t)((uint32_t)CAMELOT[p].g * scale / 255u),
            (uint8_t)((uint32_t)CAMELOT[p].b * scale / 255u)
        );
    }
    _set_keycaps_ai();
    show();
}

/* ─── Flashes efímeros ───────────────────────────────────────────────────── */

void LedRing::tap_tempo_flash() {
    /* Flash B3 en teal bright por 80ms — no bloquea loop() gracias al delay breve */
    _leds[LED_B3] = TEAL_BRIGHT;
    FastLED.show();
    delay(80);
    _leds[LED_B3] = CYAN_DIM;
    FastLED.show();
}

void LedRing::panic_flash() {
    /* Flash de TODO el anillo en rojo por 120ms */
    for (uint8_t i = 0; i < NUM_LEDS; i++) _leds[i] = CRGB(0x60, 0x00, 0x00);
    FastLED.show();
    delay(120);
    /* Restaurar el estado previo llamando al update correspondiente al modo */
    /* (el sketch llama update_*() en el siguiente tick de 50ms) */
    FastLED.show();
}

void LedRing::show() {
    FastLED.show();
}
