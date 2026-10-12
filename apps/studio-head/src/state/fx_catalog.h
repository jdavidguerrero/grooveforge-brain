#pragma once

#include <cstddef>
#include <cstdint>

#include "state/studio_state.h"

/// Qué dibuja la vista FIRMA para cada FX (FX Viz §C).
enum class FxSig : uint8_t { None, Cymatic, Granular, Ghost, Tape, Bit, Modal, Chorus, Spring, Sub };

/// Parámetro de un FX en sus unidades reales.
struct FxParam {
    const char* name;    ///< ≤ 7 caracteres si va en ENC R (celda de 104 px)
    const char* unit;
    float lo, hi, def;
    uint8_t decimals;
};

struct FxInfo {
    const char* name;
    bool available;      ///< false = IN DEV: la lista lo muestra y NAV lo salta
    FxRoute layer;       ///< capa de 05 §1, ruteo por defecto
    FxSig sig;
    FxParam p[3];        ///< ENC R, P1, P2 (ENC L es siempre DRY/WET)
};

/**
 * @brief Los 12 Signature Effects (05-fx-architecture.md §1).
 *
 * Los tres parámetros son los tres primeros "Parámetros user" del spec sin contar Mix (que va
 * en ENC L). El diseño FX Viz usa nombres de trabajo para los parámetros 2/3; manda el spec.
 */
extern const FxInfo FX_CATALOG[FX_SLOTS];

/// Valor de DRY/WET (%) y de los parámetros por defecto en todos los slots.
void fx_load_defaults(StudioState& s);

/// Posición 0–1 de @p v en el rango del parámetro (para las barras).
float fx_frac(const FxParam& p, float v);

/// Texto del valor con los decimales del parámetro.
void fx_format(const FxParam& p, float v, char* out, size_t n);

/// Siguiente slot disponible en la dirección @p dir (±1), saltando los IN DEV.
uint8_t fx_next_slot(uint8_t slot, int dir);
