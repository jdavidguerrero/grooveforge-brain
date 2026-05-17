#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
#include <math.h>

/**
 * @brief FX Modal Reverb — Sprint 2.7.
 *
 * Seis filtros bandpass en paralelo, cada uno modelando un modo resonante físico
 * de un objeto material real (campana, guadua, cristal, madera). El banco de filtros
 * colorea la señal con las resonancias características de ese material — un reverb
 * con identidad auditiva, no un algoritmo de densidad estadística.
 *
 * Arquitectura dual Q-filtro + envelope software:
 *   - Q fijo en 50 por filtro: selectividad espectral suficiente sin inestabilidad
 *     numérica (Q ultra-alto en biquad float32 puede salir del círculo unitario).
 *   - El T60 real del modo lo controla el gain del AudioMixer4 correspondiente,
 *     actualizado en update() con un coeficiente de decay exponencial precalculado.
 *
 * Signal flow:
 *   inputStream ──┬──→ _mode[0] (BP, ratio_0 × 300Hz, Q=50) → _modeMixA(ch0)
 *                 ├──→ _mode[1] (BP, ratio_1 × 300Hz, Q=50) → _modeMixA(ch1)
 *                 ├──→ _mode[2] (BP, ratio_2 × 300Hz, Q=50) → _modeMixA(ch2)
 *                 ├──→ _mode[3] (BP, ratio_3 × 300Hz, Q=50) → _modeMixA(ch3)
 *                 ├──→ _mode[4] (BP, ratio_4 × 300Hz, Q=50) → _modeMixB(ch0)
 *                 └──→ _mode[5] (BP, ratio_5 × 300Hz, Q=50) → _modeMixB(ch1)
 *                          _modeMixA → _wetMix(ch0)
 *                          _modeMixB → _wetMix(ch1)
 *                          _wetMix   → _dryWetMix(ch1)  [wet]
 *   inputStream ─────────────────────→ _dryWetMix(ch0)  [dry]
 *                          _dryWetMix → _out L+R
 *
 * CPU estimado: ~8-10% (budget en 05-fx-architecture.md §1.7: ~12%).
 * AudioMemory: begin() llama AudioMemory(30) — 6 modos + 4 mixers + overhead.
 *
 * Theory completa: apps/docs/sprints/12-modal-reverb.md
 * Refs: apps/docs/05-fx-architecture.md §1.7
 *       apps/docs/01-architecture.md §5.2
 *       Rossing, "The Science of Sound" (3rd ed., 2001), Cap. 18
 *       Fletcher y Rossing, "The Physics of Musical Instruments" (2nd ed., 1998), Cap. 2
 *       Zölzer, "DAFX: Digital Audio Effects" (2nd ed., 2011), §3.7
 *       Smith, "Introduction to Digital Filters with Audio Applications" (2007), Cap. 8
 */
class ModalReverb {
public:

    // ── Material — set de frecuencias de modos y T60 base ────────────────────────
    enum class Material { CAMPANA = 0, GUADUA, CRISTAL, MADERA };

    // ── Size — escala los T60 sin cambiar las frecuencias relativas de modos ─────
    // SMALL=×0.30, MEDIUM=×1.00, LARGE=×2.50
    // Física: objeto más grande → misma Q del material → T60 más largos porque f baja.
    // Ref: apps/docs/sprints/12-modal-reverb.md §"Size"
    enum class Size { SMALL = 0, MEDIUM, LARGE };

    // ── Datos de modo: ratio de frecuencia sobre BASE_FREQ y T60 base en segundos ─
    // Los ratios de CAMPANA y GUADUA siguen los β_nL de Euler-Bernoulli barra libre-libre.
    // Los ratios de CRISTAL siguen modos de Kirchhoff-Love para plato rectangular libre.
    // Ref: Fletcher y Rossing (1998), Cap. 2 §2.3; Rossing (2001), Cap. 18 Tabla 18.1
    struct ModeParams {
        float freqRatio;   ///< multiplicador sobre BASE_FREQ (300 Hz)
        float t60Base;     ///< tiempo de decay a -60 dB en segundos (material base, Size MEDIUM)
    };

    static constexpr float BASE_FREQ = 300.0f;  ///< frecuencia del modo 1, todos los materiales

    // Tablas de materiales — definidas en .cpp, datos de física modal real
    static const ModeParams CAMPANA[6];
    static const ModeParams GUADUA[6];
    static const ModeParams CRISTAL[6];
    static const ModeParams MADERA[6];

    ModalReverb();

    /**
     * @brief Conecta el stream de entrada e inicializa el codec SGTL5000.
     *
     * Crea las 7 AudioConnection dinámicas (6 fan-out a modos + 1 dry).
     * Debe llamarse una vez en setup(), después de configurar la fuente de audio.
     * Llama AudioMemory(30) internamente — no llamar AudioMemory() desde el sketch.
     *
     * @param inputStream Stream de audio de entrada (ej: AudioMixer4 del engine chord).
     * @param volume      Volumen del codec SGTL5000, 0.0–1.0 (default 0.5).
     */
    void begin(AudioStream& inputStream, float volume = 0.5f);

    /**
     * @brief Selecciona el material resonante.
     *
     * Carga el set de (freqRatio, t60Base) del material elegido y recalcula los
     * coeficientes biquad y los decay factors de todos los modos. Se aplica en loop(),
     * no en el audio thread — sin riesgo de xrun.
     *
     * @param mat Material (CAMPANA, GUADUA, CRISTAL, MADERA).
     */
    void setMaterial(Material mat);

    /**
     * @brief Escala el tamaño virtual del objeto resonante.
     *
     * Multiplica los T60 por: SMALL=0.30, MEDIUM=1.00, LARGE=2.50.
     * Las frecuencias de modos no cambian — el tamaño afecta solo los decays.
     *
     * @param sz Size (SMALL, MEDIUM, LARGE).
     */
    void setSize(Size sz);

    /**
     * @brief Multiplicador global de decay sobre todos los modos.
     *
     * T60_final_n = T60_base_n × size_mult × decay_mult.
     * 0.2 → decays 5× más cortos (reverb seca). 4.0 → decays 4× más largos.
     *
     * @param mult Multiplicador [0.2, 4.0].
     */
    void setDecay(float mult);

    /**
     * @brief Número de modos resonantes activos.
     *
     * Los modos 0..n-1 están activos (gain > 0 en el mixer). Los modos n..5 se
     * silencian (gain = 0). Los filtros siguen procesando — el scheduler no los omite.
     * n=1 → resonancia única (pitch resonator). n=6 → densidad modal completa.
     *
     * @param n Modos activos [1, 6].
     */
    void setDiffusion(uint8_t n);

    /**
     * @brief Balance dry/wet clásico.
     *
     * 0.0 → solo dry. 1.0 → solo wet (reverb modal pura). Sweet spot: 0.5–0.7.
     * A diferencia del Sub Genesis (aditivo), aquí el dry se atenúa al subir el wet.
     *
     * @param wet Nivel wet [0.0, 1.0].
     */
    void setMix(float wet);

    /**
     * @brief Activa o desactiva el bypass del FX.
     *
     * bypass=true:  silencia el wet (_dryWetMix ch1 = 0.0), dry pasa limpio.
     * bypass=false: restaura _mix en el canal wet.
     *
     * @param bypass true = bypass activo, false = FX activo.
     */
    void setBypass(bool bypass);

    /**
     * @brief Actualiza los decay envelopes de los modos. Llamar desde loop().
     *
     * Aplica el coeficiente de decay exponencial a _modeLevel[i] y actualiza
     * los gains del AudioMixer4 correspondiente. Thread-safe: los gain updates
     * de AudioMixer4 se aplican al inicio del próximo bloque completo de audio.
     *
     * Para v1.0 con chord sostenido: _modeLevel se mantiene en 1.0 (los filtros BP
     * se alimentan continuamente). El decay opera cuando el input se silencia.
     * Ref: apps/docs/sprints/12-modal-reverb.md §"Simplificación para v1.0"
     */
    void update();

private:
    // ── Audio objects internos (Teensy Audio Library oficial) ─────────────────────
    AudioFilterBiquad   _mode[6];      ///< 6 modos resonantes en paralelo (BP, Q=50)
    AudioMixer4         _modeMixA;     ///< modos 0–3 (AudioMixer4 acepta 4 entradas máx)
    AudioMixer4         _modeMixB;     ///< modos 4–5
    AudioMixer4         _wetMix;       ///< mezcla _modeMixA(ch0) + _modeMixB(ch1)
    AudioMixer4         _dryWetMix;    ///< dry(ch0) + wet(ch1)
    AudioOutputI2S      _out;
    AudioControlSGTL5000 _codec;

    // ── Conexiones estáticas — inicializadas en lista del constructor ─────────────
    // 11 conexiones: modos→mixA/B, mixA/B→wetMix, wetMix→dryWetMix, dryWetMix→out L+R
    AudioConnection _cMode0MixA;   // _mode[0] → _modeMixA(ch0)
    AudioConnection _cMode1MixA;   // _mode[1] → _modeMixA(ch1)
    AudioConnection _cMode2MixA;   // _mode[2] → _modeMixA(ch2)
    AudioConnection _cMode3MixA;   // _mode[3] → _modeMixA(ch3)
    AudioConnection _cMode4MixB;   // _mode[4] → _modeMixB(ch0)
    AudioConnection _cMode5MixB;   // _mode[5] → _modeMixB(ch1)
    AudioConnection _cMixAWet;     // _modeMixA → _wetMix(ch0)
    AudioConnection _cMixBWet;     // _modeMixB → _wetMix(ch1)
    AudioConnection _cWetDry;      // _wetMix   → _dryWetMix(ch1) [wet path]
    AudioConnection _cOutL;        // _dryWetMix → _out(L, ch0)
    AudioConnection _cOutR;        // _dryWetMix → _out(R, ch1)

    // ── Conexiones dinámicas — creadas en begin(), dependen de inputStream ────────
    // Fan-out: 6 desde inputStream hacia cada _mode[] + 1 hacia dry path.
    // La Teensy Audio Library soporta fan-out implícito — el scheduler ejecuta la
    // fuente una vez y distribuye el buffer a todos los destinos conectados.
    AudioConnection* _cMode[6];    // inputStream → _mode[n]
    AudioConnection* _cDry;        // inputStream → _dryWetMix(ch0) [dry path]

    // ── Estado de parámetros ──────────────────────────────────────────────────────
    Material _material   = Material::CAMPANA;
    Size     _size       = Size::MEDIUM;
    float    _decayMult  = 1.0f;    ///< multiplicador global de decay [0.2, 4.0]
    uint8_t  _diffusion  = 6;       ///< modos activos [1, 6]
    float    _mix        = 0.6f;
    bool     _bypass     = false;

    // Decay envelope por modo (software — ver theory §"arquitectura dual")
    float    _modeLevel[6]       = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    // decay_coeff_n = exp(-6.908 / (T60_final_n × 1000)) — factor por ms
    float    _modeDecayFactor[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    uint32_t _lastUpdateMs       = 0;

    /**
     * @brief Recalcula coeficientes biquad y decay factors del material actual.
     *
     * Usa _material, _size y _decayMult para derivar freq y T60 final de cada modo.
     * Llama _updateMixerGains() al finalizar.
     */
    void _applyMaterial();

    /**
     * @brief Aplica los _modeLevel[] actuales como gains en _modeMixA y _modeMixB.
     *
     * Los modos >= _diffusion se silencian (gain = 0.0) independientemente del level.
     */
    void _updateMixerGains();
};
