// apps/firmware-teensy/src/sketches/09-phase-chorus.cpp
// Sprint 2.4 — FX Phase Chorus: demo interactivo via Serial.
// Theory y signal flow: apps/docs/sprints/09-phase-chorus.md
//
// El sketch es dueño del AudioOutputI2S, AudioControlSGTL5000 y AudioMemory compartidos.
// PhaseChorus expone getOutputL()/getOutputR() para conectar al output mixer del sketch.
//
// Comandos Serial (115200 baud):
//   r<valor>  → Rate Hz   (0.1–10.0)  ej: r0.5
//   d<valor>  → Depth     (0.0–1.0)   ej: d0.7
//   t<valor>  → Drift     (0.0–1.0)   ej: t0.3
//   v<n>      → Voices    (1–4)       ej: v2
//   m<valor>  → Mix wet   (0.0–1.0)   ej: m0.7
//   b         → Toggle bypass on/off

#include <Arduino.h>
// Teensy Audio Library (oficial)
#include <Audio.h>
// Código custom Sprint 2.4
#include "../fx/phase_chorus.h"

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

// ── PhaseChorus: expone getOutputL()/getOutputR() para conectar aquí ─────────────
PhaseChorus chorus;

AudioConnection cFxL(chorus.getOutputL(), 0, outMixL, 0);
AudioConnection cFxR(chorus.getOutputR(), 0, outMixR, 0);
AudioConnection cOutL(outMixL, 0, audioOut, 0);
AudioConnection cOutR(outMixR, 0, audioOut, 1);

// ── Frecuencias base del acorde C mayor ──────────────────────────────────────────
static constexpr float FREQ_C4 = 261.63f;
static constexpr float FREQ_E4 = 329.63f;
static constexpr float FREQ_G4 = 392.00f;

// ── Estado de parámetros actuales (para el reporte Serial) ───────────────────────
static float   g_rate   = 0.5f;
static float   g_depth  = 0.7f;
static float   g_drift  = 0.3f;
static uint8_t g_voices = 2;
static float   g_mix    = 0.7f;
static bool    g_bypass = false;

void setup() {
    Serial.begin(115200);

    // CRÍTICO: AudioMemory ANTES de chorus.begin() — el chorus interno requiere el pool.
    // Ref: apps/docs/sprints/09-phase-chorus.md §"Inicialización del chorus — orden obligatorio"
    AudioMemory(20);
    codec.enable();
    codec.volume(0.5f);

    outMixL.gain(0, 1.0f);
    outMixR.gain(0, 1.0f);

    // Gains uniformes: 0.33 × 3 ≈ 1.0 amplitud total con headroom
    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);
    srcMix.gain(3, 0.0f);

    // Sawtooth: rico en armónicos impares — ideal para chorus (las múltiples
    // componentes espectrales hacen el ensanchamiento más audible que una senoide)
    osc1.begin(WAVEFORM_SAWTOOTH);
    osc1.frequency(FREQ_C4);
    osc1.amplitude(0.4f);

    osc2.begin(WAVEFORM_SAWTOOTH);
    osc2.frequency(FREQ_E4);
    osc2.amplitude(0.4f);

    osc3.begin(WAVEFORM_SAWTOOTH);
    osc3.frequency(FREQ_G4);
    osc3.amplitude(0.4f);

    // PhaseChorus: solo crea las conexiones dinámicas desde srcMix
    chorus.begin(srcMix);

    // Valores nominales: "Juno-106 chord" (ref: apps/docs/sprints/09-phase-chorus.md §Demo)
    chorus.setRate(g_rate);
    chorus.setDepth(g_depth);
    chorus.setDrift(g_drift);
    chorus.setVoices(g_voices);
    chorus.setMix(g_mix);

    Serial.println("=== Sprint 2.4 — FX Phase Chorus ===");
    Serial.println("Chord C mayor (C4 + E4 + G4) | 3 sawtooth oscs | Phase Chorus activo");
    Serial.println();
    Serial.println("Comandos:");
    Serial.println("  r<val>  = Rate Hz  0.1–10.0  (ej: r0.5)");
    Serial.println("  d<val>  = Depth    0.0–1.0   (ej: d0.7)");
    Serial.println("  t<val>  = Drift    0.0–1.0   (ej: t0.3)");
    Serial.println("  v<n>    = Voices   1–4       (ej: v2)");
    Serial.println("  m<val>  = Mix wet  0.0–1.0   (ej: m0.7)");
    Serial.println("  b       = Toggle bypass");
    Serial.println();
    Serial.printf("Defaults: Rate=%.1f Depth=%.1f Drift=%.1f Voices=%d Mix=%.1f\n",
                  g_rate, g_depth, g_drift, g_voices, g_mix);
    Serial.println();
    Serial.println("Demo sugerido:");
    Serial.println("  1. Escuchar (v2, r0.5) — chorus Juno clasico");
    Serial.println("  2. v4 → ensemble denso");
    Serial.println("  3. r5.0 → LFO rapido, casi vibrato");
    Serial.println("  4. t1.0 → drift maximo, respiracion organica");
    Serial.println("  5. b → bypass, comparar seco vs chorus");
}

void loop() {
    static uint32_t lastMs     = millis();
    static uint32_t lastReport = millis();

    uint32_t now   = millis();
    float    dt_ms = (float)(now - lastMs);
    lastMs = now;

    // Avanza LFO drift y actualiza ganancia wet en dryWetMix
    chorus.update(dt_ms);

    // Procesamiento de comandos Serial
    if (Serial.available()) {
        char cmd = Serial.read();

        switch (cmd) {
            case 'r': {
                // Clamp aquí y en la clase — g_rate debe reflejar el valor real aplicado
                float val = Serial.parseFloat();
                if (val < 0.1f)  val = 0.1f;
                if (val > 10.0f) val = 10.0f;
                g_rate = val;
                chorus.setRate(g_rate);
                Serial.printf("Rate: %.2f Hz\n", g_rate);
                break;
            }
            case 'd': {
                float val = Serial.parseFloat();
                if (val < 0.0f) val = 0.0f;
                if (val > 1.0f) val = 1.0f;
                g_depth = val;
                chorus.setDepth(g_depth);
                Serial.printf("Depth: %.2f\n", g_depth);
                break;
            }
            case 't': {
                float val = Serial.parseFloat();
                if (val < 0.0f) val = 0.0f;
                if (val > 1.0f) val = 1.0f;
                g_drift = val;
                chorus.setDrift(g_drift);
                Serial.printf("Drift: %.2f\n", g_drift);
                break;
            }
            case 'v': {
                uint8_t val = (uint8_t)Serial.parseFloat();
                if (val < 1) val = 1;
                if (val > 4) val = 4;
                g_voices = val;
                chorus.setVoices(g_voices);
                Serial.printf("Voices: %d\n", g_voices);
                break;
            }
            case 'm': {
                float val = Serial.parseFloat();
                if (val < 0.0f) val = 0.0f;
                if (val > 1.0f) val = 1.0f;
                g_mix = val;
                chorus.setMix(g_mix);
                Serial.printf("Mix: %.2f\n", g_mix);
                break;
            }
            case 'b': {
                g_bypass = !g_bypass;
                chorus.setBypass(g_bypass);
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
        Serial.printf("CPU: %.1f%%  |  Mem: %d bloques  |  Rate: %.2fHz  Depth: %.2f  Drift: %.2f  Voices: %d  Mix: %.2f  Bypass: %s\n",
                      AudioProcessorUsageMax(),
                      AudioMemoryUsageMax(),
                      g_rate, g_depth, g_drift, g_voices, g_mix,
                      g_bypass ? "ON" : "OFF");
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
        lastReport = now;
    }
}
