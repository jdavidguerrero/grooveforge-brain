#pragma once

#include <Encoder.h>
#include "shared/Config.h"

class Encoders {
public:
    Encoders();

    void begin();
    void read();

    // Escalabilidad para 8 encoders
    void setActiveCount(uint8_t count);
    uint8_t getActiveCount() const { return activeEncoders; }

    // Contexto de vista para mapeo dinámico
    void setCurrentView(uint8_t viewIndex) { currentView = viewIndex; }

    // Modificadores
    void setShiftHeld(bool held) { shiftHeld = held; }

private:
    Encoder* encoders[NUM_ENCODERS_MAX];
    long oldPositions[NUM_ENCODERS_MAX];
    uint8_t activeEncoders;
    uint8_t currentView;  // 0=Session, 1=Mix, 2=Device, 3=Note
    bool shiftHeld;

public:
    // Callback para cuando un encoder rota (moved after private members to match initialization order)
    void (*onEncoderChange)(uint8_t encoderIndex, int8_t delta);

private:

    void sendEncoderChange(uint8_t encoderIndex, int8_t delta);
};
