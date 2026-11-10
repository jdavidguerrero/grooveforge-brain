#pragma once
#include "ConsolaMap.h"

/** Pure active-low MCP debounce and note mapping, including hot-unplug release. */
class McpButtons {
public:
    struct Event { uint8_t note; bool on; };
    /** Allowed physical GPIO mask; A7/B7 are always ignored. */
    explicit McpButtons(uint8_t chip, uint16_t mask = 0x7f7f) : chip_(chip), mask_(mask & 0x7f7f) {}
    /** Stable for 5ms before publishing an edge; at most 14 events per sample. */
    uint8_t sample(uint16_t gpio, uint32_t now, Event (&events)[14]) {
        uint8_t count = 0;
        for (uint8_t i = 0; i < 14; ++i) {
            const uint16_t bit = 1u << consola::MCP_PINS[i];
            const uint8_t note = consola::mcp_note(chip_,i);
            if (!(mask_ & bit) || note == 0xff) continue;
            if ((gpio ^ candidate_) & bit) {
                candidate_ ^= bit;
                since_[i] = now;
            }
            if (((gpio ^ stable_) & bit) && uint32_t(now-since_[i]) >= 5000) {
                stable_ ^= bit;
                events[count++] = {note, !(gpio & bit)};
            }
        }
        return count;
    }
    /** Release held notes once on a failed read, reset debounce for reconnection. */
    uint8_t disconnect(Event (&events)[14]) {
        uint8_t count = 0;
        for (uint8_t i = 0; i < 14; ++i) {
            const uint16_t bit = 1u << consola::MCP_PINS[i];
            const uint8_t note = consola::mcp_note(chip_,i);
            if ((mask_ & bit) && note != 0xff && !(stable_ & bit)) events[count++] = {note,false};
        }
        candidate_ = stable_ = 0xffff;
        return count;
    }
private:
    uint8_t chip_;
    uint16_t mask_, candidate_ = 0xffff, stable_ = 0xffff;
    uint32_t since_[14] = {};
};
