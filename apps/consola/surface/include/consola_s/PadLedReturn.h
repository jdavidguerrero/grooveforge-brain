#pragma once
#include <stdint.h>

/** @brief Custom, loop-only LED23 renderer. No hardware, allocation or audio dependency. */
class PadLedReturn {
    struct Pad {
        uint8_t rgb[3]{}, flags=0, level=0, serial=255, flash=0, release=0;
        uint32_t hit_at=0, release_at=0;
    } pads_[32];
    uint8_t color_[3]{}, output_[96]{}, flags_=0, level_=0, serial_=0;
    uint8_t mode_=0, brightness_=28, transport_=0, beat_=255;
    bool extended_=false, animations_=true, dirty_=true, rendered_=false;
    uint32_t heartbeat_at_=0, sweep_at_=0, beat_at_=0, rendered_at_=0;
public:
    static constexpr uint32_t FRAME_MS=34, FADE_MS=180, HEARTBEAT_MS=1500;
    enum Flag : uint8_t {SELECTED=1,DIM=2,HELD=4,ROOT=8,PLAYHEAD=16,HIT=32};
    /** @brief Receive RGB legacy or explicit LED23 channel16; now is monotonic milliseconds. */
    bool receive(uint8_t channel,uint8_t type,uint8_t first,uint8_t second,uint32_t now=0);
    /** @brief Render at most once/34ms, with no catch-up. Returns true when evaluated. */
    bool render(uint32_t now);
    /** @brief Exact 96 RGB7 bytes for unchanged M4 CMD_LED_GRID_UPDATE. */
    const uint8_t* colors() const {return output_;}
    /** @brief Re-send the complete current frame on M4 reconnect. */
    void reconnect() {dirty_=true;}
    /** @brief Clear only when the entire nonblocking UART frame was accepted. */
    void sent() {dirty_=false;}
    /** @brief Pending frame differs or a reconnect requires a full snapshot. */
    bool dirty() const {return dirty_;}
};
