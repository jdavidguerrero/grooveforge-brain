#pragma once
#include <stdint.h>

namespace consola {
/** Loop-only policy for one independent PCA9517A EN; no GPIO, I2C or allocation. */
class I2cBranchRecovery {
public:
    enum class Action { None, Disable, Enable };
    /** Only enabled MCPs may request a reset; bits 0..3 correspond to 0x20..23. */
    explicit I2cBranchRecovery(uint8_t mcp_mask) : mask_(mcp_mask & 15) {}
    /** Count an unsuccessful configuration/read attempt; three in a row request EN. */
    void failed(uint8_t chip);
    /** Clear a chip's streak only after a complete GPIO read. */
    void succeeded(uint8_t chip);
    /** Advance with physical SDA/SCL idle and wrap-safe micros; caller applies returned edge. */
    Action tick(uint32_t now_us, bool bus_idle);
    /** Pause all firmware I2C between the request and the completed EN pulse. */
    bool paused() const { return state_ != State::Enabled; }
    /** Number of low pulses initiated since boot/clear_count, not proof of recovery. */
    uint32_t resets() const { return resets_; }
    /** Clear diagnostics without cancelling a pending pulse or failure streak. */
    void clear_count() { resets_ = 0; }
private:
    enum class State { Enabled, Pending, Low };
    State state_ = State::Enabled;
    uint8_t mask_, misses_[4]{};
    uint32_t low_since_ = 0, idle_since_ = 0, resets_ = 0;
    bool idle_seen_ = false;
};
}
