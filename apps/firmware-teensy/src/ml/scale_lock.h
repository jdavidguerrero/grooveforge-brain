#pragma once
// scale_lock.h — Cuantización de pitch a escala diatónica guiada por Key Detector ML.
//
// Scale Lock intercepta notas MIDI entrantes y las transpone al semitono más cercano
// dentro de la escala activa (detectada por KeyDetector). Resultado: el usuario puede
// tocar libremente y todo suena musically coherente con la tonalidad actual.
//
// Feature AI Tier S — el "never play a wrong note" del GrooveForge Brain.
// Cross-ref: apps/docs/sprints/27-scale-lock.md, apps/docs/04-ai-architecture.md §1

#include <stdint.h>
#include "key_detector.h"

/**
 * @brief Cuantizador de notas MIDI a escala diatónica.
 *
 * Recibe el KeyResult del KeyDetector y construye una máscara de 12 pitch
 * classes activos. Cada nota MIDI que pasa por snap() es proyectada al
 * semitono más cercano dentro de la escala, con sesgo leading-tone en empate.
 *
 * El algoritmo de snap es O(12) peor caso — siempre termina en ≤6 iteraciones
 * porque en una escala diatónica toda nota está a ≤6 semitonos de una nota
 * de escala.
 *
 * CPU: despreciable (operaciones enteras sobre arrays de 12 bools en RAM).
 * No introduce procesado en el audio ISR.
 */
class ScaleLock {
public:
    ScaleLock();

    /**
     * @brief Actualiza la escala activa con el resultado del Key Detector.
     *
     * Solo actualiza si kr.confidence >= min_confidence (histéresis de confianza).
     * Llamar cada vez que key_detector.inference() produce un nuevo resultado.
     *
     * @param kr              Resultado de KeyDetector::inference().
     * @param min_confidence  Umbral mínimo de confianza para aceptar el resultado.
     *                        Default 0.75 — ver apps/docs/04-ai-architecture.md §8.
     */
    void update(const KeyResult& kr, float min_confidence = 0.75f);

    /**
     * @brief Cuantiza una nota MIDI [0-127] a la escala activa.
     *
     * Si bypass == true o sin escala detectada: retorna note sin cambios.
     * El snap es bidireccional: busca el semitono más cercano en ambas direcciones.
     * En caso de empate, prefiere el semitono más alto (sesgo leading-tone).
     * Actualiza los contadores de estadísticas (_total_count, _snapped_count)
     * y setea _last_snap_event_flag si la nota fue cuantizada.
     *
     * @param note  Nota MIDI original [0-127].
     * @return      Nota cuantizada [0-127], o note sin cambios si bypass/sin escala.
     */
    uint8_t snap(uint8_t note);

    /**
     * @brief Consume el evento de snap (rising-edge, una lectura por evento).
     *
     * Retorna true si ocurrió un snap desde la última lectura y popula from/to.
     * Después de retornar true, el flag interno queda en 0 hasta el próximo snap.
     *
     * @param from  [out] Nota MIDI original que activó el snap.
     * @param to    [out] Nota MIDI a la que se cuantizó.
     * @return      true si hubo un snap nuevo, false si no.
     */
    bool consume_snap_event(uint8_t* from, uint8_t* to);

    /** @brief Número de notas cuantizadas desde el último reset_stats(). */
    uint16_t get_snapped_count() const { return _snapped_count; }

    /** @brief Número total de notas procesadas por snap() desde el último reset_stats(). */
    uint16_t get_total_count()   const { return _total_count; }

    /**
     * @brief Resetea todos los contadores de estadísticas y el flag de evento.
     *
     * Llamar al entrar a AI mode para que la sesión actual arranque limpia.
     */
    void reset_stats();

    /**
     * @brief Activa o desactiva el bypass (pass-through sin cuantización).
     *
     * @param bypass  true → pass-through, false → cuantización activa.
     */
    void set_bypass(bool bypass);

    /** @brief Retorna true si el bypass está activo. */
    bool is_bypass() const { return _bypass; }

    /**
     * @brief Nombre de la escala activa.
     *
     * @return Puntero a string null-terminated. Retorna "---" si no hay
     *         escala detectada con confianza suficiente.
     */
    const char* scale_name() const { return _scale_name; }

    /** @brief True si hay una escala activa con confianza suficiente. */
    bool has_scale() const { return _has_scale; }

    /**
     * @brief Raíz de la escala activa [0-11]. C=0, C#=1, ..., B=11.
     *
     * Necesario para AutoHarmonize (Sprint 37) — la raíz define qué pitch
     * classes componen la escala y en qué grado está cada nota.
     *
     * @return Pitch class de la raíz, o 0 si no hay escala detectada.
     */
    uint8_t get_key_root() const { return _has_scale ? (_active_key % 12) : 0; }

    /**
     * @brief True si la escala activa es menor natural (Aeolian).
     *
     * _active_key codifica: [0-11] = mayor, [12-23] = menor.
     *
     * @return true si menor, false si mayor o sin escala detectada.
     */
    bool get_is_minor() const { return _has_scale && (_active_key >= 12); }

    /**
     * @brief Fija la escala manualmente, ignorando el output del AI.
     *
     * Mientras manual_mode esté activo, update(KeyResult) no modifica _active_key.
     * Efecto inmediato: _active_key se actualiza en la misma llamada.
     *
     * @param key_idx  0-11 = major, 12-23 = minor. Fuera de rango → clamped a 23.
     */
    void set_manual_key(uint8_t key_idx);

    /**
     * @brief Libera el override manual — vuelve a usar la detección AI.
     *
     * El próximo update(KeyResult) con confianza suficiente actualizará _active_key.
     */
    void clear_manual();

    /** @brief true si manual override está activo. */
    bool is_manual() const { return _manual_mode; }

private:
    // Máscara de pitch classes activos en la escala actual.
    // _mask[i] == true → pitch class i está en la escala (C=0, C#=1, ..., B=11).
    bool _mask[12];

    bool    _bypass;
    bool    _has_scale;
    bool    _manual_mode;
    uint8_t _manual_key_idx;   ///< key index fijado manualmente (0-23)
    uint8_t _active_key;       ///< key index activo (manual override o AI)

    // "C_maj\0", "F#_min\0", etc. — max 7 chars + null. Formato heredado de KeyResult.name.
    char _scale_name[8];

    /** Total de notas procesadas por snap() desde el último reset_stats(). */
    uint16_t _total_count;

    /** Notas efectivamente cuantizadas (resultado != original) desde reset_stats(). */
    uint16_t _snapped_count;

    /** Nota original del último snap. Válido cuando _last_snap_event_flag == 1. */
    uint8_t _last_snap_from;

    /** Nota destino del último snap. Válido cuando _last_snap_event_flag == 1. */
    uint8_t _last_snap_to;

    /**
     * Flag rising-edge: vale 1 por un solo ciclo de consume_snap_event() después
     * de un snap efectivo, luego vuelve a 0. Garantiza que cada evento se entrega
     * exactamente una vez al frame 0x83.
     */
    uint8_t _last_snap_event_flag;

    /**
     * @brief Construye _mask a partir de root y tipo (mayor/menor).
     *
     * @param root      Nota raíz [0-11]: C=0, C#=1, ..., B=11.
     * @param is_minor  false = escala mayor diatónica, true = escala menor natural.
     */
    void _build_mask(uint8_t root, bool is_minor);
};
