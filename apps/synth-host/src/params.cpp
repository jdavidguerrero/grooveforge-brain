#include "params.h"
#include "preset_codec.h"
#include "hybrid_codec.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <set>
#include <type_traits>
namespace host {
float Param::read(const Config& c) const noexcept {
    const auto* p=reinterpret_cast<const unsigned char*>(&c)+offset;
    switch(storage) {
        case Storage::Float: { float v; std::memcpy(&v,p,sizeof v); return v; }
        case Storage::Bool: return *reinterpret_cast<const bool*>(p)?1:0;
        case Storage::U8: return *p;
        case Storage::I8: return *reinterpret_cast<const int8_t*>(p);
    }
    return 0;
}
float Param::write(Config& c,float value) const noexcept {
    value=std::clamp(value,min,max);
    // Continuous values are clamped, not snapped: step is an encoder increment.
    if(options>0) value=min+std::round((value-min)/step)*step;
    auto* p=reinterpret_cast<unsigned char*>(&c)+offset;
    switch(storage) {
        case Storage::Float: std::memcpy(p,&value,sizeof value); break;
        case Storage::Bool: *reinterpret_cast<bool*>(p)=value>=.5f; break;
        case Storage::U8: *p=uint8_t(std::lround(value)); break;
        case Storage::I8: *reinterpret_cast<int8_t*>(p)=int8_t(std::lround(value)); break;
    }
    return read(c);
}
std::string Param::kind() const { return toggle ? "bool" : options>0 ? "enum" : min<0 ? "bipolar" : "cont"; }
int engine_index(const std::string& s) {
    const auto& r=registry();
    for(size_t i=0;i<r.size();++i) if(s==r[i].name) return int(i);
    throw std::runtime_error("engine unavailable: "+s);
}
Config defaults(int e) {return registry().at(size_t(e)).default_config(e);}
Config preview_defaults(int e) {
    Config c; c.engine=e;
    const int id=registry().at(size_t(e)).firmware;
    if(id==6) {
        if(!studio::factory_layer(7,&c.layer)) throw std::runtime_error("missing RETRO BRASS");
    } else {
        c.layer.eng_a=uint8_t(id);
        if(!studio::factory_preset(uint8_t(id),0,&c.layer.a)) throw std::runtime_error("missing factory preset");
    }
    return c;
}
std::vector<Config> preview_presets(int engine) {
    std::vector<Config> result;
    const int id=registry()[size_t(engine)].firmware;
    const unsigned count=id==6?studio::factory_layer_count():studio::factory_count(uint8_t(id));
    for(unsigned n=0;n<count;++n) {
        auto c=preview_defaults(engine);
        if(id==6) {
            studio::factory_layer(uint8_t(n),&c.layer);
            if(c.layer.eng_a!=2||c.layer.eng_b!=1)continue;
        } else studio::factory_preset(uint8_t(id),uint8_t(n),&c.layer.a);
        result.push_back(c);
    }
    return result;
}
std::string preview_preset_name(const Config& c) {
    return registry()[size_t(c.engine)].firmware==6?c.layer.name:c.layer.a.name;
}
std::vector<uint8_t> preview_encode(const Config& c) {
    std::vector<uint8_t> bytes(studio::LAYER_BYTES);
    const int id=registry()[size_t(c.engine)].firmware;
    const size_t n=id==6?studio::layer_encode(c.layer,bytes.data(),bytes.size()):studio::preset_encode(c.layer.a,uint8_t(id),bytes.data(),bytes.size());
    if(!n)throw std::runtime_error("cannot encode patch");
    bytes.resize(n);return bytes;
}
Config preview_decode(int engine,const uint8_t* data,size_t size) {
    Config c=preview_defaults(engine);
    const int id=registry()[size_t(engine)].firmware;
    if(id==6) {
        if(!gf::decode_hybrid_layer(data,size,c.layer))throw std::runtime_error("invalid GFL");
        if(c.layer.eng_a!=2||c.layer.eng_b!=1)throw std::runtime_error("HYBRID catalog requires Prophet/Juno; see doc 16");
    } else {
        uint8_t decoded=0;
        if(!gf::decode_supported_preset(data,size,c.layer.a,decoded)||decoded!=id)throw std::runtime_error("invalid GFP");
    }
    return c;
}
namespace {
template<class T> void add(std::vector<Param>& out,Config& c,T& field,std::string id,std::string name,
    std::string page,float lo,float hi,float step=.01f,int options=0,std::string unit="",
    std::vector<std::string> labels={}) {
    Param p;
    p.id=std::move(id); p.name=std::move(name); p.page=std::move(page); p.unit=std::move(unit);
    p.min=lo; p.max=hi; p.step=step; p.options=options; p.labels=std::move(labels);
    p.toggle=options==2&&lo==0&&hi==1&&p.name!="Type";
    p.offset=size_t(reinterpret_cast<unsigned char*>(&field)-reinterpret_cast<unsigned char*>(&c));
    if constexpr(std::is_same_v<T,bool>) p.storage=Storage::Bool;
    else if constexpr(std::is_same_v<T,uint8_t>) p.storage=Storage::U8;
    else if constexpr(std::is_same_v<T,int8_t>) p.storage=Storage::I8;
    p.initial=std::clamp(p.read(c),lo,hi); out.push_back(std::move(p));
}
const char* osc_labels[6][4][8]={
 {{"Wave","","Level","","Pulse width","","",""},{"Wave","Detune","Level","","","","",""},{"Wave","Interval","Level","","","Fine","",""},{"Noise level","Noise pink","","","","","",""}},
 {{"Wave","Sub","Noise","Pulse width","Chorus","HPF","Range","VCA gate"}},
 {{"Frequency","Pulse width","Saw","Pulse","Sync"},{"Frequency","Fine","Pulse width","Keyboard","Saw","Triangle","Pulse","Low frequency"},{"Osc A","Osc B","Noise","Glide","Filter keyboard"}},
 {{"Shape","Sub","Level","Noise","Pulse width","","Filter morph","Bandpass"},{"Shape","Interval","Level","","Pulse width","Detune"}},
 {{"Ratio","Level","Algorithm","Feedback","Attack","Decay","Sustain","Release"},{"Ratio","Level","","","Attack","Decay","Sustain","Release"},{"Ratio","Level","","","Attack","Decay","Sustain","Release"},{"Ratio","Level","","","Attack","Decay","Sustain","Release"}},
 {{"Wave","Pulse width","Ring mix","Noise","Level"},{"Wave","Ratio","","","Level"}}
};
const char* mod_labels[6][2][8]={
 {{"Mod mix","Osc mod","Filter mod","Decay switch","VCO3 keyboard","VCO3 low","Overdrive","Filter keyboard"}},
 {{"LFO delay","BBD hiss"}},
 {{"Filter env","Osc B","Frequency A","Pulse width A","Filter"},{"LFO noise mix","Frequency A","Frequency B","Pulse width A","Pulse width B","Filter"}},
 {{"Filter env","VCO2","Frequency 1","Filter mode","Pulse width 1","Sync"}},
 {{"Wave OP1","Wave OP2","Wave OP3","Wave OP4","Detune OP1","Detune OP2","Detune OP3","Detune OP4"},{"Keyboard level","Keyboard rate","D2 rate"}},
 {{"Mono","Sync","VCO3 rate","VCO3 pitch","VCO3 cutoff","Noise color"}}
};
void half(std::vector<Param>& out,Config& c,studio::Preset& p,int e,const std::string& prefix) {
    const auto page=[&](const char* s){return prefix+s;};
    auto& v=p.patch;
    add(out,c,p.cutoff_hz,prefix+"cutoff","Cutoff",page("FILTER"),GF_CUTOFF_RANGE[e].lo,GF_CUTOFF_RANGE[e].hi,1,0,"Hz");
    add(out,c,p.reso_norm,prefix+"resonance","Resonance",page("FILTER"),0,1);
    if(e!=4) add(out,c,v.env_amt,prefix+"env_amount","Envelope amount",page("FILTER"),0,1);
    add(out,c,v.glide,prefix+"glide","Glide",page("FILTER"),0,1);
    add(out,c,v.drive,prefix+"drive","Filter drive",page("FILTER"),0,1);
    auto env=[&](const std::string& pr,float& a,float& d,float& s,float& r) {
        add(out,c,a,prefix+pr+"attack",pr+"Attack",page("ENV"),GF_ATTACK_RANGE[e].lo,GF_ATTACK_RANGE[e].hi,1,0,"ms");
        add(out,c,d,prefix+pr+"decay",pr+"Decay",page("ENV"),GF_DECAY_RANGE[e].lo,GF_DECAY_RANGE[e].hi,1,0,"ms");
        add(out,c,s,prefix+pr+"sustain",pr+"Sustain",page("ENV"),0,1);
        add(out,c,r,prefix+pr+"release",pr+"Release",page("ENV"),GF_RELEASE_RANGE[e].lo,GF_RELEASE_RANGE[e].hi,1,0,"ms");
    };
    env("",v.attack_ms,v.decay_ms,v.sustain,v.release_ms);
    if(e!=4) env("filter_",v.f_attack_ms,v.f_decay_ms,v.f_sustain,v.f_release_ms);
    add(out,c,v.vel_amp,prefix+"velocity_amp","Velocity amp",page("ENV"),0,100,1,0,"%");
    if(e!=4) add(out,c,v.vel_cut,prefix+"velocity_filter","Velocity filter",page("ENV"),0,100,1,0,"%");
    add(out,c,v.lfo_rate_hz,prefix+"lfo_rate","Rate",page("LFO"),GF_LFO_RATE_MIN,GF_LFO_RATE_MAX,.05f,0,"Hz");
    add(out,c,v.lfo_shape,prefix+"lfo_shape","Shape",page("LFO"),0,4,1,5,"",{"Sine","Triangle","Saw","Square","S&H"});
    add(out,c,v.lfo_div,prefix+"lfo_division","Division",page("LFO"),0,3,1,4,"",{"1/4","1/8","1/16","1/8T"});
    const char* dest[]={"cutoff","pitch","amp","pwm"};
    for(unsigned i=0;i<GF_DEST_COUNT;++i) if(gf_lfo_dest_available(uint8_t(e),uint8_t(i)))
        add(out,c,v.lfo_amt[i],prefix+"lfo_"+dest[i],dest[i],page("LFO"),0,1);
    for(int m=0;m<2;++m) for(unsigned st=0;st<(m?GF_MOD_STOPS[e]:GF_OSC_STOP_COUNT[e]);++st) for(int sl=0;sl<8;++sl) {
        const auto desc=m?GF_MOD_SLOT[e][st][sl]:GF_OSC_SLOT[e][st][sl];
        if(!gf_osc_used(&desc)||(desc.global&&st!=0)) continue;
        const auto id=prefix+(m?"mod.":"osc.")+std::to_string(st)+"."+std::to_string(sl);
        const char* label=m?mod_labels[e][st][sl]:osc_labels[e][st][sl];
        const std::string label_string=label&&*label?label:"Slot "+std::to_string(sl+1);
        add(out,c,m?v.mod[st][sl]:v.osc[st][sl],id,label_string,page(m?"MOD":"OSC")+" "+std::to_string(st+1),desc.lo,desc.hi,desc.step,desc.count);
    }
}
void bus(std::vector<Param>& out,Config& c,bool /*hybrid*/) {
    auto& v=c.layer.a.patch;
    const char* groups[]={"PUMP","ARP","CHORD","UNISON","MACRO"};
    const char* labels[5][8]={{"Depth","Shape","Rate","Offset","Release","Source"},{"Mode","Division","Gate","Octaves","Swing","Latch"},{"Type","Inversion","Strum","Key"},{"Voices","Detune"},{"DRIFT","GRIT","POCKET","MORPH","GRIT resonance","GRIT modulation","POCKET velocity","MOVE"}};
    for(int g=0;g<5;++g) {
        if(g<4) add(out,c,v.groove_on[g],"groove."+std::to_string(g)+".on",std::string(groups[g])+" on","GROOVE",0,1,1,2);
        for(int sl=0;sl<8;++sl) {
            auto desc=GF_GROOVE_SLOT[g][sl];
            if(!gf_osc_used(&desc)||(g==4&&sl==GF_MACRO_MORPH)) continue;
            add(out,c,v.groove[g][sl],g==4?std::string(sl==0?"drift":sl==1?"grit":sl==2?"pocket":sl==7?"move":sl==3?"morph":"macro."+std::to_string(sl)):"groove."+std::to_string(g)+"."+std::to_string(sl),labels[g][sl],g==4?"MACROS":"GROOVE",desc.lo,desc.hi,desc.step,desc.count,g==4?"%":"");
        }
    }
    const char* names[]={"Drive","Tape","Chorus","Delay","Reverb"};
    const char* fields[5][3]={{"Amount","Tone","Type"},{"Drive","Wow","Flutter"},{"Depth","Rate","Type"},{"Time","Feedback","Tone"},{"Decay","Type","Size"}};
    for(int fx=0;fx<5;++fx) {
        const auto id="fx."+std::to_string(fx)+".";
        const auto page=std::string("FX ")+names[fx];
        add(out,c,v.sfx_on[fx],id+"on","On",page,0,1,1,2);
        add(out,c,v.sfx_mix[fx],id+"mix","Mix",page,0,1);
        for(int f=0;f<3;++f) {
            const int n=GF_SFX_STEPS[fx][f];
            std::vector<std::string> options;
            if(fx==0&&f==2)options={"Overdrive","Distortion","Crush"};
            if(fx==2&&f==2)options={"Chorus","Phaser"};
            if(fx==3&&f==0)options={"1/16","1/8","1/8.","1/4","1/4."};
            if(fx==4&&f==1)options={"Campana","Guadua","Cristal","Madera","Plate"};
            if(fx==4&&f==2)options={"Small","Medium","Large"};
            add(out,c,v.sfx_p[fx][f],id+std::to_string(f),fields[fx][f],page,0,1,n?1.0f/float(n-1):.01f,n,"",std::move(options));
        }
    }
}
std::vector<Param> build(int engine) {
    Config c=defaults(engine); std::vector<Param> out;
    const int id=registry()[size_t(engine)].firmware;
    if(id==6) {
        half(out,c,c.layer.a,c.layer.eng_a,"a."); half(out,c,c.layer.b,c.layer.eng_b,"b.");
        add(out,c,c.layer.morph,"layer.morph","Morph","LAYER",0,100,1,0,"%");
        add(out,c,c.layer.delay_b_ms,"layer.delay","Delay B","LAYER",0,255,1,0,"ms");
        add(out,c,c.layer.transpose_b,"layer.transpose","Transpose B","LAYER",-24,24,1,0,"st");
        add(out,c,c.layer.gain_a_db,"layer.gain_a","Gain A","LAYER",-60,12,1,0,"dB");
        add(out,c,c.layer.gain_b_db,"layer.gain_b","Gain B","LAYER",-60,12,1,0,"dB");
        add(out,c,c.layer.b.patch.groove[4][7],"b.move","MOVE B","LAYER",0,100,1,0,"%");
    } else half(out,c,c.layer.a,id,"");
    bus(out,c,id==6);
    return out;
}
const std::vector<std::vector<Param>>& catalogs() {
    static const auto result=[] { std::vector<std::vector<Param>> r; for(size_t i=0;i<registry().size();++i) r.push_back(registry()[i].parameters(int(i))); return r; }();
    return result;
}
}
std::vector<Param> preview_catalog(int engine) {return build(engine);}
const std::vector<Param>& catalog(int engine) { return catalogs().at(size_t(engine)); }
void initialize_catalogs() {
    static const bool validated=[] {
        for(const auto& params:catalogs()) {
            if(params.size()>MAX_PARAMS)throw std::runtime_error("catalog exceeds RT bound");
            std::set<std::string> ids;
            for(const auto& p:params) {
                const size_t bytes=p.storage==Storage::Float?sizeof(float):1;
                if(p.offset>sizeof(Config)-bytes||p.id.empty()||p.page.empty()||!ids.insert(p.id).second||
                   !std::isfinite(p.min)||!std::isfinite(p.max)||!std::isfinite(p.step)||p.min>=p.max||p.step<=0||
                   !std::isfinite(p.initial)||p.initial<p.min||p.initial>p.max)
                    throw std::runtime_error("invalid parameter descriptor: "+p.id);
            }
        }
        return true;
    }();
    (void)validated;
}
size_t param_index(int engine,const std::string& id) { const auto& ps=catalog(engine); for(size_t i=0;i<ps.size();++i) if(ps[i].id==id) return i; throw std::runtime_error("unknown parameter: "+id); }
void benchmark_patch(Config& c) {
    auto configure=[](studio::Preset& p,int id) {
        for(auto& on:p.patch.groove_on) on=false;
        p.patch.groove_on[0]=true; p.patch.groove[0][0]=35; p.patch.groove[0][1]=0; p.patch.groove[0][2]=0; p.patch.groove[0][3]=0; p.patch.groove[0][4]=180;
        p.patch.release_ms=std::min(p.patch.release_ms,100.f); p.patch.f_release_ms=std::min(p.patch.f_release_ms,100.f);
        if(id==4) for(auto& op:p.patch.osc) op[7]=std::min(op[7],100.f);
        const float mixes[]={.2f,.2f,.25f,.2f,.2f};
        const float params[5][3]={{.35f,.55f,0},{.3f,.2f,.15f},{.5f,.3f,1},{.5f,.35f,.55f},{.55f,1,.5f}};
        for(int fx=0;fx<5;++fx) { p.patch.sfx_on[fx]=true; p.patch.sfx_mix[fx]=mixes[fx]; for(int f=0;f<3;++f) p.patch.sfx_p[fx][f]=params[fx][f]; }
    };
    configure(c.layer.a,c.layer.eng_a);
    if(registry()[size_t(c.engine)].firmware==6) configure(c.layer.b,c.layer.eng_b);
}
}
