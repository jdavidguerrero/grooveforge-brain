#pragma once
#include "drum_files.h"
#include <functional>
#include <chrono>
#include <sys/types.h>
#include <unistd.h>
namespace host {
struct DrumOptions {std::string data,python="python3",script="tools/drum-kits/drum_kits.py";bool checkpoint=true;};
/** @brief Control-thread rack/IPC/worker owner. Never called from audio. */
class DrumService {
    std::string epoch_=std::to_string(getpid());
    Drum& audio_;DrumOptions options_;Json manifest_=drum_empty();DrumClock clock_;
    DrumCommand pending_command_;
    size_t resident_=0,preview_bytes_=0;uint64_t token_=0;bool pending_=false,dirty_=false;
    std::chrono::steady_clock::time_point dirty_since_=std::chrono::steady_clock::now();
    std::function<void(Json)> response_,event_;std::function<void()> commit_;Json reply_;
    pid_t job_=-1;int job_fd_=-1;bool job_exited_=false;int job_status_=0;std::string job_buffer_,job_id_;unsigned job_serial_=0;
    void submit(DrumCommand,Json,std::function<void(Json)>,std::function<void()>);
    void mark_dirty();
    void snapshot();
    void start_job(const Json&,Json&);
    Json samples(unsigned offset,unsigned limit,bool& more,unsigned& next) const;
public:
    /** @brief Restore last working snapshot as stopped, validating all assets. */
    DrumService(Drum&,DrumOptions,std::function<void(Json)> events);
    ~DrumService();
    /** @brief True when a request is recognized; replies may wait for audio ACK. */
    bool request(const Json&,std::function<void(Json)> reply);
    /** @brief Drain ACKs, telemetry and nonblocking worker progress. */
    void poll();
    /** @brief Enforces one in-flight mutation with existing synth server. */
    bool pending() const noexcept{return pending_;}
    /** @brief Stable identity for this audio process, independent of saved transport. */
    std::string epoch() const {return epoch_+"-"+std::to_string(clock_.epoch);}
};
}
