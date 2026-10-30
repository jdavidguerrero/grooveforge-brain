#pragma once

#include <cstdint>
#include <cmath>

uint32_t millis();
inline void __disable_irq() {}
inline void __enable_irq() {}
inline void AudioNoInterrupts() {}
inline void AudioInterrupts() {}
using std::isfinite;
