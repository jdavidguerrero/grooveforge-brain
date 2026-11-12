#include "consola_s/I2cBranchRecovery.h"

void consola::I2cBranchRecovery::failed(uint8_t chip) {
    if (chip >= 4 || !(mask_ & (1u << chip)) || state_ != State::Enabled) return;
    if (++misses_[chip] >= 3) { state_ = State::Pending; idle_seen_ = false; }
}
void consola::I2cBranchRecovery::succeeded(uint8_t chip) {
    if (chip < 4) misses_[chip] = 0;
}
consola::I2cBranchRecovery::Action consola::I2cBranchRecovery::tick(uint32_t now, bool bus_idle) {
    if (state_ == State::Enabled) return Action::None;
    if (!bus_idle) idle_seen_ = false;
    else if (!idle_seen_) { idle_seen_ = true; idle_since_ = now; }
    if (!idle_seen_ || uint32_t(now-idle_since_) < 10) return Action::None;
    if (state_ == State::Pending) {
        state_ = State::Low; low_since_ = now; idle_seen_ = false; ++resets_;
        for (auto& streak : misses_) streak = 0;
        return Action::Disable;
    }
    if (uint32_t(now-low_since_) < 1000) return Action::None;
    state_ = State::Enabled;
    return Action::Enable;
}
