#pragma once
#include "drum.h"
#include "json.h"
#include <filesystem>
namespace host {
struct DrumDescriptor {const char* id;const char* name;const char* unit;float min,max,step;const char* options;};
/** @brief Single control-thread metadata table, indexed by DrumParam. */
const std::array<DrumDescriptor,DrumParamCount>& drum_descriptors();
/** @brief Validates and clamps a pad value using host metadata. */
float drum_value(unsigned index,const Json& value);
/** @brief Resolves an existing path and rejects traversal or symlink escape. */
std::filesystem::path drum_path(const std::filesystem::path& root,const std::string& relative);
/** @brief Reads JSON with a bounded size outside RT. */
Json drum_read(const std::filesystem::path& path);
/** @brief Atomic snapshot with fsync and optional exclusive creation. */
void drum_write(const std::filesystem::path& path,const Json& value,bool overwrite=true);
/** @brief Decodes/hash-checks WAV, checking the projected PCM budget before allocation. */
std::unique_ptr<DrumSample> drum_decode(const std::filesystem::path&,const std::string& hash,size_t remaining);
/** @brief Fully prepares a rack or throws without affecting the active rack. */
std::unique_ptr<DrumRack> drum_prepare(const Json&,const std::filesystem::path& root,size_t remaining=DRUM_RAM);
/** @brief Parses a bounded sequencer lane, preserving all 32 steps. */
DrumLane drum_lane(const Json&);
/** @brief Creates a complete empty rack, suitable for safe startup. */
Json drum_empty();
/** @brief Returns pages of eight pad parameter descriptors. */
Json drum_metadata();
}
