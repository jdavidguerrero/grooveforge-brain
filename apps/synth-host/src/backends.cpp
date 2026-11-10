#include "backends.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#ifdef GF_HAVE_JACK
#include <jack/jack.h>
#include <jack/midiport.h>
#endif
#ifdef GF_HAVE_PIPEWIRE
#include <pipewire/pipewire.h>
#include <pipewire/filter.h>
#include <pipewire/impl-module.h>
#include <pipewire/thread-loop.h>
#include <spa/control/control.h>
#include <spa/pod/iter.h>
#endif
namespace host {
namespace {
[[maybe_unused]] constexpr const char* port_names[]={"part1_L","part1_R","part2_L","part2_R","part3_L","part3_R","part4_L","part4_R","mix_L","mix_R"};
class Offline final:public Backend {
    std::atomic<bool> stop_{false};
    std::thread thread_;
public:
    Offline(Audio& audio,unsigned block):thread_([this,&audio,block] {
        std::array<std::array<float,MAX_FRAMES>,10> data{};
        std::array<float*,10> outputs{};for(size_t i=0;i<10;++i)outputs[i]=data[i].data();
        const auto period=std::chrono::nanoseconds(uint64_t(block)*1000000000/RATE);
        auto next=std::chrono::steady_clock::now();
        while(!stop_.load()) {
            audio.process(block,outputs);next+=period;
            const auto now=std::chrono::steady_clock::now();
            if(now>next+period) {audio.xruns.fetch_add(1);audio.backend_xruns.fetch_add(1);next=now;}
            std::this_thread::sleep_until(next);
        }
    }) {}
    ~Offline() override {stop_.store(true);thread_.join();}
};
#if defined(GF_HAVE_JACK) || defined(GF_HAVE_PIPEWIRE)
void append_midi(Audio& audio,Midi* events,unsigned& count,unsigned frame,uint8_t status,uint8_t a,uint8_t b) noexcept {
    if((status&15u)>=4||a>127||b>127)return;
    const auto type=status&0xf0u;
    if(type!=0x80&&type!=0x90&&type!=0xb0&&type!=0xe0)return;
    if(count==MAX_MIDI){audio.dropped.fetch_add(1);return;}
    events[count++]={frame,status,a,b};
}
#endif
#ifdef GF_HAVE_JACK
class Jack final:public Backend {
    Audio& audio_;
    jack_client_t* client_=nullptr;
    std::array<jack_port_t*,10> ports_{};
    jack_port_t* midi_=nullptr;
    std::array<Midi,MAX_MIDI> events_{};
    static int process(jack_nframes_t n,void* ctx) noexcept {
        auto& self=*static_cast<Jack*>(ctx);
        std::array<float*,10> outputs{};
        for(size_t i=0;i<10;++i)outputs[i]=static_cast<float*>(jack_port_get_buffer(self.ports_[i],n));
        if(n>MAX_FRAMES){for(auto* p:outputs)if(p)std::fill_n(p,n,0.f);self.audio_.fault.store(true);return 0;}
        auto* buffer=jack_port_get_buffer(self.midi_,n);unsigned count=0;
        const auto total=jack_midi_get_event_count(buffer);
        for(unsigned i=0;i<std::min(total,MAX_MIDI);++i){jack_midi_event_t ev{};if(jack_midi_event_get(&ev,buffer,i)==0&&ev.size>=3&&ev.time<n)append_midi(self.audio_,self.events_.data(),count,ev.time,ev.buffer[0],ev.buffer[1],ev.buffer[2]);}
        if(total>MAX_MIDI)self.audio_.dropped.fetch_add(total-MAX_MIDI);
        self.audio_.process(n,outputs,self.events_.data(),count);return 0;
    }
    void close() noexcept {if(client_){jack_deactivate(client_);jack_client_close(client_);client_=nullptr;}}
public:
    Jack(Audio& a,unsigned block):audio_(a) {
        (void)block;jack_status_t status{};client_=jack_client_open("gf-synth-host",JackNoStartServer,&status);
        if(!client_)throw std::runtime_error("cannot open JACK server");
        try {
            if(jack_get_sample_rate(client_)!=RATE)throw std::runtime_error("JACK graph must run at 48000 Hz");
            for(size_t i=0;i<10;++i){ports_[i]=jack_port_register(client_,port_names[i],JACK_DEFAULT_AUDIO_TYPE,JackPortIsOutput,0);if(!ports_[i])throw std::runtime_error("cannot create JACK output");}
            midi_=jack_port_register(client_,"midi_in",JACK_DEFAULT_MIDI_TYPE,JackPortIsInput,0);if(!midi_)throw std::runtime_error("cannot create JACK MIDI input");
            if(jack_set_process_callback(client_,process,this)!=0)throw std::runtime_error("cannot register JACK callback");
            jack_set_xrun_callback(client_,[](void* p)->int{auto& audio=static_cast<Jack*>(p)->audio_;audio.xruns.fetch_add(1);audio.backend_xruns.fetch_add(1);return 0;},this);
            jack_set_sample_rate_callback(client_,[](jack_nframes_t rate,void* p)->int{if(rate!=RATE)static_cast<Jack*>(p)->audio_.fault.store(true);return 0;},this);
            jack_on_shutdown(client_,[](void* p){static_cast<Jack*>(p)->audio_.fault.store(true);},this);
            if(jack_activate(client_)!=0)throw std::runtime_error("cannot activate JACK");
        } catch(...) {close();throw;}
    }
    ~Jack() override {close();}
};
#endif
#ifdef GF_HAVE_PIPEWIRE
class PipeWire final:public Backend {
    Audio& audio_;
    pw_thread_loop* loop_=nullptr;
    pw_filter* filter_=nullptr;
    pw_context* context_=nullptr;
    pw_core* core_=nullptr;
    spa_hook listener_{};
    std::array<void*,10> ports_{};
    void* midi_=nullptr;
    std::array<Midi,MAX_MIDI> events_{};
    pw_filter_events callbacks_{};
    bool started_=false,was_connected_=false;
    uint32_t clock_=SPA_ID_INVALID;
    uint64_t expected_=0;
    static void state(void* ctx,pw_filter_state,pw_filter_state now,const char*) noexcept {
        auto& self=*static_cast<PipeWire*>(ctx);
        if(now==PW_FILTER_STATE_ERROR||(now==PW_FILTER_STATE_UNCONNECTED&&self.was_connected_))self.audio_.fault.store(true);
        if(now==PW_FILTER_STATE_PAUSED||now==PW_FILTER_STATE_STREAMING)self.was_connected_=true;
    }
    static void process(void* ctx,spa_io_position* pos) noexcept {
        auto& s=*static_cast<PipeWire*>(ctx);
        const auto n=pos->clock.duration;
        std::array<float*,10> outputs{};
        if(n>MAX_FRAMES||n==0||pos->clock.rate.denom!=RATE*pos->clock.rate.num){s.audio_.fault.store(true);return;}
        for(size_t i=0;i<10;++i)outputs[i]=static_cast<float*>(pw_filter_get_dsp_buffer(s.ports_[i],uint32_t(n)));
        if(pos->clock.flags&SPA_IO_CLOCK_FLAG_XRUN_RECOVER){s.audio_.xruns.fetch_add(1);s.audio_.backend_xruns.fetch_add(1);}
        else if(s.clock_==pos->clock.id&&s.expected_&&pos->clock.position!=s.expected_){s.audio_.xruns.fetch_add(1);s.audio_.clock_gaps.fetch_add(1);}
        s.clock_=pos->clock.id;s.expected_=pos->clock.position+n;
        unsigned count=0;
        if(auto* b=pw_filter_dequeue_buffer(s.midi_)) {
            if(b->buffer->n_datas>0) {
                const auto& d=b->buffer->datas[0];
                if(d.data&&d.chunk&&d.chunk->offset<=d.maxsize&&d.chunk->size<=d.maxsize-d.chunk->offset&&d.chunk->size>=sizeof(spa_pod_sequence)) {
                    const auto* seq=reinterpret_cast<const spa_pod_sequence*>(static_cast<const uint8_t*>(d.data)+d.chunk->offset);
                    if(spa_pod_is_sequence(&seq->pod)&&SPA_POD_SIZE(&seq->pod)<=d.chunk->size) {
                        spa_pod_control* ctl;unsigned scanned=0;
                        SPA_POD_SEQUENCE_FOREACH(seq,ctl) {
                            if(scanned++==MAX_MIDI){s.audio_.dropped.fetch_add(1);break;}
                            if(ctl->offset>=n)continue;
                            if(ctl->type==SPA_CONTROL_Midi&&SPA_POD_BODY_SIZE(&ctl->value)>=3) {
                                const auto* m=static_cast<const uint8_t*>(SPA_POD_BODY(&ctl->value));
                                append_midi(s.audio_,s.events_.data(),count,ctl->offset,m[0],m[1],m[2]);
                            } else if(ctl->type==SPA_CONTROL_UMP&&SPA_POD_BODY_SIZE(&ctl->value)>=4) {
                                uint32_t word=0;std::memcpy(&word,SPA_POD_BODY(&ctl->value),sizeof(word));
                                if((word>>28)==2)append_midi(s.audio_,s.events_.data(),count,ctl->offset,uint8_t(word>>16),uint8_t(word>>8),uint8_t(word));
                            }
                        }
                    }
                }
            }
            pw_filter_queue_buffer(s.midi_,b);
        }
        s.audio_.process(unsigned(n),outputs,s.events_.data(),count);
    }
    void close() noexcept {
        if(started_)pw_thread_loop_stop(loop_);
        if(filter_) { spa_hook_remove(&listener_); pw_filter_destroy(filter_); }
        if(core_)pw_core_disconnect(core_);
        if(context_)pw_context_destroy(context_);
        if(loop_)pw_thread_loop_destroy(loop_);
        filter_=nullptr;core_=nullptr;context_=nullptr;loop_=nullptr;started_=false;
    }
public:
    PipeWire(Audio& a,unsigned block):audio_(a) {
        pw_init(nullptr,nullptr);
        try {
            loop_=pw_thread_loop_new("gf-synth-pw",nullptr);if(!loop_)throw std::runtime_error("cannot create PipeWire loop");
            callbacks_.version=PW_VERSION_FILTER_EVENTS;callbacks_.process=process;callbacks_.state_changed=state;
            const auto latency=std::to_string(block)+"/48000";
            // One explicit RT module; only the data loop requests RT. Control
            // and the separately launched GUI retain normal scheduling.
            context_=pw_context_new(pw_thread_loop_get_loop(loop_),pw_properties_new(
                "module.rt","false","loop.rt-prio","70",nullptr),0);
            if(!context_)throw std::runtime_error("cannot create PipeWire context");
            if(!pw_context_load_module(context_,"libpipewire-module-rt",
                "rt.prio=70 rlimits.enabled=true rtkit.enabled=true rtportal.enabled=true",nullptr))
                std::cerr<<"PipeWire RT module unavailable; check rtkit/limits and data-loop policy with chrt\n";
            // pw_filter_new needs a connected core, not the context (GCC/PipeWire 1.4 on the Pi rejects the latter).
            core_=pw_context_connect(context_,nullptr,0);
            if(!core_)throw std::runtime_error("cannot connect to PipeWire");
            filter_=pw_filter_new(core_,"gf-synth-host",pw_properties_new(PW_KEY_NODE_NAME,"gf-synth-host",PW_KEY_MEDIA_TYPE,"Audio",PW_KEY_MEDIA_CATEGORY,"Source",PW_KEY_MEDIA_ROLE,"DSP",PW_KEY_MEDIA_CLASS,"Audio/Source",PW_KEY_NODE_AUTOCONNECT,"false","node.always-process","true","node.rate","1/48000","node.lock-rate","true",PW_KEY_NODE_LATENCY,latency.c_str(),nullptr));
            if(!filter_)throw std::runtime_error("cannot create PipeWire filter");
            pw_filter_add_listener(filter_,&listener_,&callbacks_,this);
            for(size_t i=0;i<10;++i) {
                ports_[i]=pw_filter_add_port(filter_,PW_DIRECTION_OUTPUT,PW_FILTER_PORT_FLAG_MAP_BUFFERS,1,pw_properties_new(PW_KEY_FORMAT_DSP,"32 bit float mono audio",PW_KEY_PORT_NAME,port_names[i],nullptr),nullptr,0);
                if(!ports_[i])throw std::runtime_error("cannot create PipeWire audio port");
            }
            midi_=pw_filter_add_port(filter_,PW_DIRECTION_INPUT,PW_FILTER_PORT_FLAG_MAP_BUFFERS,1,pw_properties_new(PW_KEY_FORMAT_DSP,"8 bit raw midi",PW_KEY_PORT_NAME,"midi_in",nullptr),nullptr,0);
            if(!midi_||pw_filter_connect(filter_,PW_FILTER_FLAG_RT_PROCESS,nullptr,0)<0)throw std::runtime_error("cannot connect PipeWire filter");
            if(pw_thread_loop_start(loop_)<0)throw std::runtime_error("cannot start PipeWire loop");started_=true;
        } catch(...) {close();pw_deinit();throw;}
    }
    ~PipeWire() override {close();pw_deinit();}
};
#endif
}
std::unique_ptr<Backend> start_backend(const std::string& name,Audio& a,unsigned block) {
    if(name=="offline")return std::make_unique<Offline>(a,block);
#ifdef GF_HAVE_PIPEWIRE
    if(name=="pipewire")return std::make_unique<PipeWire>(a,block);
#endif
#ifdef GF_HAVE_JACK
    if(name=="jack")return std::make_unique<Jack>(a,block);
#endif
    throw std::runtime_error("backend unavailable: "+name);
}
}
