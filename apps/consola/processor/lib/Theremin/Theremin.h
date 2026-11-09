#pragma once

#include "shared/Config.h"

class Theremin {
public:
    void begin();
    void read();
private:
    int oldValues[NUM_IRS];
};
