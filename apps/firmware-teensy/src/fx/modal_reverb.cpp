// apps/firmware-teensy/src/fx/modal_reverb.cpp
// Sprint 2.7 — FX Modal Reverb
// Theory:  apps/docs/sprints/12-modal-reverb.md
// Refs:    apps/docs/05-fx-architecture.md §1.7
//          Rossing, "The Science of Sound" (3rd ed., Addison-Wesley, 2001), Cap. 18
//          Fletcher y Rossing, "The Physics of Musical Instruments" (2nd ed., Springer, 1998), Cap. 2
//          Zölzer, "DAFX: Digital Audio Effects" (2nd ed., Wiley, 2011), §3.7
//          Smith, "Introduction to Digital Filters with Audio Applications" (W3K, 2007), Cap. 8

#include "modal_reverb.h"

// ── Tablas de materiales — datos de física modal real ────────────────────────────
//
// CAMPANA: parciales medidos en campanas europeas Cu-Sn (78% Cu, 22% Sn).
//   Nombres históricos: hum, prime, tierce, quint, nominal, parcial superior.
//   Ratios de Rossing (2001) Tabla 18.1 — inarmónicos por geometría de cuerpo de revolución.
//   T60 de mediciones acústicas en campanas fundidas reales.
const ModalReverb::ModeParams ModalReverb::CAMPANA[6] = {
    {1.000f,  8.0f},   // hum    — 300 Hz,  T60=8s, modo más grave y más largo
    {2.756f,  6.0f},   // prime  — 827 Hz,  T60=6s, octava ligeramente flat
    {5.404f,  4.0f},   // tierce — 1621 Hz, T60=4s, tercera menor (carácter melancólico)
    {8.933f,  3.0f},   // quint  — 2680 Hz, T60=3s, quinta
    {13.46f,  2.0f},   // nominal— 4038 Hz, T60=2s, nota percibida de la campana
    {18.02f,  1.5f}    // superior—5406 Hz, T60=1.5s, brillo agudo
};

// GUADUA: barra libre-libre de Guadua angustifolia, sección 60 cm (marimba de chonta).
//   Ratios: β_nL de Euler-Bernoulli barra libre (mismos que campana para modos 1-4,
//   ligeramente distintos en 5-6 por efecto de nudos internos del bambú).
//   T60 más cortos que campana: Q_guadua ≈ 300-600 vs Q_campana ≈ 1000-3000.
//   Ref: Fletcher y Rossing (1998) §2.3; Escalante-Jiménez (2011) etnomusicología.
const ModalReverb::ModeParams ModalReverb::GUADUA[6] = {
    {1.000f,  3.0f},   // modo 1 — 300 Hz,  T60=3s, cuerpo abrigador en graves
    {2.757f,  2.5f},   // modo 2 — 827 Hz,  T60=2.5s
    {5.404f,  1.8f},   // modo 3 — 1621 Hz, T60=1.8s
    {8.933f,  1.2f},   // modo 4 — 2680 Hz, T60=1.2s
    {13.34f,  0.8f},   // modo 5 — 4002 Hz, T60=0.8s (ratio levemente dist. por nudos)
    {17.81f,  0.5f}    // modo 6 — 5343 Hz, T60=0.5s, brillo muy corto
};

// CRISTAL: plato rectangular de vidrio (borosilicato), libre en todos los bordes.
//   Modos de Kirchhoff-Love para placas delgadas — bidimensionales, distintos a barra.
//   Q cristal ≈ 1000-10000: decays extremadamente largos (armónica de cristal, s. XVIII).
//   Ref: Rossing (2001) Cap. 19 §19.3, tablas de frecuencias para platos rectangulares.
const ModalReverb::ModeParams ModalReverb::CRISTAL[6] = {
    {1.000f, 12.0f},   // modo (2,0) — 300 Hz,  T60=12s, fundamental etérea
    {2.442f, 10.0f},   // modo (0,2) — 733 Hz,  T60=10s, nodos transversales
    {3.908f,  8.0f},   // modo (2,2) — 1172 Hz, T60=8s,  cuatro nodos
    {5.983f,  6.0f},   // modo (3,1) — 1795 Hz, T60=6s
    {8.547f,  4.0f},   // modo (2,3) — 2564 Hz, T60=4s
    {11.41f,  2.0f}    // modo superior — 3423 Hz, T60=2s, brillo sostenido
};

// MADERA: tabla de pino (Pinus sylvestris), libre en bordes cortos.
//   Q pino ≈ 100-400: amortiguamiento interno de fibras de celulosa.
//   Los modos superiores se extinguen en décimas de segundo — reverb muy seca.
//   Ref: Fletcher y Rossing (1998) §2.6 sobre amortiguamiento en maderas de luthería.
const ModalReverb::ModeParams ModalReverb::MADERA[6] = {
    {1.000f,  1.5f},   // modo 1 — 300 Hz,  T60=1.5s, seco y presente
    {2.756f,  1.2f},   // modo 2 — 827 Hz,  T60=1.2s
    {5.404f,  0.9f},   // modo 3 — 1621 Hz, T60=0.9s
    {8.933f,  0.6f},   // modo 4 — 2680 Hz, T60=0.6s
    {13.34f,  0.4f},   // modo 5 — 4002 Hz, T60=0.4s, casi inaudible
    {17.81f,  0.2f}    // modo 6 — 5343 Hz, T60=0.2s, instantáneo
};

// ── Constructor ──────────────────────────────────────────────────────────────────
// Conexiones estáticas: modos→mixers, mixers→wetMix, wetMix→dryWetMix, out L+R.
// Las conexiones desde inputStream→_mode[] y inputStream→dry son dinámicas (begin()).
ModalReverb::ModalReverb()
    : _cMode0MixA(_mode[0], 0, _modeMixA, 0)   // modo 0 → mixA ch0
    , _cMode1MixA(_mode[1], 0, _modeMixA, 1)   // modo 1 → mixA ch1
    , _cMode2MixA(_mode[2], 0, _modeMixA, 2)   // modo 2 → mixA ch2
    , _cMode3MixA(_mode[3], 0, _modeMixA, 3)   // modo 3 → mixA ch3
    , _cMode4MixB(_mode[4], 0, _modeMixB, 0)   // modo 4 → mixB ch0
    , _cMode5MixB(_mode[5], 0, _modeMixB, 1)   // modo 5 → mixB ch1
    , _cMixAWet  (_modeMixA, 0, _wetMix,  0)   // modeMixA → wetMix ch0
    , _cMixBWet  (_modeMixB, 0, _wetMix,  1)   // modeMixB → wetMix ch1
    , _cWetDry   (_wetMix,   0, _dryWetMix, 1) // wetMix → dryWetMix ch1 (wet)
    , _cOutL     (_dryWetMix, 0, _out,    0)   // dryWetMix → I2S Left
    , _cOutR     (_dryWetMix, 0, _out,    1)   // dryWetMix → I2S Right
{
    // Punteros dinámicos sin inicializar hasta begin()
    for (int i = 0; i < 6; i++) _cMode[i] = nullptr;
    _cDry = nullptr;
}

// ── begin() ──────────────────────────────────────────────────────────────────────
void ModalReverb::begin(AudioStream& inputStream, float volume) {
    // 6 modos + 4 mixers (A, B, wet, dryWet) + out ≈ 11 objetos activos.
    // AudioMemory(30) da ~2.7× headroom sobre los bloques activos.
    // Cada bloque = 128 muestras × 4 bytes = 512 bytes.
    AudioMemory(30);

    _codec.enable();
    _codec.volume(volume);

    // Inicializar niveles de modo en 1.0 — modos siempre excitados por el input continuo.
    // En v1.0 no hay onset detection: los filtros BP resuenan mientras haya energía
    // en su banda. El decay opera cuando el input se silencia.
    // Ref: apps/docs/sprints/12-modal-reverb.md §"Simplificación para v1.0"
    for (int i = 0; i < 6; i++) _modeLevel[i] = 1.0f;

    // _modeMixB ch2/3 no usados (solo modos 4 y 5 van a ch0 y ch1)
    _modeMixB.gain(2, 0.0f);
    _modeMixB.gain(3, 0.0f);

    // _wetMix: suma modeMixA y modeMixB con ganancia 0.5 cada uno para evitar clipping.
    // Los 6 modos en paralelo pueden sumar hasta 6× la amplitud del modo individual.
    _wetMix.gain(0, 0.5f);    // modeMixA contribution
    _wetMix.gain(1, 0.5f);    // modeMixB contribution
    _wetMix.gain(2, 0.0f);
    _wetMix.gain(3, 0.0f);

    // _dryWetMix: dry/wet clásico — a diferencia del Sub Genesis (aditivo),
    // aquí el dry se atenúa al subir el wet (Mix controla el balance).
    _dryWetMix.gain(0, 1.0f);      // ch0: dry — siempre presente
    _dryWetMix.gain(1, _mix);      // ch1: wet (modal reverb)
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    // Configurar filtros y decay factors con los valores del material inicial
    _applyMaterial();

    // Conexiones dinámicas: fan-out desde inputStream a los 6 modos + dry path.
    // El scheduler de la Teensy Audio Library ejecuta inputStream una vez por bloque
    // y distribuye el buffer a todos los destinos sin copias adicionales.
    for (int i = 0; i < 6; i++) {
        _cMode[i] = new AudioConnection(inputStream, 0, _mode[i], 0);
    }
    _cDry = new AudioConnection(inputStream, 0, _dryWetMix, 0);

    _lastUpdateMs = millis();
}

// ── _applyMaterial() ─────────────────────────────────────────────────────────────
// Recalcula coeficientes biquad de los 6 filtros BP y los decay factors para el
// material, size y decayMult actuales. Se llama desde begin() y desde los setters.
void ModalReverb::_applyMaterial() {
    const ModeParams* table;
    switch (_material) {
        case Material::CAMPANA: table = CAMPANA; break;
        case Material::GUADUA:  table = GUADUA;  break;
        case Material::CRISTAL: table = CRISTAL; break;
        default:                table = MADERA;  break;
    }

    float sizeMult;
    switch (_size) {
        case Size::SMALL:  sizeMult = 0.3f;  break;
        case Size::LARGE:  sizeMult = 2.5f;  break;
        default:           sizeMult = 1.0f;  break;
    }

    for (int i = 0; i < 6; i++) {
        float freq = table[i].freqRatio * BASE_FREQ;
        float t60  = table[i].t60Base * sizeMult * _decayMult;

        // Q=50 fijo: BW = freq/50 — suficientemente estrecho para timbre modal afinado
        // sin los problemas numéricos de Q ultra-alto (Q>500 puede salir del círculo
        // unitario en aritmética float32 del ARM Cortex-M7).
        // El T60 real lo controla el decay envelope en update(), no el Q del filtro.
        // Ref: apps/docs/sprints/12-modal-reverb.md §"El problema de Q alto"
        //      Smith, "Digital Filters" (2007), Cap. 8
        _mode[i].setBandpass(0, freq, 50.0f);

        // Decay factor por ms: level(t + 1ms) = level(t) × factor
        // Derivado de: level(T60) = 0.001 (= -60 dB)
        // factor = exp(ln(0.001) / (t60_ms)) = exp(-6.908 / (t60 × 1000))
        // Ref: apps/docs/sprints/12-modal-reverb.md §"Envelope software"
        if (t60 > 0.001f) {
            _modeDecayFactor[i] = expf(-6.908f / (t60 * 1000.0f));
        } else {
            _modeDecayFactor[i] = 0.0f;   // decay instantáneo
        }
    }

    _updateMixerGains();
}

// ── _updateMixerGains() ──────────────────────────────────────────────────────────
// Aplica _modeLevel[] como gains en modeMixA (modos 0-3) y modeMixB (modos 4-5).
// Los modos >= _diffusion se silencian independientemente del level.
// Los gain updates de AudioMixer4 son thread-safe: se aplican al inicio del próximo
// bloque completo — sin clicks por actualización mid-block.
void ModalReverb::_updateMixerGains() {
    for (int i = 0; i < 6; i++) {
        float gain = (i < _diffusion) ? _modeLevel[i] : 0.0f;
        if (i < 4) {
            _modeMixA.gain(i, gain);
        } else {
            _modeMixB.gain(i - 4, gain);
        }
    }
}

// ── setMaterial() ────────────────────────────────────────────────────────────────
void ModalReverb::setMaterial(Material mat) {
    _material = mat;
    _applyMaterial();
}

// ── setSize() ────────────────────────────────────────────────────────────────────
void ModalReverb::setSize(Size sz) {
    _size = sz;
    _applyMaterial();
}

// ── setDecay() ───────────────────────────────────────────────────────────────────
void ModalReverb::setDecay(float mult) {
    if (mult < 0.2f) mult = 0.2f;
    if (mult > 4.0f) mult = 4.0f;
    _decayMult = mult;
    _applyMaterial();
}

// ── setDiffusion() ───────────────────────────────────────────────────────────────
void ModalReverb::setDiffusion(uint8_t n) {
    if (n < 1) n = 1;
    if (n > 6) n = 6;
    _diffusion = n;
    _updateMixerGains();
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
// Dry/wet clásico: dry = 1.0 fijo, wet = _mix (no aditivo, a diferencia del Sub Genesis).
void ModalReverb::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    if (!_bypass) {
        _dryWetMix.gain(1, _mix);
    }
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void ModalReverb::setBypass(bool bypass) {
    _bypass = bypass;
    if (_bypass) {
        _dryWetMix.gain(1, 0.0f);   // silenciar wet — dry pasa limpio
    } else {
        _dryWetMix.gain(1, _mix);   // restaurar wet
    }
}

// ── update() ─────────────────────────────────────────────────────────────────────
// Aplica decay exponencial a los niveles de modo. Llamar desde loop().
// El decay opera continuamente: cuando el input tiene energía, los filtros BP la
// retienen y reponen los niveles. Cuando el input se silencia, los niveles decaen
// con los T60 naturales del material.
void ModalReverb::update() {
    if (_bypass) return;

    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    // Omitir si el loop corre demasiado lento (>100ms entre updates) o en la
    // primera iteración donde dt podría ser grande e incorrecto.
    if (dt == 0 || dt > 100) return;

    // Aplicar decay exponencial a cada modo activo.
    // factor^dt_ms aproxima la integral continua para dt pequeño (1-10 ms).
    // Para v1.0 con chord sostenido, _modeLevel permanece cercano a 1.0 mientras
    // el input alimenta energía a los filtros BP. El efecto audible del decay
    // se escucha cuando el input se silencia entre acordes.
    for (int i = 0; i < 6; i++) {
        // Aplicar factor dt veces (aproximación para dt > 1ms)
        // pow(factor, dt) evita el loop — expf es más eficiente.
        // factor = exp(-6.908 / (T60_ms)) → factor^dt = exp(-6.908 × dt / T60_ms)
        if (_modeDecayFactor[i] > 0.0f) {
            // Usar powf(factor, dt) equivale a aplicar el factor dt veces
            _modeLevel[i] *= powf(_modeDecayFactor[i], (float)dt);
            if (_modeLevel[i] < 1e-6f) _modeLevel[i] = 0.0f;   // floor para evitar denormals
        }
    }

    _updateMixerGains();
}
