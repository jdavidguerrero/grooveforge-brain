// apps/firmware-teensy/src/sketches/14-granular-cloud.cpp
// Sprint 2.9 — FX Granular Cloud demo
// Theory: apps/docs/sprints/14-granular-cloud.md (pendiente)
//
// Chord C mayor (C4+E4+G4) como fuente de audio continua.
// GranularCloud como capa FX — AudioEffectGranular con modo passthrough y freeze.
//
// Comandos Serial (115200 baud):
//   g<val>  grain size       5.0–250.0 ms
//   s<val>  speed/pitch      0.25–4.0 ratio
//   z       freeze toggle
//   m<val>  mix              0.0–1.0
//   p       bypass toggle

#include <Arduino.h>
#include <Audio.h>
#include "fx/granular_cloud.h"

// ── Audio graph — fuente: chord C mayor sostenido ────────────────────────────────
// Teensy Audio Library (oficial)
AudioSynthWaveform osc1;     // C4 = 261.63 Hz
AudioSynthWaveform osc2;     // E4 = 329.63 Hz
AudioSynthWaveform osc3;     // G4 = 392.00 Hz
AudioMixer4        srcMix;

AudioConnection c1(osc1, 0, srcMix, 0);
AudioConnection c2(osc2, 0, srcMix, 1);
AudioConnection c3(osc3, 0, srcMix, 2);

// ── Output compartido ─────────────────────────────────────────────────────────────
AudioMixer4          outMixL;
AudioMixer4          outMixR;
AudioOutputI2S       audioOut;
AudioControlSGTL5000 codec;

// ── FX ───────────────────────────────────────────────────────────────────────────
// Custom — apps/firmware-teensy/src/fx/granular_cloud.h
GranularCloud fx;

AudioConnection cFxL(fx.getOutputL(), 0, outMixL, 0);
AudioConnection cFxR(fx.getOutputR(), 0, outMixR, 0);
AudioConnection cOutL(outMixL, 0, audioOut, 0);
AudioConnection cOutR(outMixR, 0, audioOut, 1);

// ── Estado de parámetros ─────────────────────────────────────────────────────────
static float g_grainSize = 100.0f;   // 5.0–250.0 ms
static float g_speed     = 1.0f;     // 0.25–4.0 ratio
static bool  g_freeze    = false;
static float g_mix       = 0.5f;     // 0.0–1.0
static bool  g_bypass    = false;

// ── setup() ──────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1200);

    AudioMemory(30);
    codec.enable();
    codec.volume(0.7f);

    outMixL.gain(0, 1.0f);
    outMixR.gain(0, 1.0f);

    osc1.begin(WAVEFORM_SAWTOOTH); osc1.frequency(261.63f); osc1.amplitude(0.25f);
    osc2.begin(WAVEFORM_SAWTOOTH); osc2.frequency(329.63f); osc2.amplitude(0.25f);
    osc3.begin(WAVEFORM_SAWTOOTH); osc3.frequency(392.00f); osc3.amplitude(0.25f);

    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);
    srcMix.gain(3, 0.0f);

    fx.begin(srcMix);

    fx.setGrainSize(g_grainSize);
    fx.setSpeed(g_speed);
    fx.setFreeze(g_freeze);
    fx.setMix(g_mix);
    fx.setBypass(g_bypass);

    Serial.println(F("=== Sprint 2.9 --- FX Granular Cloud ==="));
    Serial.println(F("Granulizador con freeze mode (AudioEffectGranular)"));
    Serial.println(F("Ref: Roads \"Microsound\" (MIT Press, 2001)"));
    Serial.println();
    Serial.println(F("  g<val>  grain size ms (5-250)  -- 100ms=nube, 20ms=pitch"));
    Serial.println(F("  s<val>  speed ratio (0.25-4.0) -- 1.0=original, 0.5=8va abajo"));
    Serial.println(F("  z       freeze toggle          -- captura nube estatica ambient"));
    Serial.println(F("  m<val>  mix (0.0-1.0)"));
    Serial.println(F("  p       bypass toggle"));
    Serial.println();
    Serial.println(F("Demo sugerido:"));
    Serial.println(F("  m0.8   subir el wet para oir el efecto"));
    Serial.println(F("  z      freeze! (la nube se congela como pad ambient)"));
    Serial.println(F("  z      unfreeze (vuelve a tiempo real)"));
    Serial.println(F("  g20    granos cortos (pitch mas definido)"));
    Serial.println(F("  g200   granos largos (textura, sin pitch aparente)"));
    Serial.println(F("  s0.5   pitch una octava abajo"));
    Serial.println(F("  s2.0   pitch una octava arriba"));
    Serial.println();
    Serial.println(F("Defaults: g100 s1.0 freeze=OFF m0.5 bypass=OFF"));
    Serial.println();
}

// ── loop() ───────────────────────────────────────────────────────────────────────
void loop() {
    // update() es no-op en v1.0 — la llamamos por consistencia con otros FX
    fx.update();

    if (Serial.available()) {
        char cmd = Serial.read();

        if (cmd == 'g') {
            float val = Serial.parseFloat();
            if (val < 5.0f)   val = 5.0f;
            if (val > 250.0f) val = 250.0f;
            g_grainSize = val;
            fx.setGrainSize(g_grainSize);
            Serial.print(F("Grain size: "));
            Serial.print(g_grainSize, 1);
            Serial.println(F(" ms"));
        }
        else if (cmd == 's') {
            float val = Serial.parseFloat();
            if (val < 0.25f) val = 0.25f;
            if (val > 4.0f)  val = 4.0f;
            g_speed = val;
            fx.setSpeed(g_speed);
            Serial.print(F("Speed: "));
            Serial.print(g_speed, 2);
            Serial.println(F("x"));
        }
        else if (cmd == 'z') {
            g_freeze = !g_freeze;
            fx.setFreeze(g_freeze);
            Serial.print(F("Freeze: "));
            Serial.println(g_freeze ? F("ON (nube estatica)") : F("OFF (passthrough)"));
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
        Serial.print(F(" | Grain: "));
        Serial.print(g_grainSize, 1);
        Serial.print(F("ms | Speed: "));
        Serial.print(g_speed, 2);
        Serial.print(F("x | Freeze: "));
        Serial.print(g_freeze ? F("ON") : F("OFF"));
        Serial.print(F(" | Mix: "));
        Serial.print(g_mix, 2);
        Serial.print(F(" | Bypass: "));
        Serial.println(g_bypass ? F("ON") : F("OFF"));
    }
}
