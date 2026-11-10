#ifdef ARDUINO
#include "consola_s/AdsScanner.h"
#include <Wire.h>
AdsScanner::AdsScanner(TwoWire& wire, uint8_t address, uint8_t channels, uint8_t channel_mask)
    : wire_(wire), address_(address), channels_(channels), channel_mask_(channel_mask) {}
bool AdsScanner::write_config() {
    const uint16_t config = 0x8000 | ((4 + channel_) << 12) | 0x0200 | 0x0100 | 0x00e0 | 3;
    wire_.beginTransmission(address_);
    wire_.write(1);
    wire_.write(uint8_t(config >> 8));
    wire_.write(uint8_t(config));
    return wire_.endTransmission() == 0;
}
bool AdsScanner::read_register(uint8_t reg, uint16_t& value) {
    wire_.beginTransmission(address_);
    wire_.write(reg);
    if (wire_.endTransmission(false) != 0) return false;
    if (wire_.requestFrom(address_, uint8_t(2)) != 2) return false;
    value = uint16_t(wire_.read()) << 8;
    value |= uint8_t(wire_.read());
    return true;
}
bool AdsScanner::poll(uint32_t now_us, uint8_t& channel, int16_t& sample) {
    if (!channel_mask_ || !retry_.due(now_us)) return false;
    while (!(channel_mask_ & (1u << channel_))) channel_ = (channel_ + 1) % channels_;
    if (state_ == State::Backoff) {
        state_ = State::Start;
    }
    bool ok = true;
    uint16_t value = 0;
    switch (state_) {
    case State::Start:
        ok = write_config();
        since_ = now_us;
        if (ok) state_ = State::Wait;
        break;
    case State::Wait:
        if (now_us - since_ < 1300) return false;
        ok = read_register(1, value);
        if (ok && (value & 0x8000)) state_ = State::Read;
        else if (ok && now_us - since_ > 10000) ok = false;
        break;
    case State::Read:
        ok = read_register(0, value);
        if (ok) {
            retry_.succeeded(); // Only a complete conversion resets backoff.
            channel = channel_;
            sample = int16_t(value);
            channel_ = (channel_ + 1) % channels_;
            state_ = State::Start;
            return true;
        }
        break;
    case State::Backoff: break;
    }
    if (!ok) {
        ++errors_;
        retry_.failed(now_us);
        since_ = now_us;
        state_ = State::Backoff;
    }
    return false;
}
uint32_t AdsScanner::errors() const { return errors_; }

#endif
