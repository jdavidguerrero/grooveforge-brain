// apps/firmware-teensy/src/sketches/16-ghost-echo.cpp
// Sprint 2.11 — FX Ghost Echo demo
// Theory: apps/docs/sprints/16-ghost-echo.md (pendiente)
//
// Chord C mayor (C4+E4+G4) como fuente de audio continua.
// GhostEcho como capa FX — delay con LP filter en path de feedback (tape color).
//
// Comandos Serial (115200 baud):
//   t<val>  delay time       10.0–1400.0 ms
//   f<val>  feedback         0.0–0.95
//   c<val>  tape LP cutoff   500.0–12000.0 Hz
//   m<val>  mix              0.0–1.0
//   p       bypass toggle

#include <Arduino.h>
#include <Audio.h>
#include "fx/ghost_echo.h"

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
// Custom — apps/firmware-teensy/src/fx/ghost_echo.h
GhostEcho fx;

AudioConnection cFxL(fx.getOutputL(), 0, outMixL, 0);
AudioConnection cFxR(fx.getOutputR(), 0, outMixR, 0);
AudioConnection cOutL(outMixL, 0, audioOut, 0);
AudioConnection cOutR(outMixR, 0, audioOut, 1);

// ── Estado de parámetros ─────────────────────────────────────────────────────────
static float g_delayMs  = 375.0f;    // 10.0–1400.0 ms (375ms = 1 beat @ 160 BPM)
static float g_feedback = 0.45f;     // 0.0–0.95
static float g_tapeLP   = 5000.0f;   // 500.0–12000.0 Hz
static float g_mix      = 0.5f;      // 0.0–1.0
static bool  g_bypass   = false;

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

    fx.setDelayMs(g_delayMs);
    fx.setFeedback(g_feedback);
    fx.setTapeLP(g_tapeLP);
    fx.setMix(g_mix);
    fx.setBypass(g_bypass);

    Serial.println(F("=== Sprint 2.11 --- FX Ghost Echo ==="));
    Serial.println(F("Delay con LP filter en feedback (tape color de cinta magnetica)"));
    Serial.println(F("Ref: Zolzer \"DAFX\" (2011) SS2.2; Schimmel \"Magnetic Tape Echo\" (AES 2010)"));
    Serial.println(F("Refs historicos: King Tubby, Lee Scratch Perry (dub delay 1970s)"));
    Serial.println();
    Serial.println(F("  t<val>  delay time ms (10-1400) -- 375ms=1 beat @160BPM"));
    Serial.println(F("  f<val>  feedback (0.0-0.95)    -- 0.45=dub sutil, 0.85=runaway dub"));
    Serial.println(F("  c<val>  tape LP Hz (500-12000)  -- 5000=cinta vintage, 1000=muy oscuro"));
    Serial.println(F("  m<val>  mix (0.0-1.0)"));
    Serial.println(F("  p       bypass toggle"));
    Serial.println();
    Serial.println(F("Demo sugerido:"));
    Serial.println(F("  t375   delay 375ms (1 beat @160BPM) -- default"));
    Serial.println(F("  t500   delay 500ms (1 beat @120BPM)"));
    Serial.println(F("  f0.7   feedback alto: ecos prolongados"));
    Serial.println(F("  f0.9   feedback muy alto: dub delay clasico"));
    Serial.println(F("  c1000  tape muy oscuro: ecos de cinta vieja"));
    Serial.println(F("  c10000 tape brillante: delay limpio"));
    Serial.println(F("  m0.8   wet alto para oir los ecos claramente"));
    Serial.println();
    Serial.println(F("Defaults: t375 f0.45 c5000 m0.5 bypass=OFF"));
    Serial.println();
}

// ── loop() ───────────────────────────────────────────────────────────────────────
void loop() {
    fx.update();   // no-op v1.0 — placeholder para Markov chain en v2.0

    if (Serial.available()) {
        char cmd = Serial.read();

        if (cmd == 't') {
            float val = Serial.parseFloat();
            if (val < 10.0f)    val = 10.0f;
            if (val > 1400.0f)  val = 1400.0f;
            g_delayMs = val;
            fx.setDelayMs(g_delayMs);
            Serial.print(F("Delay: "));
            Serial.print(g_delayMs, 0);
            Serial.println(F(" ms"));
        }
        else if (cmd == 'f') {
            float val = Serial.parseFloat();
            if (val < 0.0f)  val = 0.0f;
            if (val > 0.95f) val = 0.95f;
            g_feedback = val;
            fx.setFeedback(g_feedback);
            Serial.print(F("Feedback: "));
            Serial.println(g_feedback, 2);
        }
        else if (cmd == 'c') {
            float val = Serial.parseFloat();
            if (val < 500.0f)   val = 500.0f;
            if (val > 12000.0f) val = 12000.0f;
            g_tapeLP = val;
            fx.setTapeLP(g_tapeLP);
            Serial.print(F("Tape LP: "));
            Serial.print(g_tapeLP, 0);
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
        Serial.print(F(" | Delay: "));
        Serial.print(g_delayMs, 0);
        Serial.print(F("ms | FB: "));
        Serial.print(g_feedback, 2);
        Serial.print(F(" | TapeLP: "));
        Serial.print(g_tapeLP, 0);
        Serial.print(F("Hz | Mix: "));
        Serial.print(g_mix, 2);
        Serial.print(F(" | Bypass: "));
        Serial.println(g_bypass ? F("ON") : F("OFF"));
    }
}
