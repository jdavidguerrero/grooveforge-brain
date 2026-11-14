#pragma once
#include <stddef.h>
#include <stdint.h>
#include "consola_s/RetryGate.h"

namespace consola {
/** Bounded loop-only transactions; the hardware adapter owns Wire and its deadline. */
class CvDacBus {
public:
    /** Write a complete command; true means ACK, not EEPROM completion. */
    virtual bool write(uint8_t address, const uint8_t* data, uint8_t size) = 0;
    /** Read status/register/EEPROM bytes, returning the actual byte count. */
    virtual uint8_t read(uint8_t address, uint8_t* data, uint8_t size) = 0;
    virtual ~CvDacBus() = default;
};
/** Two populated MCP4725s: volatile safety refresh and explicit factory provisioning. */
class CvDac {
public:
    enum class Request { Started, Busy, Already };
    /** Perform at most one transaction; call twice before codec init to zero both DACs. */
    void poll(CvDacBus& bus, uint32_t now_us);
    /** Set CV1 only (12 bits); CV2 always remains zero. Reject during provisioning. */
    bool set(uint16_t value);
    /** Explicit EEPROM-zero request, one write per DAC; never invoked by boot/retry. */
    Request provision();
    /** Strict bench grammar, without modifying the input line. */
    static bool is_provision_command(const char* line);
    /** Take the final CDC result once; false leaves the caller's buffer untouched. */
    bool reply(char* out, size_t size);
    /** EEPROM work or an unconsumed result owns the command until CDC accepts it. */
    bool busy() const { return state_ != State::Idle || reply_pending_; }
    /** Availability/error/backoff per DAC index 0=0x60, 1=0x61. */
    const RetryGate& status(uint8_t chip) const { return gates_[chip]; }
private:
    enum class State { Idle, Write, Verify };
    State state_ = State::Idle;
    RetryGate gates_[2];
    uint16_t desired_ = 0, applied_[2] = {65535,65535};
    uint32_t refreshed_[2]{}, written_at_ = 0, read_at_ = 0;
    uint8_t next_ = 0, chip_ = 0, written_ = 0, verified_ = 0;
    bool reply_pending_ = false, provisioned_ = false;
    void advance();
};
}
