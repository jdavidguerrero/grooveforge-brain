#include "drum.h"
#include <algorithm>
#include <cmath>
namespace host {
Drum::~Drum(){delete rack_;delete preview_;DrumAck a;while(acknowledgements.pop(a)){delete a.retired;delete a.retired_preview;}DrumCommand c;while(commands.pop(c)){if(c.op==DrumOp::Rack)delete c.rack;if(c.op==DrumOp::Preview)delete c.sample;}}
void Drum::clear(bool cue) noexcept {for(auto& v:voices_)if(v.cue==cue)v.sample=nullptr;}
uint64_t Drum::target(unsigned lane,uint64_t ordinal) const noexcept {
    const auto& l=patterns_[state_.pattern].lanes[lane];
    const auto& step=l.steps[ordinal%l.length];
    const double offset=double(ordinal)+(ordinal%2?state_.swing:0)+step.micro;
    return pattern_origin_+uint64_t(std::llround(std::max(0.,offset)*DRUM_RATE*15/state_.bpm));
}
void Drum::refresh_schedule() noexcept {for(unsigned l=0;l<16;++l)for(unsigned i=0;i<3;++i)times_[l][i]=target(l,ordinals_[l][i]);}
void Drum::reset_schedule(uint64_t origin) noexcept {pattern_origin_=origin;state_.pattern_origin=origin;for(auto& lane:ordinals_)lane={0,1,2};horizon_.fill(3);refresh_schedule();}
void Drum::discontinuity() noexcept {++state_.epoch;++state_.rev;state_.running=false;state_.armed=false;state_.queued=-1;state_.origin=state_.sample;clear(false);clear(true);reset_schedule(state_.sample);}
void Drum::begin_block() noexcept {
    DrumCommand c;unsigned handled=0;
    const size_t limit=std::min<size_t>(255,acknowledgements.free_slots());
    while(handled++<limit&&commands.pop(c)) {
        DrumAck a;a.token=c.token;
        if(c.epoch!=std::numeric_limits<uint64_t>::max()&&c.epoch!=state_.epoch){
            a.rejected=a.stale=true;a.clock=state_;
            if(c.op==DrumOp::Rack)a.retired=c.rack;
            if(c.op==DrumOp::Preview)a.retired_preview=c.sample;
            acknowledgements.push(a);continue;
        }
        switch(c.op){
        case DrumOp::Rack:
            clear(false);clear(true);a.retired=rack_;rack_=c.rack;
            pads_=rack_->pads;patterns_=rack_->patterns;groups_=rack_->groups;
            for(auto& bus:rr_)bus.fill(0);if(c.flag)refresh_schedule();else{state_.pattern=rack_->active_pattern;state_.bpm=rack_->bpm;state_.swing=rack_->swing;reset_schedule(state_.sample);state_.queued=-1;}break;
        case DrumOp::Param:if(c.pad<16&&c.param<DrumParamCount){pads_[c.pad].params.v[c.param]=c.value;for(auto& v:voices_)if(v.sample&&v.pad==c.pad)configure_voice(v,pads_[c.pad].params);}break;
        case DrumOp::Steps:if(c.pad<16&&c.pattern<8){patterns_[c.pattern].lanes[c.pad]=c.lane;refresh_schedule();}break;
        case DrumOp::Select:
            if(c.pattern<8){if(state_.running){state_.queued=int(c.pattern);const double bar=DRUM_RATE*240/state_.bpm;const double k=std::floor(double(state_.sample-state_.origin)/bar)+1;switch_at_=state_.origin+uint64_t(std::llround(k*bar));if(switch_at_<=state_.sample)switch_at_=state_.origin+uint64_t(std::llround((k+1)*bar));}
            else{state_.pattern=c.pattern;state_.queued=-1;reset_schedule(state_.sample);}}break;
        case DrumOp::Transport:
            if(c.value>0&&!state_.running)state_.bpm=c.value;
            if(c.extra>=0){state_.swing=c.extra;refresh_schedule();}
            if(c.flag&&!state_.running){state_.running=true;state_.origin=state_.sample;reset_schedule(state_.sample);}
            else if(!c.flag){state_.running=false;state_.armed=false;state_.queued=-1;clear(false);reset_schedule(state_.sample);}break;
        case DrumOp::Record:state_.armed=c.flag;quantize_=c.param;break;
        case DrumOp::Group:if(c.pad<4)groups_[c.pad]={c.value,c.flag};break;
        case DrumOp::Preview:
            clear(true);a.retired_preview=preview_;preview_=c.sample;
            if(preview_){DrumParams params;params.v[Decay]=60000;params.v[Level]=.5f;a.rejected=!start_voice(preview_,params,16,127,true,0);}break;
        case DrumOp::Trigger:{const auto dropped=state_.dropped;hit(c.pad,unsigned(c.value),c.flag,!c.flag);a.rejected=c.flag&&state_.dropped>dropped;break;}
        }
        ++state_.rev;a.clock=state_;acknowledgements.push(a);
    }
}
void Drum::configure_voice(Voice& v,const DrumParams& params) noexcept {
    v.params=params;const auto& p=params.v;v.group=unsigned(p[Group]);v.choke=unsigned(p[Choke]);
    v.ratio=std::pow(2.,(p[Tune]+p[Cents]/100)/12);
    float vel=float(v.velocity)/127;
    if(p[VelocityCurve]==1)vel=std::sqrt(vel);else if(p[VelocityCurve]==2)vel*=vel;else if(p[VelocityCurve]==3)vel=1;
    v.gain=(1-p[VelocityLevel]+p[VelocityLevel]*vel)*p[Level];
    v.left=std::sqrt((1-p[Pan])*.5f);v.right=std::sqrt((1+p[Pan])*.5f);
    const float cutoff=std::clamp(p[Cutoff]*(1-p[VelocityTone]*(1-vel)),20.f,20000.f);
    v.coefficient=1-std::exp(-6.28318530718f*cutoff/DRUM_RATE);
}
bool Drum::start_voice(const DrumSample* sample,const DrumParams& params,unsigned pad,unsigned velocity,bool cue,unsigned layer) noexcept {
    if(!sample||sample->frames()<2)return false;
    const auto& p=params.v;const unsigned choke=unsigned(p[Choke]);
    if(choke)for(auto& v:voices_)if(v.sample&&v.cue==cue&&v.choke==choke)v.sample=nullptr;
    Voice* voice=nullptr;for(auto& v:voices_)if(!v.sample){voice=&v;break;}
    if(!voice){
        for(auto& v:voices_)if(v.cue==cue&&(!voice||v.age<voice->age))voice=&v;
        if(!voice){++state_.dropped;return false;}
        ++state_.steals;
    }
    *voice=Voice{};auto& v=*voice;v.sample=sample;v.params=params;v.pad=pad;v.group=unsigned(p[Group]);v.choke=choke;v.cue=cue;v.age=++voice_serial_;
    v.position=p[Start]*double(sample->frames());v.velocity=velocity;configure_voice(v,params);
    if(trace_&&!hits.push({state_.sample,pad,layer,cue}))++state_.dropped;
    return true;
}
void Drum::hit(unsigned pad,unsigned velocity,bool cue,bool record) noexcept {
    if(pad>=16||velocity==0||velocity>127)return;
    const auto& p=pads_[pad];
    std::array<unsigned,4> matching{};unsigned count=0;
    for(unsigned i=0;i<p.count;++i)if(velocity>=p.layers[i].low&&velocity<=p.layers[i].high)matching[count++]=i;
    if(count){const unsigned layer=matching[p.round_robin?rr_[cue?1:0][pad]++%count:0];start_voice(p.layers[layer].sample,p.params,pad,velocity,cue,layer);}
    if(record&&state_.armed&&state_.running){
        if(acknowledgements.free_slots()<=256){state_.armed=false;++state_.record_overruns;++state_.dropped;++state_.rev;return;}
        const double pos=double(state_.sample-pattern_origin_)*state_.bpm/(DRUM_RATE*15);
        const double quant=std::round(pos*quantize_/16)*16/quantize_;
        auto& lane=patterns_[state_.pattern].lanes[pad];
        const auto ordinal=uint64_t(std::floor(quant));const unsigned step=unsigned(ordinal%lane.length);
        if(!lane.steps[step].velocity){auto& s=lane.steps[step];s={uint8_t(velocity),1,float(quant-std::floor(quant))};DrumAck a;a.recorded=true;a.pattern=state_.pattern;a.pad=pad;a.step=step;a.hit=s;acknowledgements.push(a);++state_.rev;refresh_schedule();}
    }
}
void Drum::trigger(unsigned pad,unsigned velocity,bool cue) noexcept {hit(pad,velocity,cue,!cue);}
void Drum::frame(float& room_l,float& room_r,float& cue_l,float& cue_r) noexcept {
    if(state_.running){
        if(state_.queued>=0&&state_.sample>=switch_at_){state_.pattern=unsigned(state_.queued);state_.queued=-1;reset_schedule(switch_at_);++state_.rev;}
        for(unsigned lane=0;lane<16;++lane){
            for(unsigned due=0;due<3;++due){
                const unsigned slot=unsigned(std::min_element(times_[lane].begin(),times_[lane].end())-times_[lane].begin());
                if(times_[lane][slot]>state_.sample)break;
                const uint64_t ordinal=ordinals_[lane][slot];
                const auto& l=patterns_[state_.pattern].lanes[lane];const auto s=l.steps[ordinal%l.length];
                ordinals_[lane][slot]=horizon_[lane]++;times_[lane][slot]=target(lane,ordinals_[lane][slot]);
                random_^=random_<<13;random_^=random_>>17;random_^=random_<<5;
                const bool cancelled=state_.queued>=0 && pattern_origin_+uint64_t(std::llround(double(ordinal)*DRUM_RATE*15/state_.bpm))>=switch_at_;
                if(!cancelled&&s.velocity&&double(random_)/4294967296.<s.probability)hit(lane,s.velocity,false,false);
            }
        }
    }
    std::array<std::array<float,2>,4> groups{};cue_l=cue_r=0;
    bool solo=false;for(const auto& p:pads_)solo|=p.params.v[Solo]!=0;
    for(auto& v:voices_)if(v.sample){
        const auto& p=v.params.v;const auto n=v.sample->frames();const double end=p[End]*double(n);
        if(v.position>=end||v.position<0){v.sample=nullptr;continue;}
        const auto index=size_t(v.position);const float frac=float(v.position-double(index));
        const double elapsed=double(v.elapsed_frames++);
        const float ms=float(elapsed*1000/DRUM_RATE);
        float env=1;const float attack=p[EnvMode]==0?p[Attack]:0;
        if(attack>0&&ms<attack)env=ms/attack;
        const float decay_start=attack+(p[EnvMode]!=0?p[Hold]:0);
        if(ms>decay_start)env*=std::max(0.f,1-(ms-decay_start)/std::max(.01f,p[Decay]));
        if(p[FadeIn]>0)env*=std::min(1.f,ms/p[FadeIn]);
        if(p[FadeOut]>0)env*=std::min(1.f,float((end-v.position)/v.ratio*1000/DRUM_RATE)/p[FadeOut]);
        if(env<=0&&ms>=decay_start){v.sample=nullptr;continue;}
        float x[2]{};
        for(unsigned ch=0;ch<2;++ch){const unsigned c=std::min(ch,v.sample->channels-1);const auto& pcm=v.sample->pcm;
            const float input=pcm[index*v.sample->channels+c]*(1-frac)+pcm[std::min(index+1,n-1)*v.sample->channels+c]*frac;
            // Two damped one-pole stages: bounded feedback, stable for every allowed cutoff.
            const float driven=input-p[Resonance]*v.band[ch];v.z[ch]+=v.coefficient*(driven-v.z[ch]);v.band[ch]+=v.coefficient*(v.z[ch]-v.band[ch]);
            x[ch]=(p[FilterMode]==0?v.band[ch]:input-v.band[ch])*env*v.gain;
        }
        v.position+=v.ratio;x[0]*=v.left;x[1]*=v.right;
        if(v.cue){cue_l+=x[0];cue_r+=x[1];}
        else if(v.pad<16&&!pads_[v.pad].params.v[Mute]&&(!solo||pads_[v.pad].params.v[Solo])){groups[v.group][0]+=x[0];groups[v.group][1]+=x[1];state_.pads[v.pad]=std::max(state_.pads[v.pad],std::max(std::abs(x[0]),std::abs(x[1])));}
    }
    for(unsigned g=0;g<4;++g){const float gain=groups_[g].mute?0:groups_[g].level;const float l=groups[g][0]*gain,r=groups[g][1]*gain;room_l+=l;room_r+=r;state_.groups[g]=std::max(state_.groups[g],std::max(std::abs(l),std::abs(r)));}
    state_.room=std::max(state_.room,std::max(std::abs(room_l),std::abs(room_r)));state_.cue=std::max(state_.cue,std::max(std::abs(cue_l),std::abs(cue_r)));
    if(std::abs(room_l)>1||std::abs(room_r)>1||std::abs(cue_l)>1||std::abs(cue_r)>1)++state_.clipped;
    ++state_.sample;
}
void Drum::end_block() noexcept {if(state_.sample-last_published_>=1600){if(!clocks.push(state_))++state_.dropped;last_published_=state_.sample;state_.pads.fill(0);state_.groups.fill(0);state_.room=state_.cue=0;}}
}
