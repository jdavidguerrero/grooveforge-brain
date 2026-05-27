// apps/firmware-teensy/src/sketches/10-bit-sculpt.cpp
// Sprint 2.5 — FX Bit Sculpt: demo interactivo via Serial.
// Theory y signal flow: apps/docs/sprints/10-bit-sculpt.md
//
// El sketch es dueño del AudioOutputI2S, AudioControlSGTL5000 y AudioMemory compartidos.
// BitSculpt expone getOutputL()/getOutputR() para conectar al output mixer del sketch.
//
// LECCIÓN SPRINT 2.4: clampear valores en g_XXX ANTES de pasar a la clase.
// El display (y el reporte Serial) muestra el valor real aplicado, no el que llegó raw.
//
// Comandos Serial (115200 baud):
//   b<n>    → Bits       1–16        ej: b8
//   s<val>  → SampleRate 1000–48000  ej: s8000
//   c<val>  → Sculpt     0.0–1.0     ej: c0.8
//   m<val>  → Mix wet    0.0–1.0     ej: m0.5
//   p       → Toggle bypass on/off

#include <Arduino.h>
// Teensy Audio Library (oficial)
#include <Audio.h>
// Código custom Sprint 2.5
#include "../fx/bit_sculpt.h"

// ── Fuente de audio: 3 osciladores (chord C mayor — C4, E4, G4) ──────────────────
AudioSynthWaveform osc1;   // C4 — 261.63 Hz
AudioSynthWaveform osc2;   // E4 — 329.63 Hz
AudioSynthWaveform osc3;   // G4 — 392.00 Hz
AudioMixer4        srcMix;

// ── Conexiones: oscs → srcMix ─────────────────────────────────────────────────────
AudioConnection c1(osc1, 0, srcMix, 0);
AudioConnection c2(osc2, 0, srcMix, 1);
AudioConnection c3(osc3, 0, srcMix, 2);

// ── Output compartido ─────────────────────────────────────────────────────────────
AudioMixer4          outMixL;
AudioMixer4          outMixR;
AudioOutputI2S       audioOut;
AudioControlSGTL5000 codec;

// ── BitSculpt: expone getOutputL()/getOutputR() para conectar aquí ────────────────
BitSculpt fx;

AudioConnection cFxL(fx.getOutputL(), 0, outMixL, 0);
AudioConnection cFxR(fx.getOutputR(), 0, outMixR, 0);
AudioConnection cOutL(outMixL, 0, audioOut, 0);
AudioConnection cOutR(outMixR, 0, audioOut, 1);

// ── Frecuencias base del acorde C mayor ──────────────────────────────────────────
static constexpr float FREQ_C4 = 261.63f;
static constexpr float FREQ_E4 = 329.63f;
static constexpr float FREQ_G4 = 392.00f;

// ── Estado de parámetros actuales ─────────────────────────────────────────────────
// Los valores aquí reflejan los valores REALES aplicados (post-clamp) — no el input raw.
// Ref: lección aprendida Sprint 2.4.
static uint8_t g_bits       = 8;
static float   g_sampleRate = 22050.0f;
static float   g_sculpt     = 0.3f;
static float   g_mix        = 0.7f;
static bool    g_bypass     = false;

void setup() {
    Serial.begin(115200);

    AudioMemory(20);
    codec.enable();
    codec.volume(0.5f);

    outMixL.gain(0, 1.0f);
    outMixR.gain(0, 1.0f);

    // srcMix: 3 canales iguales. 0.33 × 3 ≈ 1.0 amplitud total con headroom adecuado.
    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);
    srcMix.gain(3, 0.0f);

    // Sawtooth: rico en armónicos — el bitcrusher actúa sobre múltiples parciales,
    // produciendo alias inarmónicos más evidentes que una senoide simple.
    osc1.begin(WAVEFORM_SAWTOOTH);
    osc1.frequency(FREQ_C4);
    osc1.amplitude(0.4f);

    osc2.begin(WAVEFORM_SAWTOOTH);
    osc2.frequency(FREQ_E4);
    osc2.amplitude(0.4f);

    osc3.begin(WAVEFORM_SAWTOOTH);
    osc3.frequency(FREQ_G4);
    osc3.amplitude(0.4f);

    // BitSculpt: solo crea las conexiones dinámicas desde srcMix
    fx.begin(srcMix);

    // Aplicar defaults
    fx.setBits(g_bits);
    fx.setSampleRate(g_sampleRate);
    fx.setSculpt(g_sculpt);
    fx.setMix(g_mix);

    Serial.println("=== Sprint 2.5 — FX Bit Sculpt ===");
    Serial.println("Chord C mayor (C4 + E4 + G4) | 3 sawtooth oscs | Bit Sculpt activo");
    Serial.println();
    Serial.println("Defaults: 8-bit, 22050 Hz — estilo SP-1200");
    Serial.println("Comandos:");
    Serial.println("  b<n>   Bits 1-16     (ej: b4)");
    Serial.println("  s<val> SampleRate Hz (ej: s8000)");
    Serial.println("  c<val> Sculpt 0-1    (ej: c0.8)");
    Serial.println("  m<val> Mix 0-1       (ej: m0.5)");
    Serial.println("  p      bypass toggle");
    Serial.println();
    Serial.println("Demo:");
    Serial.println("  b8 + s8000   -> Game Boy (8bit + 8kHz)");
    Serial.println("  b12 + s26000 -> SP-1200  (12bit + 26kHz)");
    Serial.println("  b4           -> crunch agresivo");
    Serial.println("  b1           -> onda cuadrada total");
    Serial.println("  c1.0         -> noise shaping maximo");
    Serial.println("  m0.3         -> blend sutil lo-fi");
    Serial.println();
}

void loop() {
    static uint32_t lastReport = millis();

    const uint32_t now = millis();

    // Avanza noise shaping y actualiza nivel de dither en ditherMix
    fx.update();

    // Procesamiento de comandos Serial
    if (Serial.available()) {
        char cmd = Serial.read();

        switch (cmd) {
            case 'b': {
                // Clamp en el sketch — g_bits refleja el valor real aplicado
                int val = (int)Serial.parseFloat();
                if (val < 1)  val = 1;
                if (val > 16) val = 16;
                g_bits = (uint8_t)val;
                fx.setBits(g_bits);
                Serial.printf("Bits: %d\n", g_bits);
                break;
            }
            case 's': {
                // Clamp en el sketch — g_sampleRate refleja el valor real aplicado
                float val = Serial.parseFloat();
                if (val < 1000.0f)  val = 1000.0f;
                if (val > 48000.0f) val = 48000.0f;
                g_sampleRate = val;
                fx.setSampleRate(g_sampleRate);
                Serial.printf("SampleRate: %.0f Hz\n", g_sampleRate);
                break;
            }
            case 'c': {
                // 'c' para Sculpt (Character) — evita colisión con 's' (SampleRate)
                float val = Serial.parseFloat();
                if (val < 0.0f) val = 0.0f;
                if (val > 1.0f) val = 1.0f;
                g_sculpt = val;
                fx.setSculpt(g_sculpt);
                Serial.printf("Sculpt: %.2f\n", g_sculpt);
                break;
            }
            case 'm': {
                float val = Serial.parseFloat();
                if (val < 0.0f) val = 0.0f;
                if (val > 1.0f) val = 1.0f;
                g_mix = val;
                fx.setMix(g_mix);
                Serial.printf("Mix: %.2f\n", g_mix);
                break;
            }
            case 'p': {
                // 'p' para pass-through (bypass) — evita colisión con 'b' (bits)
                g_bypass = !g_bypass;
                fx.setBypass(g_bypass);
                Serial.printf("Bypass: %s\n", g_bypass ? "ON" : "OFF");
                break;
            }
            default:
                // Ignorar whitespace y caracteres no reconocidos
                break;
        }
    }

    // Reporte de métricas cada 2 segundos (spec §Demo)
    if (now - lastReport >= 2000) {
        Serial.printf("CPU: %.1f%%  |  Mem: %d bloques  |  Bits: %d  SR: %.0f  Sculpt: %.2f  Mix: %.2f  Bypass: %s\n",
                      AudioProcessorUsageMax(),
                      AudioMemoryUsageMax(),
                      g_bits, g_sampleRate, g_sculpt, g_mix,
                      g_bypass ? "ON" : "OFF");
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
        lastReport = now;
    }
}
