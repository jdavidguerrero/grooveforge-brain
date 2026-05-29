#pragma once

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
 * CPU estimado por instancia: ~5 tanhf × 128 muestras/bloque ≈ 640 tanhf/bloque.
 * En Teensy 4.1 (ARM Cortex-M7 con FPU): tanhf ≈ 20-30 ciclos, a 600MHz
 * → ~0.05% CPU por instancia de filtro. 23 instancias totales (todos los
 * engines) → ~1.2% CPU adicional en worst case. Dentro del budget de 60%.
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

    virtual void update(void) override;

private:
    audio_block_t* inputQueueArray[1];

    // Estado del ladder — tensiones de salida de cada stage en voltios
    float _V[4];   // V1..V4 — output de cada BJT stage
    float _dV[4];  // derivadas de la muestra anterior (integración bilineal, ec. 15)

    float _A;   // coeficiente de tuning (ec. 20): A = π(fc/fs)(1-π(fc/fs))/(1+π(fc/fs))
    float _g;   // g = 4·fs·A·VT — coeficiente de las derivadas, precomputado en frequency()
    float _k;   // resonancia/feedback: 0 = flat, 4 = self-oscillation

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
