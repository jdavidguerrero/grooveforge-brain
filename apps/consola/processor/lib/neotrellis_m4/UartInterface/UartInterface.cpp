#include "UartInterface.h"
#include "shared/Config.h"
#include "shared/BinaryProtocol.h"
#include "MidiCommands.h"
#include "UIPanelCommands.h"
#include "NeoTrellisController.h"

extern NeoTrellisController controller;

// Use built-in Serial1 (SERCOM4) on pins 22 (RX) and 21 (TX)

void UartInterface::begin() {
    Serial.println("=== NeoTrellis M4 UART Configuration ===");
    Serial.println("Using built-in Serial1 (SERCOM4) on pins 21/22");
    
    Serial.print("Initializing UART @ ");
    Serial.print(UART_BAUD_RATE);
    Serial.println(" bps");
    
    // Initialize built-in Serial1
    Serial1.begin(UART_BAUD_RATE);
    
    // Small delay to ensure initialization
    delay(200);
    
    rxIndex = 0;
    messageComplete = false;
    everReceived = false;
    expectedPacketLength = 0;
    beginMs = millis();
    
    Serial.println("NeoTrellis M4: Serial1 initialized successfully!");
    Serial.println("Pin configuration:");
    Serial.println("  Pin 21 (SDA/JST) -> UART TX (to Teensy RX1)");
    Serial.println("  Pin 22 (SCL/JST) -> UART RX (from Teensy TX1)");
    Serial.println("Expected connections:");
    Serial.println("  Teensy Pin 1 (TX1) -> M4 Pin 22 (RX)");
    Serial.println("  Teensy Pin 0 (RX1) -> M4 Pin 21 (TX)");
    Serial.println("  GND -> GND");
    Serial.println("Waiting for data from Teensy (Serial1)...");

    Serial.println("NeoTrellis M4: Notifying Teensy of reset/disconnect state");
    sendToTeensy(CMD_DISCONNECT, nullptr, 0);
}

void UartInterface::read() {
    static unsigned long lastDebugTime = 0;
    static bool debugPrinted = false;

    if (!debugPrinted && (millis() - lastDebugTime > 5000)) {
        Serial.print("NeoTrellis M4: Serial1 status - Available: ");
        Serial.print(Serial1.available());
        Serial.println(" bytes (Checking for Teensy data via pins 21/22)");
        lastDebugTime = millis();
        debugPrinted = (millis() > 30000);
    }

    while (Serial1.available()) {
        if (!everReceived) {
            Serial.println("NeoTrellis M4: First UART byte received from Teensy!");
            everReceived = true;
        }

        uint8_t byte = Serial1.read();

        if (byte == BinaryProtocol::BINARY_SYNC_BYTE) {
            rxIndex = 0;
            expectedPacketLength = 0;
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
            Serial.println("NeoTrellis M4: UART message too long, dropping");
            rxIndex = 0;
            expectedPacketLength = 0;
            continue;
        }

        if (rxIndex >= 3 && expectedPacketLength == 0) {
            uint8_t payloadLen = rxBuffer[2];
            expectedPacketLength = BinaryProtocol::getMessageSize(payloadLen);
            if (expectedPacketLength > BUFFER_SIZE) {
                Serial.println("NeoTrellis M4: Binary packet too large");
                rxIndex = 0;
                expectedPacketLength = 0;
                continue;
            }
        }

        if (expectedPacketLength != 0) {
            if (rxIndex == expectedPacketLength) {
                messageComplete = true;
                parseMessage();
                rxIndex = 0;
                expectedPacketLength = 0;
            } else if (rxIndex > expectedPacketLength) {
                Serial.println("NeoTrellis M4: Invalid message length, dropping");
                rxIndex = 0;
                expectedPacketLength = 0;
            }
        }
    }
}

void UartInterface::parseMessage() {
    uint8_t command;
    const uint8_t* payload;
    uint8_t payloadLen;

    bool valid = BinaryProtocol::parseMessage(
        rxBuffer,
        rxIndex,
        command,
        payload,
        payloadLen
    );

    if (!valid) {
        Serial.println("NeoTrellis M4: Invalid binary message (bad checksum/format)");
        messageComplete = false;
        return;
    }

    handleTeensyCommand(command, const_cast<uint8_t*>(payload), payloadLen);
    messageComplete = false;
}

void UartInterface::handleTeensyCommand(uint8_t command, uint8_t* data, int length) {
    switch (command) {
        case CMD_HANDSHAKE:
            Serial.println("NeoTrellis M4: Handshake request received from Teensy.");
            Serial.println("NeoTrellis M4: Sending handshake reply...");
            sendToTeensy(CMD_HANDSHAKE_REPLY, nullptr, 0);
            break;
        case CMD_UART_CONFIRMATION_ANIMATION:
            Serial.println("NeoTrellis M4: Received request for connection animation.");
            controller.connectionAnimation();
            break;
        case CMD_PING:
            // Echo ping back to Teensy
            sendToTeensy(CMD_PING, nullptr, 0);
            break;
        case CMD_LED_RGB_STATE:
            if (length >= 4) {
                int padIndex = data[0];
                uint8_t r7 = data[1];
                uint8_t g7 = data[2];
                uint8_t b7 = data[3];
                controller.applyPadColor7bit(padIndex, r7, g7, b7);
                Serial.print("NeoTrellis M4: Set RGB pad ");
                Serial.print(padIndex);
                Serial.print(" -> ");
                Serial.print(r7);
                Serial.print(",");
                Serial.print(g7);
                Serial.print(",");
                Serial.println(b7);
            }
            break;
        case CMD_LED_CLIP_STATE:
            if (length >= 2) {
                int padIndex = data[0];
                int state = data[1];
                controller.setClipStateCache(padIndex, static_cast<uint8_t>(state));
            }
            break;

        case CMD_LED_GRID_UPDATE:
            // Bulk grid update: expect 32 * (R,G,B) 7-bit values = 96 bytes
            if (length == 96) {
                controller.applyGridColors7bit(data, length);
                controller.setGridInitialized(true);
                Serial.println("NeoTrellis M4: Applied bulk grid update (96 bytes)");
            } else {
                Serial.print("NeoTrellis M4: Invalid grid bulk length: ");
                Serial.println(length);
            }
            break;

        case CMD_LED_GRID_UPDATE_14:
            // Bulk grid update (14-bit per channel): 32 * (Rmsb,Rlsb,Gmsb,Glsb,Bmsb,Blsb) = 192 bytes
            if (length == 192) {
                controller.applyGridColors14bit(data, length);
                controller.setGridInitialized(true);
                Serial.println("NeoTrellis M4: Applied bulk grid update (192 bytes, 14-bit)");
            } else {
                Serial.print("NeoTrellis M4: Invalid 14-bit grid bulk length: ");
                Serial.println(length);
            }
            break;

        case CMD_LED_TRANSPORT_STATE:
            Serial.println("NeoTrellis M4: Transport state update received");
            break;

        case CMD_LED_DEVICE_STATE:
            Serial.println("NeoTrellis M4: Device state update received");
            break;
            
        case CMD_LED_PAD_UPDATE_14:
            if (length >= 7) {
                int padIndex = data[0];
                uint8_t r = (uint8_t)(((data[1] & 0x7F) << 7) | (data[2] & 0x7F));
                uint8_t g = (uint8_t)(((data[3] & 0x7F) << 7) | (data[4] & 0x7F));
                uint8_t b = (uint8_t)(((data[5] & 0x7F) << 7) | (data[6] & 0x7F));
                controller.applyPadColor8bit(padIndex, r, g, b);
                Serial.print("NeoTrellis M4: Set 14-bit RGB pad ");
                Serial.print(padIndex);
                Serial.print(" -> ");
                Serial.print(r);
                Serial.print(",");
                Serial.print(g);
                Serial.print(",");
                Serial.println(b);
            }
            break;
        case CMD_ENABLE_KEYS:
            Serial.println("NeoTrellis M4: Key scanning ENABLED by Teensy");
            controller.enableKeyScanning();
            break;
            
        case CMD_DISABLE_KEYS:
            Serial.println("NeoTrellis M4: Key scanning DISABLED by Teensy");
            controller.disableKeyScanning();
            break;

        case CMD_DISCONNECT:
            Serial.println("NeoTrellis M4: Teensy requested disconnect/reset, clearing grid");
            controller.disableKeyScanning();
            controller.allOff();
            controller.setGridInitialized(false);
            break;

        default:
            Serial.print("NeoTrellis M4: Unknown command received: 0x");
            Serial.println(command, HEX);
            break;
    }
}

void UartInterface::sendToTeensy(uint8_t command, uint8_t* data, int length) {
    // Use BinaryProtocol to build message
    uint8_t txBuffer[BUFFER_SIZE];
    uint16_t messageLen = BinaryProtocol::buildMessage(
        command,
        data,
        length,
        txBuffer,
        BUFFER_SIZE
    );

    if (messageLen > 0) {
        // Send message via Serial1
        Serial1.write(txBuffer, messageLen);
        Serial1.flush(); // Ensure data is sent immediately

        Serial.print("NeoTrellis M4: Sent command 0x");
        Serial.print(command, HEX);
        Serial.print(" with payload=");
        Serial.print(length);
        Serial.print(" bytes, total message=");
        Serial.print(messageLen);
        Serial.println(" bytes");
    } else {
        Serial.println("NeoTrellis M4: ERROR - Failed to build message (buffer too small)");
    }
}

// No custom pin mux needed; Serial1 handles pin configuration per variant mapping
