#pragma once
#include "dx7_6op_core.h"
namespace gf::dx7six {
/** @brief Host list_params record, page/bank/slot ordered, at most eight per bank. */
struct Param {
    const char *id, *name, *unit, *page;
    uint8_t bank, slot, minimum, maximum, initial, step;
    const char *kind, *options;
    uint8_t offset;
    int8_t enable_bit;
};
extern const Param params[];
extern const size_t param_count;
/** @brief Read a parameter by stable id; false for unknown ids. */
bool get_param(const Patch& patch, const char* id, float& value);
/** @brief Round/clamp a finite value with metadata; false preserves patch. Caller applies edit_patch at the audio boundary. */
bool set_param(Patch& patch, const char* id, float value);
}
