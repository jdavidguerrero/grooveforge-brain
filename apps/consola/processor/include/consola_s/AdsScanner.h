#pragma once
#include <stdint.h>
class TwoWire;

/** Adaptador hardware custom ADS1115; nunca espera una conversión. */
class AdsScanner {
public:
    /** Configura dirección, número de canales single-ended y bus compartido. */
    AdsScanner(TwoWire& wire, uint8_t address, uint8_t channels);
    /** Ejecuta como máximo una transacción. True entrega canal y muestra válidos. */
    bool poll(uint32_t now_us, uint8_t& channel, int16_t& sample);
    /** Errores acumulados de transacción I2C. */
    uint32_t errors() const;
private:
    bool write_config();
    bool read_register(uint8_t reg, uint16_t& value);
    TwoWire& wire_;
    uint8_t address_, channels_, channel_ = 0;
    enum class State { Start, Wait, Read, Backoff };
    State state_ = State::Start;
    uint32_t since_ = 0, errors_ = 0;
};
