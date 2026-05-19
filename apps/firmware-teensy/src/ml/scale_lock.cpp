// scale_lock.cpp — Implementación de ScaleLock
//
// Theory: escala mayor diatónica = 7 notas con intervalos W-W-H-W-W-W-H
//         escala menor natural   = 7 notas con intervalos W-H-W-W-H-W-W
//         (W = tono = 2 semitonos, H = semitono = 1 semitono)
//
// La máscara de 12 bools permite snap en O(1) por pitch class con búsqueda
// lineal de radio máximo 6, que garantiza terminar siempre en escala diatónica.
//
// Cross-ref: apps/docs/04-ai-architecture.md §1 (Scale Lock, AI Tier S)
//            apps/docs/sprints/27-scale-lock.md

#include "scale_lock.h"
#include <string.h>  // memset, strncpy
#include <Arduino.h> // constrain

// ---------------------------------------------------------------------------
// Tablas de intervalos (semitonos desde la root)
// ---------------------------------------------------------------------------

// Mayor diatónica (Jónica): W W H W W W H
// Grados: 1  2  3  4  5  6  7
static const uint8_t MAJOR_INTERVALS[7] = {0, 2, 4, 5, 7, 9, 11};

// Menor natural (Eólica): W H W W H W W
// Grados: 1  2  b3  4  5  b6  b7
static const uint8_t MINOR_INTERVALS[7] = {0, 2, 3, 5, 7, 8, 10};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ScaleLock::ScaleLock()
    : _bypass(false), _has_scale(false)
{
    memset(_mask, 0, sizeof(_mask));
    // "---" es el estado inicial — sin escala detectada
    strncpy(_scale_name, "---", sizeof(_scale_name));
    _scale_name[sizeof(_scale_name) - 1] = '\0';
}

// ---------------------------------------------------------------------------
// _build_mask — construye la máscara de 12 pitch classes
// ---------------------------------------------------------------------------

void ScaleLock::_build_mask(uint8_t root, bool is_minor) {
    memset(_mask, 0, sizeof(_mask));
    const uint8_t* intervals = is_minor ? MINOR_INTERVALS : MAJOR_INTERVALS;
    for (int i = 0; i < 7; i++) {
        _mask[(root + intervals[i]) % 12] = true;
    }
}

// ---------------------------------------------------------------------------
// update — acepta nuevo resultado del Key Detector
// ---------------------------------------------------------------------------

void ScaleLock::update(const KeyResult& kr, float min_confidence) {
    if (kr.confidence < min_confidence) return;

    _build_mask(kr.root_note, kr.is_minor);
    strncpy(_scale_name, kr.name, sizeof(_scale_name));
    _scale_name[sizeof(_scale_name) - 1] = '\0';
    _has_scale = true;
}

// ---------------------------------------------------------------------------
// snap — cuantiza una nota a la escala activa
// ---------------------------------------------------------------------------

uint8_t ScaleLock::snap(uint8_t note) const {
    if (_bypass || !_has_scale) return note;

    uint8_t pc = note % 12;
    if (_mask[pc]) return note;  // pitch class ya está en la escala

    // Búsqueda bidireccional — máximo 6 semitonos.
    // En una escala diatónica de 7 notas, la nota más alejada de cualquier
    // nota de escala es siempre ≤ 6 semitonos (semitono compartido entre
    // la nota más alta del delta hacia arriba y hacia abajo cierra a 7).
    for (int delta = 1; delta <= 6; delta++) {
        bool up_ok   = _mask[(pc + delta) % 12];
        bool down_ok = _mask[((int)pc - delta + 12) % 12];

        if (up_ok && down_ok) {
            // Empate: sesgo leading-tone — preferir el semitono más alto.
            // El leading tone (7mo grado) crea tensión hacia la tónica;
            // preferirlo en empate produce resoluciones más musicales.
            int snapped = (int)note + delta;
            return (uint8_t)constrain(snapped, 0, 127);
        }
        if (up_ok) {
            int snapped = (int)note + delta;
            return (uint8_t)constrain(snapped, 0, 127);
        }
        if (down_ok) {
            int snapped = (int)note - delta;
            return (uint8_t)constrain(snapped, 0, 127);
        }
    }

    // Fallback — no debería ocurrir en ninguna escala diatónica de 7 notas.
    return note;
}

// ---------------------------------------------------------------------------
// set_bypass
// ---------------------------------------------------------------------------

void ScaleLock::set_bypass(bool bypass) {
    _bypass = bypass;
}
