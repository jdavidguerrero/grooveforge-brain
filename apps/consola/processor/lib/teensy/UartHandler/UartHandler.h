#pragma once

#include <Arduino.h>

class UartHandler {
public:
    void begin();
    void read();
    void sendToNeoTrellis(uint8_t command, uint8_t* data, int length);
    void processNeoTrellisMessage();
    
private:
    static const int BUFFER_SIZE = 256;
    uint8_t rxBuffer[BUFFER_SIZE];
    int rxIndex;
    bool messageComplete;
    unsigned long lastPingMs = 0;
    unsigned long lastSeenMs = 0;
    
    
    void parseMessage();
    void handleNeoTrellisCommand(uint8_t command, uint8_t* data, int length);
};
