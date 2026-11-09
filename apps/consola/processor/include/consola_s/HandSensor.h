#pragma once
#include <stdint.h>

/** Proximidad Sharp filtrada; ventana de voltaje calibrada estima presencia. */
class HandSensor {
public:
    /** Cuentas ADS a 30/4 cm y margen de histéresis, PGA ±4,096 V. */
    struct Calibration { int32_t far; int32_t near; int32_t hysteresis; };
    /** Eventos independientes de CC y presencia; value=0 al salir. */
    struct Event { bool cc_changed; bool presence_changed; bool present; uint8_t value; };
    /** Crea sensor con extremos iniciales ajustables por unidad. */
    explicit HandSensor(Calibration calibration = {3200,21200,400});
    /** Recibe lecturas ADS; procesa a lo sumo una cada 40 ms, incluyendo wrap. */
    Event sample(int32_t raw, uint32_t now_ms);
    /** Libera presencia tras 200 ms sin datos ADC frescos. */
    Event expire(uint32_t now_ms);
private:
    Event update_presence(bool candidate, uint8_t value);
    Calibration calibration_;
    int32_t filtered_q8_ = 0;
    uint32_t last_update_ = 0, last_seen_ = 0;
    uint8_t last_midi_ = 0, confirmations_ = 0;
    bool seen_ = false, updated_ = false, filtered_ = false, present_ = false;
};
