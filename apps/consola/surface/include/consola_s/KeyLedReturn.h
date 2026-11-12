#pragma once
#include <stdint.h>

/** @brief LED27 loop renderer and current model; custom, hardware independent. */
class KeyLedReturn {
public:
    static constexpr unsigned COUNT=26, BYTES=COUNT*3, FRAME_MS=34;
    static constexpr uint32_t LIMIT_UA=500000, IDLE_UA=COUNT*1000;
    enum State : uint8_t {OFF, DIM, ON, BLINK, PULSE};
    enum Palette : uint8_t {TEAL, VIOLET, BLUE, AMBER, WHITE};
    /** @brief Accept only channel16 LED27 controls plus shared brightness/beat. */
    bool receive(uint8_t channel,uint8_t type,uint8_t cc,uint8_t value,uint32_t now);
    /** @brief Evaluate at most once/34ms, without catching up dropped frames. */
    bool render(uint32_t now);
    /** @brief Final RGB8 PWM bytes, including brightness and hard current limit. */
    const uint8_t* colors() const {return output_;}
    /** @brief Conservative estimated LED current, including 1mA idle per LED. */
    uint32_t current_ua() const {return current_ua_;}
    /** @brief Begin timed breadboard cycle (0 restores Pi); rejects count>26. */
    bool bench(uint8_t count,uint32_t now);
    /** @brief Strict CDC grammar: @GF KEYLEDS followed by a decimal count0..26. */
    static bool parse_bench(const char* text,uint8_t& count);
    /** @brief Model current in microamps; RGB channels are linear PWM8. */
    static uint32_t estimate(const uint8_t* rgb);
    /** @brief Proportionally limit a full RGB8 frame to the 500mA LED budget. */
    static uint32_t limit(uint8_t* rgb);
private:
    struct Key {uint8_t state=OFF,palette=TEAL;} keys_[COUNT];
    uint8_t output_[BYTES]{};
    uint8_t index_=0,state_=OFF,palette_=TEAL,valid_=0,brightness_=28;
    uint8_t beat_=255,tap_=255,tempo_low_=122,bench_count_=0;
    uint16_t bpm_=122;
    bool enabled_=false,animations_=true,running_=false,rendered_=false,beat_seen_=false;
    uint32_t heartbeat_at_=0,beat_at_=0,tap_at_=0,rendered_at_=0,bench_at_=0;
    uint32_t current_ua_=IDLE_UA;
};
