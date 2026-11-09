#pragma once

// Estados lógicos de clips (para LEDs y lógica interna)
enum ClipState : uint8_t {
    CLIP_STATE_EMPTY     = 0x00,
    CLIP_STATE_STOPPED   = 0x01,
    CLIP_STATE_PLAYING   = 0x02,
    CLIP_STATE_QUEUED    = 0x03,
    CLIP_STATE_RECORDING = 0x04
};
