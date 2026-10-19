#pragma once

#include "../dsp/ladder_core.h"
#include <Audio.h>
#include <math.h>

/**
 * @brief Filtro digital Moog ladder — D'Angelo & Välimäki, ICASSP 2013.
 *
 * Modelo white-box basado en análisis de circuito del ladder Moog con
 * transistores BJT (Ebers-Moll). Discretización via transformada bilineal.
 *
 * Características:
 *   - 4 polos LP (24 dB/oct), igual que el ladder físico 2N3904
 *   - No-lineal: saturación tanhf modela la distorsión de los transistores
 *   - Self-oscillation real en k = 4.0 (resonancia = 100%)
 *   - Computacionalmente eficiente: 5 tanhf + 13 sumas + 9 mult por muestra
 *
 * API drop-in para AudioFilterStateVariable (mismos métodos: frequency,
 * resonance, octaveControl). Sustituye AudioFilterStateVariable en todos
 * los engines del GrooveForge Brain.
 *
 * Coexiste con el filtro analógico físico 2N3904: este filtro opera
 * dentro de cada voz sintética (dominio digital), el 2N3904 aplica
 * carácter analógico global sobre la mezcla final.
 *
 * Nota sobre port de salida: AudioFilterStateVariable expone 3 puertos
 * (0=LP, 1=BP, 2=HP). MoogLadder4P solo expone port 0 (LP). Las
 * AudioConnection existentes que usen port 0 son directamente compatibles.
 *
 * CPU por instancia: con tanhf (7 por muestra, ~200 ciclos cada una) era ~8% (medido 2026-10-15) y los 28
 * filtros juntos colgaban el sketch 28. Desde 2026-10-16 usa 5 tanh de Padé por muestra (dsp/fast_math.h,
 * theory/fidelidad-engines.md §1): estimado ~1%, a medir. Igual solo el engine activo debe tenerlos habilitados
 * (set_enabled()).
 *
 * Refs: D'Angelo, S. & Välimäki, V. (2013). "An improved virtual analog
 *       model of the Moog ladder filter." ICASSP 2013. DOI: 10.1109/ICASSP.2013.6638782
 *       apps/docs/03-filter-design.md §0 — rol del filtro en el sistema
 *       apps/docs/01-architecture.md §5.2 — CPU budget
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
     * @brief Resonancia — mapea el rango AudioFilterStateVariable a k de D'Angelo.
     * q = 0.707 (Butterworth) → k = 0.0 (sin resonancia)
     * q = 5.0 (máximo)       → k = 4.0 (self-oscillation)
     * Drop-in para AudioFilterStateVariable::resonance().
     * @param q Resonancia en convención AudioFilterStateVariable [0.707, 5.0].
     */
    void resonance(float q);

    /**
     * @brief No-op — solo para compatibilidad de API con AudioFilterStateVariable.
     * AudioFilterStateVariable::octaveControl() controla tracking de CV;
     * el modelo D'Angelo fija la frecuencia directamente vía frequency().
     * @param n Ignorado.
     */
    void octaveControl(float n) { (void)n; }

    /**
     * @brief Habilita o salta el procesamiento del filtro en la ISR de audio.
     *
     * Cada instancia cuesta ~8% de CPU del Teensy 4.1 (5 tanhf × 128 muestras,
     * tanhf ≈ 200 ciclos medidos). Con los 28 filtros de los 6 engines activos a la
     * vez la ISR supera el 200% y el loop() deja de correr. Los engines inactivos
     * deben deshabilitar sus filtros. Deshabilitado: consume y libera el bloque de
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

    virtual void update(void) override;

private:
    audio_block_t* inputQueueArray[1];

    // Estado del ladder — tensiones de salida de cada stage en voltios
    dsp::LadderState _s;   // V1..V4 y derivadas de la muestra anterior (dsp/ladder_core.h)

    float _A;   // coeficiente de tuning (ec. 20): A = π(fc/fs)(1-π(fc/fs))/(1+π(fc/fs))
    float _g;   // g = 4·fs·A·VT — coeficiente de las derivadas, precomputado en frequency()
    volatile bool _enabled = true;  // leído en la ISR de audio, escrito desde loop()

    float _k;   // resonancia/feedback: 0 = flat, 4 = self-oscillation
    float _drive = 1.0f;            // multiplicador de saturación [1.0, 4.0]
    float _inv_drive_comp = 1.0f;   // compensación parcial de ganancia

    // Constantes del modelo analógico (valores físicos de transistores BJT @ 300K)
    static constexpr float VT     = 0.026f;              ///< tensión térmica [V] (= kT/q, 300K)
    static constexpr float INV2VT = 1.0f / (2.0f * VT); ///< 1/(2VT) — precomputado

    // Escala entrada/salida: el audio Teensy está normalizado ±1.0 (int16 / 32768).
    // DRIVE = 0.1 V/unidad → tanh(-1.0 × 0.1 / (2×0.026)) = tanh(-1.92) ≈ -0.96:
    // saturación suave, evita clipping duro en la región lineal.
    static constexpr float DRIVE     = 0.1f;
    static constexpr float INV_DRIVE = 1.0f / DRIVE;

    /** @brief Procesa una muestra. Llamado 128 veces por update(). */
    float processSample(float in);

    /** @brief Resetea estado a 0. Protección ante NaN/Inf en transitorios de parámetros. */
    void resetState();
};
