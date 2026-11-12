#include "consola_s/KeyLedReturn.h"
#include <algorithm>
#include <cstring>

bool KeyLedReturn::receive(uint8_t channel,uint8_t type,uint8_t cc,uint8_t value,uint32_t now) {
    if(channel!=16 || type!=0xb0 || value>127)return false;
    if(cc==25){brightness_=std::min<uint8_t>(31,value);return true;}
    if(cc==26){if(value>1)return false;animations_=value;return true;}
    if(cc==102){
        if(value!=27)return false;
        enabled_=true;valid_=0;running_=false;beat_=tap_=255;beat_seen_=false;
        heartbeat_at_=tap_at_=now;
        for(auto& k:keys_)k=Key{};
        return true;
    }
    if(!enabled_)return false;
    switch(cc){
    case 103: if(value>=COUNT){valid_=0;return false;}index_=value;valid_|=1;return true;
    case 104: if(value>PULSE){valid_=0;return false;}state_=value;valid_|=2;return true;
    case 105: if(value>WHITE){valid_=0;return false;}palette_=value;valid_|=4;return true;
    case 106:{
        const bool ok=value==1&&valid_==7;
        if(ok)keys_[index_]={state_,palette_};
        valid_=0;return ok;
    }
    case 107:tempo_low_=value;return true;
    case 108:{
        const unsigned bpm=unsigned(value)*128+tempo_low_;
        if(bpm<30||bpm>240)return false;
        bpm_=bpm;return true;
    }
    case 109:if(tap_!=value){tap_=value;tap_at_=now;}return true;
    case 110:heartbeat_at_=now;return true;
    case 111:if(value>1)return false;running_=value;return true;
    case 28:if(beat_!=value){beat_=value;beat_at_=now;beat_seen_=true;}return true;
    default:return false;
    }
}

bool KeyLedReturn::parse_bench(const char* text,uint8_t& count){
    if(std::strncmp(text,"@GF KEYLEDS ",12))return false;
    const char* p=text+12;while(*p==' ')++p;
    if(*p<'0'||*p>'9')return false;
    unsigned value=0;
    while(*p>='0'&&*p<='9'){
        value=value*10+unsigned(*p++-'0');if(value>COUNT)return false;
    }
    while(*p==' ')++p;
    if(*p)return false;
    count=uint8_t(value);return true;
}

bool KeyLedReturn::bench(uint8_t count,uint32_t now){
    if(count>COUNT)return false;
    bench_count_=count;bench_at_=now;return true;
}

uint32_t KeyLedReturn::estimate(const uint8_t* rgb){
    uint32_t sum=0;for(unsigned i=0;i<BYTES;++i)sum+=rgb[i];
    return IDLE_UA+(sum*20000+254)/255;
}

uint32_t KeyLedReturn::limit(uint8_t* rgb){
    uint32_t sum=0;for(unsigned i=0;i<BYTES;++i)sum+=rgb[i];
    constexpr uint32_t available=(LIMIT_UA-IDLE_UA)*255/20000;
    if(sum>available)for(unsigned i=0;i<BYTES;++i)rgb[i]=uint32_t(rgb[i])*available/sum;
    return estimate(rgb);
}

bool KeyLedReturn::render(uint32_t now){
    if(rendered_&&uint32_t(now-rendered_at_)<FRAME_MS)return false;
    rendered_=true;rendered_at_=now;
    if(bench_count_&&uint32_t(now-bench_at_)>=60000)bench_count_=0;
    static constexpr uint8_t colors[][3]={{37,81,66},{150,40,255},{61,140,255},{233,173,80},{255,255,255}};
    const bool fresh=enabled_&&uint32_t(now-heartbeat_at_)<1500;
    const unsigned phase=uint32_t(now-tap_at_)%1000;
    for(unsigned i=0;i<COUNT;++i){
        unsigned level=0,palette=keys_[i].palette;
        if(bench_count_){
            if(i<bench_count_){level=255;palette=(uint32_t(now-bench_at_)/1000)%5;}
        }else if(fresh){
            switch(keys_[i].state){
            case DIM:level=32;break;
            case ON:level=255;break;
            case BLINK:level=!animations_||phase<250||(phase>=500&&phase<750)?255:32;break;
            case PULSE:level=animations_?32+(phase<500?phase:1000-phase)*223/500:255;break;
            default:break;
            }
            // TAP follows confirmed running beats; only the stopped preview extrapolates tempo.
            if(i==18&&keys_[i].state!=OFF){
                const bool beat=running_?(beat_seen_&&uint32_t(now-beat_at_)<80):
                    (uint32_t(now-tap_at_)%(60000/bpm_)<80);
                level=!animations_||beat?255:32;
            }
        }
        for(unsigned c=0;c<3;++c)output_[i*3+c]=unsigned(colors[palette][c])*level*brightness_/(255*127);
    }
    current_ua_=limit(output_);
    return true;
}
