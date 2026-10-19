#pragma once

#include "../dsp/zdf_ladder.h"
#include <Audio.h>
#include <math.h>

/**
 * @brief Filtro ladder de 4 polos (24 dB/oct) por voz: núcleo TPT / ZDF con carácter por chip (código custom).
 *
 * Desde 2026-10-18 el núcleo es dsp::ZdfLadder (Zavalishin, *The Art of VA Filter Design*, cap. 5–6): la
 * realimentación se resuelve en la misma muestra, así que la resonancia queda afinada en todo el rango (el modelo
 * D'Angelo & Välimäki discretizado como estaba no pasaba de ~1.65 kHz al auto-oscilar; theory/fidelidad-engines.md
 * §8–9.1). setCharacter() elige el chip: Moog, ARP 4072, IR3109 (Juno-106) o CEM3320 (Prophet-5).
 *
 * API drop-in para AudioFilterStateVariable (frequency, resonance, octaveControl), solo salida LP (port 0).
 * Coexiste con el filtro analógico 2N3904 externo (ADR-003): este opera dentro de cada voz.
 * CPU: 4 etapas TPT + 2 tanh de Padé + una división por muestra; solo el engine activo lo habilita (set_enabled()).
 *
 * Refs: Zavalishin, V. *The Art of VA Filter Design*, rev. 2.1 (2018), cap. 5 y §6.6.
 *       Stinchcombe, T. *Analysis of the Moog Transistor Ladder and Derivative Filters* (2008).
 *       apps/docs/03-filter-design.md §0 · apps/docs/01-architecture.md §5.2
 */

// Código custom (no es Teensy Audio Library oficial)
class MoogLadder4P : public AudioStream {
public:
    MoogLadder4P();

    /**
     * @brief Frecuencia de corte en Hz (1 Hz – Nyquist).
     * Drop-in para AudioFilterStateVariable::frequency().
     * Precomputa el coeficiente A (ec. 20 del paper).
     * @param hz Frecuencia de corte en Hz.
     */
    void frequency(float hz);

    /**
     * @brief Resonancia en la convención de AudioFilterStateVariable.
     * q = 0.707 → k = 0 (sin resonancia) · q = 5.0 → k = 4.4 (auto-oscila desde k ≈ 4.05, amplitud ~0.5)
     * Drop-in para AudioFilterStateVariable::resonance().
     * @param q Resonancia en convención AudioFilterStateVariable [0.707, 5.0].
     */
    void resonance(float q);

    /**
     * @brief No-op — solo para compatibilidad de API con AudioFilterStateVariable.
     * La frecuencia se fija directamente con frequency().
     * @param n Ignorado.
     */
    void octaveControl(float n) { (void)n; }

    /**
     * @brief Habilita o salta el procesamiento del filtro en la ISR de audio.
     *
     * Los engines inactivos deben deshabilitar sus filtros (con el modelo anterior, los 28 filtros juntos
     * colgaban el sketch 28). Deshabilitado: consume y libera el bloque de
     * entrada sin transmitir nada (silencio aguas abajo).
     * Al re-habilitar se resetea el estado para no arrastrar energía vieja.
     * @param on true = procesa (default tras el constructor).
     */
    void set_enabled(bool on);

    /**
     * @brief Ganancia de drive (saturación en la entrada del ladder).
     * 1.0 = nominal (0.1 V/unidad), hasta 4.0 = overdrive BJT rico en armónicos.
     */
    void set_drive(float drive);

    /// Chip que se emula (saturación, compensación de graves y amplitud de auto-oscilación).
    void setCharacter(dsp::LadderCharacter c);

    virtual void update(void) override;

private:
    audio_block_t* inputQueueArray[1];

    dsp::ZdfLadder _zdf;                 // núcleo TPT / ZDF (dsp/zdf_ladder.h)
    volatile bool _enabled = true;       // leído en la ISR de audio, escrito desde loop()
    float _drive = 1.0f;                 // ganancia de entrada [0.5, 4.0]
    float _inv_drive_comp = 1.0f;        // compensación parcial de ganancia

    /** @brief Procesa una muestra. Llamado 128 veces por update(). */
    float processSample(float in);

    /** @brief Resetea estado a 0. Protección ante NaN/Inf en transitorios de parámetros. */
    void resetState();
};
