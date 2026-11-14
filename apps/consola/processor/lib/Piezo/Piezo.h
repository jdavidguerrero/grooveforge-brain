#pragma once

#include "shared/Config.h"

class Piezo {
public:
    void begin();
    void read();
private:
    int oldValues[NUM_PIEZOS];
    bool isTriggered[NUM_PIEZOS];
    unsigned long lastTriggerTime[NUM_PIEZOS];
    int mapVelocity(int rawValue);
    void handlePiezoHit(int piezo, int velocity);
};
