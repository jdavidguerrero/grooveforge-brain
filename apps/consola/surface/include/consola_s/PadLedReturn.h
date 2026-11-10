#pragma once
#include <stdint.h>
#include <string.h>

/** @brief Loop-only RGB return for SYNTH21/DRUM22, independent of channel-1 music. */
class PadLedReturn {
    uint8_t color_[3] = {};
    uint8_t pads_[96] = {};
    bool dirty_ = true;
public:
    /** @brief Accept channel16 CC20..22 then note48..79; other messages are ignored. */
    bool receive(uint8_t channel,uint8_t type,uint8_t first,uint8_t second) {
        if(channel!=16)return false;
        if(type==0xb0 && first>=20 && first<=22){
            // Bound brightness to <=24.4%, regardless of a malformed GUI value.
            color_[first-20]=second>31?31:second;
            return true;
        }
        if((type==0x90 || type==0x80) && first>=48 && first<=79){
            uint8_t next[3]={};
            if(type==0x90 && second)memcpy(next,color_,3);
            auto* pad=pads_+(first-48)*3;
            if(memcmp(next,pad,3)){memcpy(pad,next,3);dirty_=true;}
            return true;
        }
        return false;
    }
    /** @brief Exact 32-pad 7-bit payload for existing CMD_LED_GRID_UPDATE (0xA0). */
    const uint8_t* colors() const {return pads_;}
    /** @brief Set after reconnect to restore every pad, including black cells. */
    void reconnect() {dirty_=true;}
    /** @brief Clear only after the complete nonblocking UART frame was accepted. */
    void sent() {dirty_=false;}
    /** @brief Coalesces changes until the next bounded 30Hz update. */
    bool dirty() const {return dirty_;}
};
