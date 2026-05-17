// apps/firmware-teensy/src/sketches/15-spring-plate.cpp
// Sprint 2.10 — FX Spring + Plate demo
// Theory: apps/docs/sprints/15-spring-plate.md (pendiente)
//
// Chord C mayor (C4+E4+G4) como fuente de audio continua.
// SpringPlate como capa FX — dual Freeverb con tuning de spring y plate vintage.
//
// Comandos Serial (115200 baud):
//   a<n>    algorithm        0=spring 1=plate 2=blend
//   r<val>  room size        0.0–1.0 (solo activo en algorithm=2)
//   d<val>  damping          0.0–1.0 (solo activo en algorithm=2)
//   b<val>  blend            0.0–1.0 (0=spring, 1=plate, solo en algorithm=2)
//   m<val>  mix              0.0–1.0
//   p       bypass toggle

#include <Arduino.h>
#include <Audio.h>
#include "fx/spring_plate.h"

// ── Audio graph — fuente: chord C mayor sostenido ────────────────────────────────
// Teensy Audio Library (oficial)
AudioSynthWaveform osc1;     // C4 = 261.63 Hz
AudioSynthWaveform osc2;     // E4 = 329.63 Hz
AudioSynthWaveform osc3;     // G4 = 392.00 Hz
AudioMixer4        srcMix;

AudioConnection c1(osc1, 0, srcMix, 0);
AudioConnection c2(osc2, 0, srcMix, 1);
AudioConnection c3(osc3, 0, srcMix, 2);

// ── FX ───────────────────────────────────────────────────────────────────────────
// Custom — apps/firmware-teensy/src/fx/spring_plate.h
SpringPlate fx;

// ── Estado de parámetros ─────────────────────────────────────────────────────────
static uint8_t g_algorithm = 0;     // 0=spring, 1=plate, 2=blend
static float   g_roomSize  = 0.5f;  // 0.0–1.0
static float   g_damping   = 0.5f;  // 0.0–1.0
static float   g_blend     = 0.5f;  // 0.0–1.0
static float   g_mix       = 0.5f;  // 0.0–1.0
static bool    g_bypass    = false;

// ── Helpers ──────────────────────────────────────────────────────────────────────
static const char* algorithm_name(uint8_t a) {
    switch (a) {
        case 0: return "SPRING (vintage tank)";
        case 1: return "PLATE (EMT 140 style)";
        case 2: return "BLEND";
        default: return "?";
    }
}

// ── setup() ──────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1200);

    osc1.begin(WAVEFORM_SAWTOOTH); osc1.frequency(261.63f); osc1.amplitude(0.25f);
    osc2.begin(WAVEFORM_SAWTOOTH); osc2.frequency(329.63f); osc2.amplitude(0.25f);
    osc3.begin(WAVEFORM_SAWTOOTH); osc3.frequency(392.00f); osc3.amplitude(0.25f);

    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);
    srcMix.gain(3, 0.0f);

    fx.begin(srcMix, 0.7f);

    fx.setAlgorithm(g_algorithm);
    fx.setRoomSize(g_roomSize);
    fx.setDamping(g_damping);
    fx.setBlend(g_blend);
    fx.setMix(g_mix);
    fx.setBypass(g_bypass);

    Serial.println(F("=== Sprint 2.10 --- FX Spring + Plate ==="));
    Serial.println(F("Dual reverb: spring tank vintage + plate EMT 140"));
    Serial.println(F("Ref: Parker \"Efficient Simulation of Spring Reverb\" (DAFX 2011)"));
    Serial.println(F("Ref: Schroeder \"Natural Sounding Artificial Reverberation\" (JAES, 1962)"));
    Serial.println();
    Serial.println(F("  a<n>    algorithm (0=spring, 1=plate, 2=blend)"));
    Serial.println(F("  r<val>  room size 0.0-1.0  (solo en blend mode)"));
    Serial.println(F("  d<val>  damping 0.0-1.0    (solo en blend mode)"));
    Serial.println(F("  b<val>  blend 0.0-1.0      (0=spring, 1=plate, solo en blend)"));
    Serial.println(F("  m<val>  mix 0.0-1.0"));
    Serial.println(F("  p       bypass toggle"));
    Serial.println();
    Serial.println(F("Demo sugerido:"));
    Serial.println(F("  a0  spring puro (Hammond/Fender vintage, ~1.5s decay)"));
    Serial.println(F("  a1  plate puro  (EMT 140 style, ~3.5s decay, silk)"));
    Serial.println(F("  a2  blend mode"));
    Serial.println(F("  b0.0  100% spring en blend mode"));
    Serial.println(F("  b1.0  100% plate en blend mode"));
    Serial.println(F("  b0.5  mezcla 50/50 spring + plate"));
    Serial.println(F("  r0.3  room size bajo (decay corto en blend)"));
    Serial.println(F("  d0.8  damping alto (oscuro en blend)"));
    Serial.println();
    Serial.println(F("Defaults: a0 r0.5 d0.5 b0.5 m0.5 bypass=OFF"));
    Serial.println();
}

// ── loop() ───────────────────────────────────────────────────────────────────────
void loop() {
    fx.update();   // no-op v1.0 — por consistencia con otros FX

    if (Serial.available()) {
        char cmd = Serial.read();

        if (cmd == 'a') {
            uint8_t val = (uint8_t)Serial.parseInt();
            if (val > 2) val = 2;
            g_algorithm = val;
            fx.setAlgorithm(g_algorithm);
            Serial.print(F("Algorithm: "));
            Serial.println(algorithm_name(g_algorithm));
        }
        else if (cmd == 'r') {
            float val = Serial.parseFloat();
            if (val < 0.0f) val = 0.0f;
            if (val > 1.0f) val = 1.0f;
            g_roomSize = val;
            fx.setRoomSize(g_roomSize);
            Serial.print(F("Room size: "));
            Serial.println(g_roomSize, 2);
        }
        else if (cmd == 'd') {
            float val = Serial.parseFloat();
            if (val < 0.0f) val = 0.0f;
            if (val > 1.0f) val = 1.0f;
            g_damping = val;
            fx.setDamping(g_damping);
            Serial.print(F("Damping: "));
            Serial.println(g_damping, 2);
        }
        else if (cmd == 'b') {
            float val = Serial.parseFloat();
            if (val < 0.0f) val = 0.0f;
            if (val > 1.0f) val = 1.0f;
            g_blend = val;
            fx.setBlend(g_blend);
            Serial.print(F("Blend: "));
            Serial.print(g_blend, 2);
            Serial.println(F(" (0=spring, 1=plate)"));
        }
        else if (cmd == 'm') {
            float val = Serial.parseFloat();
            if (val < 0.0f) val = 0.0f;
            if (val > 1.0f) val = 1.0f;
            g_mix = val;
            fx.setMix(g_mix);
            Serial.print(F("Mix: "));
            Serial.println(g_mix, 2);
        }
        else if (cmd == 'p') {
            g_bypass = !g_bypass;
            fx.setBypass(g_bypass);
            Serial.print(F("Bypass: "));
            Serial.println(g_bypass ? F("ON") : F("OFF"));
        }
    }

    // ── Reporte de estado cada 1 segundo ─────────────────────────────────────────
    static uint32_t last_report = 0;
    uint32_t now = millis();
    if (now - last_report >= 1000) {
        last_report = now;
        Serial.print(F("CPU: "));
        Serial.print(AudioProcessorUsage(), 1);
        Serial.print(F("% | Mem: "));
        Serial.print(AudioMemoryUsage());
        Serial.print(F("/"));
        Serial.print(AudioMemoryUsageMax());
        Serial.print(F(" | Alg: "));
        Serial.print(algorithm_name(g_algorithm));
        if (g_algorithm == 2) {
            Serial.print(F(" | Room: "));
            Serial.print(g_roomSize, 2);
            Serial.print(F(" | Damp: "));
            Serial.print(g_damping, 2);
            Serial.print(F(" | Blend: "));
            Serial.print(g_blend, 2);
        }
        Serial.print(F(" | Mix: "));
        Serial.print(g_mix, 2);
        Serial.print(F(" | Bypass: "));
        Serial.println(g_bypass ? F("ON") : F("OFF"));
    }
}
