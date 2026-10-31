#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace gf {
/** @brief Generador de ruido del plugin con polos rosas adaptados a la frecuencia del host. */
class ModelDNoise {
public:
    /** @brief Precalcula los coeficientes; no cambia el estado ni consume memoria dinámica. */
    void prepare(float sample_rate) {
        constexpr std::array<float, 3> original_poles{.99765f, .96300f, .57000f};
        constexpr std::array<float, 3> original_gains{.0990460f, .2965164f, 1.0526913f};
        const float fs = std::isfinite(sample_rate) && sample_rate >= 8000 ? sample_rate : 44100.0f;
        for (size_t i = 0; i < poles_.size(); ++i) {
            poles_[i] = fs == 44100.0f ? original_poles[i] : std::pow(original_poles[i], 44100.0f / fs);
            gains_[i] = fs == 44100.0f ? original_gains[i] : original_gains[i] *
                std::sqrt((1.0f - poles_[i] * poles_[i]) / (1.0f - original_poles[i] * original_poles[i]));
        }
    }
    /** @brief Selecciona blanco (0) o rosa (1). */
    void set_color(float color) { color_ = std::clamp(color, 0.0f, 1.0f); }
    /** @brief Reinicia el generador y los tres polos. */
    void reset(uint32_t seed) { rng_ = seed ? seed : 0x1234567u; state_.fill(0); }
    /** @brief Escribe muestras de ruido. */
    void render(float* out, int count) {
        for (int n = 0; n < count; ++n) {
            rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5;
            const float white = float(int32_t(rng_)) * (1.0f / 2147483648.0f);
            for (size_t i = 0; i < state_.size(); ++i) state_[i] = poles_[i] * state_[i] + white * gains_[i];
            const float pink = (state_[0] + state_[1] + state_[2] + white * .1848f) * .22f;
            out[n] = white + color_ * (pink - white);
        }
    }
private:
    std::array<float, 3> poles_{.99765f, .96300f, .57000f};
    std::array<float, 3> gains_{.0990460f, .2965164f, 1.0526913f};
    std::array<float, 3> state_{};
    float color_ = 0;
    uint32_t rng_ = 0x1234567u;
};
}
