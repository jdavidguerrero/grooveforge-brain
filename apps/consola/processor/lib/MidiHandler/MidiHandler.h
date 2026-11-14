#pragma once

#include "shared/Config.h"
#include <Arduino.h> 

class MidiHandler {
public:
    void begin();
    void handleSysEx(uint8_t* data, unsigned int length);
    void processMidiMessages();
private:
    void handleLedClipState(uint8_t* data, unsigned int length);
    // Removed other methods - focusing only on NeoTrellis for now
};
