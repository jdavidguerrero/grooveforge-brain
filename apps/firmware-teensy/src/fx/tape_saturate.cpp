// apps/firmware-teensy/src/fx/tape_saturate.cpp
// Sprint 2.3 — FX Tape Saturate
// Theory: apps/docs/sprints/08-tape-saturate.md
// Refs:   apps/docs/05-fx-architecture.md §1.5
//         Zölzer, "DAFX" Cap. 5 "Nonlinear Processing"

#include "tape_saturate.h"
#include <math.h>

// ── Constructor ──────────────────────────────────────────────────────────────────
// Las AudioConnections que no dependen de inputStream se inicializan aquí.
// _cIn1 y _cIn2 dependen de inputStream (desconocido en tiempo de compilación)
// y se crean en begin() con new AudioConnection.
TapeSaturate::TapeSaturate()
    : _cWaveLp(_waveshaper,  0, _lpFilter,  0)   // waveshaper out → lpFilter in
    , _cLpHp  (_lpFilter,    0, _hpFilter,  0)   // lpFilter lowpass(out0) → hpFilter in
    , _cHpWet (_hpFilter,    2, _dryWetMix, 1)   // hpFilter highpass(out2) → wet channel
{}

// ── begin() ──────────────────────────────────────────────────────────────────────
// AudioMemory y codec son responsabilidad del sketch.
void TapeSaturate::begin(AudioStream& inputStream) {
    // Drive inicial: saturación sutil (curva casi lineal, apenas colorea el sonido)
    _buildWaveshapeTable(2.0f);

    // LP: cinta nueva — rolloff a 18 kHz (apenas audible, actúa como suavizante)
    _lpFilter.frequency(18000.0f);
    _lpFilter.resonance(0.7f);   // Q=0.7 ≈ Butterworth: sin pico de resonancia

    // HP: subsónico fijo — elimina contenido <20 Hz que el waveshaper puede generar
    // como offset o por acumulación numérica en el LFO de wow/flutter
    _hpFilter.frequency(20.0f);
    _hpFilter.resonance(0.7f);

    // Wet 70% / dry 30% por defecto — mezcla paralela que preserva transientes
    _dryWetMix.gain(0, 1.0f - _mix);   // ch0 = dry
    _dryWetMix.gain(1, _mix);           // ch1 = wet
    _dryWetMix.gain(2, 0.0f);
    _dryWetMix.gain(3, 0.0f);

    // Conexiones dinámicas: inputStream no era conocido en el constructor
    _cIn1 = new AudioConnection(inputStream, 0, _waveshaper, 0);  // → wet path
    _cIn2 = new AudioConnection(inputStream, 0, _dryWetMix,  0);  // → dry path
}

// ── setDrive() ───────────────────────────────────────────────────────────────────
void TapeSaturate::setDrive(float drive) {
    if (drive < 1.0f)  drive = 1.0f;
    if (drive > 10.0f) drive = 10.0f;
    _buildWaveshapeTable(drive);
}

// ── setWow() ─────────────────────────────────────────────────────────────────────
void TapeSaturate::setWow(float depth) {
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    _wowDepth = depth;
}

// ── setFlutter() ─────────────────────────────────────────────────────────────────
void TapeSaturate::setFlutter(float depth) {
    if (depth < 0.0f) depth = 0.0f;
    if (depth > 1.0f) depth = 1.0f;
    _flutterDepth = depth;
}

// ── setAge() ─────────────────────────────────────────────────────────────────────
void TapeSaturate::setAge(float amount) {
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;

    // Escala logarítmica: f = 20000 × 0.3^amount
    // amount=0 → 20000 Hz (cinta nueva), amount=1 → 6000 Hz (cinta muy desgastada)
    // Ref: apps/docs/sprints/08-tape-saturate.md §"Age"
    float cutoff = 20000.0f * powf(0.3f, amount);
    _lpFilter.frequency(cutoff);
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
void TapeSaturate::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    if (!_bypass) {
        _dryWetMix.gain(0, 1.0f - _mix);
        _dryWetMix.gain(1, _mix);
    }
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void TapeSaturate::setBypass(bool bypass) {
    _bypass = bypass;
    if (_bypass) {
        // Señal seca pasa directamente; wet silenciado
        _dryWetMix.gain(0, 1.0f);
        _dryWetMix.gain(1, 0.0f);
    } else {
        // Restaura la mezcla previa
        _dryWetMix.gain(0, 1.0f - _mix);
        _dryWetMix.gain(1, _mix);
    }
}

// ── update() ─────────────────────────────────────────────────────────────────────
void TapeSaturate::update(float dt_ms) {
    const float dt_s = dt_ms * 0.001f;

    // Avance de fases (wrap en 1.0 para mantener precisión numérica a largo plazo)
    _wowPhase     += WOW_FREQ_HZ     * dt_s;
    _wowPhase2    += WOW_FREQ_HZ     * 0.73f * dt_s;   // 0.73× rompe periodicidad sin generador de ruido
    _flutterPhase += FLUTTER_FREQ_HZ * dt_s;
    _flutterPhase2 += FLUTTER_FREQ_HZ * 1.05f * dt_s;  // 1.05× ídem para flutter

    if (_wowPhase     > 1.0f) _wowPhase     -= 1.0f;
    if (_wowPhase2    > 1.0f) _wowPhase2    -= 1.0f;
    if (_flutterPhase > 1.0f) _flutterPhase  -= 1.0f;
    if (_flutterPhase2 > 1.0f) _flutterPhase2 -= 1.0f;

    // Señal LFO: suma de dos senoides → quiebra la regularidad perfecta de un LFO sinusoidal
    // Amplitud 0.3 del secundario: suficiente para romper la simetría sin dominar la modulación
    float wow_lfo    = sinf(2.0f * PI * _wowPhase)
                     + 0.3f * sinf(2.0f * PI * _wowPhase2);
    float flutter_lfo = sinf(2.0f * PI * _flutterPhase)
                      + 0.3f * sinf(2.0f * PI * _flutterPhase2);

    // Escala cents → factor multiplicativo de frecuencia
    // wow depth=1.0 → ±15 cents; flutter depth=1.0 → ±8 cents
    // Rango spec: apps/docs/05-fx-architecture.md §1.5 "LFO drift modulando pitch ±2-15 cents"
    float cents = (_wowDepth    * 15.0f * wow_lfo)
                + (_flutterDepth * 8.0f  * flutter_lfo);

    // 2^(cents/1200) convierte cents a ratio de frecuencia (escala igual-temperada)
    _currentMod = powf(2.0f, cents / 1200.0f) - 1.0f;
}

// ── getWowFlutterMod() ────────────────────────────────────────────────────────────
float TapeSaturate::getWowFlutterMod() const {
    return _currentMod;
}

// ── _buildWaveshapeTable() ────────────────────────────────────────────────────────
void TapeSaturate::_buildWaveshapeTable(float drive) {
    // f(x) = tanh(drive * x) / tanh(drive)
    // Normalización por tanh(drive): garantiza f(1.0) = 1.0 para cualquier drive.
    // Sin normalización, la salida máxima sería tanh(drive) < 1.0 para drive < ∞.
    // Ref: Zölzer, "DAFX" Cap. 5.1, pp. 108-118
    float norm = tanhf(drive);
    // Evitar división por cero para drive muy pequeño (no debería ocurrir con clamp a 1.0)
    if (norm < 0.001f) norm = 0.001f;

    for (int i = 0; i < 257; i++) {
        float x = (float)i / 128.0f - 1.0f;    // mapea índice 0–256 → x en [-1.0, +1.0]
        _waveshapeTable[i] = tanhf(drive * x) / norm;
    }

    // AudioEffectWaveshaper aplica la tabla por lookup + interpolación lineal, O(1)/muestra
    _waveshaper.shape(_waveshapeTable, 257);
}
