// apps/firmware-teensy/src/sketches/05-moog-model-d.cpp
// Sprint 1.5 — Moog Model D Skeleton: test interactivo via Serial.
// Theory, wiring y demo: apps/docs/sprints/05-moog-model-d.md
//
// Comandos Serial (115200 baud):
//   nNN   → noteOn MIDI note NN  (ej: n69 = A4, n60 = C4, n81 = A5)
//   f     → noteOff
//   cNNN  → filter cutoff Hz     (ej: c800, c4000, c200)
//   gNNN  → glide ms             (ej: g200, g0 = sin glide)
//   dNN   → detune cents         (ej: d5, d50)
//   vNNN  → VCA attack ms        (ej: v10 = percusivo, v500 = pad)

#include <Arduino.h>
#include "../engines/moog_model_d.h"

MoogModelD engine;

void setup() {
    Serial.begin(115200);
    engine.begin(0.5f);

    Serial.println("=== Sprint 1.5 — Moog Model D ===");
    Serial.println("Comandos:");
    Serial.println("  nNN  = noteOn MIDI note  (n69=A4  n60=C4  n81=A5)");
    Serial.println("  f    = noteOff");
    Serial.println("  cNNN = filter cutoff Hz  (c800 c4000 c200)");
    Serial.println("  gNNN = glide ms          (g200 g0=sin glide)");
    Serial.println("  dNN  = detune cents      (d5 d50)");
    Serial.println("  vNNN = VCA attack ms     (v10 v500)");
}

void loop() {
    engine.update();

    if (Serial.available()) {
        char cmd = Serial.read();
        float val = Serial.parseFloat();

        switch (cmd) {
            case 'n': {
                uint8_t note = (uint8_t)val;
                engine.noteOn(note);
                Serial.printf("noteOn %d (%.1f Hz)\n", note,
                              440.0f * powf(2.0f, (note - 69) / 12.0f));
                break;
            }
            case 'f':
                engine.noteOff();
                Serial.println("noteOff");
                break;
            case 'c':
                engine.setFilterCutoff(val);
                Serial.printf("cutoff: %.0f Hz\n", val);
                break;
            case 'g':
                engine.setGlide(val);
                Serial.printf("glide: %.0f ms\n", val);
                break;
            case 'd':
                engine.setDetune(val);
                Serial.printf("detune: %.1f cents\n", val);
                break;
            case 'v':
                engine.setVCAAttack(val);
                Serial.printf("VCA attack: %.0f ms\n", val);
                break;
        }
    }

    // Métricas de CPU y memoria cada segundo
    static uint32_t last_report = 0;
    if (millis() - last_report > 1000) {
        Serial.printf("CPU: %.1f%%  Mem: %d blocks\n",
                      AudioProcessorUsageMax(),
                      AudioMemoryUsageMax());
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
        last_report = millis();
    }
}
