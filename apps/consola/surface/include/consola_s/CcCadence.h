#pragma once
#include <stdint.h>

/** Per-control 100 Hz cadence, including micros wrap. First update is immediate. */
class CcCadence {
public:
    bool due(uint32_t now) const { return !sent_ || uint32_t(now - last_) >= 10000; }
    void sent(uint32_t now) { sent_ = true; last_ = now; }
private:
    uint32_t last_ = 0;
    bool sent_ = false;
};
