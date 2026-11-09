#include "NeoTrellisLink/NeoTrellisLink.h"
#include "shared/BinaryProtocol.h"
#include "MidiCommands.h"
#include "shared/Config.h"
#include "../UartHandler/UartHandler.h"
#include "../LiveController/LiveController.h"
#include <cstring>

namespace {
constexpr uint16_t HANDSHAKE_TIMEOUT_MS = 1000;
constexpr uint16_t RECONNECT_BACKOFF_MS = 1500;
constexpr uint8_t MAX_INITIAL_ATTEMPTS = 3;
const uint8_t HANDSHAKE_PAYLOAD[] = {0x50,0x55,0x53,0x48,0x43,0x4C,0x4F,0x4E,0x45}; // "PUSHCLONE"
}

extern UartHandler uartHandler;
extern LiveController liveController;

void NeoTrellisLink::sendCommand(uint8_t command, const uint8_t* data, int dataLength) {
    if (dataLength < 0) dataLength = 0;

    // Use BinaryProtocol to build message
    // Maximum message size: SYNC + CMD + LEN + PAYLOAD + CHECKSUM
    uint8_t txBuffer[260]; // 4 overhead + 256 max payload
    uint16_t messageLen = BinaryProtocol::buildMessage(
        command,
        data,
        static_cast<uint8_t>(dataLength),
        txBuffer,
        sizeof(txBuffer)
    );

    if (messageLen > 0) {
        TEENSY_M4_SERIAL.write(txBuffer, messageLen);
        TEENSY_M4_SERIAL.flush();
    } else {
        Serial.println("Teensy: ERROR - Failed to build binary message for NeoTrellis");
    }
}

void NeoTrellisLink::sendRaw(const uint8_t* data, int length) {
    if (!data || length <= 0) return;
    TEENSY_M4_SERIAL.write(data, length);
    TEENSY_M4_SERIAL.flush();
}

void NeoTrellisLink::sendDisconnectEvent() {
    if (disconnectNotified || !everConnected) {
        return;
    }
    sendCommand(CMD_DISCONNECT, nullptr, 0);
    disconnectNotified = true;
    Serial.println("Teensy: Notified NeoTrellis of disconnect/reset state");
}

void NeoTrellisLink::setPixelColor(int key, uint32_t color) {
    if (key < 0 || key >= TOTAL_KEYS) return;
    uint8_t payload[] = {
        static_cast<uint8_t>(key),
        static_cast<uint8_t>((color >> 16) & 0xFF),
        static_cast<uint8_t>((color >> 8) & 0xFF),
        static_cast<uint8_t>(color & 0xFF)
    };
    sendCommand(CMD_LED_RGB_STATE, payload, sizeof(payload));
}

void NeoTrellisLink::triggerConnectionAnimation() {
    sendCommand(CMD_UART_CONFIRMATION_ANIMATION, nullptr, 0);
}

void NeoTrellisLink::runConnectionSweep() {
    if (!m4Connected) {
        return;
    }

    Serial.println("Teensy: Running NeoTrellis grid sweep to validate UART link...");

    uint8_t clearFrame[TOTAL_KEYS * 3];
    memset(clearFrame, 0, sizeof(clearFrame));
    sendCommand(CMD_LED_GRID_UPDATE, clearFrame, sizeof(clearFrame));
    delay(40);

    const uint8_t sweepColorR = 0x00;
    const uint8_t sweepColorG = 0x60;
    const uint8_t sweepColorB = 0x18;

    uint8_t payload[4];
    payload[1] = sweepColorR;
    payload[2] = sweepColorG;
    payload[3] = sweepColorB;

    for (int pad = 0; pad < TOTAL_KEYS; ++pad) {
        payload[0] = static_cast<uint8_t>(pad);
        sendCommand(CMD_LED_RGB_STATE, payload, sizeof(payload));
        delay(18);
    }

    Serial.println("Teensy: Sweep complete — waiting for Live colors.");
}

bool NeoTrellisLink::initializeCommunication() {
    Serial.println("Teensy: Initializing UART communication with NeoTrellis...");
    delay(100);

    handshakeAttempts = 0;
    handshakePending = false;
    disconnectNotified = false;
    setConnected(false);
    sendDisconnectEvent();

    while (!m4Connected && handshakeAttempts < MAX_INITIAL_ATTEMPTS) {
        requestHandshake();
        unsigned long waitStart = millis();
        while (!m4Connected && (millis() - waitStart) < HANDSHAKE_TIMEOUT_MS) {
            uartHandler.read();
        }
        if (!m4Connected) {
            handshakePending = false;
            delay(150);
        }
    }

    if (!m4Connected) {
        Serial.println("Teensy: ✗ NeoTrellis handshake failed after retries. Check wiring/baud.");
        return false;
    }

    Serial.println("Teensy: ✓ NeoTrellis handshake ACK received — UART link up");
    return true;
}

void NeoTrellisLink::update() {
    unsigned long now = millis();

    if (m4Connected) {
        if (now - lastPingSentMs >= UART_PING_INTERVAL_MS) {
            sendCommand(CMD_PING, nullptr, 0);
            lastPingSentMs = now;
        }
        if (lastPongMs != 0 && (now - lastPongMs) > UART_LINK_TIMEOUT_MS) {
            Serial.println("Teensy: NeoTrellis ping timeout — marking disconnected");
            setConnected(false);
        }
        return;
    }

    if (!handshakePending && (now - lastReconnectAttemptMs) >= RECONNECT_BACKOFF_MS) {
        requestHandshake();
        lastReconnectAttemptMs = now;
    } else if (handshakePending && (now - lastHandshakeRequestMs) > HANDSHAKE_TIMEOUT_MS) {
        Serial.println("Teensy: NeoTrellis handshake request timed out");
        handshakePending = false;
    }
}

void NeoTrellisLink::requestHandshake() {
    if (handshakePending) return;
    Serial.println("Teensy: Sending NeoTrellis handshake...");
    sendCommand(CMD_HANDSHAKE, HANDSHAKE_PAYLOAD, sizeof(HANDSHAKE_PAYLOAD));
    handshakePending = true;
    lastHandshakeRequestMs = millis();
    handshakeAttempts++;
}

void NeoTrellisLink::setConnected(bool connected, bool remoteRequest) {
    if (m4Connected == connected) {
        if (!connected && remoteRequest) {
            disconnectNotified = true;
        }
        return;
    }

    m4Connected = connected;
    Serial.print("Teensy: NeoTrellis UART state -> ");
    Serial.println(connected ? "CONNECTED" : "DISCONNECTED");

    if (connected) {
        handshakePending = false;
        handshakeAttempts = 0;
        lastPongMs = millis();
        lastPingSentMs = lastPongMs;
        disconnectNotified = false;
        everConnected = true;
        runConnectionSweep();
    } else {
        handshakePending = false;
        lastPongMs = 0;
        lastPingSentMs = 0;
        liveController.setHardwareReady(false);
        if (remoteRequest) {
            disconnectNotified = true;
        } else {
            sendDisconnectEvent();
        }
    }
}

void NeoTrellisLink::handleHandshakeAck() {
    Serial.println("Teensy: NeoTrellis handshake ACK received.");
    setConnected(true);
    triggerConnectionAnimation();

    // Enable key scanning on M4 after successful connection
    Serial.println("Teensy: Enabling key scanning on NeoTrellis M4...");
    sendCommand(CMD_ENABLE_KEYS, nullptr, 0);

    liveController.setHardwareReady(true);
}

void NeoTrellisLink::handlePingResponse() {
    lastPongMs = millis();
}

void NeoTrellisLink::handleDisconnectNotice() {
    Serial.println("Teensy: NeoTrellis reported disconnect/reset");
    setConnected(false, true);
}
