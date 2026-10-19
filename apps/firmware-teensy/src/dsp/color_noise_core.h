#pragma once

#include <stdint.h>

/**
 * @file color_noise_core.h
 * @brief Ruido con color continuo de blanco a rosa (código custom, lógica pura).
 *
 * Rosa: filtro "economy" de Paul Kellet (3 polos, ±0.5 dB de −3 dB/oct entre ~40 Hz y ~10 kHz a 44.1 kHz,
 * musicdsp.org #76). `color` mezcla blanco (0) y rosa (1): el ARP 2600 tiene color continuo; el Model D, switch
 * WHITE / PINK (theory/fidelidad-engines.md §11.2 y §11.4).
 */

namespace dsp {

class ColorNoiseCore {
public:
    void set_color(float c) { color_ = c < 0.0f ? 0.0f : c > 1.0f ? 1.0f : c; }
    void reset(uint32_t seed) { rng_ = seed ? seed : 0x1234567u; b0_ = b1_ = b2_ = 0.0f; }

    void render(float* out, int n) {
        for (int i = 0; i < n; ++i) {
            rng_ ^= rng_ << 13;
            rng_ ^= rng_ >> 17;
            rng_ ^= rng_ << 5;
            const float w = float(int32_t(rng_)) * (1.0f / 2147483648.0f);
            b0_ = 0.99765f * b0_ + w * 0.0990460f;
            b1_ = 0.96300f * b1_ + w * 0.2965164f;
            b2_ = 0.57000f * b2_ + w * 1.0526913f;
            const float pink = (b0_ + b1_ + b2_ + w * 0.1848f) * 0.22f;   // ~mismo RMS que el blanco
            out[i] = w + color_ * (pink - w);
        }
    }

private:
    float color_ = 0.0f;
    uint32_t rng_ = 0x1234567u;
    float b0_ = 0.0f, b1_ = 0.0f, b2_ = 0.0f;
};

}  // namespace dsp
