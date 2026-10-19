// apps/firmware-teensy/src/engines/moog_model_d.cpp
// Moog Minimoog Model D — PolyBLEP VCOs, AnalogEnv RC, Mono Legato Stack & Ladder Drive.

#include "moog_model_d.h"
#include <math.h>

MoogModelD::MoogModelD() :
    _c1(_osc1,   0, _mixer,  0),
    _c2(_osc2,   0, _mixer,  1),
    _c3(_osc3,   0, _mixer,  2),
    _c4(_noise,  0, _mixer,  3),
    _c5(_mixer,  0, _filter, 0),
    _c6(_filter, 0, _vcaEnv, 0)
{}

void MoogModelD::begin(float volume) {
    (void)volume;

    // Semillas de drift analógico distintas para cada VCO
    _osc1.setSeed(0x13579BDFu);
    _osc2.setSeed(0x2468ACE0u);
    _osc3.setSeed(0x369CF258u);
    _osc1.setDrift(0.0007f);
    _osc2.setDrift(0.0009f);
    _osc3.setDrift(0.0008f);

    _osc1.begin(WAVEFORM_SAWTOOTH);
    _osc2.begin(WAVEFORM_SAWTOOTH);
    _osc3.begin(WAVEFORM_SAWTOOTH);
    _osc1.amplitude(1.0f);
    _osc2.amplitude(1.0f);
    _osc3.amplitude(1.0f);

    _updateOscFreqs();
    _applyMixerGains();

    _filter.frequency(_filterCutoff);
    _filter.resonance(0.7f);
    _filter.set_drive(1.35f);   // ligera saturación cálida del mixer CP3 sobre el ladder
    _filter.octaveControl(1.0f);

    _filterEnv.setAttackOvershoot(0.3f);   // contorno Moog: carga RC muy curva (fidelidad §10)
    _vcaEnv.attackOvershoot(0.3f);
    _filterEnv.setAttack(20.0f);
    _filterEnv.setDecay(300.0f);
    _filterEnv.setSustain(0.4f);
    _filterEnv.setRelease(400.0f);

    _vcaEnv.attack(10.0f);
    _vcaEnv.decay(200.0f);
    _vcaEnv.sustain(0.8f);
    _vcaEnv.release(400.0f);

    _lastUpdateMs = millis();
}

void MoogModelD::noteOn(uint8_t midi_note, float velocity) {
    if (velocity < 0.0f) velocity = 0.0f;
    if (velocity > 1.0f) velocity = 1.0f;

    const bool wasActive = (_noteStackLen > 0) && _noteActive;

    // Quitar la nota si ya estaba en la pila y empujarla al tope (última nota tiene prioridad)
    for (uint8_t i = 0; i < _noteStackLen; ++i) {
        if (_noteStack[i] == midi_note) {
            for (uint8_t j = i; j + 1 < _noteStackLen; ++j) _noteStack[j] = _noteStack[j + 1];
            --_noteStackLen;
            break;
        }
    }
    if (_noteStackLen < MONO_STACK_MAX) {
        _noteStack[_noteStackLen++] = midi_note;
    } else {
        for (uint8_t j = 0; j + 1 < MONO_STACK_MAX; ++j) _noteStack[j] = _noteStack[j + 1];
        _noteStack[MONO_STACK_MAX - 1] = midi_note;
    }

    _velGain       = 0.35f + 0.65f * velocity;
    _velBrightness = 0.50f + 0.50f * velocity;
    _applyMixerGains();

    _targetFreq = _midiToFreq(midi_note);
    if (_glideMs < 1.0f || !wasActive) {
        _currentFreq = _targetFreq;
    }
    _updateOscFreqs();

    // En modo legato, si ya había una tecla pulsada, no re-disparamos desde cero
    _filterEnv.noteOn(_legato && wasActive);
    if (!(_legato && wasActive)) {
        _vcaEnv.noteOn();
    }
    _noteActive = true;
}

void MoogModelD::noteOff(uint8_t midi_note) {
    bool removedTop = false;
    for (uint8_t i = 0; i < _noteStackLen; ++i) {
        if (_noteStack[i] == midi_note) {
            if (i == _noteStackLen - 1) removedTop = true;
            for (uint8_t j = i; j + 1 < _noteStackLen; ++j) _noteStack[j] = _noteStack[j + 1];
            --_noteStackLen;
            break;
        }
    }

    if (_noteStackLen > 0) {
        if (removedTop) {
            // Regresar a la nota anterior en la pila (trino / legato auténtico)
            _targetFreq = _midiToFreq(_noteStack[_noteStackLen - 1]);
            if (_glideMs < 1.0f) _currentFreq = _targetFreq;
            _updateOscFreqs();
        }
        return;
    }

    // Ya no quedan notas en la pila → disparar release
    _filterEnv.noteOff();
    _vcaEnv.noteOff();
    _noteActive = false;
}

void MoogModelD::noteOff() {
    _noteStackLen = 0;
    _filterEnv.noteOff();
    _vcaEnv.noteOff();
    _noteActive = false;
}

void MoogModelD::setWaveform(uint8_t osc_idx, uint16_t waveform) {
    switch (osc_idx) {
        case 0: _osc1.begin(waveform); break;
        case 1: _osc2.begin(waveform); break;
        case 2: _osc3.begin(waveform); break;
    }
}

void MoogModelD::setDetune(float cents) {
    _detuneCents = cents;
    _updateOscFreqs();
}

void MoogModelD::setVco3Interval(float semitones) {
    _vco3Semitones = semitones;
    _updateOscFreqs();
}

void MoogModelD::setVco3Detune(float cents) {
    _vco3Detune = cents;
    _updateOscFreqs();
}

void MoogModelD::setPulseWidth(float pw) {
    if (pw < 0.05f) pw = 0.05f;
    if (pw > 0.95f) pw = 0.95f;
    _pulseWidth = pw;
}

void MoogModelD::setOscLevels(float osc1, float osc2, float osc3, float noise) {
    _lvl1     = osc1;
    _lvl2     = osc2;
    _lvl3     = osc3;
    _lvlNoise = noise;
    _applyMixerGains();
}

void MoogModelD::_applyMixerGains() {
    // Compresión suave tipo consola CP3 cuando la suma supera 1.0 para evitar hard-clip int16 en AudioMixer4
    const float sum = _lvl1 + _lvl2 + _lvl3 + _lvlNoise;
    const float norm = (sum > 1.15f) ? (1.15f / sum) : 1.0f;
    const float g = norm * _velGain;
    _mixer.gain(0, _lvl1 * g);
    _mixer.gain(1, _lvl2 * g);
    _mixer.gain(2, _lvl3 * g);
    _mixer.gain(3, _lvlNoise * g * 0.6f);
}

void MoogModelD::setFilterCutoff(float hz) {
    _filterCutoff = hz;
}

void MoogModelD::setFilterResonance(float q) {
    _filter.resonance(q);
}

void MoogModelD::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

void MoogModelD::setDrive(float drive) {
    _filter.set_drive(drive);
}

void MoogModelD::setFilterAttack(float ms)     { _filterEnv.setAttack(ms); }
void MoogModelD::setFilterDecay(float ms)      { _filterEnv.setDecay(ms); }
void MoogModelD::setFilterSustain(float level) { _filterEnv.setSustain(level); }
void MoogModelD::setFilterRelease(float ms)    { _filterEnv.setRelease(ms); }

void MoogModelD::setVCAAttack(float ms)     { _vcaEnv.attack(ms); }
void MoogModelD::setVCADecay(float ms)      { _vcaEnv.decay(ms); }
void MoogModelD::setVCASustain(float level) { _vcaEnv.sustain(level); }
void MoogModelD::setVCARelease(float ms)    { _vcaEnv.release(ms); }

void MoogModelD::setGlide(float ms) {
    _glideMs = ms;
}

void MoogModelD::set_dsp_enabled(bool on) {
    _osc1.enable(on);
    _osc2.enable(on);
    _osc3.enable(on);
    _filter.set_enabled(on);
}

void MoogModelD::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    // 1. Glide exponencial estable (sin salto ni overshoot)
    if (_glideMs >= 1.0f && fabsf(_targetFreq - _currentFreq) > 0.01f) {
        const float k = 1.0f - expf(-float(dt) / _glideMs);
        _currentFreq += (_targetFreq - _currentFreq) * k;
        _updateOscFreqs();
    } else if (_currentFreq != _targetFreq) {
        _currentFreq = _targetFreq;
        _updateOscFreqs();
    }

    // 2. Filter envelope RC analógico (sin salto en release)
    const float envLevel = _filterEnv.update((float)dt);

    // 3. LFO → PITCH y PWM
    if (_lfo.pitch_ratio != _appliedPitchRatio) {
        _appliedPitchRatio = _lfo.pitch_ratio;
        _updateOscFreqs();
    }
    const float pw = _pulseWidth + 0.40f * _lfo.pwm;
    _osc1.pulseWidth(pw);
    _osc2.pulseWidth(pw);
    _osc3.pulseWidth(pw);

    // 4. Aplicar cutoff + modulación del envelope + sensibilidad a velocidad
    float modulated = (_filterCutoff + _filterEnvAmt * _velBrightness * envLevel * 8000.0f) * _lfo.cutoff_mul;
    if (modulated < 20.0f)    modulated = 20.0f;
    if (modulated > 20000.0f) modulated = 20000.0f;

    _filter.frequency(modulated);
}

void MoogModelD::_updateOscFreqs() {
    const float f = _currentFreq * _lfo.pitch_ratio;
    // Desafinación fija de cada VCO discreto (voice card del Model D, fidelidad §10): VCO 2 +1.8 ¢, VCO 3 −2.2 ¢
    _osc1.frequency(f);
    _osc2.frequency(f * _centsToRatio(_detuneCents + 1.8f));
    _osc3.frequency(f * _semitonesToRatio(_vco3Semitones) * _centsToRatio(_vco3Detune - 2.2f));
}

float MoogModelD::_midiToFreq(uint8_t midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

float MoogModelD::_centsToRatio(float cents) {
    return powf(2.0f, cents / 1200.0f);
}

float MoogModelD::_semitonesToRatio(float semitones) {
    return powf(2.0f, semitones / 12.0f);
}
