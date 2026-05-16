// Sprint 1.1 — Hello Tone: 440Hz sine via I2S nativo → SGTL5000.
// Theory, wiring y demo: apps/docs/sprints/01-hello-tone.md

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

AudioSynthWaveform    sine;
AudioOutputI2S        audioOut;
AudioControlSGTL5000  codec;

AudioConnection c1(sine, 0, audioOut, 0);  // sine → canal L
AudioConnection c2(sine, 0, audioOut, 1);  // sine → canal R

void setup() {
    AudioMemory(8);           // 8 bloques × 256 bytes = 2KB — holgado para 2 conexiones
    codec.enable();
    codec.volume(0.5);        // 50% — protege los oídos en el primer test
    sine.begin(WAVEFORM_SINE);
    sine.frequency(440.0f);   // La4 — referencia universal de afinación
    sine.amplitude(0.8f);     // 80% del rango dinámico
}

void loop() {
    // El audio corre en la ISR de la Teensy Audio Library — el loop solo monitorea.
    static uint32_t last = 0;
    if (millis() - last > 1000) {
        Serial.printf("CPU: %.1f%% | Mem: %d blocks\n",
            AudioProcessorUsageMax(),
            AudioMemoryUsageMax());
        AudioProcessorUsageMaxReset();
        AudioMemoryUsageMaxReset();
        last = millis();
    }
}
