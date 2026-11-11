#include "drum.h"
#include <memory>
#include <iostream>
using namespace host;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
static std::unique_ptr<DrumRack> rack(){
    auto r=std::make_unique<DrumRack>();auto s=std::make_unique<DrumSample>();s->pcm.assign(4800,.5f);
    for(unsigned i=0;i<2;++i){auto& p=r->pads[i];p.count=1;p.layers[0]={s.get(),1,127};p.params.v[Choke]=1;p.params.v[FadeOut]=0;}
    r->samples.push_back(std::move(s));return r;
}
static void render(Drum& d){d.begin_block();for(unsigned i=0;i<64;++i){float l=0,r=0,cl=0,cr=0;d.frame(l,r,cl,cr);}DrumAck a;while(d.acknowledgements.pop(a)){delete a.retired;delete a.retired_preview;}}
static void load(Drum& d,std::unique_ptr<DrumRack> r){DrumCommand c;c.op=DrumOp::Rack;c.rack=r.release();d.commands.push(c);render(d);}
static void hit(Drum& d,unsigned pad=0,bool cue=false){DrumCommand c;c.op=DrumOp::Trigger;c.pad=pad;c.value=80;c.flag=cue;d.commands.push(c);}
int main(){try{
    for(unsigned gate=0;gate<5;++gate){auto d=std::make_unique<Drum>();auto r=rack();
        if(gate==0)r->pads[0].params.v[Mute]=1;
        if(gate==1)r->groups[0].mute=true;
        if(gate==2)r->groups[0].level=0;
        if(gate==3)r->pads[1].params.v[Solo]=1;
        load(*d,std::move(r));hit(*d,0,gate==4);render(*d);check(d->inspect().led_hits[0]==0,"inaudible strike flashed");}
    auto d=std::make_unique<Drum>();load(*d,rack());hit(*d);hit(*d,1);render(*d);
    check(d->inspect().led_hits[0]==0&&d->inspect().led_hits[1]==1,"choke before audio fabricated flash");
    hit(*d);render(*d);check(d->inspect().led_hits[0]==1&&d->inspect().led_velocity[0]==80,"audible velocity not published");
    check(d->inspect().led_choked[1]&&!d->inspect().led_choked[0],"choke output state");
    render(*d);check(d->inspect().led_hits[0]==1,"sustained sample retriggered LED");
    auto p=std::make_unique<Drum>();auto r=rack();r->patterns[0].lanes[0].steps[0]={100,0,0};load(*p,std::move(r));
    DrumCommand c;c.op=DrumOp::Transport;c.flag=true;p->commands.push(c);render(*p);check(p->inspect().led_hits[0]==0,"probability0 flashed");
    std::cout<<"LED telemetry: probability, mute/solo/group, cue, choke, velocity, cumulative counters PASS\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
