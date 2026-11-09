#pragma once
#include <stdint.h>

/** Detector custom: una ventana de pico por golpe y liberación con histéresis. */
class FsrVelocity {
public:
    /** Calibración ADC por pad; release < threshold < maximum. */
    struct Calibration { uint16_t threshold; uint16_t maximum; uint16_t release; };
    /** Resultado de una muestra; velocity=0 para note-off. */
    struct Event { bool changed; bool on; uint8_t velocity; };
    /** Construye detector con calibración explícita y tiempos en microsegundos. */
    explicit FsrVelocity(Calibration calibration, uint32_t window_us = 4000,
                         uint32_t guard_us = 12000);
    /** Procesa una muestra monotónica (micros unsigned, permite wrap). */
    Event sample(uint16_t value, uint32_t now_us);
private:
    enum class State { Armed, Peak, Held, Guard };
    Calibration calibration_;
    uint32_t window_us_, guard_us_, attack_us_ = 0;
    uint16_t peak_ = 0;
    State state_ = State::Armed;
};
