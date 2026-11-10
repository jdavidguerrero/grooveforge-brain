#include "backends.h"
#include "server.h"
#include "bench_support.h"
#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <unistd.h>
#include <stdexcept>
namespace {
volatile sig_atomic_t stopping=0;
void stop(int) {stopping=1;}
using namespace host;
void render(const std::string& wav,const std::string& report,double seconds,unsigned block) {
    if(seconds<=0||seconds>3600)throw std::runtime_error("render seconds must be 0..3600");
    auto audio=std::make_unique<Audio>(initial_configs(true));
    auto data=std::make_unique<std::array<std::array<float,MAX_FRAMES>,10>>();
    std::array<float*,10> outputs{};for(size_t i=0;i<10;++i)outputs[i]=(*data)[i].data();
    const uint64_t measured=uint64_t(std::llround(seconds*RATE)),warmup=2*RATE;
    if(measured==0)throw std::runtime_error("render duration must include at least one sample");
    std::vector<uint8_t> pcm(size_t(measured)*4);std::vector<double> costs;costs.reserve(size_t((measured+block-1)/block));
    uint64_t frames=0,misses=0,nonfinite=0,clipped=0;double sum=0,squares=0,peak=0;
    while(frames<warmup+measured) {
        const unsigned n=unsigned(std::min<uint64_t>(block,frames<warmup?warmup-frames:warmup+measured-frames));
        std::array<Midi,MAX_MIDI> events{};unsigned count=0;
        for(unsigned f=0;f<n;++f) {Midi tmp[32];const unsigned found=pattern(frames+f,tmp);for(unsigned j=0;j<found;++j){tmp[j].frame=f;events[count++]=tmp[j];}}
        const auto begin=std::chrono::steady_clock::now();audio->process(n,outputs,events.data(),count);
        const double cost=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();
        if(frames>=warmup) {
            const double pct=cost*RATE/n*100;costs.push_back(pct);sum+=cost;if(pct>100)++misses;
            for(unsigned f=0;f<n;++f)for(unsigned ch=0;ch<2;++ch) {
                const double x=outputs[8+ch][f];if(!std::isfinite(x))++nonfinite;
                peak=std::max(peak,std::abs(x));squares+=x*x;if(x>1||x<-1)++clipped;
                const int v=int(std::lround(std::clamp(x,-1.,1.)*32767));const auto offset=size_t((frames-warmup+f)*4+ch*2);pcm[offset]=uint8_t(v);pcm[offset+1]=uint8_t(unsigned(v)>>8);
            }
        }
        Ack ack;while(audio->acknowledgements.pop(ack))delete ack.retired;Telemetry t;while(audio->telemetry.pop(t)){};
        if(audio->fault.load())throw std::runtime_error("render audio fault");frames+=n;
    }
    std::sort(costs.begin(),costs.end());const double duration=double(measured)/RATE;
    const auto hash=bench::sha256(pcm);bench::write_wav(wav,pcm,RATE);
    Json parts=Json::array();const char* engines[]={"model_d","juno","ob6","hybrid"};const int notes[]={1,6,4,6};for(int i=0;i<4;++i)parts.push(object({{"engine",engines[i]},{"held_notes",notes[i]}}));
    Json result=object({{"scenario","four-part"},{"variant","wet"},{"parts",parts},{"sample_rate",int(RATE)},{"block_frames",int(block)},{"audio_seconds",duration},{"warmup_seconds",2},{"render_wall_seconds",sum},{"real_time_factor",sum/duration},{"core_percent_mean",sum/duration*100},{"core_percent_p99",costs[size_t(std::ceil(double(costs.size())*.99))-1]},{"core_percent_max",costs.back()},{"deadline_misses",int64_t(misses)},{"pcm_sha256",hash},{"nonfinite_samples",int64_t(nonfinite)},{"clipped_samples",int64_t(clipped)},{"peak",peak},{"rms",std::sqrt(squares/double(measured*2))},{"compiler",__VERSION__},{"build_type",GF_HOST_BUILD_TYPE},{"policy",GF_HOST_POLICY}});
    const auto output=result.dump();if(!report.empty()){std::ofstream out(report);out.exceptions(std::ios::badbit|std::ios::failbit);out<<output<<'\n';}std::cout<<output<<'\n';
    if(nonfinite||squares<=1e-16)throw std::runtime_error("render is nonfinite or silent");
}
}
int main(int argc,char** argv) {
    try {
        const char* runtime=std::getenv("XDG_RUNTIME_DIR");const char* home=std::getenv("HOME");
        std::string socket=runtime?std::string(runtime)+"/gf-synth.sock":"/tmp/gf-synth-"+std::to_string(getuid())+".sock";
        std::string presets=std::string(home?home:"/tmp")+"/.local/share/gf-synth/presets",state,repo,wav,report;
#ifdef GF_HAVE_PIPEWIRE
        std::string backend="pipewire";
#else
        std::string backend="offline";
#endif
        DrumOptions drum_options;std::vector<int> connected_fds;unsigned block=256;double seconds=0;bool wet=false,no_state=false;
        for(int i=1;i<argc;++i) {
            const std::string arg=argv[i];auto value=[&](){if(i+1>=argc)throw std::runtime_error("missing value: "+arg);return std::string(argv[++i]);};
            if(arg=="--drum-data")drum_options.data=value();else if(arg=="--drum-kit-script")drum_options.script=value();else if(arg=="--drum-kit-python")drum_options.python=value();else if(arg=="--control-fd")connected_fds.push_back(std::stoi(value()));else if(arg=="--backend")backend=value();else if(arg=="--socket")socket=value();else if(arg=="--presets")presets=value();else if(arg=="--state")state=value();else if(arg=="--repo-presets")repo=value();else if(arg=="--block")block=unsigned(std::stoul(value()));else if(arg=="--seconds")seconds=std::stod(value());else if(arg=="--render")wav=value();else if(arg=="--report")report=value();else if(arg=="--benchmark")wet=true;else if(arg=="--no-state")no_state=true;
            else if(arg=="--help") {std::cout<<"gf-synth-host [--backend pipewire|jack|offline] [--socket PATH] [--presets DIR]\n  [--state PATH|--no-state] [--repo-presets DIR] [--block 256] [--seconds N] [--benchmark]\n  --render FILE.wav --seconds 20 [--report FILE.json]   deterministic wet benchmark\n  [--drum-data DIR] [--drum-kit-python PYTHON] [--drum-kit-script FILE]\n  --control-fd FD (repeatable): inherit connected Unix sockets, for sandbox tests\n";return 0;}else throw std::runtime_error("unknown argument: "+arg);
        }
        if(block<16||block>MAX_FRAMES||!std::isfinite(seconds)||seconds<0)throw std::runtime_error("invalid block or duration");
        initialize_catalogs();
        if(!wav.empty()){render(wav,report,seconds?seconds:20,block);return 0;}
        if(state.empty())state=(std::filesystem::path(presets)/"last-state.json").string();if(no_state){state.clear();drum_options.checkpoint=false;}
        Presets library(presets,repo);State current=restore_state(state,wet);std::array<Config,4> configs;for(unsigned p=0;p<4;++p)configs[p]=current[p].config;
        auto audio=std::make_unique<Audio>(configs);
        for(unsigned p=0;p<4;++p){current[p].identity=audio->identity(p);audio->commands.push({Operation::Level,0,p,0,current[p].level,nullptr});audio->commands.push({Operation::Mute,0,p,0,float(current[p].mute),nullptr});}
        std::signal(SIGINT,stop);std::signal(SIGTERM,stop);std::signal(SIGPIPE,SIG_IGN);
        auto connection=start_backend(backend,*audio,block);
        std::cerr<<"gf-synth-host 0.1.0 backend="<<backend<<" rate=48000 socket="<<socket<<'\n';
        serve(*audio,current,library,socket,state,stopping,seconds,connected_fds,drum_options);return 0;
    }catch(const std::exception& e){std::cerr<<"gf-synth-host: "<<e.what()<<'\n';return 1;}
}
