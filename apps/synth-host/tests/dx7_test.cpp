#include "audio.h"
#include "engines/dx7_6op_params.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void drain(host::Audio& audio){host::Ack a;while(audio.acknowledgements.pop(a))delete a.retired;host::Telemetry t;while(audio.telemetry.pop(t)){};}
}
int main(){try{
    using namespace host;
    initialize_catalogs();const int e=engine_index("dx7_6op");
    auto configs=initial_configs();for(auto& c:configs)c=defaults(e);
    auto audio=std::make_unique<Audio>(configs);
    std::array<std::array<float,128>,10> data{};std::array<float*,10> outputs{};
    for(size_t i=0;i<10;++i)outputs[i]=data[i].data();
    const auto& metadata=catalog(e);
    // Check every adapter field against the actual firmware getter/setter, including independent enable bits.
    auto c=defaults(e);auto reference=c.six;
    for(size_t i=0;i<gf::dx7six::param_count;++i){
        const auto& q=gf::dx7six::params[i];
        const auto& p=metadata[param_index(e,q.id)];
        require(p.page==q.page&&p.bank==q.bank&&p.slot==q.slot,"firmware/host grouping");
        for(float v:{float(q.minimum)-100.f,float(q.maximum)+100.f,(float(q.minimum)+q.maximum)*.5f}){
            const float applied=p.write(c,v);float expected;
            require(gf::dx7six::set_param(reference,q.id,v)&&gf::dx7six::get_param(reference,q.id,expected),"firmware setter");
            require(applied==expected&&c.six.vced==reference.vced&&c.six.enabled==reference.enabled,"host clamping differs from descriptor");
        }
    }
    double energy=0,wet_difference=0;
    auto wet_config=defaults(e);benchmark_patch(wet_config);auto wet=make_engine(wet_config);wet->note(60,100.f/127);
    for(unsigned block=0;block<400;++block){
        Midi events[3]{};unsigned count=0;
        if(block==0)events[count++]={17,0x90,60,100};
        if(block==20)events[count++]={0,0xb0,64,127};
        if(block==40)events[count++]={0,0x80,60,0};
        if(block==70)events[count++]={13,0xe0,0,80};
        if(block==110)events[count++]={0,0xb0,64,0};
        audio->process(128,outputs,events,count);drain(*audio);
        for(unsigned f=0;f<128;++f){
            for(const auto& channel:data)require(std::isfinite(channel[f]),"DX7 NaN/Inf");
            if(block==0&&f<17)require(data[0][f]==0,"timestamp ignored");
            if(block==100)energy+=data[0][f]*data[0][f];
            float l,r;wet->frame(l,r);require(std::isfinite(l)&&std::isfinite(r),"wet DX7 nonfinite");
            if(block>5&&block<20)wet_difference+=std::abs(l*.25f-data[0][f]);
            require(data[8][f]==data[0][f],"DX7 part/mix routing");
        }
    }
    require(energy>1e-9,"sustain did not hold DX7 note");require(wet_difference>1e-3,"shared FX bus has no effect");
    const Midi ccs[]={{0,0xb0,14,127},{0,0xb0,15,127},{0,0xb0,16,127},{0,0xb0,19,127},{0,0xb0,71,127},{0,0xb0,74,0}};
    audio->process(128,outputs,ccs,6);drain(*audio);
    const auto& active=audio->identity(0)->config;
    for(const char* id:{"drift","grit","pocket","move"})require(metadata[param_index(e,id)].read(active)==100,"DX7 macro CC");
    require(metadata[param_index(e,"cutoff")].read(active)==20&&metadata[param_index(e,"resonance")].read(active)==1,"DX7 filter CC");
    // Each voice macro must affect audio, while the user-visible VCED remains raw.
    for(int slot:{0,1,2}) {
        auto dry_config=defaults(e);dry_config.six.vced[134]=4;dry_config.six.vced[135]=1;
        for(int op=0;op<6;++op)dry_config.six.vced[op*21+16]=80;
        auto macro_config=dry_config;
        macro_config.layer.a.patch.groove[4][slot]=100;
        macro_config.layer.a.patch.groove[4][5]=100;
        macro_config.layer.a.patch.groove[4][6]=100;
        auto dry=make_engine(dry_config),macro=make_engine(macro_config);
        dry->note(60,100.f/127);macro->note(60,100.f/127);
        double difference=0;
        for(int sample=0;sample<12000;++sample){float a,b,c,d;dry->frame(a,b);macro->frame(c,d);require(std::isfinite(c)&&std::isfinite(d),"macro nonfinite");difference+=std::abs(a-c);}
        require(difference>.001,"DX7 macro has no audible effect");
        require(macro->config.six.vced==dry->config.six.vced,"macro changed saved VCED");
    }
    // At full MIDI bend the default sine must rise by two semitones, not the core's native three.
    int crossings[2]={};
    for(int bent=0;bent<2;++bent){
        auto voice=make_engine(defaults(e));voice->bend(bent?16383:8192);voice->note(60,100.f/127);
        float previous=0;
        for(int sample=0;sample<60000;++sample){float l,r;voice->frame(l,r);if(sample>=12000&&previous<0&&l>=0)++crossings[bent];previous=l;}
    }
    require(crossings[0]>200&&std::abs(double(crossings[1])/crossings[0]-std::exp2(2./12))<.01,"host bend is not two semitones");
    require(!audio->fault.load(),"DX7 audio fault");
    std::cout<<"DX7: all 151 firmware descriptors, clamp/round, note timestamps, sustain, bend, six CCs, shared FX and finite non-silent render: PASS\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
