#pragma once

#include <Arduino.h>

class NeoTrellisLink {
public:
    void sendCommand(uint8_t command, const uint8_t* data, int dataLength);
    void sendRaw(const uint8_t* data, int length);
    void setPixelColor(int key, uint32_t color);
    void triggerConnectionAnimation();
    void runConnectionSweep();

    bool initializeCommunication();
    void update();

    void setConnected(bool connected, bool remoteRequest = false);
    bool isConnected() const { return m4Connected; }

    void handleHandshakeAck();
    void handlePingResponse();
    void handleDisconnectNotice();

private:
    void requestHandshake();
    void sendDisconnectEvent();

    bool m4Connected = false;
    bool handshakePending = false;
    bool disconnectNotified = false;
    bool everConnected = false;
    unsigned long lastHandshakeRequestMs = 0;
    unsigned long lastReconnectAttemptMs = 0;
    unsigned long lastPingSentMs = 0;
    unsigned long lastPongMs = 0;
    uint8_t handshakeAttempts = 0;
};
