#pragma once
#include <stdint.h>
/** Device availability and wrap-safe one-second retry backoff. No logging per failure. */
class RetryGate {
public:
    /** True when a live device may be polled or an absent device may be probed. */
    bool due(uint32_t now) const { return !waiting_ || uint32_t(now - since_) >= 1000000; }
    /** Mark a failed I2C transaction and defer the next probe. */
    void failed(uint32_t now) { online_ = false; waiting_ = true; since_ = now; ++errors_; }
    /** Mark a successful probe, restoring normal polling. */
    void succeeded() { online_ = true; waiting_ = false; }
    /** Last observed availability, not proof of continuous electrical connection. */
    bool online() const { return online_; }
    /** Cumulative failed transactions. */
    uint32_t errors() const { return errors_; }
private:
    bool online_ = false, waiting_ = false;
    uint32_t since_ = 0, errors_ = 0;
};
