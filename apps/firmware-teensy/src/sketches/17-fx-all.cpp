// apps/firmware-teensy/src/sketches/17-fx-all.cpp
// Sketch combinado Sprints 2.8–2.11 — un solo archivo para probar los 4 FX.
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  CAMBIAR ESTE NÚMERO Y REFLASHEAR:                                      │
// │                                                                         │
// │    1 = Cymatic Resonator  (Sprint 2.8)  — 4 filtros BP, ratios 1:2:3:5 │
// │    2 = Granular Cloud     (Sprint 2.9)  — granulizador + freeze mode    │
// │    3 = Spring + Plate     (Sprint 2.10) — dual reverb crossfade         │
// │    4 = Ghost Echo         (Sprint 2.11) — delay + tape LP en feedback   │
// └─────────────────────────────────────────────────────────────────────────┘

#define ACTIVE_FX 1

// ── Por qué no hay "cambio de FX en runtime" ─────────────────────────────────
// Teensy Audio Library solo admite UNA instancia de AudioOutputI2S en el programa.
// Cada clase FX contiene su propio AudioOutputI2S privado como miembro. Si se
// instanciaran dos FX a la vez, el segundo AudioOutputI2S registraría un segundo
// callback DMA → conflicto de hardware → crash o silencio.
// La selección via #define resuelve el conflicto en tiempo de compilación:
// solo un FX existe en memoria por flash. El cambio requiere reflashear (~5s).

#include <Arduino.h>
#include <Audio.h>

// ── Fuente de audio compartida: chord C mayor (C4 + E4 + G4) ─────────────────
// Teensy Audio Library (oficial)
AudioSynthWaveform osc1;   // C4 = 261.63 Hz
AudioSynthWaveform osc2;   // E4 = 329.63 Hz
AudioSynthWaveform osc3;   // G4 = 392.00 Hz
AudioMixer4        srcMix;

AudioConnection c1(osc1, 0, srcMix, 0);
AudioConnection c2(osc2, 0, srcMix, 1);
AudioConnection c3(osc3, 0, srcMix, 2);

// ═════════════════════════════════════════════════════════════════════════════
// FX 1 — CYMATIC RESONATOR (Sprint 2.8)
// ═════════════════════════════════════════════════════════════════════════════
#if ACTIVE_FX == 1

#include "fx/cymatic_resonator.h"
CymaticResonator fx;

static float   g_tune      = 1.0f;    // 0.5–2.0
static uint8_t g_density   = 4;       // 1–4
static float   g_resonance = 30.0f;   // 10–80 Q
static float   g_lfoRate   = 0.5f;    // 0.1–3.0 Hz
static float   g_mix       = 0.7f;
static bool    g_bypass    = false;

void setup() {
    Serial.begin(115200);
    delay(1200);

    osc1.begin(WAVEFORM_SAWTOOTH); osc1.frequency(261.63f); osc1.amplitude(0.25f);
    osc2.begin(WAVEFORM_SAWTOOTH); osc2.frequency(329.63f); osc2.amplitude(0.25f);
    osc3.begin(WAVEFORM_SAWTOOTH); osc3.frequency(392.00f); osc3.amplitude(0.25f);
    srcMix.gain(0, 0.33f); srcMix.gain(1, 0.33f); srcMix.gain(2, 0.33f); srcMix.gain(3, 0.0f);

    fx.begin(srcMix, 0.7f);
    fx.setTune(g_tune); fx.setDensity(g_density); fx.setResonance(g_resonance);
    fx.setLfoRate(g_lfoRate); fx.setMix(g_mix); fx.setBypass(g_bypass);

    Serial.println(F("=== FX 1/4: CYMATIC RESONATOR (Sprint 2.8) ==="));
    Serial.println(F("4 filtros BP en paralelo, ratios 1:2:3:5 sobre C4=261Hz"));
    Serial.println(F("LFO modula tune +-5% — movimiento cristalino"));
    Serial.println(F("  t<val>  tune ratio    0.5-2.0  (1.0=C4, 0.5=C3, 2.0=C5)"));
    Serial.println(F("  d<n>    density       1-4 modos activos"));
    Serial.println(F("  r<val>  resonance Q   10-80    (30=sweet spot)"));
    Serial.println(F("  l<val>  lfo rate Hz   0.1-3.0"));
    Serial.println(F("  m<val>  mix           0.0-1.0"));
    Serial.println(F("  p       bypass toggle"));
    Serial.println(F("Demo: d1 (un modo) -> d4 (pleno) -> r70 (metalico) -> t0.5 (8va abajo)"));
}

void loop() {
    fx.update();
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 't') {
            float v = Serial.parseFloat();
            if (v < 0.5f) v = 0.5f; if (v > 2.0f) v = 2.0f;
            g_tune = v; fx.setTune(g_tune);
            Serial.print(F("Tune: ")); Serial.println(g_tune, 2);
        } else if (cmd == 'd') {
            uint8_t v = (uint8_t)Serial.parseInt();
            if (v < 1) v = 1; if (v > 4) v = 4;
            g_density = v; fx.setDensity(g_density);
            Serial.print(F("Density: ")); Serial.println(g_density);
        } else if (cmd == 'r') {
            float v = Serial.parseFloat();
            if (v < 10.0f) v = 10.0f; if (v > 80.0f) v = 80.0f;
            g_resonance = v; fx.setResonance(g_resonance);
            Serial.print(F("Resonance Q: ")); Serial.println(g_resonance, 1);
        } else if (cmd == 'l') {
            float v = Serial.parseFloat();
            if (v < 0.1f) v = 0.1f; if (v > 3.0f) v = 3.0f;
            g_lfoRate = v; fx.setLfoRate(g_lfoRate);
            Serial.print(F("LFO Rate: ")); Serial.print(g_lfoRate, 2); Serial.println(F(" Hz"));
        } else if (cmd == 'm') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
            g_mix = v; fx.setMix(g_mix);
            Serial.print(F("Mix: ")); Serial.println(g_mix, 2);
        } else if (cmd == 'p') {
            g_bypass = !g_bypass; fx.setBypass(g_bypass);
            Serial.print(F("Bypass: ")); Serial.println(g_bypass ? F("ON") : F("OFF"));
        }
    }
    static uint32_t t0 = 0; uint32_t now = millis();
    if (now - t0 >= 1000) { t0 = now;
        Serial.print(F("CPU:")); Serial.print(AudioProcessorUsage(),1);
        Serial.print(F("% Mem:")); Serial.print(AudioMemoryUsage());
        Serial.print(F(" | t:")); Serial.print(g_tune,2);
        Serial.print(F(" d:")); Serial.print(g_density);
        Serial.print(F(" Q:")); Serial.print(g_resonance,0);
        Serial.print(F(" lfo:")); Serial.print(g_lfoRate,1);
        Serial.print(F("Hz m:")); Serial.print(g_mix,2);
        Serial.print(F(" byp:")); Serial.println(g_bypass ? F("ON") : F("OFF"));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// FX 2 — GRANULAR CLOUD (Sprint 2.9)
// ═════════════════════════════════════════════════════════════════════════════
#elif ACTIVE_FX == 2

#include "fx/granular_cloud.h"
GranularCloud fx;

static float g_grainSize = 100.0f;   // 5–250 ms
static float g_speed     = 1.0f;     // 0.25–4.0
static bool  g_freeze    = false;
static float g_mix       = 0.5f;
static bool  g_bypass    = false;

void setup() {
    Serial.begin(115200);
    delay(1200);

    osc1.begin(WAVEFORM_SAWTOOTH); osc1.frequency(261.63f); osc1.amplitude(0.25f);
    osc2.begin(WAVEFORM_SAWTOOTH); osc2.frequency(329.63f); osc2.amplitude(0.25f);
    osc3.begin(WAVEFORM_SAWTOOTH); osc3.frequency(392.00f); osc3.amplitude(0.25f);
    srcMix.gain(0, 0.33f); srcMix.gain(1, 0.33f); srcMix.gain(2, 0.33f); srcMix.gain(3, 0.0f);

    fx.begin(srcMix, 0.7f);
    fx.setGrainSize(g_grainSize); fx.setSpeed(g_speed);
    fx.setFreeze(g_freeze); fx.setMix(g_mix); fx.setBypass(g_bypass);

    Serial.println(F("=== FX 2/4: GRANULAR CLOUD (Sprint 2.9) ==="));
    Serial.println(F("AudioEffectGranular — passthrough + freeze mode"));
    Serial.println(F("Ref: Roads \"Microsound\" (MIT Press, 2001)"));
    Serial.println(F("  g<val>  grain size ms  5-250  (100ms=nube, 20ms=pitch)"));
    Serial.println(F("  s<val>  speed ratio    0.25-4.0"));
    Serial.println(F("  z       freeze toggle  (captura nube ambient)"));
    Serial.println(F("  m<val>  mix            0.0-1.0"));
    Serial.println(F("  p       bypass toggle"));
    Serial.println(F("Demo: m0.8 -> z freeze -> g20 -> g200 -> s0.5 -> s2.0"));
}

void loop() {
    fx.update();
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'g') {
            float v = Serial.parseFloat();
            if (v < 5.0f) v = 5.0f; if (v > 250.0f) v = 250.0f;
            g_grainSize = v; fx.setGrainSize(g_grainSize);
            Serial.print(F("Grain: ")); Serial.print(g_grainSize,1); Serial.println(F(" ms"));
        } else if (cmd == 's') {
            float v = Serial.parseFloat();
            if (v < 0.25f) v = 0.25f; if (v > 4.0f) v = 4.0f;
            g_speed = v; fx.setSpeed(g_speed);
            Serial.print(F("Speed: ")); Serial.print(g_speed,2); Serial.println(F("x"));
        } else if (cmd == 'z') {
            g_freeze = !g_freeze; fx.setFreeze(g_freeze);
            Serial.println(g_freeze ? F("Freeze: ON") : F("Freeze: OFF"));
        } else if (cmd == 'm') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
            g_mix = v; fx.setMix(g_mix);
            Serial.print(F("Mix: ")); Serial.println(g_mix,2);
        } else if (cmd == 'p') {
            g_bypass = !g_bypass; fx.setBypass(g_bypass);
            Serial.println(g_bypass ? F("Bypass: ON") : F("Bypass: OFF"));
        }
    }
    static uint32_t t0 = 0; uint32_t now = millis();
    if (now - t0 >= 1000) { t0 = now;
        Serial.print(F("CPU:")); Serial.print(AudioProcessorUsage(),1);
        Serial.print(F("% Mem:")); Serial.print(AudioMemoryUsage());
        Serial.print(F(" | g:")); Serial.print(g_grainSize,0);
        Serial.print(F("ms s:")); Serial.print(g_speed,2);
        Serial.print(F("x frz:")); Serial.print(g_freeze ? F("ON") : F("OFF"));
        Serial.print(F(" m:")); Serial.print(g_mix,2);
        Serial.print(F(" byp:")); Serial.println(g_bypass ? F("ON") : F("OFF"));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// FX 3 — SPRING + PLATE (Sprint 2.10)
// ═════════════════════════════════════════════════════════════════════════════
#elif ACTIVE_FX == 3

#include "fx/spring_plate.h"
SpringPlate fx;

static uint8_t g_algorithm = 0;      // 0=spring 1=plate 2=blend
static float   g_roomSize  = 0.5f;   // 0.0–1.0
static float   g_damping   = 0.5f;   // 0.0–1.0
static float   g_blend     = 0.5f;   // 0.0–1.0 (solo cuando alg=2)
static float   g_mix       = 0.5f;
static bool    g_bypass    = false;

static const char* alg_name(uint8_t a) {
    switch(a) { case 0: return "SPRING"; case 1: return "PLATE"; default: return "BLEND"; }
}

void setup() {
    Serial.begin(115200);
    delay(1200);

    osc1.begin(WAVEFORM_SAWTOOTH); osc1.frequency(261.63f); osc1.amplitude(0.25f);
    osc2.begin(WAVEFORM_SAWTOOTH); osc2.frequency(329.63f); osc2.amplitude(0.25f);
    osc3.begin(WAVEFORM_SAWTOOTH); osc3.frequency(392.00f); osc3.amplitude(0.25f);
    srcMix.gain(0, 0.33f); srcMix.gain(1, 0.33f); srcMix.gain(2, 0.33f); srcMix.gain(3, 0.0f);

    fx.begin(srcMix, 0.7f);
    fx.setAlgorithm(g_algorithm); fx.setRoomSize(g_roomSize);
    fx.setDamping(g_damping); fx.setBlend(g_blend);
    fx.setMix(g_mix); fx.setBypass(g_bypass);

    Serial.println(F("=== FX 3/4: SPRING + PLATE (Sprint 2.10) ==="));
    Serial.println(F("Dual Freeverb — spring vintage (Hammond 1941) + plate (EMT 140 1957)"));
    Serial.println(F("  a<n>    algorithm   0=spring 1=plate 2=blend"));
    Serial.println(F("  r<val>  roomsize    0.0-1.0  (solo con a2)"));
    Serial.println(F("  d<val>  damping     0.0-1.0  (solo con a2)"));
    Serial.println(F("  b<val>  blend       0.0-1.0  (0=spring, 1=plate, solo con a2)"));
    Serial.println(F("  m<val>  mix         0.0-1.0"));
    Serial.println(F("  p       bypass toggle"));
    Serial.println(F("Demo: a0 spring -> a1 plate -> a2 + b0.0 -> b0.5 -> b1.0"));
}

void loop() {
    fx.update();
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 'a') {
            uint8_t v = (uint8_t)Serial.parseInt();
            if (v > 2) v = 2;
            g_algorithm = v; fx.setAlgorithm(g_algorithm);
            Serial.print(F("Algorithm: ")); Serial.println(alg_name(g_algorithm));
        } else if (cmd == 'r') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
            g_roomSize = v; fx.setRoomSize(g_roomSize);
            Serial.print(F("RoomSize: ")); Serial.println(g_roomSize,2);
        } else if (cmd == 'd') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
            g_damping = v; fx.setDamping(g_damping);
            Serial.print(F("Damping: ")); Serial.println(g_damping,2);
        } else if (cmd == 'b') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
            g_blend = v; fx.setBlend(g_blend);
            Serial.print(F("Blend: ")); Serial.print(g_blend,2);
            Serial.println(F(" (0=spring, 1=plate)"));
        } else if (cmd == 'm') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
            g_mix = v; fx.setMix(g_mix);
            Serial.print(F("Mix: ")); Serial.println(g_mix,2);
        } else if (cmd == 'p') {
            g_bypass = !g_bypass; fx.setBypass(g_bypass);
            Serial.println(g_bypass ? F("Bypass: ON") : F("Bypass: OFF"));
        }
    }
    static uint32_t t0 = 0; uint32_t now = millis();
    if (now - t0 >= 1000) { t0 = now;
        Serial.print(F("CPU:")); Serial.print(AudioProcessorUsage(),1);
        Serial.print(F("% Mem:")); Serial.print(AudioMemoryUsage());
        Serial.print(F(" | alg:")); Serial.print(alg_name(g_algorithm));
        Serial.print(F(" room:")); Serial.print(g_roomSize,2);
        Serial.print(F(" damp:")); Serial.print(g_damping,2);
        Serial.print(F(" blend:")); Serial.print(g_blend,2);
        Serial.print(F(" m:")); Serial.print(g_mix,2);
        Serial.print(F(" byp:")); Serial.println(g_bypass ? F("ON") : F("OFF"));
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// FX 4 — GHOST ECHO (Sprint 2.11)
// ═════════════════════════════════════════════════════════════════════════════
#elif ACTIVE_FX == 4

#include "fx/ghost_echo.h"
GhostEcho fx;

static float g_delayMs  = 375.0f;   // 10–1400 ms
static float g_feedback = 0.45f;    // 0.0–0.95
static float g_tapeLP   = 5000.0f;  // 500–12000 Hz
static float g_mix      = 0.5f;
static bool  g_bypass   = false;

void setup() {
    Serial.begin(115200);
    delay(1200);

    osc1.begin(WAVEFORM_SAWTOOTH); osc1.frequency(261.63f); osc1.amplitude(0.25f);
    osc2.begin(WAVEFORM_SAWTOOTH); osc2.frequency(329.63f); osc2.amplitude(0.25f);
    osc3.begin(WAVEFORM_SAWTOOTH); osc3.frequency(392.00f); osc3.amplitude(0.25f);
    srcMix.gain(0, 0.33f); srcMix.gain(1, 0.33f); srcMix.gain(2, 0.33f); srcMix.gain(3, 0.0f);

    fx.begin(srcMix, 0.7f);
    fx.setDelayMs(g_delayMs); fx.setFeedback(g_feedback);
    fx.setTapeLP(g_tapeLP); fx.setMix(g_mix); fx.setBypass(g_bypass);

    Serial.println(F("=== FX 4/4: GHOST ECHO (Sprint 2.11) ==="));
    Serial.println(F("Delay + LP filter en feedback path (tape color)"));
    Serial.println(F("Markov chain AI: deferred v2.0 (requiere TinyML Sprint 4.5+)"));
    Serial.println(F("  t<val>  delay ms    10-1400  (375ms=160BPM 3/8, 500ms=120BPM 1/4)"));
    Serial.println(F("  f<val>  feedback    0.0-0.95 (0.45=4-5 repeticiones)"));
    Serial.println(F("  c<val>  tape LP Hz  500-12000 (5000=cinta vintage, 1000=muy oscura)"));
    Serial.println(F("  m<val>  mix         0.0-1.0"));
    Serial.println(F("  p       bypass toggle"));
    Serial.println(F("Demo: t500 f0.7 c1500 (dub delay oscuro) -> c10000 (delay brillante)"));
}

void loop() {
    fx.update();
    if (Serial.available()) {
        char cmd = Serial.read();
        if (cmd == 't') {
            float v = Serial.parseFloat();
            if (v < 10.0f) v = 10.0f; if (v > 1400.0f) v = 1400.0f;
            g_delayMs = v; fx.setDelayMs(g_delayMs);
            Serial.print(F("Delay: ")); Serial.print(g_delayMs,0); Serial.println(F(" ms"));
        } else if (cmd == 'f') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f; if (v > 0.95f) v = 0.95f;
            g_feedback = v; fx.setFeedback(g_feedback);
            Serial.print(F("Feedback: ")); Serial.println(g_feedback,2);
        } else if (cmd == 'c') {
            float v = Serial.parseFloat();
            if (v < 500.0f) v = 500.0f; if (v > 12000.0f) v = 12000.0f;
            g_tapeLP = v; fx.setTapeLP(g_tapeLP);
            Serial.print(F("Tape LP: ")); Serial.print(g_tapeLP,0); Serial.println(F(" Hz"));
        } else if (cmd == 'm') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f; if (v > 1.0f) v = 1.0f;
            g_mix = v; fx.setMix(g_mix);
            Serial.print(F("Mix: ")); Serial.println(g_mix,2);
        } else if (cmd == 'p') {
            g_bypass = !g_bypass; fx.setBypass(g_bypass);
            Serial.println(g_bypass ? F("Bypass: ON") : F("Bypass: OFF"));
        }
    }
    static uint32_t t0 = 0; uint32_t now = millis();
    if (now - t0 >= 1000) { t0 = now;
        Serial.print(F("CPU:")); Serial.print(AudioProcessorUsage(),1);
        Serial.print(F("% Mem:")); Serial.print(AudioMemoryUsage());
        Serial.print(F(" | t:")); Serial.print(g_delayMs,0);
        Serial.print(F("ms fb:")); Serial.print(g_feedback,2);
        Serial.print(F(" LP:")); Serial.print(g_tapeLP,0);
        Serial.print(F("Hz m:")); Serial.print(g_mix,2);
        Serial.print(F(" byp:")); Serial.println(g_bypass ? F("ON") : F("OFF"));
    }
}

#else
    #error "ACTIVE_FX debe ser 1, 2, 3 o 4"
#endif
