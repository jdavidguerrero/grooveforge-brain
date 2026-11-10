#pragma once
#include "ui/layer_bank.h"
#include "engines/dx7_6op_core.h"
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace host {
class Engine;
constexpr unsigned MAX_PARAMS=512;
/** @brief Fully owned POD patch; never shares mutable storage across threads. */
struct Config {
    studio::LayerPreset layer;
    int engine=0;
    gf::dx7six::Patch six{};
    // New adapters can store their own parameter values without extending the host protocol.
    std::array<float,MAX_PARAMS> extension{};
};
enum class Storage { Float, Bool, U8, I8, Bit };
/** @brief One descriptor is used for metadata, validation, patch read and patch write. */
struct Param {
    std::string id,name,page,unit;
    float min=0,max=1,initial=0,step=.01f;
    int options=0;
    int bank=-1, slot=-1, bit=0;
    bool toggle=false;
    size_t offset=0;
    Storage storage=Storage::Float;
    std::vector<std::string> labels;
    /** @brief Reads the mapped POD field without allocation. */
    float read(const Config&) const noexcept;
    /** @brief Clamps/quantizes then writes the mapped POD field without allocation. */
    float write(Config&,float) const noexcept;
    /** @brief GUI kind from the descriptor. */
    std::string kind() const;
};
using Factory=std::unique_ptr<Engine>(*)(const Config&);
struct Registration {
    const char* name;
    int firmware;
    Factory create;
    Config (*default_config)(int);
    std::vector<Param> (*parameters)(int);
    std::vector<Config> (*presets)(int);
    std::string (*preset_name)(const Config&);
    std::vector<uint8_t> (*encode)(const Config&);
    Config (*decode)(int,const uint8_t*,size_t);
};
/** @brief Shared adapters for the existing portable previews; new engines supply their own callbacks. */
Config preview_defaults(int engine);
std::vector<Param> preview_catalog(int engine);
std::vector<Config> preview_presets(int engine);
std::string preview_preset_name(const Config&);
std::vector<uint8_t> preview_encode(const Config&);
Config preview_decode(int engine,const uint8_t* data,size_t size);
/** @brief DX7 six-operator adapter codecs and metadata, shared with firmware/VST. */
Config six_defaults(int engine);
std::vector<Param> six_catalog(int engine);
std::vector<Config> six_presets(int engine);
std::string six_preset_name(const Config&);
std::vector<uint8_t> six_encode(const Config&);
Config six_decode(int engine,const uint8_t*,size_t);
/** @brief Registry is the only list of available engines. */
const std::vector<Registration>& registry();
/** @brief Resolves a public contract name; throws on unavailable engines. */
int engine_index(const std::string&);
/** @brief Returns a stable catalog, initialized on control before audio starts. */
const std::vector<Param>& catalog(int engine);
/** @brief Shared factory patch, HYBRID defaults to RETRO BRASS. */
Config defaults(int engine);
/** @brief Initializes every catalog outside audio. */
void initialize_catalogs();
/** @brief Resolve ID or reject it. */
size_t param_index(int engine,const std::string& id);
/** @brief Fixed repeatable four-part wet workload, same FX settings as engine bench. */
void benchmark_patch(Config&);
}
