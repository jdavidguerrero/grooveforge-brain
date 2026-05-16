// Sprint 1.2 — Multi-OSC + ADSR: 3 osciladores con detune + envelope ADSR.
// Theory, wiring y demo: apps/docs/sprints/02-multi-osc-adsr.md

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>

AudioSynthWaveform    osc1;
AudioSynthWaveform    osc2;
AudioSynthWaveform    osc3;
AudioMixer4           mixer;
AudioEffectEnvelope   envelope;
AudioOutputI2S        audioOut;
AudioControlSGTL5000  codec;

AudioConnection c1(osc1,     0, mixer,    0);
AudioConnection c2(osc2,     0, mixer,    1);
AudioConnection c3(osc3,     0, mixer,    2);
AudioConnection c4(mixer,    0, envelope, 0);
AudioConnection c5(envelope, 0, audioOut, 0);  // L
AudioConnection c6(envelope, 0, audioOut, 1);  // R

// Parámetros ajustables por Serial en runtime
float baseFreq  = 440.0f;  // A4 — referencia seleccionable (ver Sprint 1.1 learnings)
float detuneHz  = 2.0f;    // beating de 2Hz entre osc1 y osc2
float subLevel  = 0.3f;    // nivel del sub-octave en el mixer

void setup() {
    Serial.begin(115200);
    AudioMemory(16);

    codec.enable();
    codec.volume(0.5);

    osc1.begin(WAVEFORM_SAWTOOTH);
    osc2.begin(WAVEFORM_SAWTOOTH);
    osc3.begin(WAVEFORM_SAWTOOTH);

    osc1.frequency(baseFreq);
    osc2.frequency(baseFreq + detuneHz);
    osc3.frequency(baseFreq / 2.0f);

    osc1.amplitude(1.0f);
    osc2.amplitude(1.0f);
    osc3.amplitude(1.0f);

    // Suma 0.5 + 0.5 + 0.3 = 1.3 — leve saturación intencional (warm clipping)
    mixer.gain(0, 0.5f);
    mixer.gain(1, 0.5f);
    mixer.gain(2, subLevel);
    mixer.gain(3, 0.0f);

    // ADSR tipo pad: etapas claramente audibles para el demo
    envelope.attack(50.0f);
    envelope.decay(200.0f);
    envelope.sustain(0.7f);
    envelope.release(300.0f);

    Serial.println("Sprint 1.2 — Multi-OSC + ADSR");
    Serial.println("n=noteOn  f=noteOff");
    Serial.println("a<ms>=attack  d<ms>=decay  s<0-100>=sustain  r<ms>=release");
    Serial.println("t<hz>=detune  b<hz>=baseFreq");
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        float val = Serial.parseFloat();

        switch (cmd) {
            case 'n':
                envelope.noteOn();
                Serial.println("noteOn");
                break;
            case 'f':
                envelope.noteOff();
                Serial.println("noteOff");
                break;
            case 'a':
                envelope.attack(val);
                Serial.printf("attack: %.0f ms\n", val);
                break;
            case 'd':
                envelope.decay(val);
                Serial.printf("decay: %.0f ms\n", val);
                break;
            case 's':
                envelope.sustain(val / 100.0f);
                Serial.printf("sustain: %.0f%%\n", val);
                break;
            case 'r':
                envelope.release(val);
                Serial.printf("release: %.0f ms\n", val);
                break;
            case 't':
                detuneHz = val;
                osc2.frequency(baseFreq + detuneHz);
                Serial.printf("detune: %.1f Hz\n", detuneHz);
                break;
            case 'b':
                baseFreq = val;
                osc1.frequency(baseFreq);
                osc2.frequency(baseFreq + detuneHz);
                osc3.frequency(baseFreq / 2.0f);
                Serial.printf("baseFreq: %.1f Hz\n", baseFreq);
                break;
        }
    }

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
