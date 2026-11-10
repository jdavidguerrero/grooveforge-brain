#include "consola_s/RelativeEncoder.h"
uint8_t RelativeEncoder::encode(int32_t detents) {
    if (detents > 63) detents = 63;
    if (detents < -63) detents = -63;
    return uint8_t(detents) & 0x7f;
}
uint8_t RelativeEncoder::update(int32_t delta) {
    remainder_ += delta;
    int64_t detents = remainder_ / 4;
    if (detents > 63) detents = 63;
    if (detents < -63) detents = -63;
    remainder_ -= detents * 4;
    return encode(int32_t(detents));
}
