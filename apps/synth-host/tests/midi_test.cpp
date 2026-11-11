#include "midi.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>

using namespace host;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void expect(uint32_t first,uint32_t second,uint8_t status,uint8_t a,uint8_t b) {
    const uint32_t words[]={first,second};Midi event;
    require(decode_ump(words,(first>>28)==2?4:8,37,event)==((first>>28)==2?4:8),"packet length");
    require(event.frame==37&&event.status==status&&event.a==a&&event.b==b,"converted MIDI value/timestamp");
}
static void ignored(uint32_t first,uint32_t second=0) {
    const uint32_t words[]={first,second,0x20903c7f,0x20903c7f};Midi event{99,0x90,60,127};
    require(decode_ump(words,sizeof(words),37,event)>0&&event.status==0,"ignored packet emitted a note");
}
int main() {
    try {
        expect(0x20903c7f,0,0x90,60,127);expect(0x21813c40,0,0x81,60,64);
        expect(0x2f923c00,0,0x92,60,0); // MIDI1 zero velocity remains a release.
        expect(0x20b34a40,0,0xb3,74,64);expect(0x20e00040,0,0xe0,0,64);
        expect(0x20c32a00,0,0xc3,42,0);expect(0x20d17f00,0,0xd1,127,0);
        expect(0x40903c00,0xffff0000,0x90,60,127);
        expect(0x4f813c00,0x80000000,0x81,60,64);
        expect(0x40923c00,0,0x92,60,1);expect(0x40923c00,0x01ff0000,0x92,60,1);
        expect(0x40823c00,0,0x82,60,0);
        expect(0x40990f01,0x80001234,0x99,15,64); // Attributes stripped; drum channel preserved.
        struct Scale {uint32_t input;uint8_t value,bend_lsb;};
        for(const auto& v:{Scale{0,0,0},Scale{0x80000000,64,0},Scale{0xffffffff,127,127}}) {
            expect(0x40b34a00,v.input,0xb3,74,v.value);expect(0x40d10000,v.input,0xd1,v.value,0);
            expect(0x40e20000,v.input,0xe2,v.bend_lsb,v.value);
        }
        expect(0x40e00000,0x00040000,0xe0,1,0); // Bend uses all 14 bits, LSB first.
        expect(0x40c30000,0x2a000000,0xc3,42,0);
        expect(0x40c30001,0x7f007f7f,0xc3,127,0); // Bank valid, deliberately not applied.
        ignored(0x00000000);ignored(0x10f80000);ignored(0x30120102);ignored(0x50120102);
        ignored(0xd0000000);ignored(0xf0000000);
        for(unsigned status=0;status<16;++status)
            if(status!=8&&status!=9&&status!=11&&status!=12&&status!=13&&status!=14)
                ignored(0x40003c00|(status<<20),0xffffffff);
        ignored(0x20703c7f);ignored(0x2090807f);ignored(0x20903c80);ignored(0x20c00001);
        ignored(0x40908000);ignored(0x40b08000);ignored(0x40b04a01);ignored(0x40e00001);
        ignored(0x40d00100);ignored(0x40c00002);ignored(0x40c00100);ignored(0x40c00000,0x80000000);
        Midi event;
        const uint32_t truncated[]={0x40903c00};
        require(decode_ump(truncated,4,0,event)==0&&event.status==0,"truncated MIDI2");
        for(size_t n=0;n<4;++n)require(decode_ump(truncated,n,0,event)==0,"short UMP word");
        require(decode_ump(nullptr,8,0,event)==0,"null UMP");
        const uint32_t long_packet[]={0x50000000,0,0,0};
        require(decode_ump(long_packet,12,0,event)==0,"truncated 128-bit packet");
        const uint32_t reserved_packet[]={0xb0000000,0};
        require(decode_ump(reserved_packet,8,0,event)==0,"truncated 96-bit reserved packet");
        std::array<uint8_t,9> unaligned{};const uint32_t packet[]={0x40913c00,0x80000000};
        std::memcpy(unaligned.data()+1,packet,8);
        require(decode_ump(unaligned.data()+1,8,23,event)==8&&event.status==0x91&&event.b==64,"unaligned native words");
        require(decode_ump(unaligned.data(),7,0,event)==0,"partial trailing word");
        const uint8_t on[]={0x90,60,0},off[]={0x80,60,64},program[]={0xc3,42},pressure[]={0xd1,64};
        require(decode_midi1(on,3,19,event)&&event.b==0&&event.frame==19,"raw MIDI1 zero velocity");
        require(decode_midi1(off,3,0,event)&&event.status==0x80,"raw note off");
        const uint8_t cc[]={0xb3,74,64},bend[]={0xe1,127,127};
        require(decode_midi1(cc,3,0,event)&&event.status==0xb3&&event.b==64,"raw CC");
        require(decode_midi1(bend,3,0,event)&&event.status==0xe1&&event.a==127&&event.b==127,"raw bend");
        require(decode_midi1(program,2,0,event)&&event.a==42&&event.b==0,"raw program");
        require(decode_midi1(pressure,2,0,event)&&event.a==64,"raw pressure");
        require(!decode_midi1(on,2,0,event)&&!decode_midi1(on,4,0,event),"invalid MIDI1 size");
        const uint8_t bad[]={0x90,128,60},running[]={60,100},system[]={0xf8,0};
        require(!decode_midi1(bad,3,0,event)&&!decode_midi1(running,2,0,event)&&!decode_midi1(system,2,0,event),"bad MIDI1");
        // Walk past an ignored 128-bit SysEx containing words that look like notes.
        const uint32_t stream[]={0x50900000,0x20903c7f,0x20903c7f,0x20903c7f,0x21993c00,0x4f990f00,0xffff0000};
        size_t offset=0;unsigned notes=0;
        while(offset<sizeof(stream)) {
            const auto consumed=decode_ump(reinterpret_cast<const uint8_t*>(stream)+offset,sizeof(stream)-offset,29,event);
            require(consumed>0,"mixed stream stalled");offset+=consumed;
            if(event.status){++notes;require(event.frame==29&&event.status==0x99,"mixed stream order/channel");}
        }
        require(notes==2&&event.a==15&&event.b==127,"SysEx payload parsed as notes");
        for(unsigned channel=0;channel<16;++channel) {
            require(routed_midi({0,uint8_t(0x90|channel),60,100})==(channel<4||channel==9),"channel mapping");
            require(!routed_midi({0,uint8_t(0xc0|channel),42,0}),"program unexpectedly routed");
        }
        std::cout<<"MIDI1/UMP types 2/4: notes, velocity zero, CC, bend, program, pressure, groups, lengths, ignored types, routing OK\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
