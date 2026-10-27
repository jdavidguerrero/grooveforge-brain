// apps/firmware-teensy/src/fx/tape_saturate.cpp
// Sprint 2.3 — FX Tape Saturate
// Theory: apps/docs/sprints/08-tape-saturate.md
// Refs:   apps/docs/05-fx-architecture.md §1.5
//         Zölzer, "DAFX" Cap. 5 "Nonlinear Processing"

#include "tape_saturate.h"
#include <math.h>

TapeSaturate::TapeSaturate() {}

TapeSaturate::~TapeSaturate() { delete _cIn; }

// ── begin() ──────────────────────────────────────────────────────────────────────
// AudioMemory y codec son responsabilidad del sketch.
void TapeSaturate::begin(AudioStream& inputStream) {
    _stage.set_drive(2.0f);              // saturación sutil hasta que el preset diga otra cosa
    _stage.set_mix(_mix);
    _stage.set_bypass(_bypass);
    _cIn = new AudioConnection(inputStream, 0, _stage, 0);
}

// ── setDrive() ───────────────────────────────────────────────────────────────────
void TapeSaturate::setDrive(float drive) { _stage.set_drive(drive); }

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
    _lpApplied = 20000.0f * powf(0.3f, amount);
    _stage.set_cutoff(_lpApplied);
}

// ── setMix() ─────────────────────────────────────────────────────────────────────
void TapeSaturate::setMix(float wet) {
    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;
    _mix = wet;
    _stage.set_mix(_mix);
}

// ── setBypass() ──────────────────────────────────────────────────────────────────
void TapeSaturate::setBypass(bool bypass) {
    _bypass = bypass;
    _stage.set_bypass(bypass);
    if (_bypass) _currentMod = 0.0f;
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
    float f = (_wowDepth > 0.0f || _flutterDepth > 0.0f) ? baseCutoff * azMod : baseCutoff;
    if (f < 2500.0f)  f = 2500.0f;
    if (f > 20000.0f) f = 20000.0f;
    // El stage suaviza el corte por muestra; solo se le manda si cambió (> 1 %)
    if (_lpApplied < 0.0f || fabsf(f - _lpApplied) > 0.01f * _lpApplied) {
        _lpApplied = f;
        _stage.set_cutoff(f);
    }
}

// ── getWowFlutterMod() ────────────────────────────────────────────────────────────
float TapeSaturate::getWowFlutterMod() const {
    return _currentMod;
}
