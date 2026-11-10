#include "audio.h"
#include "bench_support.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
namespace host {
static_assert(std::atomic<uint64_t>::is_always_lock_free);
static_assert(std::atomic<bool>::is_always_lock_free);
std::array<Config,4> initial_configs(bool wet) {
    std::array<Config,4> c{defaults(engine_index("model_d")),defaults(engine_index("juno")),defaults(engine_index("ob6")),defaults(engine_index("hybrid"))};
    if(wet) for(auto& p:c) benchmark_patch(p);
    return c;
}
Audio::Audio(const std::array<Config,4>& configs) {
    initialize_catalogs();
    std::array<std::unique_ptr<Engine>,4> temp;
    for(unsigned p=0;p<4;++p) {
        if(catalog(configs[p].engine).size()>MAX_PARAMS) throw std::runtime_error("catalog exceeds RT bound");
        temp[p]=make_engine(configs[p]);
    }
    for(unsigned p=0;p<4;++p) {engines_[p]=temp[p].release();gain_[p]=.25f;map_cc(p);}
}
Audio::~Audio() {for(auto* p:engines_)delete p;Ack a;while(acknowledgements.pop(a))delete a.retired;Command c;while(commands.pop(c))if(c.op==Operation::Swap)delete c.engine;}
void Audio::map_cc(unsigned p) noexcept {
    constexpr const char* ids[]={"drift","grit","pocket","move","resonance","cutoff"};
    for(unsigned i=0;i<6;++i) {
        cc_[p][i]=-1;
        const auto& ps=engines_[p]->params;
        for(size_t j=0;j<ps.size();++j) {
            // HYBRID filter CCs edit both halves, with a second write below.
            if(ps[j].id==ids[i]||(i>=4&&ps[j].id==(i==4?"a.resonance":"a.cutoff"))) cc_[p][i]=int(j);
        }
    }
}
void Audio::midi_control(const Midi& m) noexcept {
    const unsigned p=m.status&15u;
    if(p>=4)return;
    auto* e=engines_[p]; const unsigned type=m.status&0xf0u;
    if(type!=0xb0)return;
    if(m.a==64)return;
    constexpr uint8_t numbers[]={14,15,16,19,71,74};
    for(unsigned i=0;i<6;++i) if(m.a==numbers[i]&&cc_[p][i]>=0) {
        const auto index=unsigned(cc_[p][i]);const auto& desc=e->params[index];
        const float norm=float(m.b)/127;
        const float value=i==5?desc.min*std::pow(desc.max/desc.min,norm):desc.min+norm*(desc.max-desc.min);
        desc.write(e->config,value);changed_[p][index]=true;
        if(e->config.layer.eng_a<6&&registry()[size_t(e->config.engine)].firmware==6&&i>=4) {
            const char* id=i==4?"b.resonance":"b.cutoff";
            for(size_t j=0;j<e->params.size();++j) if(e->params[j].id==id) {
                const auto& d=e->params[j]; d.write(e->config,i==5?d.min*std::pow(d.max/d.min,norm):norm);changed_[p][j]=true;
            }
        }
    }
}
void Audio::midi_note(const Midi& m) noexcept {
    const unsigned p=m.status&15u;if(p>=4)return;
    const auto type=m.status&0xf0u;
    if(type==0x80||type==0x90) engines_[p]->note(m.a,type==0x90?float(m.b)/127:0);
    else if(type==0xe0)engines_[p]->bend(int(m.a)|(int(m.b)<<7));
    else if(type==0xb0&&m.a==64)engines_[p]->sustain(m.b>=64);
}
void Audio::process(unsigned frames,const std::array<float*,10>& out,const Midi* midi,unsigned count) noexcept {
    bench::FlushToZero ftz;
    const auto start=std::chrono::steady_clock::now();
    if(frames==0||frames>MAX_FRAMES) {fault.store(true);return;}
    block.store(frames,std::memory_order_relaxed);
    std::array<Ack,255> acks{};unsigned ack_count=0;
    Command command; unsigned handled=0;
    const auto command_limit=std::min<size_t>(255,acknowledgements.free_slots());
    while(handled++<command_limit&&commands.pop(command)) {
        const unsigned p=command.part;Ack ack;ack.token=command.token;
        if(p>=4) {fault.store(true);continue;}
        switch(command.op) {
            case Operation::Swap:ack.retired=engines_[p];engines_[p]=command.engine;map_cc(p);changed_[p].fill(false);break;
            case Operation::Param:if(command.param<engines_[p]->params.size()){engines_[p]->params[command.param].write(engines_[p]->config,command.value);changed_[p][command.param]=true;}break;
            case Operation::Level:gain_[p]=std::pow(10.f,command.value/20);break;
            case Operation::Mute:mute_[p]=command.value!=0;break;
            case Operation::Note:engines_[p]->note(uint8_t(command.param),command.value/127);break;
        }
        // Queue capacity is larger than the command queue; control drains ACK before submitting.
        acks[ack_count++]=ack;
    }
    count=std::min(count,MAX_MIDI);
    for(unsigned i=0;i<count;++i) midi_control(midi[i]);
    for(unsigned p=0;p<4;++p) {
        bool dirty=false;for(size_t j=0;j<engines_[p]->params.size();++j)dirty|=changed_[p][j];
        if(dirty)engines_[p]->apply();
    }
    for(unsigned i=0;i<ack_count;++i)if(!acknowledgements.push(acks[i]))fault.store(true);
    unsigned next=0;
    for(unsigned f=0;f<frames;++f) {
        while(next<count&&midi[next].frame<=f) midi_note(midi[next++]);
        float mix_l=0,mix_r=0;
        for(unsigned p=0;p<4;++p) {
            float l,r;engines_[p]->frame(l,r);const float gain=mute_[p]?0:gain_[p];l*=gain;r*=gain;
            if(!std::isfinite(l)||!std::isfinite(r)) {l=r=0;fault.store(true);}
            if(out[p*2])out[p*2][f]=l;if(out[p*2+1])out[p*2+1][f]=r;
            mix_l+=l;mix_r+=r;
            peaks_[p]=std::max(peaks_[p],std::max(std::abs(l),std::abs(r)));squares_[p]+=(double(l)*l+double(r)*r)*.5;
        }
        if(out[8])out[8][f]=mix_l;if(out[9])out[9][f]=mix_r;
        peaks_[4]=std::max(peaks_[4],std::max(std::abs(mix_l),std::abs(mix_r)));squares_[4]+=(double(mix_l)*mix_l+double(mix_r)*mix_r)*.5;
    }
    for(unsigned p=0;p<4;++p) for(size_t j=0;j<engines_[p]->params.size();++j) if(changed_[p][j]) {
        Ack event;event.parameter=true;
        event.change={engines_[p],p,unsigned(j),engines_[p]->params[j].read(engines_[p]->config)};
        if(acknowledgements.push(event)) changed_[p][j]=false;
        else dropped.fetch_add(1,std::memory_order_relaxed);
    }
    meter_frames_+=frames;meter_clock_+=frames;
    if(meter_clock_>=meter_due_) {
        Telemetry t;
        for(size_t i=0;i<5;++i) {t.peak[i]=20*std::log10(std::max(peaks_[i],1e-6f));t.rms[i]=float(20*std::log10(std::max(std::sqrt(squares_[i]/double(meter_frames_)),1e-6)));}
        if(!telemetry.push(t))dropped.fetch_add(1,std::memory_order_relaxed);
        meter_due_=(meter_clock_/(RATE/30)+1)*(RATE/30);
        meter_frames_=0;peaks_.fill(0);squares_.fill(0);
    }
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    const double pct=seconds*RATE/double(frames)*100;
    if(pct>100){xruns.fetch_add(1,std::memory_order_relaxed);deadline_misses.fetch_add(1,std::memory_order_relaxed);}
    ++cpu_histogram_[std::min<size_t>(511,size_t(std::max(0.,pct)*2))];++cpu_blocks_;
    cpu_frames_+=frames;cpu_seconds_+=seconds;cpu_max_=std::max(cpu_max_,pct);
    if(cpu_frames_>=RATE) {
        Telemetry t;t.cpu=true;t.mean=cpu_seconds_*RATE/double(cpu_frames_)*100;t.max=cpu_max_;
        const uint32_t rank=(cpu_blocks_*99+99)/100;uint32_t cumulative=0;
        for(size_t bin=0;bin<cpu_histogram_.size();++bin){cumulative+=cpu_histogram_[bin];if(cumulative>=rank){t.p99_upper=bin==511?cpu_max_:std::min(cpu_max_,double(bin+1)*.5);break;}}
        cpu_histogram_.fill(0);cpu_blocks_=0;
        if(!telemetry.push(t))dropped.fetch_add(1,std::memory_order_relaxed);
        cpu_frames_=0;cpu_seconds_=cpu_max_=0;
    }
    heartbeat.fetch_add(1,std::memory_order_release);
}
unsigned pattern(uint64_t frame,Midi* out) noexcept {
    unsigned count=0;
    constexpr unsigned roots[]={36,48,60,48},voices[]={1,6,4,6},offsets[]={0,3,7,10,12,15};
    constexpr int shifts[]={0,2,-2,5};
    const auto phase=frame%(2*RATE); const auto cycle=frame/(2*RATE);
    if(phase==0||phase==RATE*3/2) for(unsigned p=0;p<4;++p) for(unsigned v=0;v<voices[p];++v)
        out[count++]={0,uint8_t((phase==0?0x90:0x80)|p),uint8_t(int(roots[p]+offsets[v])+shifts[cycle%4]),uint8_t(std::lround((.78f-.035f*float(v))*127))};
    if(frame%(RATE/2)==0) for(unsigned p=0;p<4;++p) {const int bend=(frame/(RATE/2))%2?8704:8192;out[count++]={0,uint8_t(0xe0|p),uint8_t(bend&127),uint8_t(bend>>7)};}
    return count;
}
}
