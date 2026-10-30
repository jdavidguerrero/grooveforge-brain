#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

/* Adapted from Audio Library for Teensy 3.X, effect_freeverb.cpp.
 * Copyright (c) 2018, Paul Stoffregen, paul@pjrc.com.
 * Development of this audio library was funded by PJRC.COM, LLC by sales of
 * Teensy and Audio Adaptor boards. Please support PJRC's efforts to develop
 * open source software by purchasing Teensy or other PJRC products.
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

namespace gf {
/** @brief Adaptación de Freeverb estéreo de PJRC con memoria externa. */
class PlateReverbPreview {
public:
    /** @brief Muestras PCM16 necesarias para las líneas a la frecuencia indicada. */
    static size_t required_samples(float fs) {
        size_t count = 0;
        for (unsigned c = 0; c < 2; ++c) {
            for (auto n : COMBS) count += scaled(n + c * 23, fs);
            for (auto n : ALLPASS) count += scaled(n + c * 23, fs);
        }
        return count;
    }
    /** @brief Vincula y limpia el buffer previamente reservado por el host. */
    bool prepare(int16_t* memory, size_t count, float fs) {
        if (!memory || count < required_samples(fs)) return ready_ = false;
        memory_ = memory; count_ = required_samples(fs);
        size_t offset = 0;
        for (unsigned c = 0; c < 2; ++c) {
            for (unsigned i = 0; i < 8; ++i) bind(combs_[c][i], memory, offset, scaled(COMBS[i] + c * 23, fs));
            for (unsigned i = 0; i < 4; ++i) bind(allpass_[c][i], memory, offset, scaled(ALLPASS[i] + c * 23, fs));
        }
        ready_ = true; reset(); return true;
    }
    /** @brief Limpia las colas y el estado de filtros. */
    void reset() {
        if (!ready_) return;
        std::fill(memory_, memory_ + count_, int16_t(0));
        for (auto& channel : combs_) for (auto& line : channel) { line.index = 0; line.filter = 0; }
        for (auto& channel : allpass_) for (auto& line : channel) line.index = 0;
    }
    /** @brief Configura roomsize y damping normalizados de Freeverb. */
    void set(float room, float damping) {
        room = std::clamp(room, 0.0f, 1.0f); damping = std::clamp(damping, 0.0f, 1.0f);
        feedback_ = int(room * 9175.04f) + 22937;
        damp1_ = int(damping * 13107.2f); damp2_ = 32768 - damp1_;
    }
    /** @brief Procesa una muestra mono hacia dos salidas húmedas. */
    void process(float in, float& left, float& right) {
        if (!ready_) { left = right = 0; return; }
        const int16_t sample = int16_t(std::clamp(in, -1.0f, 0.9999695f) * 32768.0f);
        const int input = sat(int64_t(sample) * 8738, 17);
        float* outputs[] = {&left, &right};
        for (unsigned c = 0; c < 2; ++c) {
            int sum = 0;
            for (auto& line : combs_[c]) {
                const int value = line.data[line.index]; sum += value;
                line.filter = sat(int64_t(value) * damp2_ + int64_t(line.filter) * damp1_, 15);
                line.data[line.index] = int16_t(sat(input + sat(int64_t(line.filter) * feedback_, 15), 0));
                if (++line.index == line.length) line.index = 0;
            }
            int output = sat(int64_t(sum) * 31457, 17);
            for (auto& line : allpass_[c]) {
                const int value = line.data[line.index];
                line.data[line.index] = int16_t(output + (value >> 1));
                output = sat(value - output, 1);
                if (++line.index == line.length) line.index = 0;
            }
            *outputs[c] = float(sat(output * 30, 0)) / 32768.0f;
        }
    }
private:
    struct Line { int16_t* data = nullptr; uint32_t length = 0, index = 0; int filter = 0; };
    static constexpr std::array<unsigned, 8> COMBS{1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static constexpr std::array<unsigned, 4> ALLPASS{556, 441, 341, 225};
    static uint32_t scaled(unsigned n, float fs) { return std::max(1u, uint32_t(std::lround(n * fs / 44100.0f))); }
    static void bind(Line& line, int16_t* memory, size_t& offset, uint32_t length) {
        line.data = memory + offset; line.length = length; offset += length;
    }
    static int sat(int64_t value, unsigned shift) { return int(std::clamp<int64_t>(value >> shift, -32768, 32767)); }
    std::array<std::array<Line, 8>, 2> combs_{};
    std::array<std::array<Line, 4>, 2> allpass_{};
    int16_t* memory_ = nullptr;
    size_t count_ = 0;
    int feedback_ = 27524, damp1_ = 6553, damp2_ = 26215;
    bool ready_ = false;
};
}
