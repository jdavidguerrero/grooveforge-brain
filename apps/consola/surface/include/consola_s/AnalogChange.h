#pragma once
#include <stdint.h>

/** EMA α=1/4 y deadband en unidades MIDI respecto al último envío. */
class AnalogChange {
public:
    /** Rango ADC positivo y deadband MIDI (1–127). */
    explicit AnalogChange(uint16_t maximum = 4095, uint8_t deadband = 2);
    /** Actualiza filtro; devuelve true y valor MIDI solo si corresponde enviar. */
    bool sample(int32_t raw, uint8_t& midi);
private:
    uint16_t maximum_;
    uint8_t deadband_, last_ = 0;
    int32_t filtered_q8_ = 0;
    bool initialized_ = false;
};
