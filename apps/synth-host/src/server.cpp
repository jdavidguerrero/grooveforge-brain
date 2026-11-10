#include "server.h"
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <poll.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#ifdef GF_HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif
namespace host {
namespace fs=std::filesystem;
namespace {
using Clock=std::chrono::steady_clock;
std::string read_file(const fs::path& path) {
    if(fs::file_size(path)>65536)throw std::runtime_error("file too large: "+path.string());
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("cannot read: "+path.string());
    return std::string(std::istreambuf_iterator<char>(in),{});
}
void write_all(int fd,const std::string& data) {
    size_t offset=0;
    while(offset<data.size()) {const auto n=write(fd,data.data()+offset,data.size()-offset);if(n<0&&errno==EINTR)continue;if(n<=0)throw std::runtime_error("write failed");offset+=size_t(n);}
}
void atomic_write(const fs::path& path,const std::string& data,bool exclusive=false) {
    fs::create_directories(path.parent_path());
    const auto temp=path.string()+".tmp.XXXXXX";std::vector<char> name(temp.begin(),temp.end());name.push_back(0);
    int fd=mkstemp(name.data());if(fd<0)throw std::runtime_error("cannot create state temporary");
    try {
        write_all(fd,data);if(fsync(fd)!=0)throw std::runtime_error("fsync failed");close(fd);fd=-1;
        if(exclusive) {if(link(name.data(),path.c_str())!=0)throw std::runtime_error("preset exists or cannot create file");if(unlink(name.data())!=0)throw std::runtime_error("cannot remove temporary");}
        else if(rename(name.data(),path.c_str())!=0)throw std::runtime_error("cannot replace state");
        const int dir=open(path.parent_path().c_str(),O_RDONLY);if(dir>=0){fsync(dir);close(dir);}
    } catch(...) {if(fd>=0)close(fd);unlink(name.data());throw;}
}
bool valid_name(const std::string& name) {if(name.empty()||name.size()>96||name=="."||name=="..")return false;for(unsigned char c:name)if(c<32||c=='/'||c=='\\'||c==127)return false;return true;}
std::string hex(const uint8_t* data,size_t n) {constexpr char digits[]="0123456789abcdef";std::string s;s.reserve(n*2);for(size_t i=0;i<n;++i){s+=digits[data[i]>>4];s+=digits[data[i]&15];}return s;}
std::vector<uint8_t> unhex(const std::string& s) {if(s.size()%2||s.size()>16000)throw std::runtime_error("invalid preset data");std::vector<uint8_t> b;auto nib=[](char c)->unsigned{if(c>='0'&&c<='9')return unsigned(c-'0');if(c>='a'&&c<='f')return unsigned(c-'a'+10);throw std::runtime_error("invalid hex");};for(size_t i=0;i<s.size();i+=2)b.push_back(uint8_t(nib(s[i])*16+nib(s[i+1])));return b;}
Json config_json(const Config& c) {
    const auto& entry=registry().at(size_t(c.engine));
    const auto data=entry.encode(c);
    return object({{"engine",entry.name},{"data",hex(data.data(),data.size())}});
}
Config config_from(const Json& j) {
    const int e=engine_index(j.at("engine").str());const auto data=unhex(j.at("data").str());
    Config c=registry()[size_t(e)].decode(e,data.data(),data.size());
    if(c.engine!=e)throw std::runtime_error("preset adapter returned wrong engine");
    // Metadata/codec constraints are checked before an engine can reach audio.
    for(const auto& p:catalog(e)) if(!std::isfinite(p.read(c)))throw std::runtime_error("nonfinite patch");
    return c;
}
Json preset_json(const Preset& p) {return object({{"format",1},{"name",p.name},{"role",p.role},{"patch",config_json(p.config)},{"tags",p.tags},{"favorite",p.favorite}});}
Preset preset_from(const Json& j) {
    if(j.at("format").integer()!=1)throw std::runtime_error("unsupported preset format");
    Preset p;p.name=j.at("name").str();if(!valid_name(p.name))throw std::runtime_error("invalid preset name");
    p.config=config_from(j.at("patch"));p.role=j.at("role").str();p.tags=j.at("tags");
    for(size_t i=0;i<p.tags.size();++i)(void)p.tags.at(i).str();p.favorite=j.at("favorite").boolean();return p;
}
Json values(const PartState& p,const std::string& page="") {
    Json result;bool found=page.empty();for(const auto& d:catalog(p.config.engine))if(page.empty()||d.page==page){result.set(d.id,d.read(p.config));found=true;}
    if(!found)throw std::runtime_error("unknown page: "+page);return result;
}
Json params_json(int e) {
    Json pages=Json::array();std::vector<std::string> names;
    for(const auto& p:catalog(e))if(std::find(names.begin(),names.end(),p.page)==names.end())names.push_back(p.page);
    for(const auto& name:names) {
        Json banks=Json::array(),bank=Json::array();
        for(const auto& p:catalog(e))if(p.page==name) {
            Json param=object({{"id",p.id},{"name",p.name},{"unit",p.unit},{"min",p.min},{"max",p.max},{"default",p.initial},{"step",p.step},{"kind",p.kind()}});
            if(p.options>0){Json opts=Json::array();for(int i=0;i<p.options;++i)opts.push(i<int(p.labels.size())?p.labels[size_t(i)]:std::to_string(p.min+float(i)*p.step));param.set("options",opts);}
            bank.push(param);if(bank.size()==8){banks.push(bank);bank=Json::array();}
        }
        if(bank.size())banks.push(bank);pages.push(object({{"name",name},{"banks",banks}}));
    }
    return object({{"pages",pages}});
}
unsigned part_number(const Json& j) {const auto p=j.at("part").integer();if(p<1||p>4)throw std::runtime_error("part must be 1..4");return unsigned(p-1);}
int midi_number(const Json& j,const char* key) {const auto x=j.at(key).integer();if(x<0||x>127)throw std::runtime_error(std::string(key)+" must be 0..127");return int(x);}
std::string role_for(std::string name) {if(name.find("BASS")!=std::string::npos)return "BASS";if(name.find("PAD")!=std::string::npos||name.find("STR")!=std::string::npos)return "PAD";if(name.find("LEAD")!=std::string::npos)return "LEAD";return "CHORDS";}
}
Presets::Presets(const std::string& directory,const std::string& repo):directory_(fs::absolute(directory)) {
    fs::create_directories(directory_);
    for(size_t i=0;i<registry().size();++i) {
        const auto& reg=registry()[i];
        unsigned n=0;
        for(const auto& c:reg.presets(int(i))) {
            const auto name=reg.preset_name(c);
            Preset p;p.name=std::string(reg.name)+":"+name;p.role=role_for(name);p.config=c;p.tags.push("factory");p.tags.push(p.role);
            if(entries.count(p.name))p.name+=" "+std::to_string(n);
            entries.emplace(p.name,p);++n;
        }
    }
    auto import=[&](const fs::path& dir) {
        if(!fs::exists(dir))return;
        unsigned seen=0;
        for(const auto& f:fs::recursive_directory_iterator(dir,fs::directory_options::skip_permission_denied)) {
            if(++seen>4096)throw std::runtime_error("preset scan exceeds 4096 entries");
            if(!f.is_regular_file()||f.is_symlink())continue;
            const auto ext=f.path().extension().string();
            if(ext!=".json"&&ext!=".gfp"&&ext!=".gfl")continue;
            if(f.path().filename()=="last-state.json")continue;
            try {
                Preset p;
                if(ext==".json")p=preset_from(Json::parse(read_file(f.path())));
                else {
                    const auto bytes=read_file(f.path());const auto* data=reinterpret_cast<const uint8_t*>(bytes.data());
                    Config c;bool decoded=false;
                    for(size_t e=0;e<registry().size()&&!decoded;++e) {
                        try {c=registry()[e].decode(int(e),data,bytes.size());decoded=true;}catch(const std::exception&){}
                    }
                    if(!decoded)throw std::runtime_error("no engine accepts this preset / unsupported GFL pair");
                    p.config=c;p.name=std::string(registry()[size_t(c.engine)].name)+":"+f.path().stem().string();p.role=role_for(p.name);p.tags.push("imported");p.tags.push(p.role);
                }
                if(!entries.emplace(p.name,p).second)std::cerr<<"preset duplicate skipped: "<<p.name<<'\n';
            }catch(const std::exception& e){std::cerr<<"preset skipped "<<f.path()<<": "<<e.what()<<'\n';}
        }
    };
    if(!repo.empty())import(repo);import(directory_);
}
void Presets::save(const Preset& p) {
    if(!valid_name(p.name))throw std::runtime_error("invalid preset name");
    if(entries.count(p.name))throw std::runtime_error("preset exists");
    const auto filename="user-"+hex(reinterpret_cast<const uint8_t*>(p.name.data()),p.name.size())+".json";
    atomic_write(directory_/filename,preset_json(p).dump()+"\n",true);entries.emplace(p.name,p);
}
Json part_json(const PartState& p,unsigned part) {return object({{"part",int(part+1)},{"engine",registry()[size_t(p.config.engine)].name},{"preset",p.preset},{"role",p.role},{"level_db",p.level},{"mute",p.mute},{"modified",p.modified}});}
State restore_state(const std::string& file,bool wet) {
    State result;const auto configs=initial_configs(wet);const char* roles[]={"BASS","CHORDS","LEAD","PAD"};
    for(unsigned p=0;p<4;++p){result[p].config=configs[p];result[p].role=roles[p];result[p].preset=std::string(registry()[size_t(configs[p].engine)].name)+":"+registry()[size_t(configs[p].engine)].preset_name(configs[p]);}
    if(file.empty()||!fs::exists(file))return result;
    try {
        const auto j=Json::parse(read_file(file));if(j.at("format").integer()!=1)throw std::runtime_error("invalid state format");
        const auto parts=j.at("parts");if(parts.size()!=4)throw std::runtime_error("state needs four parts");State loaded=result;
        for(unsigned p=0;p<4;++p){const auto item=parts.at(size_t(p));auto& s=loaded[p];s.config=config_from(item.at("patch"));s.preset=item.at("preset").str();s.role=item.at("role").str();s.level=float(item.at("level_db").number());if(s.level<-60||s.level>12)throw std::runtime_error("invalid level");s.mute=item.at("mute").boolean();s.modified=item.at("modified").boolean();}return loaded;
    }catch(const std::exception& e){std::cerr<<"state ignored: "<<e.what()<<'\n';return result;}
}
void persist_state(const std::string& file,const State& state) {
    if(file.empty())return;Json parts=Json::array();for(unsigned p=0;p<4;++p){auto j=part_json(state[p],p);j.set("patch",config_json(state[p].config));parts.push(j);}atomic_write(fs::absolute(file),object({{"format",1},{"parts",parts}}).dump()+"\n");
}
namespace {
struct Client {int fd=-1;uint64_t serial=0;std::string input,output;unsigned topics=0;};
constexpr unsigned METERS=1,STATE=2,CPU=4,PARAM=8;
class Server {
    Audio& audio_;State& state_;Presets& presets_;
    std::string socket_,state_file_;
    int listener_=-1,lock_=-1;bool bound_=false;
    std::vector<Client> clients_;
    uint64_t serial_=0,token_=0;size_t cursor_=0;
    bool pending_=false,dirty_=false;
    uint64_t pending_client_=0,pending_token_=0;
    Json pending_response_;std::function<void()> commit_;
    Clock::time_point pending_since_,dirty_since_=Clock::now();
    unsigned changed_part_=4;bool state_event_=false;
    void send(Client& c,const Json& j) {if(c.fd<0)return;const auto data=j.dump()+"\n";if(c.output.size()+data.size()>262144){close(c.fd);c.fd=-1;c.input.clear();c.output.clear();}else c.output+=data;}
    Client* client(uint64_t id) {for(auto& c:clients_)if(c.serial==id&&c.fd>=0)return &c;return nullptr;}
    void broadcast(unsigned topic,const Json& j) {for(auto& c:clients_)if(c.topics&topic)send(c,j);}
    void dirty() {if(!dirty_)dirty_since_=Clock::now();dirty_=true;}
    void state_changed(unsigned p) {dirty();broadcast(STATE,object({{"event","state"},{"part",part_json(state_[p],p)}}));}
    void drain() {
        Ack ack;
        while(audio_.acknowledgements.pop(ack)) {
            if(ack.parameter) {
                const auto& event=ack.change;
                if(event.part>=4)continue;
                auto& p=state_[event.part];
                if(event.engine!=p.identity||event.param>=catalog(p.config.engine).size())continue;
                const auto& d=catalog(p.config.engine)[event.param];
                // A delayed echo must not mark a just-saved identical patch modified.
                if(d.read(p.config)!=event.value) {
                    d.write(p.config,event.value);
                    const bool notify=!p.modified;p.modified=true;dirty();
                    if(notify)state_changed(event.part);
                }
                broadcast(STATE|PARAM,object({{"event","param"},{"part",int(event.part+1)},{"param",d.id},{"value",event.value}}));
                continue;
            }
            delete ack.retired;
            if(!pending_||ack.token!=pending_token_)continue;
            if(commit_)commit_();
            commit_={};
            if(auto* c=client(pending_client_))send(*c,pending_response_);
            if(state_event_)state_changed(changed_part_);
            pending_=false;state_event_=false;changed_part_=4;
        }
        Telemetry t;
        while(audio_.telemetry.pop(t)) {
            if(t.cpu) broadcast(CPU,object({{"event","cpu"},{"mean_pct",t.mean},{"max_pct",t.max},{"p99_upper_pct",t.p99_upper},{"xruns",int64_t(audio_.xruns.load())},{"deadline_misses",int64_t(audio_.deadline_misses.load())},{"backend_xruns",int64_t(audio_.backend_xruns.load())},{"clock_discontinuities",int64_t(audio_.clock_gaps.load())},{"dropped_events",int64_t(audio_.dropped.load())}}));
            else {Json parts=Json::array();for(unsigned p=0;p<4;++p)parts.push(object({{"peak_db",t.peak[p]},{"rms_db",t.rms[p]}}));broadcast(METERS,object({{"event","meters"},{"parts",parts},{"mix",object({{"peak_db",t.peak[4]},{"rms_db",t.rms[4]}})}}));}
        }
    }
    void submit(Client& c,Command command,Json response,unsigned changed=4,bool notify=false,std::function<void()> commit={}) {
        command.token=++token_;
        if(!audio_.commands.push(command))throw std::runtime_error("audio queue full");
        commit_=std::move(commit);pending_=true;pending_token_=command.token;pending_client_=c.serial;pending_response_=std::move(response);changed_part_=changed;state_event_=notify;pending_since_=Clock::now();
    }
    void request(Client& client,const Json& j) {
        const auto id=j.at("id").integer();const auto cmd=j.at("cmd").str();
        Json response=object({{"id",id},{"ok",true}});
        auto merge=[&](const char* key,Json value){response.set(key,std::move(value));};
        if(cmd=="hello") {
            (void)j.at("client").str();Json engines=Json::array();for(const auto& e:registry())engines.push(e.name);
            merge("version","0.1.0");merge("sample_rate",int(RATE));merge("block",int(audio_.block.load()));merge("engines",engines);
        } else if(cmd=="get_state") {Json parts=Json::array();for(unsigned p=0;p<4;++p)parts.push(part_json(state_[p],p));merge("parts",parts);}
        else if(cmd=="list_params")merge("pages",params_json(engine_index(j.at("engine").str())).at("pages"));
        else if(cmd=="get_params") {const auto p=part_number(j);merge("values",values(state_[p],j.has("page")?j.at("page").str():""));}
        else if(cmd=="list_presets") {
            const auto engine=j.has("engine")?j.at("engine").str():"",role=j.has("role")?j.at("role").str():"",tag=j.has("tag")?j.at("tag").str():"";
            if(!engine.empty())(void)engine_index(engine);Json list=Json::array();
            for(const auto& pair:presets_.entries) {const auto& p=pair.second;if(!engine.empty()&&engine!=registry()[size_t(p.config.engine)].name)continue;if(!role.empty()&&role!=p.role)continue;bool found=tag.empty();for(size_t i=0;i<p.tags.size();++i)found|=p.tags.at(i).str()==tag;if(!found)continue;list.push(object({{"name",p.name},{"engine",registry()[size_t(p.config.engine)].name},{"tags",p.tags},{"favorite",p.favorite}}));}merge("presets",list);
        } else if(cmd=="subscribe") {
            const auto topics=j.at("topics");unsigned mask=0;for(size_t i=0;i<topics.size();++i) {const auto s=topics.at(i).str();if(s=="meters")mask|=METERS;else if(s=="state")mask|=STATE;else if(s=="cpu")mask|=CPU;else if(s=="param")mask|=PARAM;else throw std::runtime_error("unknown topic: "+s);}client.topics=mask;
        } else {
            const auto p=part_number(j);auto& s=state_[p];Command command;command.part=p;
            if(cmd=="set_param") {
                const auto param=j.at("param").str();const auto index=param_index(s.config.engine,param);const double number=j.at("value").number();
                Config updated=s.config;const auto& d=catalog(s.config.engine)[index];const float value=d.write(updated,float(std::clamp(number,double(d.min),double(d.max))));
                merge("part",int(p+1));merge("param",param);merge("value",value);
                command.op=Operation::Param;command.param=unsigned(index);command.value=value;submit(client,command,response,p,true,[this,p,index,value]{auto& part=state_[p];catalog(part.config.engine)[index].write(part.config,value);part.modified=true;});return;
            } else if(cmd=="set_engine"||cmd=="load_preset") {
                PartState updated=s;
                if(cmd=="set_engine") {updated.config=defaults(engine_index(j.at("engine").str()));updated.preset=std::string(registry()[size_t(updated.config.engine)].name)+":"+registry()[size_t(updated.config.engine)].preset_name(updated.config);}
                else {const auto it=presets_.entries.find(j.at("name").str());if(it==presets_.entries.end())throw std::runtime_error("preset not found");updated.config=it->second.config;updated.preset=it->second.name;}
                updated.modified=false;auto next=make_engine(updated.config);updated.identity=next.get();
                command.op=Operation::Swap;command.engine=next.get();merge("part",part_json(updated,p));submit(client,command,response,p,true,[this,p,updated]{state_[p]=updated;});
                next.release();return;
            } else if(cmd=="set_level") {const float db=float(std::clamp(j.at("db").number(),-60.,12.));PartState updated=s;updated.level=db;merge("part",part_json(updated,p));command.op=Operation::Level;command.value=db;submit(client,command,response,p,true,[this,p,db]{state_[p].level=db;});return;}
            else if(cmd=="set_mute") {const bool mute=j.at("mute").boolean();PartState updated=s;updated.mute=mute;merge("part",part_json(updated,p));command.op=Operation::Mute;command.value=mute;submit(client,command,response,p,true,[this,p,mute]{state_[p].mute=mute;});return;}
            else if(cmd=="note") {command.op=Operation::Note;command.param=unsigned(midi_number(j,"note"));command.value=float(midi_number(j,"velocity"));submit(client,command,response);return;}
            else if(cmd=="save_preset") {Preset preset;preset.name=j.at("name").str();preset.config=s.config;preset.role=s.role;preset.tags.push(s.role);presets_.save(preset);s.preset=preset.name;s.modified=false;state_changed(p);}
            else throw std::runtime_error("unknown command: "+cmd);
        }
        send(client,response);
    }
    void handle_line(Client& c,const std::string& line) {
        Json id(nullptr);
        try {const auto j=Json::parse(line);if(j.has("id")){const auto candidate=j.at("id");if(candidate.is(json_type_int))id=candidate;}request(c,j);}
        catch(const std::exception& e){send(c,object({{"id",id},{"ok",false},{"error",e.what()}}));}
    }
public:
    Server(Audio& a,State& s,Presets& p,std::string path,std::string file,const std::vector<int>& connected_fds):audio_(a),state_(s),presets_(p),socket_(std::move(path)),state_file_(std::move(file)) {
        if(!connected_fds.empty()){for(int fd:connected_fds){if(fd<0||fcntl(fd,F_SETFL,O_NONBLOCK)!=0)throw std::runtime_error("invalid inherited control fd");clients_.push_back({fd,++serial_,"","",0});}return;}
        if(socket_.size()>=sizeof(sockaddr_un::sun_path))throw std::runtime_error("socket path too long");
        lock_=open((socket_+".lock").c_str(),O_CREAT|O_RDWR,0600);if(lock_<0||flock(lock_,LOCK_EX|LOCK_NB)!=0){if(lock_>=0)close(lock_);lock_=-1;throw std::runtime_error("socket already owned or runtime directory unavailable");}
        try {
            struct stat info{};if(lstat(socket_.c_str(),&info)==0){if(!S_ISSOCK(info.st_mode)||info.st_uid!=getuid())throw std::runtime_error("refusing to remove non-owned socket");unlink(socket_.c_str());}
            listener_=::socket(AF_UNIX,SOCK_STREAM,0);if(listener_<0)throw std::runtime_error("socket failed");
            sockaddr_un address{};address.sun_family=AF_UNIX;std::memcpy(address.sun_path,socket_.c_str(),socket_.size()+1);
            if(bind(listener_,reinterpret_cast<sockaddr*>(&address),sizeof(address))!=0)throw std::runtime_error(std::string("socket bind failed: ")+std::strerror(errno));bound_=true;
            if(chmod(socket_.c_str(),0600)!=0||listen(listener_,32)!=0||fcntl(listener_,F_SETFL,O_NONBLOCK)!=0)throw std::runtime_error("socket setup failed");
        }catch(...){cleanup();throw;}
    }
    void cleanup() noexcept {for(auto& c:clients_)if(c.fd>=0)close(c.fd);if(listener_>=0)close(listener_);if(bound_)unlink(socket_.c_str());if(lock_>=0)close(lock_);listener_=lock_=-1;bound_=false;}
    ~Server(){cleanup();}
    void run(volatile sig_atomic_t& stop,double seconds) {
        const auto begin=Clock::now();auto last_alive=begin,last_notify=begin;uint64_t heartbeat=audio_.heartbeat.load();
#ifdef GF_HAVE_SYSTEMD
        sd_notify(0,"READY=1");
#endif
        while(!stop) {
            if(audio_.fault.load())throw std::runtime_error("audio/backend fault");
            drain();const auto now=Clock::now();
            if(seconds>0&&std::chrono::duration<double>(now-begin).count()>=seconds)break;
            if(pending_&&now-pending_since_>std::chrono::milliseconds(500)) {
                // A queued swap still belongs to audio; do not destroy it while shutting down.
                throw std::runtime_error("audio ACK timeout");
            }
            const auto current=audio_.heartbeat.load();if(current!=heartbeat){heartbeat=current;last_alive=now;}
            if(now-last_alive>std::chrono::seconds(5))throw std::runtime_error("audio watchdog stalled");
#ifdef GF_HAVE_SYSTEMD
            if(now-last_notify>std::chrono::seconds(1)&&now-last_alive<std::chrono::seconds(2)){sd_notify(0,"WATCHDOG=1");last_notify=now;}
#else
            (void)last_notify;
#endif
            if(dirty_&&!pending_&&now-dirty_since_>std::chrono::milliseconds(100)) {persist_state(state_file_,state_);dirty_=false;}
            std::vector<pollfd> fds{{listener_,POLLIN,0}};for(const auto& c:clients_)fds.push_back({c.fd,short(POLLIN|(c.output.empty()?0:POLLOUT)),0});
            const int result=poll(fds.data(),nfds_t(fds.size()),2);if(result<0&&errno!=EINTR)throw std::runtime_error("poll failed");
            // Handle clients before accept, so fds and clients have matching indices.
            for(size_t i=0;i<clients_.size();++i) {
                auto& c=clients_[i];if(c.fd<0)continue;const auto events=fds[i+1].revents;
                if(events&(POLLERR|POLLHUP|POLLNVAL)){close(c.fd);c.fd=-1;continue;}
                if(events&POLLIN){char buffer[8192];const auto n=recv(c.fd,buffer,sizeof(buffer),0);if(n>0)c.input.append(buffer,size_t(n));else if(n==0||(errno!=EAGAIN&&errno!=EINTR)){close(c.fd);c.fd=-1;continue;}if(c.input.size()>65536){close(c.fd);c.fd=-1;continue;}}
                if(events&POLLOUT){const auto n=::send(c.fd,c.output.data(),c.output.size(),0);if(n>0)c.output.erase(0,size_t(n));else if(n<0&&errno!=EAGAIN&&errno!=EINTR){close(c.fd);c.fd=-1;continue;}}
            }
            if(fds[0].revents&POLLIN) {
                const int fd=accept(listener_,nullptr,nullptr);if(fd>=0){if(clients_.size()>=32||fcntl(fd,F_SETFL,O_NONBLOCK)!=0)close(fd);else clients_.push_back({fd,++serial_,"","",0});}
            }
            // At most one in-flight mutation globally: state reads follow all previous ACKs.
            if(!pending_&&!clients_.empty())for(size_t n=0;n<clients_.size();++n){const auto index=(cursor_+n)%clients_.size();auto& c=clients_[index];if(c.fd<0)continue;const auto end=c.input.find('\n');if(end!=std::string::npos){const auto line=c.input.substr(0,end);c.input.erase(0,end+1);cursor_=(index+1)%clients_.size();handle_line(c,line);break;}}
            clients_.erase(std::remove_if(clients_.begin(),clients_.end(),[](const Client& c){return c.fd<0;}),clients_.end());
        }
        // Drain a final submitted command before persisting and releasing ownership.
        const auto until=Clock::now()+std::chrono::milliseconds(500);
        while(pending_&&Clock::now()<until){drain();usleep(1000);}
        persist_state(state_file_,state_);
#ifdef GF_HAVE_SYSTEMD
        sd_notify(0,"STOPPING=1");
#endif
    }
};
}
void serve(Audio& audio,State& state,Presets& presets,const std::string& socket,const std::string& file,volatile sig_atomic_t& stop,double seconds,const std::vector<int>& connected_fds) {Server server(audio,state,presets,socket,file,connected_fds);server.run(stop,seconds);}
}
