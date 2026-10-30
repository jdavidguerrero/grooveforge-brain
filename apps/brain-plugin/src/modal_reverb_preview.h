#pragma once

#include "dsp/modal_modes.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace gf {
/** @brief Aproximación flotante de los seis biquads de ModalReverb del Brain. */
class ModalReverbPreview {
public:
    /** @brief Configura frecuencia del host y limpia las colas. */
    void prepare(float fs) { fs_ = fs; reset(); configure(material_, size_, decay_); }
    /** @brief Limpia el estado resonante sin cambiar parámetros. */
    void reset() { for (auto& f : filters_) f.x1 = f.x2 = f.y1 = f.y2 = 0.0; }
    /** @brief Ajusta material, tamaño y multiplicador de T60. */
    void configure(uint8_t material, uint8_t size, float decay) {
        material_ = std::min<uint8_t>(material, 3); size_ = std::min<uint8_t>(size, 2);
        decay_ = std::clamp(decay, 0.2f, 4.0f);
        const float size_multiplier = size_ == 0 ? 0.3f : size_ == 2 ? 2.5f : 1.0f;
        for (unsigned i = 0; i < filters_.size(); ++i) {
            auto& f = filters_[i];
            const auto& mode = dsp::MODAL_MODES[material_][i];
            const float t60 = mode.t60Base * size_multiplier * decay_;
            const double q = std::min(150.0f, 14.0f + 22.0f * std::sqrt(t60));
            const double omega = 300.0 * mode.freqRatio * 6.283185307179586 / fs_;
            const double alpha = std::sin(omega) / (2.0 * q), scale = 1.0 / (1.0 + alpha);
            f.b0 = alpha * scale; f.a1 = -2.0 * std::cos(omega) * scale; f.a2 = (1.0 - alpha) * scale;
            f.gain = 0.30f + 0.70f * (t60 / (t60 + 1.8f));
        }
    }
    /** @brief Devuelve la suma húmeda de los seis resonadores para una muestra. */
    float process(float input) {
        double wet = 0.0;
        for (auto& f : filters_) {
            const double y = f.b0 * (double(input) - f.x2) - f.a1 * f.y1 - f.a2 * f.y2;
            f.x2 = f.x1; f.x1 = input; f.y2 = f.y1; f.y1 = y;
            wet += y * f.gain;
        }
        return float(wet * 0.5);
    }
private:
    struct Filter { double b0 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0; float gain = 0; };
    std::array<Filter, 6> filters_{};
    float fs_ = 44100.0f, decay_ = 1.0f;
    uint8_t material_ = 0, size_ = 1;
};
}
