// apps/firmware-teensy/src/sketches/13-cymatic-resonator.cpp
// Sprint 2.8 — FX Cymatic Resonator demo
// Theory: apps/docs/sprints/13-cymatic-resonator.md (pendiente)
//
// Chord C mayor (C4+E4+G4) como fuente de audio continua.
// CymaticResonator como capa FX — 4 filtros bandpass resonantes en paralelo
// (ratios 1:2:3:5 sobre 440Hz) con LFO de modulación ±5%.
//
// Comandos Serial (115200 baud):
//   t<val>  tune ratio      0.5–2.0
//   d<n>    density         1–4 (modos activos)
//   r<val>  resonance Q     10.0–80.0
//   l<val>  lfo rate        0.1–3.0 Hz
//   m<val>  mix             0.0–1.0
//   p       bypass toggle

#include <Arduino.h>
#include <Audio.h>
#include "fx/cymatic_resonator.h"

// ── Audio graph — fuente: chord C mayor sostenido ────────────────────────────────
// Teensy Audio Library (oficial)
AudioSynthWaveform osc1;     // C4 = 261.63 Hz
AudioSynthWaveform osc2;     // E4 = 329.63 Hz
AudioSynthWaveform osc3;     // G4 = 392.00 Hz
AudioMixer4        srcMix;   // suma mono para el FX

AudioConnection c1(osc1, 0, srcMix, 0);
AudioConnection c2(osc2, 0, srcMix, 1);
AudioConnection c3(osc3, 0, srcMix, 2);

// ── Output compartido ─────────────────────────────────────────────────────────────
AudioMixer4          outMixL;
AudioMixer4          outMixR;
AudioOutputI2S       audioOut;
AudioControlSGTL5000 codec;

// ── FX ───────────────────────────────────────────────────────────────────────────
// Custom — apps/firmware-teensy/src/fx/cymatic_resonator.h
CymaticResonator fx;

AudioConnection cFxL(fx.getOutputL(), 0, outMixL, 0);
AudioConnection cFxR(fx.getOutputR(), 0, outMixR, 0);
AudioConnection cOutL(outMixL, 0, audioOut, 0);
AudioConnection cOutR(outMixR, 0, audioOut, 1);

// ── Estado de parámetros (fuente de verdad — clamp doble: aquí y en el setter) ──
static float   g_tune       = 1.0f;    // 0.5–2.0
static uint8_t g_density    = 4;       // 1–4
static float   g_resonance  = 30.0f;   // 10.0–80.0 Q
static float   g_lfoRate    = 0.5f;    // 0.1–3.0 Hz
static float   g_mix        = 0.7f;    // 0.0–1.0
static bool    g_bypass     = false;

// ── setup() ──────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1200);

    AudioMemory(30);
    codec.enable();
    codec.volume(0.7f);

    outMixL.gain(0, 1.0f);
    outMixR.gain(0, 1.0f);

    // Fuente: chord C mayor en sawtooth, amplitud 0.25 por osc
    osc1.begin(WAVEFORM_SAWTOOTH); osc1.frequency(261.63f); osc1.amplitude(0.25f);
    osc2.begin(WAVEFORM_SAWTOOTH); osc2.frequency(329.63f); osc2.amplitude(0.25f);
    osc3.begin(WAVEFORM_SAWTOOTH); osc3.frequency(392.00f); osc3.amplitude(0.25f);

    // gain 1/3 por canal para evitar clipping en la suma de 3 oscs
    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);
    srcMix.gain(3, 0.0f);

    // begin() crea las conexiones dinámicas desde srcMix
    fx.begin(srcMix);

    fx.setTune(g_tune);
    fx.setDensity(g_density);
    fx.setResonance(g_resonance);
    fx.setLfoRate(g_lfoRate);
    fx.setMix(g_mix);
    fx.setBypass(g_bypass);

    Serial.println(F("=== Sprint 2.8 --- FX Cymatic Resonator ==="));
    Serial.println(F("4 filtros BP en paralelo (ratios 1:2:3:5 sobre 440Hz)"));
    Serial.println(F("LFO modula tune +-5% para movimiento cristalino"));
    Serial.println();
    Serial.println(F("  t<val>  tune ratio (0.5-2.0)   -- 1.0=La, 0.5=La-8va, 2.0=La+8va"));
    Serial.println(F("  d<n>    density modos (1-4)    -- 1=fundamental, 4=pleno"));
    Serial.println(F("  r<val>  resonance Q (10-80)    -- Q=30 sweet spot"));
    Serial.println(F("  l<val>  lfo rate Hz (0.1-3.0)  -- 0.5Hz=respiracion sutil"));
    Serial.println(F("  m<val>  mix (0.0-1.0)"));
    Serial.println(F("  p       bypass toggle"));
    Serial.println();
    Serial.println(F("Demo sugerido:"));
    Serial.println(F("  d1  solo fundamental (resonancia simple)"));
    Serial.println(F("  d4  todos los modos (pleno cristalino)"));
    Serial.println(F("  r60 Q alto (muy estrecho, metalico)"));
    Serial.println(F("  t0.5 tune una octava abajo (graves)"));
    Serial.println(F("  l2.0 LFO rapido (vibrante)"));
    Serial.println();
    Serial.println(F("Defaults: t1.0 d4 r30.0 l0.5 m0.7 bypass=OFF"));
    Serial.println();
}

// ── loop() ───────────────────────────────────────────────────────────────────────
void loop() {
    // Actualizar LFO del resonador — avanza fase y aplica modulación de tune
    fx.update();

    // ── Parser de comandos Serial ─────────────────────────────────────────────────
    if (Serial.available()) {
        char cmd = Serial.read();

        if (cmd == 't') {
            float val = Serial.parseFloat();
            if (val < 0.5f) val = 0.5f;
            if (val > 2.0f) val = 2.0f;
            g_tune = val;
            fx.setTune(g_tune);
            Serial.print(F("Tune: "));
            Serial.println(g_tune, 2);
        }
        else if (cmd == 'd') {
            uint8_t val = (uint8_t)Serial.parseInt();
            if (val < 1) val = 1;
            if (val > 4) val = 4;
            g_density = val;
            fx.setDensity(g_density);
            Serial.print(F("Density: "));
            Serial.print(g_density);
            Serial.println(F(" modos"));
        }
        else if (cmd == 'r') {
            float val = Serial.parseFloat();
            if (val < 10.0f) val = 10.0f;
            if (val > 80.0f) val = 80.0f;
            g_resonance = val;
            fx.setResonance(g_resonance);
            Serial.print(F("Resonance Q: "));
            Serial.println(g_resonance, 1);
        }
        else if (cmd == 'l') {
            float val = Serial.parseFloat();
            if (val < 0.1f) val = 0.1f;
            if (val > 3.0f) val = 3.0f;
            g_lfoRate = val;
            fx.setLfoRate(g_lfoRate);
            Serial.print(F("LFO Rate: "));
            Serial.print(g_lfoRate, 2);
            Serial.println(F(" Hz"));
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
        Serial.print(F(" | Tune: "));
        Serial.print(g_tune, 2);
        Serial.print(F(" | Density: "));
        Serial.print(g_density);
        Serial.print(F(" | Q: "));
        Serial.print(g_resonance, 1);
        Serial.print(F(" | LFO: "));
        Serial.print(g_lfoRate, 2);
        Serial.print(F("Hz | Mix: "));
        Serial.print(g_mix, 2);
        Serial.print(F(" | Bypass: "));
        Serial.println(g_bypass ? F("ON") : F("OFF"));
    }
}
