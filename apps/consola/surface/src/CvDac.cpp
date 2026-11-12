#include "consola_s/CvDac.h"
#include <stdio.h>
#include <string.h>

bool consola::CvDac::is_provision_command(const char* line) {
    return !strcmp(line,"@GF CV EEPROM0");
}
bool consola::CvDac::set(uint16_t value) {
    if (value > 4095 || busy()) return false;
    desired_ = value;
    return true;
}
consola::CvDac::Request consola::CvDac::provision() {
    if (busy()) return Request::Busy;
    if (provisioned_) return Request::Already;
    desired_ = 0;
    written_ = verified_ = chip_ = 0;
    state_ = State::Write;
    return Request::Started;
}
void consola::CvDac::advance() {
    if (++chip_ < 2) state_ = State::Write;
    else {
        state_ = State::Idle;
        reply_pending_ = true;
        provisioned_ = verified_ == 3;
    }
}
void consola::CvDac::poll(CvDacBus& bus, uint32_t now) {
    if (state_ == State::Write) {
        const uint8_t data[] = {0x60,0,0};
        if (!bus.write(0x60+chip_,data,sizeof(data))) {
            gates_[chip_].failed(now);
            applied_[chip_] = 65535;
            advance();
        } else {
            written_ |= 1u << chip_;
            written_at_ = read_at_ = now;
            state_ = State::Verify;
        }
        return;
    }
    if (state_ == State::Verify) {
        if (uint32_t(now-written_at_) >= 100000) {
            gates_[chip_].failed(now);
            applied_[chip_] = 65535;
            advance();
            return;
        }
        if (uint32_t(now-read_at_) < 1000) return;
        read_at_ = now;
        uint8_t data[5]{};
        if (bus.read(0x60+chip_,data,sizeof(data)) != sizeof(data) || !(data[0] & 0x80)) return;
        // Ignore don't-care bits. DAC data is left-justified, EEPROM data right-justified.
        const bool zero = !(data[0] & 0x06) && data[1] == 0 && !(data[2] & 0xf0) &&
                          !(data[3] & 0x6f) && data[4] == 0;
        if (zero) {
            verified_ |= 1u << chip_;
            gates_[chip_].succeeded();
            applied_[chip_] = 0;
            refreshed_[chip_] = now;
        } else {
            gates_[chip_].failed(now);
            applied_[chip_] = 65535;
        }
        advance();
        return;
    }
    for (uint8_t n=0;n<2;++n) {
        const uint8_t chip = next_;
        next_ ^= 1;
        const uint16_t value = chip == 0 ? desired_ : 0;
        if (!gates_[chip].due(now)) continue;
        if (applied_[chip] == value && uint32_t(now-refreshed_[chip]) < 1000000) continue;
        const uint8_t data[] = {uint8_t(value >> 8),uint8_t(value)};
        if (!bus.write(0x60+chip,data,sizeof(data))) {
            gates_[chip].failed(now);
            applied_[chip] = 65535;
        } else {
            gates_[chip].succeeded();
            applied_[chip] = value;
            refreshed_[chip] = now;
        }
        return;
    }
}
bool consola::CvDac::reply(char* out, size_t size) {
    if (!reply_pending_) return false;
    const int n = verified_ == 3
        ? snprintf(out,size,"@GF OK CV EEPROM0 VERIFIED=60,61 WRITTEN=%u\n",written_)
        : snprintf(out,size,"@GF ERR CV EEPROM0 WRITTEN=%u VERIFIED=%u\n",written_,verified_);
    if (n < 0 || size_t(n) >= size) return false;
    reply_pending_ = false;
    return true;
}
