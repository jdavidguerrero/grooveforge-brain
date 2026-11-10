#pragma once
#include "audio.h"
#include "json.h"
#include <array>
#include <csignal>
#include <filesystem>
#include <map>
#include <string>
namespace host {
struct PartState {Config config;std::string preset,role;float level=-12.0412f;bool mute=false,modified=false;Engine* identity=nullptr;};
using State=std::array<PartState,4>;
struct Preset {std::string name,role;Config config;Json tags=Json::array();bool favorite=false;};
/** @brief Factory and user preset storage; all I/O occurs on control. */
class Presets {
    std::filesystem::path directory_;
public:
    std::map<std::string,Preset> entries;
    Presets(const std::string& directory,const std::string& repo);
    /** @brief Creates a new preset exclusively; refuses duplicate names. */
    void save(const Preset&);
};
/** @brief Restores validated state or logs a corrupt-file diagnostic and returns factory defaults. */
State restore_state(const std::string& file,bool wet=false);
/** @brief Atomic fsync+rename snapshot outside audio. */
void persist_state(const std::string& file,const State&);
/** @brief Unix JSON-lines event loop, bounded clients/buffers and audio ACKs. */
void serve(Audio&,State&,Presets&,const std::string& socket,const std::string& state_file,volatile sig_atomic_t& stop,double seconds=0,const std::vector<int>& connected_fds={});
/** @brief Public part serialization required by contract §2. */
Json part_json(const PartState&,unsigned part);
}
