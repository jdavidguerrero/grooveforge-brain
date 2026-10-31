#pragma once
#include <cstdint>

// Adapted from PJRC Audio/synth_whitenoise.cpp; copyright and permission notice:
// tools/gemelo-baseline/PJRC-LICENSE.txt.
namespace gf {
/** @brief Generador PCM16 equivalente al ruido blanco PJRC a amplitud unitaria. */
class PjrcWhiteNoise {
public:
    /** @brief Reinicia con una semilla positiva y acotada al estado de Park–Miller. */
    void reset(uint32_t seed) { state_ = seed > 0 && seed < 2147483647u ? seed : 1u; }
    /** @brief Avanza una muestra y devuelve el entero con signo de los 16 bits bajos. */
    int16_t next() {
        uint32_t hi = 16807u * (state_ >> 16);
        uint32_t lo = 16807u * (state_ & 0xffffu);
        lo += (hi & 0x7fffu) << 16;
        lo += hi >> 15;
        lo = (lo & 0x7fffffffu) + (lo >> 31);
        state_ = lo;
        const int32_t low = int32_t(lo & 0xffffu);
        return int16_t(low >= 32768 ? low - 65536 : low);
    }
private:
    uint32_t state_ = 1;
};
}
