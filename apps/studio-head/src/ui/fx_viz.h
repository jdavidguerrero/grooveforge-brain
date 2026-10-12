#pragma once

#include <cstdint>

#include "state/studio_state.h"
#include "ui/painter.h"

/// Estado visual de la gráfica (FX Viz §A: bypass y hold en las tres vistas).
enum class VizState : uint8_t { Main, Bypass, Hold };

constexpr int VIZ_W = 728, VIZ_H = 256;
constexpr int METER_W = 24, METER_H = 328;

/**
 * @brief Gráfica de FX MAIN, 728×256: ONDA (C1), ESPECTRO (C2) o FIRMA (§C) del FX a la vista.
 *
 * Se llama desde LV_EVENT_DRAW_MAIN; solo emite rects sólidos y texto (theory doc §5.4). Lo que
 * depende de parámetros (curvas, ecos, envolventes) se calcula acá; lo que depende del audio
 * llega en StudioState::fx.
 */
void viz_draw(const Painter& p, const StudioState& s, VizState st, uint32_t now_ms);

/// Medidor IN estéreo de 24×328 (v3 04a): barras, peak hold y cajas de clip.
struct MeterView {
    uint8_t level[2];   ///< alto de barra en px (0–272)
    uint8_t peak[2];    ///< peak hold en px
    bool clip[2];       ///< clip enganchado (2 s)
    int8_t pad_db;
};
void meter_draw(const Painter& p, const MeterView& m);
