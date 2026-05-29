#pragma once
/**
 * @file auto_harmonize.h
 * @brief Armonización diatónica automática — Sprint 37, Fase 6 Layer 1 AI.
 *
 * Dado un MIDI note y la tonalidad activa (de ScaleLock/KeyDetector), calcula
 * la nota de una segunda voz armónica diatónica (tercera o sexta).
 *
 * ## Por qué es determinístico (no ML)
 *
 * La tabla de intervalos diatónicos es fija para cada escala. El ML ya hizo
 * su trabajo en Sprint 4.3 (detectar la tonalidad). A partir de ahí, la
 * armonización es matemática pura: buscar el grado de la nota en la escala,
 * consultar la tabla, sumar el offset. < 1 µs en Cortex-M7.
 *
 * ## Tablas de intervalos (semitones por grado de escala)
 *
 * Escala mayor — terceras:  {4, 3, 3, 4, 4, 3, 3}
 * Escala mayor — sextas:    {9, 9, 8, 9, 9, 8, 8}
 * Escala menor — terceras:  {3, 3, 4, 3, 3, 4, 4}
 * Escala menor — sextas:    {8, 8, 9, 8, 8, 9, 9}
 *
 * ## Voice tracking
 *
 * La clase mantiene un mapa de hasta 6 pares original→harmony para garantizar
 * que el noteOff libere la voz correcta (crítico para Prophet5 polyfónico).
 *
 * Cross-ref: apps/docs/sprints/37-auto-harmonization.md
 *            apps/docs/04-ai-architecture.md §1 (memory budget)
 *            apps/docs/01-architecture.md §4.1 (polyphony targets)
 */

#include <stdint.h>

/** @brief Intervalo de armonía — tercera o sexta diatónica. */
enum class HarmonyInterval : uint8_t {
    THIRD = 0,  ///< Tercera diatónica (3-4 semitonos según grado)
    SIXTH = 1,  ///< Sexta diatónica (8-9 semitonos según grado)
};

/**
 * @brief Armonizador diatónico automático.
 *
 * Uso típico en midi_note_on():
 * @code
 *   if (g_auto_harm.enabled && g_scale_lock.has_scale()) {
 *       uint8_t h = g_auto_harm.get_harmony_note(
 *           play_note,
 *           g_scale_lock.get_key_root(),
 *           g_scale_lock.get_is_minor());
 *       if (h != 0xFF) {
 *           prophet.noteOn(h, velocity_norm);
 *           g_auto_harm.store_harmony(play_note, h);
 *       }
 *   }
 * @endcode
 */
class AutoHarmonize {
public:
    AutoHarmonize();

    /**
     * @brief Calcula la nota de la voz armónica para una nota dada.
     *
     * Busca el grado de la nota en la escala y aplica el offset de la tabla.
     * Si la nota no está en la escala (nota cromática), retorna 0xFF (no armonizar).
     *
     * Voice leading básico: si la nota harmónica saltaría más de una octava,
     * se baja una octava para mantener las voces próximas.
     *
     * @param midi_note  Nota original [0-127] (post Scale Lock snap).
     * @param key_root   Raíz de la escala [0-11]. C=0, C#=1, ..., B=11.
     * @param is_minor   false = escala mayor diatónica, true = menor natural.
     * @param interval   Tipo de intervalo. Default: THIRD.
     * @return           Nota MIDI de la voz armónica [0-127], o 0xFF si la nota
     *                   no está en la escala activa.
     */
    uint8_t get_harmony_note(uint8_t      midi_note,
                             uint8_t      key_root,
                             bool         is_minor,
                             HarmonyInterval interval = HarmonyInterval::THIRD) const;

    /**
     * @brief Registra el par original→harmony para posterior noteOff correcto.
     *
     * Usa el primer slot libre del mapa (tamaño 6 — polyphony limit).
     * Si todos los slots están ocupados, no registra (overflow silencioso —
     * el noteOff simplemente no encontrará el par y lo ignorará).
     *
     * @param original_note  Nota original que activó la armonía.
     * @param harmony_note   Nota harmónica que se está tocando.
     * @return               harmony_note para uso en cadena (convenience).
     */
    uint8_t store_harmony(uint8_t original_note, uint8_t harmony_note);

    /**
     * @brief Recupera y elimina el par de armonía para una nota original.
     *
     * Llamar en midi_note_off() para obtener qué nota harmony liberar.
     *
     * @param original_note  Nota original cuyo par se busca.
     * @return               Nota harmónica a liberar, o 0xFF si no hay par registrado.
     */
    uint8_t pop_harmony(uint8_t original_note);

    // ── Estado público — controlado por el sketch ─────────────────────────────
    bool            enabled  = false;                     ///< ON/OFF. Toggle con B4 en AI mode.
    HarmonyInterval interval = HarmonyInterval::THIRD;    ///< Tercera o sexta.

private:
    // Mapa de hasta 6 pares activos (mismo límite de polyphony que Prophet5).
    // No usa dynamic allocation — array estático de tamaño fijo.
    struct NoteMap {
        uint8_t original;  ///< Nota MIDI original.
        uint8_t harmony;   ///< Nota MIDI harmónica correspondiente.
        bool    active;    ///< Slot en uso.
    };
    NoteMap _map[6];
};
