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

// ── Destructor ───────────────────────────────────────────────────────────────────
TapeSaturate::~TapeSaturate() {
    delete _cIn1;
    delete _cIn2;
}

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
    _ageAmount = amount;

    // Escala logarítmica: f = 20000 × 0.3^amount
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
        _dryWetMix.gain(0, 1.0f);
        _dryWetMix.gain(1, 0.0f);
        _currentMod = 0.0f;
    } else {
        _dryWetMix.gain(0, 1.0f - _mix);
        _dryWetMix.gain(1, _mix);
    }
}

// ── update() ─────────────────────────────────────────────────────────────────────
void TapeSaturate::update(float dt_ms) {
    if (_bypass) {
        _currentMod = 0.0f;
        return;
    }
    const float dt_s = dt_ms * 0.001f;

    _wowPhase      += WOW_FREQ_HZ     * dt_s;
    _wowPhase2     += WOW_FREQ_HZ     * 0.73f * dt_s;
    _flutterPhase  += FLUTTER_FREQ_HZ * dt_s;
    _flutterPhase2 += FLUTTER_FREQ_HZ * 1.05f * dt_s;

    if (_wowPhase      > 1.0f) _wowPhase      -= 1.0f;
    if (_wowPhase2     > 1.0f) _wowPhase2     -= 1.0f;
    if (_flutterPhase  > 1.0f) _flutterPhase  -= 1.0f;
    if (_flutterPhase2 > 1.0f) _flutterPhase2 -= 1.0f;

    float wow_lfo     = sinf(2.0f * PI * _wowPhase)
                      + 0.3f * sinf(2.0f * PI * _wowPhase2);
    float flutter_lfo = sinf(2.0f * PI * _flutterPhase)
                      + 0.3f * sinf(2.0f * PI * _flutterPhase2);

    float cents = (_wowDepth    * 18.0f * wow_lfo)
                + (_flutterDepth * 10.0f * flutter_lfo);

    _currentMod = powf(2.0f, cents / 1200.0f) - 1.0f;

    // Modulación de azimut de cabezal sobre el filtro de cinta (Age LP)
    const float baseCutoff = 20000.0f * powf(0.3f, _ageAmount);
    const float azMod = 1.0f + 0.12f * (_wowDepth * wow_lfo + 0.5f * _flutterDepth * flutter_lfo);
    float f = baseCutoff * azMod;
    if (f < 2500.0f)  f = 2500.0f;
    if (f > 20000.0f) f = 20000.0f;
    _lpFilter.frequency(f);
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
