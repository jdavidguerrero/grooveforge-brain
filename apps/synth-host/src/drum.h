#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "rt_queue.h"
#include <vector>
#include <memory>
#include <limits>
namespace host {
constexpr unsigned DRUM_RATE=48000,DRUM_PADS=16,DRUM_VOICES=64;
constexpr size_t DRUM_RAM=256ull*1024*1024;
/** @brief Predecoded immutable interleaved mono/stereo PCM, owned outside RT. */
struct DrumSample {unsigned channels=1;std::vector<float> pcm;size_t frames() const noexcept{return pcm.size()/channels;}};
enum DrumParam : unsigned {Tune,Cents,Attack,Hold,Decay,EnvMode,FilterMode,Cutoff,Resonance,Start,End,FadeIn,FadeOut,Pan,Send,VelocityCurve,VelocityLevel,VelocityTone,Choke,Group,Level,Mute,Solo,DrumParamCount};
/** @brief POD parameter block; normalized trims, milliseconds and MIDI velocity. */
struct DrumParams {std::array<float,DrumParamCount> v{0,0,0,0,2000,0,0,20000,0,0,1,0,2,0,0,0,1,0,0,0,.5f,0,0};};
struct DrumLayer {const DrumSample* sample=nullptr;unsigned low=1,high=127;};
struct DrumPad {DrumParams params;std::array<DrumLayer,4> layers{};unsigned count=0;bool round_robin=false;};
struct DrumStep {uint8_t velocity=0;float probability=1,micro=0;};
struct DrumLane {unsigned length=16;std::array<DrumStep,32> steps{};};
struct DrumPattern {std::array<DrumLane,16> lanes{};};
struct DrumGroup {float level=1;bool mute=false;};
/** @brief Prepared rack; vectors are built and destroyed only by control. */
struct DrumRack {
    std::array<DrumPad,16> pads{};
    std::array<DrumPattern,8> patterns{};
    std::array<DrumGroup,4> groups{};
    std::vector<std::unique_ptr<DrumSample>> samples;
    size_t bytes=0;unsigned active_pattern=0;double bpm=122,swing=0;
};
enum class DrumOp {Rack,Param,Steps,Select,Transport,Record,Group,Preview,Trigger};
struct DrumCommand {DrumOp op=DrumOp::Trigger;uint64_t token=0,epoch=std::numeric_limits<uint64_t>::max();unsigned pad=0,param=0,pattern=0;float value=0,extra=0;bool flag=false;DrumRack* rack=nullptr;DrumSample* sample=nullptr;DrumLane lane;};
struct DrumClock {uint64_t sample=0,origin=0,pattern_origin=0,epoch=0,rev=0,steals=0,clipped=0,dropped=0,record_overruns=0;bool running=false,armed=false;unsigned pattern=0;int queued=-1;double bpm=122,swing=0;std::array<uint64_t,16> led_hits{};std::array<uint8_t,16> led_velocity{},led_choked{};std::array<float,16> pads{};std::array<float,4> groups{};float room=0,cue=0;};
struct DrumAck {uint64_t token=0;DrumRack* retired=nullptr;DrumSample* retired_preview=nullptr;bool recorded=false,rejected=false,stale=false;unsigned pattern=0,pad=0,step=0;DrumStep hit;DrumClock clock;};
struct DrumHit {uint64_t sample=0;unsigned pad=0,layer=0;bool cue=false;};
/** @brief Sampler and sample-domain sequencer. Methods below never allocate or lock. */
class Drum {
    struct Voice {const DrumSample* sample=nullptr;DrumParams params;double position=0,ratio=1;float gain=0,left=1,right=1,coefficient=1;float z[2]{},band[2]{};uint64_t age=0,elapsed_frames=0;unsigned pad=0,group=0,choke=0,velocity=127;bool cue=false,led_reported=false;};
    DrumRack* rack_=nullptr;DrumSample* preview_=nullptr;
    std::array<Voice,64> voices_{};
    std::array<DrumPad,16> pads_{};
    std::array<DrumPattern,8> patterns_{};
    std::array<DrumGroup,4> groups_{};
    std::array<std::array<unsigned,16>,2> rr_{};
    std::array<std::array<uint64_t,3>,16> ordinals_{},times_{};
    std::array<uint64_t,16> horizon_{};
    DrumClock state_;
    uint64_t pattern_origin_=0,switch_at_=0,voice_serial_=0,last_published_=0;
    unsigned quantize_=16;uint32_t random_=0x47f0a123;
    bool trace_=false;
    uint64_t target(unsigned lane,uint64_t ordinal) const noexcept;
    void reset_schedule(uint64_t origin) noexcept;
    void refresh_schedule() noexcept;
    void hit(unsigned pad,unsigned velocity,bool cue,bool record) noexcept;
    void configure_voice(Voice&,const DrumParams&) noexcept;
    bool start_voice(const DrumSample*,const DrumParams&,unsigned,unsigned,bool,unsigned) noexcept;
    void clear(bool cue) noexcept;
public:
    Spsc<DrumCommand,256> commands;
    Spsc<DrumAck,4096> acknowledgements;
    Spsc<DrumClock,64> clocks;
    Spsc<DrumHit,4096> hits;
    /** @brief Control/test-only construction; tracing is disabled for production. */
    explicit Drum(bool trace=false):trace_(trace){}
    ~Drum();
    /** @brief Apply bounded commands at block start; retired storage returned via ACK. */
    void begin_block() noexcept;
    /** @brief Audio-owner reset after a backend clock gap; waits for a fresh PLAY. */
    void discontinuity() noexcept;
    /** @brief MIDI channel 10 uses notes 0..15 with exact graph timestamps. */
    void trigger(unsigned pad,unsigned velocity,bool cue=false) noexcept;
    /** @brief Render one frame into additive room and exclusive cue outputs. */
    void frame(float& room_l,float& room_r,float& cue_l,float& cue_r) noexcept;
    /** @brief Publish coalescible telemetry at at most 30 Hz. */
    void end_block() noexcept;
    /** @brief Inspect only while audio is stopped, for deterministic tests. */
    const DrumClock& inspect() const noexcept{return state_;}
};
}
