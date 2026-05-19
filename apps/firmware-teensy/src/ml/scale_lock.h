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
     *
     * @param note  Nota MIDI original [0-127].
     * @return      Nota cuantizada [0-127], o note sin cambios si bypass/sin escala.
     */
    uint8_t snap(uint8_t note) const;

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

private:
    // Máscara de pitch classes activos en la escala actual.
    // _mask[i] == true → pitch class i está en la escala (C=0, C#=1, ..., B=11).
    bool _mask[12];

    bool _bypass;
    bool _has_scale;

    // "C_maj\0", "F#_min\0", etc. — max 7 chars + null. Formato heredado de KeyResult.name.
    char _scale_name[8];

    /**
     * @brief Construye _mask a partir de root y tipo (mayor/menor).
     *
     * @param root      Nota raíz [0-11]: C=0, C#=1, ..., B=11.
     * @param is_minor  false = escala mayor diatónica, true = escala menor natural.
     */
    void _build_mask(uint8_t root, bool is_minor);
};
