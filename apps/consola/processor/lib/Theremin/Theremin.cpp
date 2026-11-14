#include "Theremin.h"
#include <Arduino.h>
#include <MIDI.h>

extern midi::MidiInterface<midi::SerialMIDI<usb_midi_class>> MIDI;

void Theremin::begin() {
    int pins[] = IR_PINS;
    for (int i = 0; i < NUM_IRS; i++) {
        pinMode(pins[i], INPUT);
        oldValues[i] = -1;
    }
}

void Theremin::read() {
    int pins[] = IR_PINS;
    for (int i = 0; i < NUM_IRS; i++) {
        int value = analogRead(pins[i]) / 8; // 10-bit to 7-bit
        if (abs(value - oldValues[i]) > 2) { // Add some tolerance
            MIDI.sendControlChange(20 + i, value, MIDI_CHANNEL);
            oldValues[i] = value;
        }
    }
}
