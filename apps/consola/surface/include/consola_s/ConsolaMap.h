#pragma once
#include <stdint.h>

#ifndef GF_CONSOLA_MINIMAL
#define GF_CONSOLA_MINIMAL 1
#endif
// Explicit opt-in only after wiring MASTER to 22, 3.3 V and GND.
#ifndef GF_CONSOLA_WIRED_POTS
#define GF_CONSOLA_WIRED_POTS 0
#endif
namespace consola {
/// One identity used by the actual USB descriptor and native tests.
#define GF_CONSOLA_PRODUCT_CHARS {'G','F',' ','C','o','n','s','o','l','a'}
inline constexpr char USB_PRODUCT_TEXT[] = {'G','F',' ','C','o','n','s','o','l','a','\0'};
inline constexpr uint8_t MIDI_CHANNEL = 1;
inline constexpr uint8_t ENCODER_CC[] = {16,17,18,19,20,21,22,23,35};
inline constexpr uint8_t ENCODER_PINS[][2] = {{2,3},{4,5},{6,9},{10,11},{12,30},{31,32},{33,34},{35,36},{37,40}};
inline constexpr uint8_t MCP_PINS[] = {0,1,2,3,4,5,6,8,9,10,11,12,13,14};
/// Logical input order from pin map 17 §3; 0xff means unassigned/invalid.
constexpr uint8_t mcp_note(uint8_t chip, uint8_t input) {
    constexpr uint8_t notes[4][14] = {
        {84,85,86,87,255,255,255,80,81,82,83,255,255,255},
        {89,90,91,92,255,255,255,101,97,96,95,94,255,255},
        {98,102,103,104,105,106,100,107,110,111,99,255,255,255},
        {112,113,114,115,116,117,88,122,255,255,255,255,255,255}
    };
    return chip < 4 && input < 14 ? notes[chip][input] : 0xff;
}
/// No floating analog inputs or unconnected encoders are sampled in minimal mode.
struct Profile {
    uint16_t encoders;
    uint8_t fsrs, pots, ads_channels[2];
    uint16_t mcp_inputs[4];
    bool selector;
};
inline constexpr Profile MINIMAL{0x10f, 0x03, GF_CONSOLA_WIRED_POTS & 0x07, {0x05,0x00}, {0x007f,0x007f,0x0000,0x0000}, false};
inline constexpr Profile FULL{0x1ff, 0x0f, 0x07, {0x0f,0x0f}, {0x0f0f,0x1f0f,0x0f7f,0x017f}, true};
/// Minimal ADS0 AIN2 is Sharp1; full-panel hands remain on ADS1 AIN2/3.
constexpr int8_t hand_index(bool minimal, uint8_t chip, uint8_t channel) {
    return minimal ? (chip == 0 && channel == 2 ? 0 : -1) :
           (chip == 1 && channel >= 2 && channel < 4 ? channel - 2 : -1);
}
}
