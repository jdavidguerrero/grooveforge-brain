#pragma once

#include "shared/Config.h"

class CapButtons {
public:
    void begin();
    void read();
private:
    bool oldStates[NUM_CAPACITIVE_BUTTONS];
    unsigned long lastReadTime;
    void handleButtonPress(int button);
    void handleButtonRelease(int button);
};
