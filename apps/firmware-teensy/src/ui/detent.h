#pragma once

#include <stdint.h>

/**
 * @file detent.h
 * @brief Cuenta de detents de un encoder con histéresis (código custom, lógica pura).
 *
 * Un detent cuenta solo cuando la cuenta cruda se aleja `ppd` pulsos completos del último detent: el vaivén de
 * ±1 pulso del contacto en reposo (medido en el hardware, 2026-10-19) nunca mueve el valor. Con la división simple
 * (raw >> 1), ese vaivén en el borde de un detent subía y bajaba el parámetro un paso ("rebote").
 */
namespace studio {

class DetentCounter {
public:
    explicit DetentCounter(int32_t ppd = 2) : ppd_(ppd) {}

    /// Lee la cuenta cruda y devuelve la posición en detents (acumulada).
    int32_t update(int32_t raw) {
        while (raw - anchor_ >= ppd_) {
            anchor_ += ppd_;
            ++pos_;
        }
        while (raw - anchor_ <= -ppd_) {
            anchor_ -= ppd_;
            --pos_;
        }
        return pos_;
    }

    int32_t pos() const { return pos_; }

private:
    int32_t ppd_;
    int32_t anchor_ = 0;
    int32_t pos_ = 0;
};

}  // namespace studio
