#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "drum_service.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <spawn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
namespace host {
namespace fs=std::filesystem;
namespace {
unsigned index(const Json& j,const char* field,unsigned bound){auto x=j.at(field).integer();if(x<0||uint64_t(x)>=bound)throw std::runtime_error(std::string(field)+" out of range");return unsigned(x);}
std::string safe_name(const std::string& s){if(s.empty()||s.size()>96||s=="."||s==".."||s.find_first_of("/\\")!=std::string::npos)throw std::runtime_error("invalid rack name");for(unsigned char c:s)if(c<32||c==127)throw std::runtime_error("invalid rack name");return s;}
Json clock_json(const DrumClock& c,const std::string& epoch){const double steps=double(c.sample-c.origin)*c.bpm/(48000*15);return object({{"audio_epoch",epoch+"-"+std::to_string(c.epoch)},{"sample_counter",std::to_string(c.sample)},{"origin_sample",std::to_string(c.origin)},{"pattern_origin_sample",std::to_string(c.pattern_origin)},{"pattern_step",std::to_string(c.running?uint64_t(double(c.sample-c.pattern_origin)*c.bpm/(48000*15)):0)},{"rev",std::to_string(c.rev)},{"running",c.running},{"armed",c.armed},{"pattern",int(c.pattern)},{"queued",c.queued},{"bpm",c.bpm},{"swing",c.swing},{"bar",c.running?int64_t(steps/16)+1:1},{"step",c.running?int64_t(steps)%16:0},{"voice_steals",int64_t(c.steals)},{"clipped",int64_t(c.clipped)},{"dropped",int64_t(c.dropped)},{"record_overruns",int64_t(c.record_overruns)}});}
}
DrumService::DrumService(Drum& audio,DrumOptions options,std::function<void(Json)> events):audio_(audio),options_(std::move(options)),event_(std::move(events)){
    epoch_+="-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    if(options_.data.empty()){const char* env=std::getenv("GF_DRUM_DATA"),*home=std::getenv("HOME");options_.data=env?env:std::string(home?home:"/tmp")+"/.local/share/gf-drum";}
    options_.data=fs::absolute(options_.data).string();
    if(options_.checkpoint&&fs::exists(fs::path(options_.data)/"last-drum.json")){
        try{auto j=drum_read(fs::path(options_.data)/"last-drum.json");auto rack=drum_prepare(j,options_.data);resident_=rack->bytes;manifest_=j;DrumCommand c;c.op=DrumOp::Rack;c.rack=rack.get();c.token=++token_;if(!audio_.commands.push(c))throw std::runtime_error("drum startup queue full");pending_command_=c;pending_=true;response_=[](Json){};reply_=object({{"ok",true}});rack.release();}
        catch(const std::exception& e){std::cerr<<"DRUM restore ignored: "<<e.what()<<'\n';}
    }
}
DrumService::~DrumService(){if(job_fd_>=0)close(job_fd_);if(job_>0&&!job_exited_){kill(job_,SIGTERM);waitpid(job_,nullptr,0);}try{if(dirty_)snapshot();}catch(const std::exception& e){std::cerr<<"DRUM checkpoint failed: "<<e.what()<<'\n';}}
void DrumService::mark_dirty(){if(!dirty_)dirty_since_=std::chrono::steady_clock::now();dirty_=true;}
void DrumService::snapshot(){if(options_.checkpoint)drum_write(fs::path(options_.data)/"last-drum.json",manifest_);dirty_=false;}
void DrumService::submit(DrumCommand c,Json reply,std::function<void(Json)> response,std::function<void()> commit){c.token=++token_;c.epoch=clock_.epoch;if(!audio_.commands.push(c))throw std::runtime_error("drum audio queue full");pending_command_=c;pending_=true;reply_=std::move(reply);response_=std::move(response);commit_=std::move(commit);}
Json DrumService::samples(unsigned offset,unsigned limit,bool& more,unsigned& next) const {
    Json list=Json::array();more=false;next=offset;
    auto dir=fs::path(options_.data)/"racks";if(!fs::exists(dir))return list;
    std::vector<fs::path> paths;for(const auto& f:fs::directory_iterator(dir))if(f.path().extension()==".json"&&!f.is_symlink()){paths.push_back(f.path());if(paths.size()>=1024)break;}
    std::sort(paths.begin(),paths.end());unsigned position=0;size_t bytes=0;
    for(const auto& file:paths){
        Json rack;try{rack=drum_read(file);}catch(const std::exception&){continue;}
        auto pads=rack.at("pads"),sources=rack.at("sources");
        for(size_t p=0;p<pads.size();++p){auto pad=pads.at(p);auto layers=pad.at("layers");
            for(size_t l=0;l<layers.size();++l){
                if(position++<offset)continue;
                Json source;for(size_t k=0;k<sources.size();++k)if(sources.at(k).at("id").str()==pad.at("source_id").str())source=sources.at(k);
                auto entry=object({{"name",pad.at("name")},{"category",pad.at("category")},{"source",source},{"layer",layers.at(l)},{"recipe",pad.has("recipe")?pad.at("recipe"):Json(nullptr)}});
                const size_t size=entry.dump().size();if(size>120*1024)throw std::runtime_error("catalog entry exceeds IPC budget");
                if(list.size()>=limit||bytes+size>120*1024){more=true;return list;}
                list.push(entry);bytes+=size;++next;
            }
        }
    }
    return list;
}
void DrumService::start_job(const Json& j,Json& reply){
    if(job_>0)throw std::runtime_error("drum worker busy");if(!fs::is_regular_file(options_.script))throw std::runtime_error("drum kit worker unavailable; configure --drum-kit-script");
    const auto cmd=j.at("cmd").str();Json spec=j;spec.set("data_dir",options_.data);
    if(cmd=="generate_variations"){unsigned pad=index(j,"pad",16);auto p=manifest_.at("pads").at(pad);if(!p.has("recipe")||!p.at("recipe").is(json_type_object))throw std::runtime_error("variations require ENGINE pad");bool engine=false;auto sources=manifest_.at("sources");for(size_t k=0;k<sources.size();++k)if(sources.at(k).at("id").str()==p.at("source_id").str())engine=sources.at(k).at("kind").str()=="ENGINE";if(!engine)throw std::runtime_error("variations require ENGINE pad");auto count=j.at("count").integer();auto spread=j.at("spread").number();if(count<1||count>8||spread<0||spread>1)throw std::runtime_error("invalid variations request");spec.set("recipe",p.at("recipe"));}
    else if(cmd=="import_folder"){(void)j.at("path").str();(void)j.at("licence_note").str();if(j.has("trim"))(void)j.at("trim").boolean();}
    job_id_=std::to_string(getpid())+"-"+std::to_string(++job_serial_);const auto jobfile=fs::path(options_.data)/"jobs"/(job_id_+".json");drum_write(jobfile,spec);
    int fds[2];if(pipe(fds))throw std::runtime_error("worker pipe failed");fcntl(fds[0],F_SETFD,FD_CLOEXEC);fcntl(fds[1],F_SETFD,FD_CLOEXEC);
    posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);posix_spawn_file_actions_adddup2(&actions,fds[1],STDOUT_FILENO);posix_spawn_file_actions_addclose(&actions,fds[0]);posix_spawn_file_actions_addclose(&actions,fds[1]);
    std::vector<std::string> args={options_.python,options_.script,"worker","--job",jobfile.string()};std::vector<char*> argv;for(auto& a:args)argv.push_back(a.data());argv.push_back(nullptr);
    const auto result=posix_spawnp(&job_,options_.python.c_str(),&actions,nullptr,argv.data(),environ);posix_spawn_file_actions_destroy(&actions);close(fds[1]);if(result){close(fds[0]);job_=-1;throw std::runtime_error("cannot spawn drum worker");}job_exited_=false;job_status_=0;job_fd_=fds[0];fcntl(job_fd_,F_SETFL,O_NONBLOCK);job_buffer_.clear();reply.set("job_id",job_id_);
}
bool DrumService::request(const Json& j,std::function<void(Json)> response){
    const auto cmd=j.at("cmd").str();
    static const std::vector<std::string> commands={"get_drum_state","list_pad_params","load_rack","set_pad_param","pad_trigger","set_steps","set_pattern","transport","record_arm","set_group","list_racks","save_rack","list_drum_samples","set_pad_sample","preview","import_folder","generate_variations","render_rack","save_pattern"};
    if(std::find(commands.begin(),commands.end(),cmd)==commands.end())return false;
    Json reply=object({{"id",j.at("id")},{"ok",true}});DrumCommand c;std::function<void()> commit=[]{};
    if(cmd=="get_drum_state"){reply.set("rack",manifest_);reply.set("clock",clock_json(clock_,epoch_));reply.set("ram_bytes",int64_t(resident_+preview_bytes_));reply.set("ram_limit",int64_t(DRUM_RAM));reply.set("data_dir",options_.data);}
    else if(cmd=="list_pad_params")reply.set("pages",drum_metadata());
    else if(cmd=="list_racks"){Json racks=Json::array();auto dir=fs::path(options_.data)/"racks";if(fs::exists(dir))for(const auto& f:fs::directory_iterator(dir)){if(racks.size()>=256)break;if(f.path().extension()!=".json"||f.is_symlink())continue;try{auto rack=drum_read(f.path());racks.push(object({{"name",rack.at("name")},{"path",fs::relative(f.path(),options_.data).string()},{"id",rack.at("id")}}));}catch(const std::exception&) {}}reply.set("racks",racks);}
    else if(cmd=="list_drum_samples"){
        const unsigned offset=j.has("offset")?index(j,"offset",65537):0;
        const unsigned limit=j.has("limit")?index(j,"limit",129):64;if(!limit)throw std::runtime_error("limit must be 1..128");
        bool more=false;unsigned next=offset;reply.set("samples",samples(offset,limit,more,next));reply.set("has_more",more);reply.set("next_offset",int(next));
    }
    else if(cmd=="save_rack"||cmd=="save_pattern"){
        const auto name=safe_name(j.at("name").str());auto copy=Json::parse(manifest_.dump());copy.set("name",name);const bool overwrite=j.has("overwrite")&&j.at("overwrite").boolean();
        if(cmd=="save_pattern"){auto path=fs::path(options_.data)/"patterns"/(name+".json");drum_write(path,object({{"schema_version",1},{"name",name},{"pattern",manifest_.at("patterns").at(clock_.pattern)}}),overwrite);reply.set("path",fs::relative(path,options_.data).string());}
        else{copy.set("id",name);const auto path=fs::path(options_.data)/"racks"/(name+".json");drum_write(path,copy,overwrite);manifest_=copy;mark_dirty();reply.set("path",fs::relative(path,options_.data).string());if(dirty_)event_(object({{"event","drum"},{"rack",manifest_}}));}
    }
    else if(cmd=="import_folder"||cmd=="generate_variations"||cmd=="render_rack"){start_job(j,reply);}
    else if(cmd=="load_rack"||cmd=="set_pad_sample"){
        Json next=cmd=="load_rack"?drum_read(drum_path(options_.data,j.at("path").str())):Json::parse(manifest_.dump());
        if(cmd=="set_pad_sample"){unsigned pad=index(j,"pad",16);auto p=next.at("pads").at(pad);Json layers=Json::array();layers.push(j.at("layer"));p.set("layers",layers);p.set("layer_mode","velocity");if(j.has("name"))p.set("name",j.at("name"));if(j.has("source")){auto source=j.at("source");p.set("source_id",source.at("id"));bool found=false;auto sources=next.at("sources");for(size_t k=0;k<sources.size();++k)found|=sources.at(k).at("id").str()==source.at("id").str();if(!found)sources.push(source);}p.set("recipe",j.has("recipe")?j.at("recipe"):Json(nullptr));}
        if(clock_.running){next.set("bpm",clock_.bpm);next.set("swing",clock_.swing);}
        auto rack=drum_prepare(next,options_.data,DRUM_RAM-resident_-preview_bytes_);const size_t bytes=rack->bytes;c.op=DrumOp::Rack;c.flag=cmd=="set_pad_sample"||clock_.running;c.rack=rack.get();commit=[this,next,bytes]{manifest_=next;manifest_.set("selected_pattern",int(clock_.pattern));resident_=bytes;mark_dirty();};submit(c,reply,std::move(response),commit);rack.release();return true;
    }
    else if(cmd=="set_pad_param"){
        c.op=DrumOp::Param;c.pad=index(j,"pad",16);const auto name=j.at("param").str();unsigned param=0;for(;param<DrumParamCount;++param)if(name==drum_descriptors()[param].id)break;if(param==DrumParamCount)throw std::runtime_error("unknown pad param");c.param=param;c.value=drum_value(param,j.at("value"));auto params=manifest_.at("pads").at(c.pad).at("params");if((param==Start&&c.value>=params.at("end").number())||(param==End&&c.value<=params.at("start").number()))throw std::runtime_error("start must precede end");reply.set("value",c.value);commit=[this,c,name]{manifest_.at("pads").at(c.pad).at("params").set(name,c.value);mark_dirty();};
    }
    else if(cmd=="pad_trigger"){c.op=DrumOp::Trigger;c.pad=index(j,"pad",16);const auto vel=index(j,"velocity",128);if(vel==0)throw std::runtime_error("velocity must be 1..127");c.value=float(vel);c.flag=j.has("cue")&&j.at("cue").boolean();}
    else if(cmd=="preview"){
        c.op=DrumOp::Preview;
        if(!(j.has("stop")&&j.at("stop").boolean())){Json layer;if(j.has("pad")){const auto pad=index(j,"pad",16);auto layers=manifest_.at("pads").at(pad).at("layers");if(layers.size()==0)throw std::runtime_error("empty pad");c.op=DrumOp::Trigger;c.pad=pad;c.value=127;c.flag=true;submit(c,reply,std::move(response),[]{});return true;}else layer=j.at("layer");auto sample=drum_decode(drum_path(options_.data,layer.at("path").str()),layer.at("hash").str(),DRUM_RAM-resident_-preview_bytes_);const auto bytes=sample->pcm.size()*sizeof(float);c.sample=sample.get();submit(c,reply,std::move(response),[this,bytes]{preview_bytes_=bytes;});sample.release();return true;}commit=[this]{preview_bytes_=0;};
    }
    else if(cmd=="set_pattern"){c.op=DrumOp::Select;c.pattern=index(j,"pattern",8);reply.set("queued",clock_.running);reply.set("pattern",int(c.pattern));}
    else if(cmd=="set_steps"){c.op=DrumOp::Steps;c.pattern=index(j,"pattern",8);c.pad=index(j,"lane",16);c.lane=drum_lane(j);auto lane=object({{"length",j.at("length")},{"steps",j.at("steps")}});commit=[this,c,lane]{manifest_.at("patterns").at(c.pattern).at("lanes").set_at(c.pad,lane);mark_dirty();};}
    else if(cmd=="transport"){
        c.op=DrumOp::Transport;const auto action=j.at("action").str();if(action!="play"&&action!="stop")throw std::runtime_error("transport action must be play/stop");c.flag=action=="play";c.value=0;c.extra=-1;
        if(j.has("bpm")){const auto bpm=j.at("bpm").number();if(bpm<60||bpm>200)throw std::runtime_error("bpm must be 60..200");if(clock_.running&&bpm!=clock_.bpm)throw std::runtime_error("stop before changing BPM");c.value=float(bpm);}
        if(j.has("swing")){const auto swing=j.at("swing").number();if(swing<0||swing>.5)throw std::runtime_error("invalid swing");c.extra=float(swing);}commit=[this,c]{if(c.value>0)manifest_.set("bpm",c.value);if(c.extra>=0)manifest_.set("swing",c.extra);mark_dirty();};
    }
    else if(cmd=="record_arm"){c.op=DrumOp::Record;c.flag=j.at("armed").boolean();c.param=j.has("quantize")?unsigned(j.at("quantize").integer()):16;if(c.param!=16&&c.param!=32)throw std::runtime_error("quantize must be 16/32");}
    else if(cmd=="set_group"){
        c.op=DrumOp::Group;c.pad=index(j,"group",4);auto g=manifest_.at("groups").at(c.pad);c.value=float(j.has("level")?j.at("level").number():g.at("level").number());c.flag=j.has("mute")?j.at("mute").boolean():g.at("mute").boolean();if(c.value<0||c.value>2)throw std::runtime_error("group level out of range");const auto name=j.has("name")?safe_name(j.at("name").str()):g.at("name").str();commit=[this,c,name]{auto g=manifest_.at("groups").at(c.pad);g.set("level",c.value);g.set("mute",c.flag);g.set("name",name);mark_dirty();};
    }
    else return false;
    if(cmd=="set_pad_param"||cmd=="pad_trigger"||cmd=="preview"||cmd=="set_pattern"||cmd=="set_steps"||cmd=="transport"||cmd=="record_arm"||cmd=="set_group"){submit(c,reply,std::move(response),commit);return true;}
    response(reply);return true;
}
void DrumService::poll(){
    DrumAck ack;while(audio_.acknowledgements.pop(ack)){
        delete ack.retired;delete ack.retired_preview;
        if(ack.token||ack.clock.rev)clock_=ack.clock;
        if(ack.recorded){auto lane=manifest_.at("patterns").at(ack.pattern).at("lanes").at(ack.pad);auto hit=object({{"velocity",int(ack.hit.velocity)},{"probability",ack.hit.probability},{"micro",ack.hit.micro}});lane.at("steps").set_at(ack.step,hit);mark_dirty();event_(object({{"event","recorded"},{"pattern",int(ack.pattern)},{"lane",int(ack.pad)},{"step",int(ack.step)},{"hit",hit}}));}
        else if(pending_&&ack.token==token_){
            if(commit_&&!ack.stale)commit_();if(!ack.stale&&pending_command_.op==DrumOp::Select&&clock_.queued<0){manifest_.set("selected_pattern",int(clock_.pattern));mark_dirty();}pending_=false;commit_={};const auto& c=pending_command_;
            reply_.set("ram_bytes",int64_t(resident_+preview_bytes_));
            if(ack.rejected){reply_.set("ok",false);reply_.set("error",ack.stale?"stale audio epoch; request fresh state":"voice capacity: cue cannot steal ROOM voices");}
            if(c.op==DrumOp::Transport||c.op==DrumOp::Select||c.op==DrumOp::Record)reply_.set("clock",clock_json(clock_,epoch_));
            response_(reply_);response_={};
            if(ack.stale)return;
            if(c.op==DrumOp::Rack)event_(object({{"event","drum"},{"rack",manifest_}}));
            else if(c.op==DrumOp::Param)event_(object({{"event","drum"},{"kind","pad_param"},{"pad",int(c.pad)},{"param",drum_descriptors()[c.param].id},{"value",c.value}}));
            else if(c.op==DrumOp::Steps)event_(object({{"event","drum"},{"kind","steps"},{"pattern",int(c.pattern)},{"lane",int(c.pad)},{"data",manifest_.at("patterns").at(c.pattern).at("lanes").at(c.pad)}}));
            else if(c.op==DrumOp::Group)event_(object({{"event","drum"},{"kind","group"},{"group",int(c.pad)},{"data",manifest_.at("groups").at(c.pad)}}));
        }
    }
    DrumClock c;bool clock=false;while(audio_.clocks.pop(c)){if(c.sample>=clock_.sample&&c.rev>=clock_.rev){if(c.pattern!=clock_.pattern){manifest_.set("selected_pattern",int(c.pattern));mark_dirty();}clock_=c;clock=true;}}if(clock){event_(object({{"event","clock"},{"clock",clock_json(clock_,epoch_)}}));Json pads=Json::array(),groups=Json::array();for(auto v:clock_.pads)pads.push(v);for(auto v:clock_.groups)groups.push(v);event_(object({{"event","drum_meters"},{"pads",pads},{"groups",groups},{"room",clock_.room},{"cue",clock_.cue}}));}
    if(dirty_&&!pending_&&std::chrono::steady_clock::now()-dirty_since_>=std::chrono::milliseconds(100))try{snapshot();}catch(const std::exception& e){dirty_=false;event_(object({{"event","drum_error"},{"error",e.what()}}));}
    bool eof=false;
    if(job_fd_>=0){
        for(unsigned batch=0;batch<8;++batch){
            char buffer[8192];const auto n=read(job_fd_,buffer,sizeof(buffer));
            if(n==0){eof=true;break;}if(n<0){if(errno!=EAGAIN&&errno!=EINTR)eof=true;break;}
            job_buffer_.append(buffer,size_t(n));size_t end;
            while((end=job_buffer_.find('\n'))!=std::string::npos){
                auto line=job_buffer_.substr(0,end);job_buffer_.erase(0,end+1);
                if(line.size()>65536){if(!job_exited_)kill(job_,SIGTERM);continue;}
                try{auto e=Json::parse(line);e.set("event","drum_job");e.set("job_id",job_id_);event_(e);}catch(const std::exception&){}
            }
            if(job_buffer_.size()>65536){job_buffer_.clear();if(!job_exited_)kill(job_,SIGTERM);}
        }
    }
    if(job_>0&&!job_exited_){const auto pid=waitpid(job_,&job_status_,WNOHANG);if(pid==job_)job_exited_=true;}
    if(job_exited_&&eof){
        if(!WIFEXITED(job_status_)||WEXITSTATUS(job_status_)!=0)event_(object({{"event","drum_job"},{"job_id",job_id_},{"status","error"},{"error","worker exited unsuccessfully"}}));
        close(job_fd_);job_fd_=-1;job_=-1;job_exited_=false;
    }
}
}
