#include "audio.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
using namespace host;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Buffers {
    std::array<std::array<float,128>,10> samples{};
    std::array<float*,10> outputs{};
    Buffers(){for(size_t i=0;i<10;++i)outputs[i]=samples[i].data();}
};
void drain(Audio& audio){Ack ack;while(audio.acknowledgements.pop(ack))delete ack.retired;Telemetry t;while(audio.telemetry.pop(t)){};}
void queue_test(){
    Spsc<uint64_t,64> queue;
    constexpr uint64_t n=200000;
    std::thread producer([&]{for(uint64_t i=0;i<n;++i)while(!queue.push(i))std::this_thread::yield();});
    bool ordered=true;
    for(uint64_t i=0;i<n;++i){uint64_t value;while(!queue.pop(value))std::this_thread::yield();ordered&=value==i;}
    producer.join();require(ordered,"SPSC changed order/data");require(queue.free_slots()==63,"SPSC capacity");
}
void routing_test(){
    auto configs=initial_configs();
    auto loud=std::make_unique<Audio>(configs),quiet=std::make_unique<Audio>(configs);
    Buffers a,b;
    for(unsigned p=0;p<4;++p) {
        loud->commands.push({Operation::Level,0,p,0,p==0?0.f:-60.f,nullptr});
        quiet->commands.push({Operation::Level,0,p,0,p==0?-6.f:-60.f,nullptr});
        if(p){loud->commands.push({Operation::Mute,0,p,0,1,nullptr});quiet->commands.push({Operation::Mute,0,p,0,1,nullptr});}
    }
    const Midi on{64,0x90,48,100};
    loud->process(128,a.outputs,&on,1);quiet->process(128,b.outputs,&on,1);
    for(unsigned i=0;i<64;++i)require(a.samples[0][i]==0,"note played before timestamp");
    double energy=0;
    for(int block=0;block<40;++block){
        for(unsigned f=0;f<128;++f){energy+=double(a.samples[0][f])*a.samples[0][f];require(std::abs(a.samples[0][f]*std::pow(10.f,-6.f/20)-b.samples[0][f])<1e-6f,"level gain");require(a.samples[0][f]==a.samples[8][f],"mix routing");}
        loud->process(128,a.outputs);quiet->process(128,b.outputs);drain(*loud);drain(*quiet);
    }
    require(energy>1e-8,"note did not sound");
    quiet->commands.push({Operation::Mute,100,0,0,1,nullptr});
    quiet->process(128,b.outputs);
    for(const auto& channel:b.samples)for(float v:channel)require(v==0,"mute leaked audio");
    // Test the actual CC fields, channel isolation and both HYBRID filter halves.
    const Midi controls[]={{0,0xb0,14,127},{0,0xb1,15,127},{0,0xb2,16,127},{0,0xb3,19,127},{0,0xb3,71,127},{0,0xb3,74,0},{0,0xb4,14,127}};
    loud->process(128,a.outputs,controls,7);
    require(loud->identity(0)->config.layer.a.patch.groove[4][0]==100,"CC14 DRIFT");
    require(loud->identity(1)->config.layer.a.patch.groove[4][1]==100,"CC15 GRIT");
    require(loud->identity(2)->config.layer.a.patch.groove[4][2]==100,"CC16 POCKET");
    const auto& layer=loud->identity(3)->config.layer;
    require(layer.a.patch.groove[4][7]==100,"CC19 MOVE");
    require(layer.a.reso_norm==1&&layer.b.reso_norm==1,"CC71 HYBRID resonance");
    require(layer.a.cutoff_hz==20&&layer.b.cutoff_hz==20,"CC74 log lower endpoint");
    require(loud->identity(1)->config.layer.a.patch.groove[4][0]==0,"channel 5 leaked to part 2");
    drain(*loud);drain(*quiet);
}
void feedback_test(){
    auto audio=std::make_unique<Audio>(initial_configs());Buffers buffers;
    for(unsigned i=0;i<220;++i){
        std::array<Midi,24> midi{};unsigned count=0;
        for(unsigned p=0;p<4;++p)for(uint8_t cc:{14,15,16,19,71,74})midi[count++]={0,uint8_t(0xb0|p),cc,uint8_t(i%2?1:60)};
        audio->process(128,buffers.outputs,midi.data(),count);
    }
    require(audio->dropped.load()>0,"feedback overflow not exercised");
    // No ACK may be lost even when parameter feedback fills the shared FIFO.
    audio->commands.push({Operation::Note,999,0,48,0,nullptr});
    audio->process(128,buffers.outputs);drain(*audio);
    audio->process(128,buffers.outputs);
    bool found=false;Ack ack;
    while(audio->acknowledgements.pop(ack)){if(!ack.parameter&&ack.token==999)found=true;delete ack.retired;}
    require(found,"full feedback FIFO lost command ACK");require(!audio->fault.load(),"overflow caused audio fault");
}
}
int main(){try{queue_test();routing_test();feedback_test();std::cout<<"Audio: SPSC 200000 ordered transfers, note timestamps, level/mute/mix, channels/CC and feedback backpressure: PASS\n";}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
