#include "Piezo.h"
#include <Arduino.h>
#include <MIDI.h>

extern midi::MidiInterface<midi::SerialMIDI<usb_midi_class>> MIDI;

#define PIEZO_THRESHOLD 50
#define PIEZO_TIMEOUT 50  // 50ms timeout to avoid retriggering

void Piezo::begin() {
    int pins[] = PIEZO_PINS;
    for (int i = 0; i < NUM_PIEZOS; i++) {
        pinMode(pins[i], INPUT);
        oldValues[i] = 0;
        isTriggered[i] = false;
        lastTriggerTime[i] = 0;
    }
}

void Piezo::read() {
    int pins[] = PIEZO_PINS;
    unsigned long currentTime = millis();
    
    for (int i = 0; i < NUM_PIEZOS; i++) {
        int value = analogRead(pins[i]);
        
        // Check if sensor was hit and not recently triggered
        if (value > PIEZO_THRESHOLD && !isTriggered[i] && 
            (currentTime - lastTriggerTime[i]) > PIEZO_TIMEOUT) {
            
            int velocity = mapVelocity(value);
            handlePiezoHit(i, velocity);
            
            isTriggered[i] = true;
            lastTriggerTime[i] = currentTime;
        }
        // Reset trigger state when value drops below threshold
        else if (value <= PIEZO_THRESHOLD && isTriggered[i]) {
            isTriggered[i] = false;
        }
        
        oldValues[i] = value;
    }
}

int Piezo::mapVelocity(int rawValue) {
    // Map analog reading (0-1023) to MIDI velocity (1-127)
    // Apply some curve to make it more musical
    int mapped = map(rawValue, PIEZO_THRESHOLD, 1023, 1, 127);
    return constrain(mapped, 1, 127);
}

void Piezo::handlePiezoHit(int piezo, int velocity) {
    // Send MIDI note for drum sounds (kick, snare, hi-hat, crash)
    int noteNumber = 36 + piezo; // C2 + offset
    MIDI.sendNoteOn(noteNumber, velocity, MIDI_CHANNEL);
    
    // Send note off after short duration for drum sounds
    MIDI.sendNoteOff(noteNumber, 0, MIDI_CHANNEL);
}
