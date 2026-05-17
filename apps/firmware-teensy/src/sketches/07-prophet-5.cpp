// apps/firmware-teensy/src/sketches/07-prophet-5.cpp
// Sprint 2.2 — Prophet-5 Engine: test interactivo via Serial.
// Theory, voice allocation y demo: apps/docs/sprints/07-prophet-5.md
//
// Comandos Serial (115200 baud):
//   nNN   → noteOn  MIDI note NN  (ej: n60=C4  n64=E4  n67=G4)
//   xNN   → noteOff MIDI note NN  (ej: x60  — permite mantener acordes)
//   cNNN  → setFilterCutoff Hz    (ej: c800  c4000)
//   mN.N  → setCrossmod amount    (ej: m0.0  m0.3  m1.0)
//   iNN   → setOscBInterval semitones (ej: i0=unísono  i7=quinta  i12=octava)
//   wAS   → setOscAWaveform SAWTOOTH
//   wAQ   → setOscAWaveform SQUARE
//   wBS   → setOscBWaveform SAWTOOTH
//   wBQ   → setOscBWaveform SQUARE

#include <Arduino.h>
#include "../engines/prophet_5.h"

Prophet5 engine;

void setup() {
    Serial.begin(115200);
    engine.begin(0.5f);

    Serial.println("=== Sprint 2.2 — Sequential Circuits Prophet-5 ===");
    Serial.println("5 voces polifónicas | 2 VCOs por voz | Voice stealing FIFO");
    Serial.println();
    Serial.println("Comandos:");
    Serial.println("  nNN   = noteOn  MIDI note  (n60=C4  n64=E4  n67=G4  n69=A4)");
    Serial.println("  xNN   = noteOff MIDI note  (x60  x64  x67)");
    Serial.println("  cNNN  = filter cutoff Hz   (c500 c2000 c8000)");
    Serial.println("  mN.N  = crossmod amount    (m0.0 m0.3 m1.0)");
    Serial.println("  iNN   = oscB interval semi (i0=unison  i7=fifth  i12=oct)");
    Serial.println("  wAS   = oscA sawtooth");
    Serial.println("  wAQ   = oscA square");
    Serial.println("  wBS   = oscB sawtooth");
    Serial.println("  wBQ   = oscB square");
    Serial.println();

    // Demo de arranque: acorde C mayor (C4 + E4 + G4)
    // 3 notas × 2 VCOs = 6 osciladores simultáneos
    Serial.println("Demo: acorde C mayor (n60 + n64 + n67)");
    engine.noteOn(60);   // C4
    engine.noteOn(64);   // E4
    engine.noteOn(67);   // G4
    Serial.println("Tocando C mayor — usar xNN para soltar notas individualmente");
}

void loop() {
    engine.update();

    if (Serial.available()) {
        char cmd = Serial.read();

        switch (cmd) {
            case 'n': {
                uint8_t note = (uint8_t)Serial.parseInt();
                engine.noteOn(note);
                Serial.printf("noteOn  %d (%.2f Hz)\n",
                              note, 440.0f * powf(2.0f, (note - 69) / 12.0f));
                break;
            }

            case 'x': {
                uint8_t note = (uint8_t)Serial.parseInt();
                engine.noteOff(note);
                Serial.printf("noteOff %d\n", note);
                break;
            }

            case 'c': {
                float hz = Serial.parseFloat();
                engine.setFilterCutoff(hz);
                Serial.printf("filter cutoff: %.0f Hz\n", hz);
                break;
            }

            case 'm': {
                float amount = Serial.parseFloat();
                engine.setCrossmod(amount);
                Serial.printf("crossmod: %.2f\n", amount);
                break;
            }

            case 'i': {
                float semi = Serial.parseFloat();
                engine.setOscBInterval(semi);
                Serial.printf("oscB interval: %.0f semitones\n", semi);
                break;
            }

            case 'w': {
                // Formato: wAS, wAQ, wBS, wBQ
                // Segundo char: 'A' = oscA, 'B' = oscB
                // Tercer char:  'S' = sawtooth, 'Q' = square
                char osc  = (char)Serial.read();
                char wave = (char)Serial.read();

                uint16_t wf = (wave == 'Q' || wave == 'q')
                              ? WAVEFORM_SQUARE : WAVEFORM_SAWTOOTH;
                const char* wname = (wf == WAVEFORM_SQUARE) ? "SQUARE" : "SAWTOOTH";

                if (osc == 'A' || osc == 'a') {
                    engine.setOscAWaveform(wf);
                    Serial.printf("oscA: %s\n", wname);
                } else if (osc == 'B' || osc == 'b') {
                    engine.setOscBWaveform(wf);
                    Serial.printf("oscB: %s\n", wname);
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
