#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// The two-tap circular delay below is adapted from Teensy Audio Library effect_chorus.cpp.
// Copyright (c) 2014, Pete (El Supremo). MIT license; full text: tools/gemelo-baseline/PJRC-LICENSE.txt.

namespace gf {
/** @brief Adaptación portátil del chorus PJRC y mezcla de PhaseChorus Studio. */
class PhaseChorusPreview {
public:
    /** @brief Inicializa la tabla del waveshaper usada por el firmware. */
    void prepare() {
        for (unsigned i = 0; i < table_.size(); ++i) {
            const float x = float(i) / 128.0f - 1.0f;
            table_[i] = int16_t(32767.0f * std::tanh(1.5f * x) / std::tanh(1.5f));
        }
        reset();
    }
    /** @brief Limpia el estado de la línea sin reconstruir la tabla. */
    void reset() { delay_.fill(0); index_ = 0; p1_ = p2_ = p3_ = 0.0f; wet_ = mix_ * depth_; }
    /** @brief Ajusta profundidad, velocidad, mezcla y activación. */
    void set(bool enabled, float depth, float rate, float mix) {
        enabled_ = enabled;
        depth_ = std::clamp(depth, 0.0f, 1.0f);
        rate_ = std::clamp(rate, 0.1f, 10.0f);
        mix_ = std::clamp(mix, 0.0f, 1.0f);
        wet_ = mix_ * depth_;
    }
    /** @brief Avanza la mezcla modulada por el reloj de control. */
    void tick(float dt) {
        p1_ += rate_ * dt; p2_ += rate_ * 0.743f * dt; p3_ += rate_ * 1.329f * dt;
        if (p1_ > 1.0f) p1_ -= 1.0f;
        if (p2_ > 1.0f) p2_ -= 1.0f;
        if (p3_ > 1.0f) p3_ -= 1.0f;
        const float lfo = (std::sin(6.28318530f * p1_) + 0.08f * std::sin(6.28318530f * p2_) +
                           0.04f * std::sin(6.28318530f * p3_)) / 1.12f;
        wet_ = std::clamp(mix_ * depth_ * (1.0f + 0.3f * lfo), 0.0f, 1.0f);
    }
    /** @brief Procesa una muestra conservando el estado del delay durante el bypass. */
    float process(float input) {
        const auto pcm = int16_t(std::clamp(input, -1.0f, 0.9999695f) * 32768.0f);
        const unsigned x = unsigned(int(pcm) + 32768);
        const unsigned i = x >> 8;
        const int a = table_[i], b = table_[i + 1];
        const int16_t shaped = int16_t(a + ((b - a) * int(x & 255u) >> 8));
        index_ = (index_ + 1) % delay_.size();
        delay_[index_] = shaped;
        const auto other = (index_ + 1) % delay_.size();
        const float chorus = float((int(shaped) + int(delay_[other])) / 2) / 32768.0f;
        return enabled_ ? std::clamp(input * (1.0f - 0.5f * mix_) + wet_ * chorus, -1.0f, 1.0f) : input;
    }
private:
    std::array<int16_t, 600> delay_{};
    std::array<int16_t, 257> table_{};
    size_t index_ = 0;
    float p1_ = 0, p2_ = 0, p3_ = 0, wet_ = 0, depth_ = 0, rate_ = 0.5f, mix_ = 0;
    bool enabled_ = false;
};
}
