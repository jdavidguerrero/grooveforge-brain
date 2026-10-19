#include "panel_leds.h"

#include "studio_ui.h"

#include <gf_params.h>

namespace studio {

namespace {
constexpr uint8_t KEY_DIM = 24, KEY_ON = 200;
}

RingArc ring_arc(uint8_t pos, uint8_t count) {
    if (count == 0) return {0, 0};
    if (pos >= count) pos = uint8_t(count - 1);
    const uint8_t a = uint8_t(pos * RING_LEDS / count);
    const uint8_t b = uint8_t((pos + 1) * RING_LEDS / count);
    return {a, uint8_t(b > a ? b - a : 1)};
}

void nav_position(const GF_UiState& ui, uint8_t engine, uint8_t preset_rows, uint8_t* pos, uint8_t* count) {
    if (engine >= GF_ENGINE_COUNT) engine = 0;
    if (ui.mode) { *pos = ui.fx_slot; *count = GF_UI_FX_SLOTS; return; }
    switch (ui.page) {
        case ENGINE:   *pos = ui.cursor; *count = 6; break;
        case SYNTH_FX: *pos = ui.cursor; *count = SFX_SLOTS; break;
        case OSC:      *pos = ui.cursor; *count = GF_OSC_STOP_COUNT[engine]; break;
        case ENV:      *pos = ui.cursor; *count = 2; break;
        case LFO:      *pos = ui.cursor; *count = uint8_t(1 + GF_MOD_STOPS[engine]); break;
        case PRESETS:  *pos = ui.cursor; *count = preset_rows ? preset_rows : 1; break;
        case PANEL:    *pos = 3; *count = 4; break;
        default:       *pos = ui.page <= SPECTRUM ? ui.page : 0; *count = 4; break;   // las cuatro vistas
    }
}

void key_levels(const GF_UiState& ui, uint8_t out[KEY_LEDS]) {
    for (uint8_t i = 0; i < KEY_LEDS; ++i) out[i] = KEY_DIM;
    if (!ui.mode) {
        if (ui.page == OSC) out[0] = KEY_ON;
        if (ui.page == ENV) out[1] = KEY_ON;
        if (ui.page == LFO) out[2] = KEY_ON;
        if (ui.page == SYNTH_FX || ui.page == PRESETS || ui.page == ENGINE) out[3] = KEY_ON;
    }
    out[4] = KEY_ON;                                   // MODE siempre encendida: el color dice SYNTH o FX
    out[5] = (ui.shift || ui.fx_hold) ? KEY_ON : KEY_DIM;
}

}  // namespace studio
