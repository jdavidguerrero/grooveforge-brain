#pragma once
#include <cstddef>
#include <stdexcept>
#include "engine.h"
#include "midi.h"
#include "drum.h"
#include <atomic>
#include <array>
#include <cstdint>
#include <type_traits>
namespace host {
constexpr unsigned RATE=48000,MAX_FRAMES=8192,MAX_MIDI=2048;
enum class Operation { Param,Swap,Level,Mute,Note };
struct Command {Operation op=Operation::Note;uint64_t token=0;unsigned part=0,param=0;float value=0;Engine* engine=nullptr;};
struct ParamChange {Engine* engine=nullptr;unsigned part=0,param=0;float value=0;};
// A single FIFO preserves ordering between retired engines, ACKs and parameter events.
struct Ack {uint64_t token=0;Engine* retired=nullptr;bool parameter=false;ParamChange change;};
struct Telemetry {
    bool cpu=false;
    std::array<float,5> peak{},rms{};
    double mean=0,max=0,p99_upper=0;
};
/** @brief Audio owner. Construction/destruction and all engine preparation belong to control. */
class Audio {
    std::array<Engine*,4> engines_{};
    std::array<float,4> gain_{};
    std::array<bool,4> mute_{};
    std::array<std::array<int,6>,4> cc_{};
    std::array<std::array<bool,MAX_PARAMS>,4> changed_{};
    std::array<double,5> squares_{};
    std::array<float,5> peaks_{};
    uint64_t meter_frames_=0,cpu_frames_=0,meter_clock_=0,meter_due_=RATE/30;
    double cpu_seconds_=0,cpu_max_=0;
    std::array<uint32_t,512> cpu_histogram_{};
    uint32_t cpu_blocks_=0;
    void map_cc(unsigned p) noexcept;
    void midi_control(const Midi&) noexcept;
    void midi_note(const Midi&) noexcept;
public:
    Drum drum;
    Spsc<Command,256> commands;
    Spsc<Ack,4096> acknowledgements;
    Spsc<Telemetry,128> telemetry;
    std::atomic<uint64_t> xruns{0},deadline_misses{0},backend_xruns{0},clock_gaps{0},dropped{0},heartbeat{0};
    std::atomic<unsigned> block{128};
    std::atomic<bool> fault{false},clock_reset_requested{false};
    /** @brief Prepares the four initial patches and meter/CC tables. */
    explicit Audio(const std::array<Config,4>& configs);
    ~Audio();
    /** @brief Initial identity, called only before starting the backend. */
    Engine* identity(unsigned part) const noexcept {return engines_[part];}
    Audio(const Audio&)=delete;
    Audio& operator=(const Audio&)=delete;
    /** @brief Main callback, ten legacy outputs plus optional stereo cue; MIDI sorted by frame, bounded to MAX_MIDI. */
    void process(unsigned frames,const std::array<float*,10>& outputs,const Midi* midi=nullptr,unsigned count=0,const std::array<float*,2>& cue={}) noexcept;
};
/** @brief Default part engines in role order; benchmark enables the measured wet workload. */
std::array<Config,4> initial_configs(bool benchmark=false);
/** @brief Deterministic MIDI fixture for one frame, used by offline benchmark and safety test. */
unsigned pattern(uint64_t frame,Midi* out) noexcept;
}
