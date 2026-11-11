#include "midi.h"
#include <algorithm>
#include <cstring>

namespace host {
bool decode_midi1(const void* data,size_t size,uint32_t frame,Midi& event) noexcept {
    event={};
    if(!data||size<2)return false;
    const auto* m=static_cast<const uint8_t*>(data);
    const unsigned type=m[0]&0xf0u;
    if(type<0x80||type>0xe0)return false;
    const size_t length=(type==0xc0||type==0xd0)?2:3;
    if(size!=length||m[1]>127||(length==3&&m[2]>127))return false;
    event={frame,m[0],m[1],length==3?m[2]:uint8_t(0)};
    return true;
}

size_t decode_ump(const void* data,size_t size,uint32_t frame,Midi& event) noexcept {
    event={};
    if(!data||size<4||size%4)return 0;
    uint32_t first=0;std::memcpy(&first,data,4);
    const unsigned type=first>>28;
    // Reserved types still have defined lengths; never interpret their payload as a header.
    constexpr uint8_t WORDS[]={1,1,1,2,2,4,1,1,2,2,2,3,3,4,4,4};
    const size_t length=WORDS[type]*4;
    if(size<length)return 0;
    const auto status=uint8_t(first>>16);
    if(type==2) {
        const uint8_t bytes[]={status,uint8_t(first>>8),uint8_t(first)};
        const auto voice=status&0xf0u;
        const bool short_message=voice==0xc0||voice==0xd0;
        if(!short_message||bytes[2]==0)
            decode_midi1(bytes,short_message?2:3,frame,event);
    } else if(type==4) {
        uint32_t second=0;std::memcpy(&second,static_cast<const uint8_t*>(data)+4,4);
        const auto index=uint8_t(first>>8);
        const auto detail=uint8_t(first);
        switch(status&0xf0u) {
        case 0x80:case 0x90:
            if(index>127)break;
            event={frame,status,index,uint8_t(second>>25)};
            // MIDI2 velocity zero is a note-on; MIDI1 receivers treat it as note-off.
            if((status&0xf0u)==0x90)event.b=std::max(uint8_t(1),event.b);
            break;
        case 0xb0:
            if(index<=127&&detail==0)event={frame,status,index,uint8_t(second>>25)};
            break;
        case 0xc0:
            if(index==0&&(detail&~1u)==0&&(second&0x80ff8080u)==0)
                event={frame,status,uint8_t(second>>24),0};
            break;
        case 0xd0:
            if((first&0xffffu)==0)event={frame,status,uint8_t(second>>25),0};
            break;
        case 0xe0:
            if((first&0xffffu)==0) {
                const auto bend=second>>18;
                event={frame,status,uint8_t(bend&127u),uint8_t(bend>>7)};
            }
            break;
        default:break;
        }
    }
    return length;
}

bool routed_midi(const Midi& event) noexcept {
    const auto channel=event.status&15u;
    if((channel>=4&&channel!=9)||event.a>127||event.b>127)return false;
    const auto type=event.status&0xf0u;
    return type==0x80||type==0x90||type==0xb0||type==0xe0;
}
}
