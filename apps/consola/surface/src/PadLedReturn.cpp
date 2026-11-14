#include "consola_s/PadLedReturn.h"
#include <string.h>
#include <algorithm>

bool PadLedReturn::receive(uint8_t channel,uint8_t type,uint8_t first,uint8_t second,uint32_t now) {
    if(channel!=16 || second>127)return false;
    if(type==0xb0) {
        if(first>=20&&first<=22) {color_[first-20]=extended_?second:std::min<uint8_t>(31,second);return true;}
        if(first==23&&second==23) {
            extended_=true;mode_=0;flags_=level_=serial_=0;transport_=0;beat_=255;
            memset(color_,0,sizeof(color_));for(auto& p:pads_)p=Pad{};
            heartbeat_at_=sweep_at_=now;rendered_=false;return true;
        }
        if(!extended_)return false;
        switch(first) {
        case 24:if(second>2)return false;if(mode_!=second){mode_=second;sweep_at_=now;}return true;
        case 25:brightness_=std::min<uint8_t>(31,second);return true;
        case 26:animations_=second!=0;return true;
        case 27:transport_=second&7;return true;
        case 28:if(beat_!=second){beat_=second;beat_at_=now;}return true;
        case 29:if(second>63)return false;flags_=second;return true;
        case 30:level_=second;return true;
        case 31:serial_=second;return true;
        case 119:heartbeat_at_=now;return true;
        default:return false;
        }
    }
    if((type!=0x90&&type!=0x80)||first<48||first>79)return false;
    const unsigned index=first-48;
    const bool on=type==0x90&&second;
    if(!extended_) {
        uint8_t next[3]{};if(on)memcpy(next,color_,3);
        auto* out=output_+index*3;
        if(memcmp(next,out,3)){memcpy(out,next,3);dirty_=true;}
        return true;
    }
    auto& p=pads_[index];
    if(!on){p=Pad{};return true;}
    if((p.flags&HELD)&&!(flags_&HELD)&&mode_==1){p.release=p.level;p.release_at=now;}
    if((flags_&HIT)&&p.serial!=serial_) {p.flash=level_;p.hit_at=now;}
    if(flags_&DIM){p.flash=0;p.release=0;}
    memcpy(p.rgb,color_,3);p.flags=flags_;p.level=level_;p.serial=serial_;
    return true;
}

bool PadLedReturn::render(uint32_t now) {
    if(!extended_ || (rendered_&&uint32_t(now-rendered_at_)<FRAME_MS))return false;
    rendered_=true;rendered_at_=now;
    uint8_t next[96]{};
    const bool lost=mode_&&uint32_t(now-heartbeat_at_)>=HEARTBEAT_MS;
    const auto fade=[now](uint8_t peak,uint32_t at) -> unsigned {
        const uint32_t age=now-at;
        return age<FADE_MS?unsigned(peak)*(FADE_MS-age)/FADE_MS:0;
    };
    for(unsigned i=0;i<32;++i) {
        const auto& p=pads_[i];unsigned rgb[3]{};
        if(lost){rgb[0]=127;rgb[1]=65;}
        else if(mode_) {
            unsigned level=p.level;
            if((p.flags&HIT)&&!(p.flags&HELD))level=16;
            if(p.flags&ROOT)level=std::max(level,42u);
            if(p.flags&SELECTED)level=std::max(level,55u);
            if(animations_)level=std::max(level,std::max(fade(p.flash,p.hit_at),fade(p.release,p.release_at)));
            for(unsigned c=0;c<3;++c)rgb[c]=unsigned(p.rgb[c])*level/127;
            if((p.flags&PLAYHEAD)&&(transport_&1))for(auto& c:rgb)c=std::max(c,105u);
            const bool edge=i<8||i>=24||i%8==0||i%8==7;
            if((transport_&2)&&(p.flags&SELECTED))rgb[0]=std::max(rgb[0],75u);
            if((transport_&4)&&edge) {
                const unsigned phase=now%600;
                const unsigned pulse=animations_?25+unsigned(phase<300?phase:600-phase)/4:60;
                for(auto& c:rgb)c=std::max(c,pulse);
            }
            if(animations_&&(transport_&1)&&edge&&uint32_t(now-beat_at_)<100)
                for(auto& c:rgb)c=std::max(c,unsigned(80u*(100-uint32_t(now-beat_at_))/100));
            if(animations_&&uint32_t(now-sweep_at_)<480&&i%8==uint32_t(now-sweep_at_)/60)
                for(auto& c:rgb)c=std::max(c,45u);
            if(p.flags&DIM)for(auto& c:rgb)c/=6;
        }
        for(unsigned c=0;c<3;++c)next[i*3+c]=uint8_t(std::min(127u,rgb[c])*brightness_/127);
    }
    if(memcmp(output_,next,96)){memcpy(output_,next,96);dirty_=true;}
    return true;
}
