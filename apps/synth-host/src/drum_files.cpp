#include "drum_files.h"
#include "bench_support.h"
#include <cstring>
#include <fstream>
#include <map>
#include <fcntl.h>
#include <unistd.h>
namespace host {
namespace fs=std::filesystem;
const std::array<DrumDescriptor,DrumParamCount>& drum_descriptors(){
    static const std::array<DrumDescriptor,DrumParamCount> d{{
        {"tune","TUNE","st",-24,24,1,""},{"cents","FINE","ct",-100,100,1,""},
        {"attack","ATTACK","ms",0,10000,1,""},{"hold","HOLD","ms",0,60000,10,""},{"decay","DECAY","ms",1,60000,10,""},
        {"env_mode","AMP ENV","",0,1,1,"AD|HD"},{"filter_mode","FILTER","",0,1,1,"LP|HP"},{"cutoff","TONE","Hz",20,20000,50,""},
        {"resonance","RESO","",0,.95f,.01f,""},{"start","START","",0,.999f,.001f,""},{"end","END","",.001f,1,.001f,""},
        {"fade_in","FADE IN","ms",0,10000,1,""},{"fade_out","FADE OUT","ms",0,10000,1,""},
        {"pan","PAN","",-1,1,.01f,""},{"send","SEND","",0,1,.01f,""},{"velocity_curve","VEL CURVE","",0,3,1,"LINEAR|SOFT|HARD|FIXED"},
        {"velocity_level","VEL LEVEL","",0,1,.01f,""},{"velocity_tone","VEL TONE","",0,1,.01f,""},
        {"choke","CHOKE","",0,16,1,""},{"group","OUTPUT","",0,3,1,"KICKS|SKINS|WOODS|METALS"},
        {"level","LEVEL","",0,2,.01f,""},{"mute","MUTE","",0,1,1,"OFF|ON"},{"solo","SOLO","",0,1,1,"OFF|ON"}
    }};return d;
}
float drum_value(unsigned i,const Json& value){if(i>=DrumParamCount)throw std::runtime_error("unknown pad param");const auto& d=drum_descriptors()[i];const auto x=std::clamp(value.number(),double(d.min),double(d.max));return float((d.options[0]||i==Choke)?std::round(x):x);}
fs::path drum_path(const fs::path& root,const std::string& relative){
    if(relative.empty()||relative.find('\0')!=std::string::npos||fs::path(relative).is_absolute())throw std::runtime_error("expected relative drum path");
    const auto base=fs::weakly_canonical(root),p=fs::canonical(base/relative);auto b=base.begin(),v=p.begin();for(;b!=base.end();++b,++v)if(v==p.end()||*b!=*v)throw std::runtime_error("drum path escapes data directory");return p;
}
Json drum_read(const fs::path& path){if(fs::file_size(path)>2*1024*1024)throw std::runtime_error("drum JSON too large");std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("cannot read drum JSON");return Json::parse(std::string(std::istreambuf_iterator<char>(in),{}));}
void drum_write(const fs::path& path,const Json& value,bool overwrite){
    fs::create_directories(path.parent_path());const auto text=value.dump()+"\n";const auto tmp=path.string()+".tmp.XXXXXX";std::vector<char> name(tmp.begin(),tmp.end());name.push_back(0);int fd=mkstemp(name.data());if(fd<0)throw std::runtime_error("cannot create drum snapshot");
    try{size_t offset=0;while(offset<text.size()){auto n=write(fd,text.data()+offset,text.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0)throw std::runtime_error("drum write failed");offset+=size_t(n);}if(fsync(fd))throw std::runtime_error("drum fsync failed");close(fd);fd=-1;
        if(overwrite){if(rename(name.data(),path.c_str()))throw std::runtime_error("drum rename failed");}else{if(link(name.data(),path.c_str()))throw std::runtime_error("rack exists");unlink(name.data());}
        int dir=open(path.parent_path().c_str(),O_RDONLY);if(dir>=0){fsync(dir);close(dir);}
    }catch(...){if(fd>=0)close(fd);unlink(name.data());throw;}
}
static unsigned u16(const uint8_t* p){return unsigned(p[0])|(unsigned(p[1])<<8);}
static uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
std::unique_ptr<DrumSample> drum_decode(const fs::path& path,const std::string& hash,size_t remaining){
    const auto bytes=fs::file_size(path);if(bytes<44||bytes>DRUM_RAM+65536)throw std::runtime_error("invalid WAV size / RAM budget");
    // Read headers first, before allocating file bytes or decoded PCM.
    std::ifstream in(path,std::ios::binary);uint8_t header[12]{};in.read(reinterpret_cast<char*>(header),12);
    if(std::memcmp(header,"RIFF",4)||std::memcmp(header+8,"WAVE",4)||uint64_t(u32(header+4))+8>bytes)throw std::runtime_error("expected RIFF WAVE");
    unsigned format=0,channels=0,rate=0,bits=0,align=0;size_t data_at=0,data_size=0;
    for(uint64_t offset=12;offset+8<=bytes;){uint8_t chunk[8];in.seekg(std::streamoff(offset));in.read(reinterpret_cast<char*>(chunk),8);const auto size=u32(chunk+4);if(offset+8+size>bytes)throw std::runtime_error("truncated WAV chunk");
        if(!std::memcmp(chunk,"fmt ",4)){if(size<16)throw std::runtime_error("short WAV format");uint8_t fmt[40]{};in.read(reinterpret_cast<char*>(fmt),std::min<unsigned>(40,size));format=u16(fmt);channels=u16(fmt+2);rate=u32(fmt+4);align=u16(fmt+12);bits=u16(fmt+14);if(format==65534&&size>=40)format=u16(fmt+24);}
        else if(!std::memcmp(chunk,"data",4)){data_at=size_t(offset+8);data_size=size;}
        offset+=8+size+(size%2);
    }
    if((format!=1&&format!=3)||channels<1||channels>2||rate<8000||rate>192000||(format==3?bits!=32:(bits!=16&&bits!=24&&bits!=32))||!align||align!=channels*(bits/8)||!data_at||!data_size||data_size%align)throw std::runtime_error("unsupported WAV format");
    const size_t source_frames=data_size/align;const size_t frames=size_t(std::llround(double(source_frames)*48000/rate));
    if(frames<2||frames>remaining/(channels*sizeof(float)))throw std::runtime_error("RAM budget exceeded (256 MiB)");
    std::vector<uint8_t> raw(bytes);in.clear();in.seekg(0);in.read(reinterpret_cast<char*>(raw.data()),std::streamsize(bytes));if(!in)throw std::runtime_error("WAV read failed");
    if(hash.size()!=64||bench::sha256(raw)!=hash)throw std::runtime_error("WAV hash mismatch");
    auto sample=std::make_unique<DrumSample>();sample->channels=channels;sample->pcm.resize(frames*channels);
    auto get=[&](size_t f,unsigned ch){const auto* p=raw.data()+data_at+f*align+ch*(bits/8);float value=0;if(format==3){uint32_t word=u32(p);std::memcpy(&value,&word,4);}else if(bits==16)value=float(int16_t(u16(p)))/32768;else if(bits==24){int32_t word=int32_t(uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16));if(word&0x800000)word|=int32_t(0xff000000);value=float(word)/8388608;}else value=float(int32_t(u32(p)))/2147483648.f;if(!std::isfinite(value))throw std::runtime_error("nonfinite WAV sample");return value;};
    for(size_t f=0;f<frames;++f){const double source=double(f)*rate/48000;const size_t a=std::min(size_t(source),source_frames-1),b=std::min(a+1,source_frames-1);const float t=float(source-double(a));for(unsigned ch=0;ch<channels;++ch)sample->pcm[f*channels+ch]=get(a,ch)*(1-t)+get(b,ch)*t;}return sample;
}
DrumLane drum_lane(const Json& j){DrumLane lane;const auto length=j.at("length").integer();if(length<1||length>32)throw std::runtime_error("lane length must be 1..32");lane.length=unsigned(length);const auto steps=j.at("steps");if(steps.size()!=32)throw std::runtime_error("lane needs 32 steps");for(unsigned i=0;i<32;++i){const auto s=steps.at(i);const auto velocity=s.at("velocity").integer();const double probability=s.at("probability").number(),micro=s.at("micro").number();if(velocity<0||velocity>127||probability<0||probability>1||micro<-.5||micro>.5)throw std::runtime_error("invalid step");lane.steps[i]={uint8_t(velocity),float(probability),float(micro)};}return lane;}
std::unique_ptr<DrumRack> drum_prepare(const Json& j,const fs::path& root,size_t remaining){
    if(j.at("schema_version").integer()!=1)throw std::runtime_error("unsupported rack schema");(void)j.at("id").str();(void)j.at("name").str();
    auto rack=std::make_unique<DrumRack>();if(j.has("selected_pattern")){auto active=j.at("selected_pattern").integer();if(active<0||active>7)throw std::runtime_error("invalid selected pattern");rack->active_pattern=unsigned(active);}rack->bpm=j.at("bpm").number();rack->swing=j.at("swing").number();if(rack->bpm<60||rack->bpm>200||rack->swing<0||rack->swing>.5)throw std::runtime_error("invalid rack tempo");
    const auto sources=j.at("sources");for(size_t i=0;i<sources.size();++i){auto s=sources.at(i);auto kind=s.at("kind").str();if(kind!="ENGINE"&&kind!="IMPORTED"&&kind!="RECORDED")throw std::runtime_error("invalid source kind");(void)s.at("licence_note").str();}
    const auto pads=j.at("pads");if(pads.size()!=16)throw std::runtime_error("rack needs 16 pads");std::map<std::string,const DrumSample*> decoded;
    for(unsigned i=0;i<16;++i){auto pad=pads.at(i);auto& target=rack->pads[i];const auto params=pad.at("params");for(unsigned k=0;k<DrumParamCount;++k)if(params.has(drum_descriptors()[k].id))target.params.v[k]=drum_value(k,params.at(drum_descriptors()[k].id));if(target.params.v[Start]>=target.params.v[End])throw std::runtime_error("start must precede end");
        auto mode=pad.at("layer_mode").str();if(mode!="velocity"&&mode!="round_robin")throw std::runtime_error("invalid layer mode");target.round_robin=mode=="round_robin";auto layers=pad.at("layers");if(layers.size()>4)throw std::runtime_error("up to four layers");target.count=unsigned(layers.size());
        std::array<unsigned,128> coverage{};
        for(unsigned k=0;k<target.count;++k){auto layer=layers.at(k);const auto hash=layer.at("hash").str();auto path=drum_path(root,layer.at("path").str());const auto low=layer.at("velocity_min").integer(),high=layer.at("velocity_max").integer();if(low<1||high>127||low>high)throw std::runtime_error("invalid velocity range");for(int64_t v=low;v<=high;++v)++coverage[size_t(v)];
            const DrumSample* sample=nullptr;auto it=decoded.find(hash);if(it==decoded.end()){auto owned=drum_decode(path,hash,remaining-rack->bytes);rack->bytes+=owned->pcm.size()*sizeof(float);sample=owned.get();rack->samples.push_back(std::move(owned));decoded.emplace(hash,sample);}else{sample=it->second;/* The path was contained; require identical content for duplicate refs too. */if(path.filename()!=hash+".wav")throw std::runtime_error("duplicate hash refs must use canonical filename");}
            target.layers[k]={sample,unsigned(low),unsigned(high)};
        }
        if(target.count)for(unsigned v=1;v<=127;++v)if(!coverage[v]||(!target.round_robin&&coverage[v]!=1))throw std::runtime_error("velocity layers must cover 1..127 without overlap");
    }
    const auto patterns=j.at("patterns");if(patterns.size()!=8)throw std::runtime_error("rack needs eight patterns");for(unsigned p=0;p<8;++p){auto lanes=patterns.at(p).at("lanes");if(lanes.size()!=16)throw std::runtime_error("pattern needs 16 lanes");for(unsigned l=0;l<16;++l)rack->patterns[p].lanes[l]=drum_lane(lanes.at(l));}
    auto groups=j.at("groups");if(groups.size()!=4)throw std::runtime_error("rack needs four groups");for(unsigned g=0;g<4;++g){auto item=groups.at(g);float level=float(item.at("level").number());if(level<0||level>2)throw std::runtime_error("invalid group level");rack->groups[g]={level,item.at("mute").boolean()};}return rack;
}
Json drum_empty(){
    Json pads=Json::array(),patterns=Json::array(),groups=Json::array();for(unsigned p=0;p<16;++p){Json params;DrumParams defaults;for(unsigned k=0;k<DrumParamCount;++k)params.set(drum_descriptors()[k].id,defaults.v[k]);pads.push(object({{"name","EMPTY"},{"source_id",""},{"category","perc"},{"layers",Json::array()},{"layer_mode","velocity"},{"params",params}}));}
    for(unsigned p=0;p<8;++p){Json lanes=Json::array();for(unsigned l=0;l<16;++l){Json steps=Json::array();for(unsigned s=0;s<32;++s)steps.push(object({{"velocity",0},{"probability",1.},{"micro",0.}}));lanes.push(object({{"length",16},{"steps",steps}}));}patterns.push(object({{"lanes",lanes}}));}
    for(const char* name:{"KICKS","SKINS","WOODS","METALS"})groups.push(object({{"name",name},{"level",1.},{"mute",false}}));
    return object({{"schema_version",1},{"id","empty"},{"name","NO RACK"},{"root","A"},{"mode","menor"},{"tags",Json::array()},{"sources",Json::array()},{"pads",pads},{"patterns",patterns},{"groups",groups},{"bpm",122.},{"swing",0.}});
}
Json drum_metadata(){
    const std::array<std::array<unsigned,8>,4> pages{{{{Tune,Decay,Cutoff,Start,Pan,Send,VelocityCurve,Choke}},{{Start,End,FadeIn,FadeOut,Attack,Hold,EnvMode,Cents}},{{FilterMode,Cutoff,Resonance,VelocityLevel,VelocityTone,Choke,Group,Level}},{{Mute,Solo,Tune,Cents,Attack,Hold,Decay,Pan}}}};
    const char* names[]={"SOUND","TRIM / ENV","FILTER / MIX","PERFORM"};Json result=Json::array();for(unsigned p=0;p<4;++p){Json bank=Json::array();for(unsigned k:pages[p]){const auto& d=drum_descriptors()[k];Json options=Json::array();std::istringstream parts(d.options);std::string word;while(std::getline(parts,word,'|'))options.push(word);bank.push(object({{"id",d.id},{"name",d.name},{"unit",d.unit},{"min",d.min},{"max",d.max},{"step",d.step},{"default",DrumParams{}.v[k]},{"kind",d.options[0]?"enum":d.min<0?"bipolar":"cont"},{"options",options}}));}Json banks=Json::array();banks.push(bank);result.push(object({{"name",names[p]},{"banks",banks}}));}return result;
}
}
