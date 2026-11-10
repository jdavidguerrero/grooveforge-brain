#pragma once
#include <stdint.h>
#include "RetryGate.h"
class TwoWire;

/** Adaptador hardware custom ADS1115; nunca espera una conversión. */
class AdsScanner {
public:
    /** Configura dirección, número de canales single-ended y bus compartido. */
    AdsScanner(TwoWire& wire, uint8_t address, uint8_t channels, uint8_t channel_mask = 0x0f);
    /** Un paso de estado, hasta dos transacciones Wire; nunca espera conversión. */
    bool poll(uint32_t now_us, uint8_t& channel, int16_t& sample);
    /** Errores acumulados de transacción I2C. */
    uint32_t errors() const;
    /** Last observed I2C availability, enabled devices only. */
    bool online() const { return retry_.online(); }
    uint32_t retry_us() const { return retry_.retry_us(); }
private:
    bool write_config();
    bool read_register(uint8_t reg, uint16_t& value);
    TwoWire& wire_;
    uint8_t address_, channels_, channel_ = 0, channel_mask_;
    RetryGate retry_;
    enum class State { Start, Wait, Read, Backoff };
    State state_ = State::Start;
    uint32_t since_ = 0, errors_ = 0;
};
