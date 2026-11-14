#include "audio.h"
#include "drum_files.h"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <unistd.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
thread_local bool realtime=false;
std::atomic<unsigned> violations{0};
void* operator new(std::size_t n) {if(realtime)violations.fetch_add(1);if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{if(realtime)violations.fetch_add(1);std::free(p);}
void operator delete[](void* p) noexcept{::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept{::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept{::operator delete(p);}
void* operator new(std::size_t n,std::align_val_t alignment) {
    if(realtime)violations.fetch_add(1);
    void* p=nullptr;if(posix_memalign(&p,size_t(alignment),n?n:1)==0)return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n,std::align_val_t alignment){return ::operator new(n,alignment);}
void operator delete(void* p,std::align_val_t) noexcept{if(realtime)violations.fetch_add(1);std::free(p);}
void operator delete[](void* p,std::align_val_t alignment) noexcept{::operator delete(p,alignment);}
void operator delete(void* p,std::size_t,std::align_val_t alignment) noexcept{::operator delete(p,alignment);}
void operator delete[](void* p,std::size_t,std::align_val_t alignment) noexcept{::operator delete(p,alignment);}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
using namespace host;
static void drain(Drum& d){DrumAck a;while(d.acknowledgements.pop(a)){delete a.retired;delete a.retired_preview;}DrumClock c;while(d.clocks.pop(c)){};}
static double process(Drum& d,unsigned n){realtime=true;d.begin_block();double energy=0;for(unsigned f=0;f<n;++f){float l=0,r=0,cl=0,cr=0;d.frame(l,r,cl,cr);energy+=l*l+r*r; if(!std::isfinite(l)||!std::isfinite(cl))std::abort();}d.end_block();realtime=false;drain(d);return energy;}
static std::unique_ptr<DrumRack> fixture(){auto r=std::make_unique<DrumRack>();auto sample=std::make_unique<DrumSample>();sample->pcm.assign(4800,.2f);for(auto& p:r->pads){p.count=1;p.layers[0].sample=sample.get();}r->samples.push_back(std::move(sample));return r;}
static void load(Drum& d,std::unique_ptr<DrumRack> r){DrumCommand c;c.op=DrumOp::Rack;c.rack=r.release();require(d.commands.push(c),"load queue");process(d,1);}
static void play(Drum& d){DrumCommand c;c.op=DrumOp::Transport;c.flag=true;c.extra=-1;d.commands.push(c);}
static void exact(unsigned block){
    auto d=std::make_unique<Drum>(true);auto r=fixture();r->bpm=122;r->swing=.25;auto& lane=r->patterns[0].lanes[0];lane.length=7;for(unsigned i=0;i<7;++i)lane.steps[i]={100,1,float(i%2?.4:-.4)};r->patterns[0].lanes[1].steps[0]={127,0,0};load(*d,std::move(r));const auto origin=d->inspect().sample;play(*d);
    std::vector<uint64_t> found;while(d->inspect().sample<origin+48000*8){process(*d,block);DrumHit h;while(d->hits.pop(h)){require(h.pad==0,"probability zero fired");found.push_back(h.sample);}}
    std::vector<uint64_t> expected;for(unsigned i=0;i<100;++i){const auto t=origin+uint64_t(std::llround(std::max(0.,double(i)+(i%2?.25:0)+(i%7%2?.4:-.4))*48000*15/122));if(t<d->inspect().sample)expected.push_back(t);}std::sort(expected.begin(),expected.end());require(found==expected,"sample exact schedule/swing/micro/polymeter mismatch");
}
static void crossed(){auto d=std::make_unique<Drum>(true);auto r=fixture();r->swing=.5;auto& l=r->patterns[0].lanes[0];l.steps[1]={100,1,.5};l.steps[2]={100,1,-.5};load(*d,std::move(r));play(*d);process(*d,24000);DrumHit a,b;require(d->hits.pop(a)&&d->hits.pop(b)&&a.sample<b.sample,"crossed micro events must render in timestamp order");}
static void layers_choke(){
    auto d=std::make_unique<Drum>(true);auto r=fixture();auto* sample=r->samples[0].get();r->pads[0].count=2;r->pads[0].layers[0]={sample,1,63};r->pads[0].layers[1]={sample,64,127};r->pads[0].params.v[Choke]=1;r->pads[1].params.v[Choke]=1;auto zero=std::make_unique<DrumSample>();zero->pcm.assign(4800,0);r->pads[1].layers[0].sample=zero.get();r->samples.push_back(std::move(zero));load(*d,std::move(r));
    DrumCommand c;c.op=DrumOp::Trigger;c.pad=0;c.value=50;d->commands.push(c);process(*d,1);DrumHit h;require(d->hits.pop(h)&&h.layer==0,"soft layer");c.value=120;d->commands.push(c);require(process(*d,50)>0,"room sample silent");require(d->hits.pop(h)&&h.layer==1,"hard layer");
    // A cue strike with the same choke must leave ROOM audible.
    c.flag=true;d->commands.push(c);require(process(*d,50)>0,"cue choked room");d->hits.pop(h);
    auto silent=std::make_unique<DrumSample>();silent->pcm.assign(1000,0);DrumCommand preview;preview.op=DrumOp::Preview;preview.sample=silent.release();d->commands.push(preview);process(*d,1);
    c.flag=false;c.pad=1;d->commands.push(c);require(process(*d,20)==0,"same-bus choke failed");
    while(d->hits.pop(h)){};auto rr=fixture();rr->pads[0].round_robin=true;rr->pads[0].count=2;rr->pads[0].layers[1]=rr->pads[0].layers[0];load(*d,std::move(rr));c.pad=0;for(unsigned i=0;i<4;++i){d->commands.push(c);process(*d,1);require(d->hits.pop(h)&&h.layer==i%2,"round robin selection");c.flag=true;d->commands.push(c);process(*d,1);require(d->hits.pop(h)&&h.cue&&h.layer==i%2,"cue round robin must be independent");c.flag=false;}
}
static void switch_record(){
    auto d=std::make_unique<Drum>(true);auto r=fixture();r->bpm=122;r->patterns[0].lanes[0].steps[0]={100,1,0};r->patterns[1].lanes[1].steps[0]={110,1,0};load(*d,std::move(r));const auto origin=d->inspect().sample;play(*d);process(*d,1000);DrumHit h;require(d->hits.pop(h)&&h.pad==0&&h.sample==origin,"initial step");DrumCommand c;c.op=DrumOp::Select;c.pattern=1;d->commands.push(c);const auto boundary=origin+uint64_t(std::llround(48000.*240/122));while(d->inspect().sample<=boundary+300)process(*d,256);require(d->hits.pop(h)&&h.pad==1&&h.sample==boundary,"pattern did not switch on bar");require(!d->hits.pop(h),"old pattern fired on switch");
    c=DrumCommand{};c.op=DrumOp::Record;c.flag=true;c.param=16;d->commands.push(c);process(*d,1);c.op=DrumOp::Trigger;c.flag=false;c.pad=3;c.value=100;d->commands.push(c);realtime=true;d->begin_block();realtime=false;DrumAck a;bool recorded=false;while(d->acknowledgements.pop(a)){if(a.recorded)recorded=true;delete a.retired;delete a.retired_preview;}require(recorded,"live recording missing");d->commands.push(c);realtime=true;d->begin_block();realtime=false;while(d->acknowledgements.pop(a))require(!a.recorded,"record overwrote occupied step");
    c.op=DrumOp::Transport;c.flag=false;c.value=0;c.extra=-1;d->commands.push(c);require(process(*d,256)==0&&!d->inspect().running&&!d->inspect().armed,"STOP must clear ROOM and record arm");
}
static void strict_boundary(){
    auto d=std::make_unique<Drum>(true);auto rack=fixture();rack->bpm=122;rack->patterns[1].lanes[1].steps[0]={100,1,0};load(*d,std::move(rack));const auto origin=d->inspect().sample;play(*d);
    const auto first=uint64_t(std::llround(48000.*240/122));process(*d,unsigned(first));
    DrumCommand c;c.op=DrumOp::Select;c.pattern=1;d->commands.push(c);process(*d,1);require(d->inspect().pattern==0&&d->inspect().queued==1,"switch on rounded boundary must target strictly next bar");
    const auto second=origin+uint64_t(std::llround(48000.*480/122));while(d->inspect().sample<=second)process(*d,128);DrumHit hit;require(d->hits.pop(hit)&&hit.sample==second&&hit.pad==1,"strict next bar sample mismatch");
}
static void cue_buses_and_capacity(){
    auto d=std::make_unique<Drum>();load(*d,fixture());
    DrumCommand c;c.op=DrumOp::Trigger;c.pad=0;c.value=127;c.flag=true;d->commands.push(c);
    realtime=true;d->begin_block();double room=0,cue=0;for(unsigned i=0;i<128;++i){float l=0,r=0,cl=0,cr=0;d->frame(l,r,cl,cr);room+=l*l+r*r;cue+=cl*cl+cr*cr;}realtime=false;drain(*d);
    require(room==0&&cue>0,"preview leaked to room or silent cue");
    load(*d,fixture());c.flag=false;for(unsigned i=0;i<65;++i)d->commands.push(c);process(*d,1);require(d->inspect().steals==1,"voice pool not bounded at 64");
    c.flag=true;d->commands.push(c);process(*d,1);require(d->inspect().dropped==1,"CUE stole a ROOM voice at capacity");
    DrumCommand param;param.op=DrumOp::Param;param.pad=0;param.param=Level;param.value=0;d->commands.push(param);require(process(*d,32)==0,"live pad level did not affect active voices");
    auto audio=std::make_unique<Audio>(initial_configs());DrumCommand load_command;load_command.op=DrumOp::Rack;load_command.rack=fixture().release();audio->drum.commands.push(load_command);
    std::array<std::array<float,128>,10> data{};std::array<float*,10> output{};for(unsigned i=0;i<10;++i)output[i]=data[i].data();Midi note;const uint32_t ump[]={0x40990000,0xffff0000};require(decode_ump(ump,sizeof(ump),37,note)==8&&routed_midi(note),"UMP drum routing failed");
    realtime=true;audio->process(128,output,&note,1);realtime=false;
    for(unsigned i=0;i<37;++i)require(data[8][i]==0,"channel10 MIDI fired before timestamp");require(data[8][37]>0,"channel10 MIDI timestamp was delayed");
}
static void recovery_epoch(){
    auto d=std::make_unique<Drum>();load(*d,fixture());play(*d);process(*d,1);require(d->inspect().running,"PLAY missing");
    realtime=true;d->discontinuity();realtime=false;require(!d->inspect().running&&d->inspect().epoch==1,"clock gap must stop and roll epoch");
    DrumCommand c;c.op=DrumOp::Transport;c.flag=true;c.epoch=0;d->commands.push(c);realtime=true;d->begin_block();realtime=false;
    DrumAck ack;require(d->acknowledgements.pop(ack)&&ack.stale&&!d->inspect().running,"stale PLAY replayed across epoch");
    c.epoch=1;d->commands.push(c);process(*d,1);require(d->inspect().running,"fresh PLAY rejected after clock gap");
    c=DrumCommand{};c.op=DrumOp::Record;c.flag=true;c.param=16;d->commands.push(c);process(*d,1);
    DrumAck busy;for(unsigned i=0;i<3900;++i)d->acknowledgements.push(busy);
    realtime=true;d->trigger(0,100);realtime=false;
    require(!d->inspect().armed&&d->inspect().record_overruns==1,"record overflow must disarm and report, never silently drop edits");drain(*d);
}
static void ram(){namespace fs=std::filesystem;const auto dir=fs::temp_directory_path()/("gf-drum-ram-"+std::to_string(getpid()));fs::create_directories(dir);auto path=dir/"huge.wav";unsigned char h[44]={'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x80,0xbb,0,0,0,0x77,1,0,2,0,16,0,'d','a','t','a',0,0,0,0};const uint32_t n=200000000;for(unsigned i=0;i<4;++i){h[4+i]=uint8_t((n+36)>>(8*i));h[40+i]=uint8_t(n>>(8*i));}std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(h),44);out.seekp(43+n);out.put(0);out.close();bool rejected=false;try{drum_decode(path,std::string(64,'0'),DRUM_RAM);}catch(const std::exception& e){rejected=std::string(e.what()).find("RAM budget")!=std::string::npos;}fs::remove_all(dir);require(rejected,"projected decoded RAM must reject before reading/hash/allocation");}
int main(){try{for(unsigned block:{1u,128u,256u,511u})exact(block);crossed();layers_choke();switch_record();strict_boundary();cue_buses_and_capacity();recovery_epoch();ram();require(violations==0,"heap activity on sampler RT thread");std::cout<<"DRUM: exact timing 122 BPM blocks 1/128/256/511, polymeter, crossed micro/swing, probability, layers/RR, choke/CUE, bar switch, record preservation, STOP, RAM; allocations/frees="<<violations<<"\n";}catch(const std::exception& e){realtime=false;std::cerr<<e.what()<<'\n';return 1;}}
