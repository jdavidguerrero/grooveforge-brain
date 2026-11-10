#include "drum.h"
#include "bench_support.h"
#include "json.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
using namespace host;
int main(int argc,char** argv){
    const unsigned block=256;auto drum=std::make_unique<Drum>();auto rack=std::make_unique<DrumRack>();auto sample=std::make_unique<DrumSample>();sample->channels=2;sample->pcm.resize(48000*26*2);
    for(size_t i=0;i<sample->frames();++i){sample->pcm[i*2]=float(std::sin(i*.031)*.1);sample->pcm[i*2+1]=float(std::sin(i*.037)*.1);}
    for(auto& p:rack->pads){p.count=1;p.layers[0].sample=sample.get();p.params.v[Decay]=60000;p.params.v[Level]=.01f;p.params.v[Cutoff]=3000;p.params.v[Resonance]=.3f;}
    rack->bytes=sample->pcm.size()*4;rack->samples.push_back(std::move(sample));DrumCommand c;c.op=DrumOp::Rack;c.rack=rack.release();drum->commands.push(c);
    for(unsigned i=0;i<64;++i){c=DrumCommand{};c.op=DrumOp::Trigger;c.pad=i%16;c.value=110;drum->commands.push(c);}
    c=DrumCommand{};c.op=DrumOp::Transport;c.flag=true;c.extra=-1;drum->commands.push(c);
    std::vector<double> costs;costs.reserve(48000*20/block+1);double checksum=0;
    bench::FlushToZero ftz;
    for(uint64_t frame=0;frame<22*48000;frame+=block){
        const auto begin=std::chrono::steady_clock::now();drum->begin_block();for(unsigned i=0;i<block;++i){float l=0,r=0,cl=0,cr=0;drum->frame(l,r,cl,cr);checksum+=l+r;}drum->end_block();
        const double pct=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count()*48000/block*100;
        if(frame>=2*48000)costs.push_back(pct);DrumAck a;while(drum->acknowledgements.pop(a)){delete a.retired;delete a.retired_preview;}DrumClock clock;while(drum->clocks.pop(clock)){}
    }
    std::sort(costs.begin(),costs.end());auto result=object({{"scenario","sampler-64-stereo-voices-clock"},{"sample_rate",48000},{"block",int(block)},{"warmup_seconds",2},{"measured_seconds",20},{"mean_pct",std::accumulate(costs.begin(),costs.end(),0.)/double(costs.size())},{"p99_pct",costs[size_t(std::ceil(costs.size()*.99))-1]},{"max_pct",costs.back()},{"deadline_misses",int(std::count_if(costs.begin(),costs.end(),[](double v){return v>100;}))},{"voice_steals",int64_t(drum->inspect().steals)},{"clipped",int64_t(drum->inspect().clipped)},{"checksum",checksum},{"compiler",__VERSION__},{"policy","Release -O2 -fno-fast-math -ffp-contract=off + FTZ; no RT scheduling"}});
    std::cout<<result.dump()<<'\n';if(argc==2){std::ofstream out(argv[1]);out<<result.dump()<<'\n';return out?0:1;}return 0;
}
