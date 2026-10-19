#pragma once

#include <cstddef>
#include <cstdint>

#include "state/studio_state.h"

/// Formas de onda que conoce la UI (íconos y trazas de la página OSC / LFO).
enum class Wave : uint8_t { Saw, Tri, Sqr, Pulse, Sine, SampleHold, Noise, SawPulse };

const char* wave_name(Wave w);

enum class SlotKind : uint8_t {
    WaveSel,     ///< índice en OscEngine::waves
    Number,      ///< valor con unidad (CT, ST, %)
    Ratio,       ///< multiplicador, "×1.50"
    MixAB,       ///< % de A, se muestra "60:40"
    Toggle,      ///< interruptor: OFF / ON
    KbdTrack,    ///< keyboard tracking del filtro: OFF / HALF / FULL
    ChorusMode,  ///< Juno-106 BBD Chorus: OFF / I / II / I+II
    HpfMode,     ///< Juno-106 4-pos HPF: BST / FLAT / 225 / 720
    Choice,      ///< discreto con etiquetas propias (OscSlot::labels, índice desde lo)
};

/// Un slot de la plantilla OSC (diseño v3 §A: {name, unit, kind, range}).
struct OscSlot {
    const char* name;
    const char* unit;
    SlotKind kind;
    float lo, hi;
    uint8_t decimals;
    bool signed_value;  ///< muestra "+7" / "−12"
    bool global;        ///< el valor es del engine, no del oscilador (se guarda en el oscilador 0)
    const char* const* labels = nullptr;   ///< SlotKind::Choice
};

/// Fuente en la fila de selección: oscilador (parada de NAV) o fuente solo de nivel (SUB, NOISE, RING).
struct OscSource { const char* name; bool stop; };

/**
 * @brief Plantilla OSC de un engine: paradas de NAV, fuentes y 4 slots por parada.
 *
 * Sale de lo que el firmware implementa hoy (src/engines/, un .h por engine), no del instrumento original. Un slot
 * nulo se dibuja como NOT USED y en la fila 2 como NO FUNCTION.
 */
struct OscEngine {
    uint8_t stops;
    uint8_t sources;
    OscSource src[5];
    const OscSlot* slot[OSC_STOPS][OSC_SLOTS];
    Wave waves[5];
    uint8_t wave_count;
    bool fm;            ///< DX7: página de operadores (ADR-005)
};

const OscEngine& osc_engine(uint8_t engine_id);

/// Parada de MOD de la página LFO / MOD (theory/prophet-5-completo.md §3.2): nombre y 8 slots.
struct ModStop {
    const char* name;
    const OscSlot* slot[OSC_SLOTS];
};

/// Cantidad de paradas de MOD del engine (0 = solo LFO) y cada parada.
uint8_t mod_stop_count(uint8_t engine_id);
const ModStop& mod_stop(uint8_t engine_id, int stop);

/// Valores iniciales de la página OSC para los 6 engines (demo y primer arranque).
void osc_load_defaults(StudioState& s);

/// Dónde vive el valor de un slot (los globales, en el oscilador 0).
float& osc_value(StudioState& s, uint8_t engine, int stop, int slot);
float osc_value(const StudioState& s, uint8_t engine, int stop, int slot);

/// Texto del valor y posición 0–1 para la barra de la fila 2.
void osc_format(const OscEngine& e, const OscSlot& sl, float v, char* out, size_t n);
float osc_frac(const OscEngine& e, const OscSlot& sl, float v);
