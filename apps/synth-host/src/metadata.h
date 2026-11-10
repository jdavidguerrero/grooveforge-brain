#pragma once
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "json.h"
namespace host {
/** @brief Contract metadata generated from the active engine registry. */
Json params_json(int engine);
}
