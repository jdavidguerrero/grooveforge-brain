// apps/firmware-teensy/src/sketches/06-juno-106.cpp
// Sprint 2.1 — Juno-106 Engine: test interactivo via Serial.
// Theory, wiring y demo: apps/docs/sprints/06-juno-106.md
//
// Comandos Serial (115200 baud):
//   nNN   → noteOn MIDI note NN       (ej: n69=A4  n60=C4  n57=A3)
//   f     → noteOff
//   c0    → chorus off
//   c1    → chorus Modo I  (sutil, 0.5Hz — pads/strings)
//   c2    → chorus Modo II (pronunciado, 1.0Hz — bass/lead)
//   cNNN  → si val > 2: setFilterCutoff(val) Hz  (ej: c800 c4000)
//   wS    → waveform SAWTOOTH (por defecto)
//   wQ    → waveform SQUARE

#include <Arduino.h>
#include "../engines/juno_106.h"

Juno106 engine;

void setup() {
    Serial.begin(115200);
    engine.begin(0.5f);

    Serial.println("=== Sprint 2.1 — Roland Juno-106 ===");
    Serial.println("Comandos:");
    Serial.println("  nNN   = noteOn MIDI note  (n69=A4  n60=C4  n57=A3)");
    Serial.println("  f     = noteOff");
    Serial.println("  c0    = chorus OFF");
    Serial.println("  c1    = chorus Modo I  (sutil   — pads/strings)");
    Serial.println("  c2    = chorus Modo II (pronunciado — bass/lead)");
    Serial.println("  cNNN  = filter cutoff Hz  (c800 c1200 c4000)  [val > 2]");
    Serial.println("  wS    = DCO sawtooth");
    Serial.println("  wQ    = DCO square");
}

void loop() {
    engine.update();

    if (Serial.available()) {
        char cmd = Serial.read();

        switch (cmd) {
            case 'n': {
                uint8_t note = (uint8_t)Serial.parseInt();
                engine.noteOn(note);
                Serial.printf("noteOn %d (%.1f Hz)\n", note,
                              440.0f * powf(2.0f, (note - 69) / 12.0f));
                break;
            }

            case 'f':
                engine.noteOff();
                Serial.println("noteOff");
                break;

            case 'c': {
                // El primer dígito distingue entre chorus commands y cutoff:
                //   '0' → chorus off
                //   '1' → chorus Modo I
                //   '2' → chorus Modo II
                //   > 2 → setFilterCutoff(val)
                // Se lee como int para cubrir valores multi-dígito (ej: c1200)
                int val = (int)Serial.parseInt();
                if (val == 0) {
                    engine.setChorus(false);
                    Serial.println("chorus OFF");
                } else if (val == 1) {
                    engine.setChorus(true, 1);
                    Serial.println("chorus Modo I (sutil)");
                } else if (val == 2) {
                    engine.setChorus(true, 2);
                    Serial.println("chorus Modo II (pronunciado)");
                } else {
                    // val > 2 → interpretado como cutoff Hz
                    engine.setFilterCutoff((float)val);
                    Serial.printf("filter cutoff: %d Hz\n", val);
                }
                break;
            }

            case 'w': {
                char wave = (char)Serial.read();
                if (wave == 'S' || wave == 's') {
                    engine.setWaveform(WAVEFORM_SAWTOOTH);
                    Serial.println("DCO: SAWTOOTH");
                } else if (wave == 'Q' || wave == 'q') {
                    engine.setWaveform(WAVEFORM_SQUARE);
                    Serial.println("DCO: SQUARE");
                }
                break;
            }

            default:
                // Ignorar whitespace y caracteres no reconocidos
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
