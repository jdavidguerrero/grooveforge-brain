// apps/firmware-teensy/src/sketches/08-tape-saturate.cpp
// Sprint 2.3 — FX Tape Saturate: demo interactivo via Serial.
// Theory y signal flow: apps/docs/sprints/08-tape-saturate.md
//
// El sketch es dueño del AudioOutputI2S, AudioControlSGTL5000 y AudioMemory compartidos.
// TapeSaturate expone getOutputL()/getOutputR() para conectar al output mixer del sketch.
// Este patrón permite declarar múltiples FX en el mismo sketch sin conflictos de I2S.
//
// Comandos Serial (115200 baud):
//   d<valor>  → Drive (1.0–10.0)       ej: d5.0
//   w<valor>  → Wow depth (0.0–1.0)    ej: w0.8
//   f<valor>  → Flutter depth (0.0–1.0) ej: f0.5
//   a<valor>  → Age (0.0–1.0)          ej: a0.7
//   b         → Toggle bypass on/off
//   m<valor>  → Mix wet (0.0–1.0)      ej: m0.5

#include <Arduino.h>
// Teensy Audio Library (oficial)
#include <Audio.h>
// Código custom Sprint 2.3
#include "../fx/tape_saturate.h"

// ── Fuente de audio: 3 osciladores (chord C mayor — C4, E4, G4) ──────────────────
// Patrón: grafo independiente del engine → valida el FX sin acoplar a Prophet5.
// Los 3 AudioSynthWaveform simulan las voces que normalmente vendría del engine.
AudioSynthWaveform osc1;   // C4 — 261.63 Hz
AudioSynthWaveform osc2;   // E4 — 329.63 Hz
AudioSynthWaveform osc3;   // G4 — 392.00 Hz
AudioMixer4        srcMix;

// ── Conexiones: oscs → srcMix ─────────────────────────────────────────────────────
AudioConnection c1(osc1, 0, srcMix, 0);
AudioConnection c2(osc2, 0, srcMix, 1);
AudioConnection c3(osc3, 0, srcMix, 2);

// ── Output compartido — una sola instancia de AudioOutputI2S por sketch ──────────
// TapeSaturate ya no contiene AudioOutputI2S: el sketch es dueño del output.
AudioMixer4          outMixL;
AudioMixer4          outMixR;
AudioOutputI2S       audioOut;
AudioControlSGTL5000 codec;

// ── TapeSaturate: expone getOutputL()/getOutputR() para conectar aquí ────────────
TapeSaturate tape;

// Conexiones FX → output compartido (creadas después de tape, para que el linker
// resuelva el orden de los objetos AudioStream en el grafo del scheduler)
AudioConnection cFxL(tape.getOutputL(), 0, outMixL, 0);
AudioConnection cFxR(tape.getOutputR(), 0, outMixR, 0);
AudioConnection cOutL(outMixL, 0, audioOut, 0);
AudioConnection cOutR(outMixR, 0, audioOut, 1);

// ── Frecuencias base del acorde C mayor ──────────────────────────────────────────
static constexpr float FREQ_C4 = 261.63f;
static constexpr float FREQ_E4 = 329.63f;
static constexpr float FREQ_G4 = 392.00f;

// ── Estado de parámetros actuales (para el reporte Serial) ───────────────────────
static float g_drive   = 2.0f;
static float g_wow     = 0.3f;
static float g_flutter = 0.1f;
static float g_age     = 0.2f;
static float g_mix     = 0.7f;
static bool  g_bypass  = false;

void setup() {
    Serial.begin(115200);

    // El sketch es dueño de AudioMemory y el codec — TapeSaturate ya no los llama.
    // 80 bloques: mismo budget que antes (Prophet-5 compatible).
    AudioMemory(80);

    codec.enable();
    codec.volume(0.5f);

    // outMixL/R: solo el canal 0 activo (FX conectado en ch0). Gain 1.0 = pass-through.
    outMixL.gain(0, 1.0f);
    outMixR.gain(0, 1.0f);

    // srcMix: gain uniforme para los 3 osciladores (0.33 × 3 = ~1.0 amplitud total)
    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);
    srcMix.gain(3, 0.0f);

    // Osciladores: sawtooth (rico en armónicos — ideal para demostrar saturación)
    osc1.begin(WAVEFORM_SAWTOOTH);
    osc1.frequency(FREQ_C4);
    osc1.amplitude(0.4f);

    osc2.begin(WAVEFORM_SAWTOOTH);
    osc2.frequency(FREQ_E4);
    osc2.amplitude(0.4f);

    osc3.begin(WAVEFORM_SAWTOOTH);
    osc3.frequency(FREQ_G4);
    osc3.amplitude(0.4f);

    // TapeSaturate: solo crea las conexiones dinámicas desde srcMix
    tape.begin(srcMix);

    // Valores de demo: saturación sutil que demuestra calidez sin exagerar
    tape.setDrive(g_drive);
    tape.setWow(g_wow);
    tape.setFlutter(g_flutter);
    tape.setAge(g_age);
    tape.setMix(g_mix);

    Serial.println("=== Sprint 2.3 — FX Tape Saturate ===");
    Serial.println("Chord C mayor (C4 + E4 + G4) | 3 sawtooth oscs | Tape Saturate activo");
    Serial.println();
    Serial.println("Comandos:");
    Serial.println("  d<val>  = Drive    1.0–10.0  (ej: d5.0)");
    Serial.println("  w<val>  = Wow      0.0–1.0   (ej: w0.8)");
    Serial.println("  f<val>  = Flutter  0.0–1.0   (ej: f0.5)");
    Serial.println("  a<val>  = Age      0.0–1.0   (ej: a0.7)");
    Serial.println("  m<val>  = Mix wet  0.0–1.0   (ej: m0.5)");
    Serial.println("  b       = Toggle bypass");
    Serial.println();
    Serial.printf("Defaults: Drive=%.1f Wow=%.1f Flutter=%.1f Age=%.1f Mix=%.1f\n",
                  g_drive, g_wow, g_flutter, g_age, g_mix);
    Serial.println();
    Serial.println("Demo sugerido:");
    Serial.println("  1. Escuchar (bypass off actualmente)");
    Serial.println("  2. 'b' — bypass ON, comparar señal limpia");
    Serial.println("  3. 'b' — bypass OFF, notar calidez");
    Serial.println("  4. d8.0 — saturación agresiva");
    Serial.println("  5. a0.9 — rolloff de cinta envejecida");
    Serial.println("  6. w1.0 — wow pronunciado");
}

void loop() {
    static uint32_t lastMs     = millis();
    static uint32_t lastReport = millis();

    uint32_t now   = millis();
    float    dt_ms = (float)(now - lastMs);
    lastMs = now;

    // Avanza LFOs de Wow y Flutter
    tape.update(dt_ms);

    // Aplica modulación de pitch de Wow+Flutter a los 3 osciladores
    // f_modulada = f_base * (1.0 + mod)
    float mod = tape.getWowFlutterMod();
    osc1.frequency(FREQ_C4 * (1.0f + mod));
    osc2.frequency(FREQ_E4 * (1.0f + mod));
    osc3.frequency(FREQ_G4 * (1.0f + mod));

    // Procesamiento de comandos Serial
    if (Serial.available()) {
        char cmd = Serial.read();

        switch (cmd) {
            case 'd': {
                g_drive = Serial.parseFloat();
                tape.setDrive(g_drive);
                Serial.printf("Drive: %.2f\n", g_drive);
                break;
            }
            case 'w': {
                g_wow = Serial.parseFloat();
                tape.setWow(g_wow);
                Serial.printf("Wow: %.2f\n", g_wow);
                break;
            }
            case 'f': {
                g_flutter = Serial.parseFloat();
                tape.setFlutter(g_flutter);
                Serial.printf("Flutter: %.2f\n", g_flutter);
                break;
            }
            case 'a': {
                g_age = Serial.parseFloat();
                tape.setAge(g_age);
                Serial.printf("Age: %.2f\n", g_age);
                break;
            }
            case 'm': {
                g_mix = Serial.parseFloat();
                tape.setMix(g_mix);
                Serial.printf("Mix: %.2f\n", g_mix);
                break;
            }
            case 'b': {
                g_bypass = !g_bypass;
                tape.setBypass(g_bypass);
                Serial.printf("Bypass: %s\n", g_bypass ? "ON" : "OFF");
                break;
            }
            default:
                // Ignorar whitespace y caracteres no reconocidos
                break;
        }
    }

    // Reporte de métricas cada 1 segundo
    if (now - lastReport >= 1000) {
        Serial.printf("CPU: %.1f%%  |  Mem: %d bloques  |  Drive: %.2f  Wow: %.2f  Flutter: %.2f  Age: %.2f  Mix: %.2f  Bypass: %s\n",
                      AudioProcessorUsageMax(),
                      AudioMemoryUsageMax(),
                      g_drive, g_wow, g_flutter, g_age, g_mix,
                      g_bypass ? "ON" : "OFF");
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
        lastReport = now;
    }
}
