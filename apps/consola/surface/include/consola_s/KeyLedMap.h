#pragma once
#include <string_view>

/** @brief Physical PCB-K chain: left 0..16, cable NEW -> PLAY, right 17..25 (doc27). */
namespace key_leds {
/** @brief CSV key reference and centre in mm from the panel's left/front edges. */
struct Position { const char* ref; float x_mm, y_mm; };
/** @brief Single C++ source for LED27 index order and panel coordinates. */
inline constexpr Position CHAIN[] = {
    {"BACK", 64.0f, 305.0f},
    {"MENU", 64.0f, 286.5f},
    {"NAV_L", 64.0f, 268.0f},
    {"NAV_R", 64.0f, 249.5f},
    {"BROWSE", 64.0f, 231.0f},
    {"TAP", 58.0f, 174.0f},
    {"FILL", 39.0f, 174.0f},
    {"CUE_ENG", 20.0f, 174.0f},
    {"BUILD", 20.0f, 155.0f},
    {"DROP", 39.0f, 155.0f},
    {"BREAK", 58.0f, 155.0f},
    {"SHIFT", 20.0f, 132.5f},
    {"UNDO", 39.0f, 132.5f},
    {"DELETE", 98.0f, 160.5f},
    {"DUPLICATE", 98.0f, 145.5f},
    {"QUANTIZE", 98.0f, 130.5f},
    {"NEW", 98.0f, 115.5f},
    {"PLAY", 132.0f, 39.5f},
    {"STOP", 151.0f, 39.5f},
    {"REC", 170.0f, 39.5f},
    {"LOOP", 189.0f, 39.5f},
    {"METRO", 208.0f, 39.5f},
    {"SCENE4_PAGE_R", 242.0f, 115.5f},
    {"SCENE3_PAGE_L", 242.0f, 130.5f},
    {"SCENE2_OCT-", 242.0f, 145.5f},
    {"SCENE1_OCT+", 242.0f, 160.5f},
};
/** @brief Number of pixels sent to the physical chain. */
inline constexpr unsigned COUNT = sizeof(CHAIN) / sizeof(CHAIN[0]);
/** @brief Return the wire index for a CSV reference, or COUNT if absent. */
constexpr unsigned index_of(std::string_view ref) {
    for(unsigned i=0;i<COUNT;++i)if(ref==CHAIN[i].ref)return i;
    return COUNT;
}
}
