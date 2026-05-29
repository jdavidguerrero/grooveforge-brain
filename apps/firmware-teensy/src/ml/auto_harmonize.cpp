/**
 * @file auto_harmonize.cpp
 * @brief Implementación de armonización diatónica automática.
 *
 * Ver auto_harmonize.h para documentación completa.
 * Ver apps/docs/sprints/37-auto-harmonization.md — Theory (tablas verificadas).
 */

#include "auto_harmonize.h"

// ── Tablas de intervalos diatónicos ──────────────────────────────────────────
//
// Cada array tiene 7 entradas (una por grado de escala, 1-indexed en el doc
// pero 0-indexed en código). El valor es la cantidad de semitonos a sumar a
// la nota original para obtener la armonía.
//
// Verificación de integridad: suma de THIRDS = 24 (2 octavas). Suma de SIXTHS = 60 (5 octavas).

/** Escala mayor (jónico) — intervalos de tercera. C→E(+4), D→F(+3), ..., B→D(+3). */
static constexpr int8_t MAJOR_THIRDS[] = {4, 3, 3, 4, 4, 3, 3};

/** Escala mayor — intervalos de sexta. C→A(+9), D→B(+9), E→C(+8), ... */
static constexpr int8_t MAJOR_SIXTHS[] = {9, 9, 8, 9, 9, 8, 8};

/** Escala menor natural (eólico) — intervalos de tercera. A→C(+3), B→D(+3), C→E(+4), ... */
static constexpr int8_t MINOR_THIRDS[] = {3, 3, 4, 3, 3, 4, 4};

/** Escala menor natural — intervalos de sexta. A→F(+8), B→G(+8), C→A(+9), ... */
static constexpr int8_t MINOR_SIXTHS[] = {8, 8, 9, 8, 8, 9, 9};

// ── Offsets de semitonos de cada grado de la escala (desde la raíz) ─────────

/** Escala mayor (W-W-H-W-W-W-H): C D E F G A B */
static constexpr uint8_t MAJOR_STEPS[] = {0, 2, 4, 5, 7, 9, 11};

/** Escala menor natural (W-H-W-W-H-W-W): A B C D E F G */
static constexpr uint8_t MINOR_STEPS[] = {0, 2, 3, 5, 7, 8, 10};

// ── AutoHarmonize ─────────────────────────────────────────────────────────────

AutoHarmonize::AutoHarmonize() {
    for (auto& m : _map) {
        m.active   = false;
        m.original = 0xFF;
        m.harmony  = 0xFF;
    }
}

uint8_t AutoHarmonize::get_harmony_note(uint8_t midi_note,
                                        uint8_t key_root,
                                        bool    is_minor,
                                        HarmonyInterval interval) const {
    // 1. Calcular pitch class de la nota (0-11)
    uint8_t pc = midi_note % 12;

    // 2. Determinar qué grado de escala es esta nota (búsqueda lineal en 7 elementos)
    const uint8_t* steps = is_minor ? MINOR_STEPS : MAJOR_STEPS;
    int degree = -1;
    for (int i = 0; i < 7; i++) {
        if (((key_root + steps[i]) % 12) == pc) {
            degree = i;
            break;
        }
    }

    // Nota cromática — no en la escala → no armonizar
    if (degree < 0) return 0xFF;

    // 3. Seleccionar tabla según tipo de escala e intervalo
    const int8_t* table;
    if (interval == HarmonyInterval::THIRD) {
        table = is_minor ? MINOR_THIRDS : MAJOR_THIRDS;
    } else {
        table = is_minor ? MINOR_SIXTHS : MAJOR_SIXTHS;
    }

    // 4. Calcular nota harmónica
    int harmony = (int)midi_note + (int)table[degree];

    // 5. Voice leading: mantener la segunda voz dentro de una octava de la primera.
    //    Si harmony > original + 12, bajar una octava para evitar el salto.
    //    (Aplica principalmente a sextas en registros altos.)
    if (harmony > (int)midi_note + 12) harmony -= 12;

    // 6. Clamp al rango MIDI válido
    if (harmony < 0)   harmony = 0;
    if (harmony > 127) harmony = 127;

    return (uint8_t)harmony;
}

uint8_t AutoHarmonize::store_harmony(uint8_t orig, uint8_t harm) {
    for (auto& m : _map) {
        if (!m.active) {
            m.original = orig;
            m.harmony  = harm;
            m.active   = true;
            return harm;
        }
    }
    // Todos los slots ocupados — overflow silencioso (nunca debería pasar con 6 voces)
    return 0xFF;
}

uint8_t AutoHarmonize::pop_harmony(uint8_t orig) {
    for (auto& m : _map) {
        if (m.active && m.original == orig) {
            uint8_t h  = m.harmony;
            m.active   = false;
            m.original = 0xFF;
            m.harmony  = 0xFF;
            return h;
        }
    }
    return 0xFF;  // sin par registrado para esta nota
}
