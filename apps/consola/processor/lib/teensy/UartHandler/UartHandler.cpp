#include "UartHandler/UartHandler.h"
#include "shared/Config.h"
#include "shared/BinaryProtocol.h"
#include "MidiCommands.h"
#include "../UIBridge.h"
#include "../NeoTrellisLink/NeoTrellisLink.h"
#include "../LiveController/LiveController.h"

extern UIBridge uiBridge;
extern NeoTrellisLink neoTrellisLink;
extern LiveController liveController;

void UartHandler::begin() {
    TEENSY_M4_SERIAL.begin(UART_BAUD_RATE);
    rxIndex = 0;
    messageComplete = false;
    lastSeenMs = millis();
    Serial.print("Teensy: UART handler listening @ ");
    Serial.println(UART_BAUD_RATE);
}

void UartHandler::read() {
    while (TEENSY_M4_SERIAL.available()) {
        uint8_t byte = TEENSY_M4_SERIAL.read();
        lastSeenMs = millis();

        if (byte == BinaryProtocol::BINARY_SYNC_BYTE) {
            rxIndex = 0;
            messageComplete = false;
            rxBuffer[rxIndex++] = byte;
            continue;
        }

        if (rxIndex == 0) {
            continue;
        }

        if (rxIndex < BUFFER_SIZE) {
            rxBuffer[rxIndex++] = byte;
        } else {
            Serial.println("Teensy: UART buffer overflow, dropping packet");
            rxIndex = 0;
            continue;
        }

        if (rxIndex >= 3) {
            uint8_t payloadLen = rxBuffer[2] & 0x7F;
            uint16_t expectedLength = BinaryProtocol::getMessageSize(payloadLen);
            if (expectedLength > BUFFER_SIZE) {
                Serial.println("Teensy: Binary message too large");
                rxIndex = 0;
                continue;
            }
            if (rxIndex == expectedLength) {
                messageComplete = true;
                parseMessage();
                rxIndex = 0;
            } else if (rxIndex > expectedLength) {
                Serial.println("Teensy: Binary message exceeded expected length");
                rxIndex = 0;
            }
        }
    }
}

void UartHandler::parseMessage() {
    if (!messageComplete) {
        return;
    }

    uint8_t command = 0;
    const uint8_t* payload = nullptr;
    uint8_t payloadLen = 0;
    bool valid = BinaryProtocol::parseMessage(rxBuffer, rxIndex, command, payload, payloadLen);
    if (!valid) {
        Serial.println("Teensy: Invalid binary UART frame");
        return;
    }

    Serial.print("Teensy: UART CMD 0x");
    Serial.print(command, HEX);
    Serial.print(" LEN ");
    Serial.println(payloadLen);

    handleNeoTrellisCommand(command, const_cast<uint8_t*>(payload), payloadLen);
}

void UartHandler::sendToNeoTrellis(uint8_t command, uint8_t* data, int length) {
    if (length < 0) length = 0;
    uint8_t buffer[BUFFER_SIZE];
    uint16_t frameLen = BinaryProtocol::buildMessage(
        command,
        data,
        static_cast<uint8_t>(length),
        buffer,
        sizeof(buffer)
    );
    if (frameLen == 0) {
        Serial.println("Teensy: Failed to build UART frame for NeoTrellis");
        return;
    }
    TEENSY_M4_SERIAL.write(buffer, frameLen);
    TEENSY_M4_SERIAL.flush();
}

void UartHandler::processNeoTrellisMessage() {}

void UartHandler::handleNeoTrellisCommand(uint8_t command, uint8_t* data, int length) {
    lastSeenMs = millis();

    switch (command) {
        case CMD_HANDSHAKE_REPLY:
            Serial.println("Teensy: Received CMD_HANDSHAKE_REPLY");
            neoTrellisLink.handleHandshakeAck();
            break;
        case CMD_PING:
            neoTrellisLink.handlePingResponse();
            break;
        case CMD_DISCONNECT:
            neoTrellisLink.handleDisconnectNotice();
            break;
        default:
            uiBridge.handleUARTCommand(command, data, static_cast<uint8_t>(length));
            break;
    }
}
