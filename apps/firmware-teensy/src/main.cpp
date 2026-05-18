/**
 * @file main.cpp
 * @brief GrooveForge Brain — Teensy 4.1 firmware entry point.
 *
 * Sprint 3.3: UI subsystem (encoders, buttons, LEDs) integrated.
 * Audio engines and FX are compiled but not instantiated here yet —
 * engine selection and audio graph wiring are Sprint 4.x scope.
 *
 * Refs: apps/docs/01-architecture.md §3.3 — pin mapping
 *       apps/docs/sprints/20-encoders-buttons.md — UI theory + wiring table
 */

#include <Arduino.h>
#include "ui/encoders.h"
#include "ui/buttons.h"
#include "ui/leds.h"
#include "ui/ui_manager.h"

// ── Pin 25: CD4066 filter bypass control (01-architecture.md §3.3) ──────────
static constexpr uint8_t PIN_FILTER_BYPASS = 25;

// ── UI subsystem instances ───────────────────────────────────────────────────
static Encoders    g_encoders;
static Buttons     g_buttons;
static Leds        g_leds;
static UIManager   g_ui;

// ── Filter bypass state ──────────────────────────────────────────────────────
static bool g_filter_bypassed = false;

// ── Application-level callbacks ─────────────────────────────────────────────

static void on_param_changed(uint8_t param_id, float delta) {
    // TODO Sprint 4.x: route to active engine
    // For now: reflect cutoff/resonance changes on serial for debugging.
    if (param_id == PARAM_CUTOFF) {
        Serial.printf("[UI] cutoff delta=%.2f\n", delta);
    } else if (param_id == PARAM_RESONANCE) {
        Serial.printf("[UI] resonance delta=%.2f\n", delta);
    }
}

static void on_button(uint8_t btn_idx) {
    switch (btn_idx) {
        case 0: Serial.println("[UI] B1 pressed"); break;
        case 1: Serial.println("[UI] B2 pressed"); break;
        case 2: Serial.println("[UI] B3 pressed"); break;
        case 3: Serial.println("[UI] B4 pressed"); break;
        case BTN_CONFIRM:
            Serial.println("[UI] ENC NAV confirm");
            break;
        case BTN_MODE_SWITCH:
            Serial.println("[UI] mode switch");
            // Toggle ring color between synth (teal) and FX (purple) modes.
            static bool fx_mode = false;
            fx_mode = !fx_mode;
            if (fx_mode) {
                g_leds.set_all(CRGB(0x53, 0x4A, 0xB7));  // #534AB7 purple
            } else {
                g_leds.set_all(CRGB(0x1D, 0x9E, 0x75));  // #1D9E75 teal
            }
            g_leds.show();
            break;
        case 6:  // BTN_FILTER_BYPASS (ENC L push → CD4066 toggle)
            g_filter_bypassed = !g_filter_bypassed;
            digitalWrite(PIN_FILTER_BYPASS, g_filter_bypassed ? HIGH : LOW);
            Serial.printf("[UI] filter bypass=%s\n", g_filter_bypassed ? "ON" : "OFF");
            break;
        case 7:  // BTN_RESONANCE_RESET (ENC R push)
            Serial.println("[UI] resonance reset to 0");
            // TODO Sprint 4.x: g_engine.setFilterResonance(0.0f);
            break;
        default:
            break;
    }
}

static void on_nav(int32_t delta) {
    Serial.printf("[UI] nav delta=%d\n", delta);
    // TODO Sprint 4.x: update active menu position and refresh display via Bridge.
}

// ── Arduino entry points ─────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    // CD4066 filter bypass: start with filter active (LOW = filter in circuit).
    pinMode(PIN_FILTER_BYPASS, OUTPUT);
    digitalWrite(PIN_FILTER_BYPASS, LOW);

    g_encoders.init();
    g_buttons.init();
    g_leds.init();

    g_ui.init(&g_encoders, &g_buttons, &g_leds);
    g_ui.on_param_changed(on_param_changed);
    g_ui.on_button(on_button);
    g_ui.on_nav(on_nav);

    // Boot indication: brief teal pulse on ring.
    g_leds.set_all(CRGB(0x1D, 0x9E, 0x75));
    g_leds.show();
    delay(300);
    g_leds.clear();

    Serial.println("[BOOT] GrooveForge Brain — Sprint 3.3 UI ready");
}

void loop() {
    g_ui.update();
}
