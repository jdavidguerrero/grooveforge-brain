#pragma once
#include "audio.h"
#include <memory>
#include <string>
namespace host {
/** @brief Owns backend lifecycle; destruction stops callbacks before Audio is destroyed. */
class Backend {public:virtual ~Backend()=default;};
/** @brief Starts requested backend, or fails explicitly if it was not compiled. */
std::unique_ptr<Backend> start_backend(const std::string&,Audio&,unsigned block);
}
