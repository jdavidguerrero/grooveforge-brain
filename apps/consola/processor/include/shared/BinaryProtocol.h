#pragma once

#include <Arduino.h>
#include <cstring>

// Simple binary framing used between Teensy ↔ NeoTrellis over UART:
// [SYNC][CMD][LEN][PAYLOAD...][CHECKSUM]
// SYNC is fixed (0xAA) so receivers can resync quickly.
// The checksum is an XOR of CMD, LEN and each payload byte.
// All functions here are header-only to avoid linking issues on both targets.
class BinaryProtocol {
public:
    static constexpr uint8_t BINARY_SYNC_BYTE = 0xAA;
    static constexpr uint8_t HEADER_SIZE = 4; // SYNC + CMD + LEN + CHECKSUM

    // Compute the total message size for a payload of payloadLen bytes.
    static constexpr uint16_t getMessageSize(uint8_t payloadLen) {
        return static_cast<uint16_t>(payloadLen) + HEADER_SIZE;
    }

    // Build a framed message into outBuffer. Returns total bytes written or 0 on error.
    static uint16_t buildMessage(uint8_t command,
                                 const uint8_t* payload,
                                 uint8_t payloadLen,
                                 uint8_t* outBuffer,
                                 uint16_t bufferSize) {
        if (!outBuffer) {
            return 0;
        }
        if (payloadLen > 0 && payload == nullptr) {
            return 0;
        }

        const uint16_t totalLen = getMessageSize(payloadLen);
        if (bufferSize < totalLen) {
            return 0;
        }

        outBuffer[0] = BINARY_SYNC_BYTE;
        outBuffer[1] = command;
        outBuffer[2] = payloadLen;

        uint8_t checksum = computeChecksum(command, payloadLen, payload);

        if (payloadLen > 0) {
            memcpy(&outBuffer[3], payload, payloadLen);
        }

        outBuffer[totalLen - 1] = checksum;
        return totalLen;
    }

    // Parse a framed message from buffer. Returns true when the frame validates.
    static bool parseMessage(const uint8_t* buffer,
                             uint16_t length,
                             uint8_t& command,
                             const uint8_t*& payload,
                             uint8_t& payloadLen) {
        if (!buffer || length < HEADER_SIZE) {
            return false;
        }
        if (buffer[0] != BINARY_SYNC_BYTE) {
            return false;
        }

        command = buffer[1];
        payloadLen = buffer[2];

        const uint16_t expectedLen = getMessageSize(payloadLen);
        if (length != expectedLen) {
            return false;
        }

        payload = payloadLen ? &buffer[3] : nullptr;
        const uint8_t expectedChecksum = buffer[length - 1];
        const uint8_t computedChecksum = computeChecksum(command, payloadLen, payload);
        return expectedChecksum == computedChecksum;
    }

private:
    static uint8_t computeChecksum(uint8_t command,
                                   uint8_t payloadLen,
                                   const uint8_t* payload) {
        uint8_t checksum = command ^ payloadLen;
        if (payload && payloadLen > 0) {
            for (uint8_t i = 0; i < payloadLen; ++i) {
                checksum ^= payload[i];
            }
        }
        return checksum;
    }
};
