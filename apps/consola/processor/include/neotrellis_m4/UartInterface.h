#pragma once

#include <Arduino.h>
#include "shared/Config.h"

class UartInterface {
public:
    void begin();
    void read();
    void sendToTeensy(uint8_t command, uint8_t* data, int length);

private:
    static const int BUFFER_SIZE = 256;
    uint8_t rxBuffer[BUFFER_SIZE];
    int rxIndex;
    bool messageComplete;
    bool everReceived = false;
    unsigned long beginMs = 0;
    uint16_t expectedPacketLength = 0;
    
    void parseMessage();
    void handleTeensyCommand(uint8_t command, uint8_t* data, int length);
};
