#pragma once
#include <stdint.h>
// Fault-injection bus for the real AdsScanner implementation, native only.
class TwoWire {
public:
    unsigned transactions = 0;
    bool nack = false, read_fault = false, ready = true;
    uint8_t reg = 0, writes = 0, reads = 0;
    void beginTransmission(uint8_t) { ++transactions; writes = 0; }
    void write(uint8_t value) { if (!writes++) reg = value; }
    uint8_t endTransmission(bool = true) { return nack ? 2 : 0; }
    uint8_t requestFrom(uint8_t, uint8_t count) { ++transactions; reads = 0; return read_fault ? 0 : count; }
    int read() { return reg == 1 ? (reads++ == 0 && ready ? 0x80 : 0) : (reads++ == 0 ? 0x20 : 0); }
};
