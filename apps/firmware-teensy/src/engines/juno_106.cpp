// apps/firmware-teensy/src/engines/juno_106.cpp
// Roland Juno-106 — 6-voice PolyBLEP DCO (Saw+Pulse simultaneous), AnalogEnv RC & MN3009 BBD Chorus + HPF.

#include "juno_106.h"
#include <math.h>

Juno106::Juno106() :
    // Voz 0
    _c0 (_dco[0],      0, _voiceMix[0], 0),
    _c1 (_sub[0],      0, _voiceMix[0], 1),
    _c2 (_noise[0],    0, _voiceMix[0], 2),
    _c3 (_voiceMix[0], 0, _filter[0],   0),
    _c4 (_filter[0],   0, _ampEnv[0],   0),
    // Voz 1
    _c5 (_dco[1],      0, _voiceMix[1], 0),
    _c6 (_sub[1],      0, _voiceMix[1], 1),
    _c7 (_noise[1],    0, _voiceMix[1], 2),
    _c8 (_voiceMix[1], 0, _filter[1],   0),
    _c9 (_filter[1],   0, _ampEnv[1],   0),
    // Voz 2
    _c10(_dco[2],      0, _voiceMix[2], 0),
    _c11(_sub[2],      0, _voiceMix[2], 1),
    _c12(_noise[2],    0, _voiceMix[2], 2),
    _c13(_voiceMix[2], 0, _filter[2],   0),
    _c14(_filter[2],   0, _ampEnv[2],   0),
    // Voz 3
    _c15(_dco[3],      0, _voiceMix[3], 0),
    _c16(_sub[3],      0, _voiceMix[3], 1),
    _c17(_noise[3],    0, _voiceMix[3], 2),
    _c18(_voiceMix[3], 0, _filter[3],   0),
    _c19(_filter[3],   0, _ampEnv[3],   0),
    // Voz 4
    _c20(_dco[4],      0, _voiceMix[4], 0),
    _c21(_sub[4],      0, _voiceMix[4], 1),
    _c22(_noise[4],    0, _voiceMix[4], 2),
    _c23(_voiceMix[4], 0, _filter[4],   0),
    _c24(_filter[4],   0, _ampEnv[4],   0),
    // Voz 5
    _c25(_dco[5],      0, _voiceMix[5], 0),
    _c26(_sub[5],      0, _voiceMix[5], 1),
    _c27(_noise[5],    0, _voiceMix[5], 2),
    _c28(_voiceMix[5], 0, _filter[5],   0),
    _c29(_filter[5],   0, _ampEnv[5],   0),
    // ampEnv → masterMixA (voces 0-3)
    _c30(_ampEnv[0],   0, _masterMixA,  0),
    _c31(_ampEnv[1],   0, _masterMixA,  1),
    _c32(_ampEnv[2],   0, _masterMixA,  2),
    _c33(_ampEnv[3],   0, _masterMixA,  3),
    // ampEnv → masterMixB (voces 4-5)
    _c34(_ampEnv[4],   0, _masterMixB,  0),
    _c35(_ampEnv[5],   0, _masterMixB,  1),
    // master bus → chorus BBD MN3009 + HPF
    _c36(_masterMixA,  0, _masterBus,   0),
    _c37(_masterMixB,  0, _masterBus,   1),
    _c38(_masterBus,   0, _chorus,      0)
{}

void Juno106::begin(float volume) {
    (void)volume;

    for (uint8_t i = 0; i < JUNO_VOICES; i++) {

        // Voice card y curva del envelope del chip (fidelidad §10)

        _spread[i] = dsp::voice_spread(0x10601u + i * 7919u, 0.3f, 0.04f);

        _filterEnv[i].setAttackOvershoot(3.0f);

        _ampEnv[i].attackOvershoot(3.0f);
        // El Juno-106 usa DCOs controlados por reloj digital (drift casi nulo ~0.00015)
        _dco[i].setSeed(0x41060001u + i * 97u);
        _sub[i].setSeed(0x41061001u + i * 97u);
        _dco[i].setDrift(0.00015f);
        _sub[i].setDrift(0.00015f);

        // Por defecto Saw+Pulse simultáneos para la riqueza clásica del Juno-106
        _dco[i].begin(AudioSynthPolyBlep::WAVEFORM_SAW_PULSE);
        _dco[i].amplitude(1.0f);
        _dco[i].frequency(440.0f);
        _dco[i].pulseWidth(_pulseWidth);

        _sub[i].begin(WAVEFORM_SQUARE);
        _sub[i].amplitude(1.0f);
        _sub[i].frequency(220.0f);

        _noise[i].amplitude(1.0f);

        _voiceMix[i].gain(0, 0.60f);
        _voiceMix[i].gain(1, _subLevel);
        _voiceMix[i].gain(2, _noiseLevel * 0.5f);
        _voiceMix[i].gain(3, 0.0f);

        _filter[i].frequency(_filterCutoff);
        _filter[i].resonance(0.7f);
        _filter[i].set_drive(1.1f);
        _filter[i].setCharacter(dsp::LadderCharacter::Ir3109);   // IR3109: 4 OTA (fidelidad §9.1)
        _filter[i].octaveControl(1.0f);

        _filterEnv[i].setAttack(10.0f);
        _filterEnv[i].setDecay(200.0f);
        _filterEnv[i].setSustain(0.5f);
        _filterEnv[i].setRelease(300.0f);

        _ampEnv[i].attack(10.0f);
        _ampEnv[i].decay(200.0f);
        _ampEnv[i].sustain(0.5f);
        _ampEnv[i].release(300.0f);
    }

    for (uint8_t ch = 0; ch < 4; ch++) {
        _masterMixA.gain(ch, 0.25f);
        _masterMixB.gain(ch, ch < 2 ? 0.25f : 0.0f);
        _masterBus.gain(ch, ch < 2 ? 1.0f : 0.0f);
    }

    _chorus.set_mode(_chorusMode);
    _chorus.set_hpf(_hpfPos);

    _lastUpdateMs = millis();
}

void Juno106::noteOn(uint8_t midi_note, float velocity) {
    if (velocity < 0.0f) velocity = 0.0f;
    if (velocity > 1.0f) velocity = 1.0f;

    int8_t voice = _findVoiceForNote(midi_note);
    if (voice < 0) voice = _findFreeVoice();
    if (voice < 0) voice = _stealOldestVoice();

    _triggerVoice((uint8_t)voice, midi_note, velocity);
}

void Juno106::noteOff(uint8_t midi_note) {
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice >= 0) _releaseVoice((uint8_t)voice);
}

void Juno106::setWaveform(uint16_t waveform) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _dco[i].begin(waveform);
    }
}

void Juno106::setPulseWidth(float pw) {
    if (pw < 0.05f) pw = 0.05f;
    if (pw > 0.95f) pw = 0.95f;
    _pulseWidth = pw;
}

void Juno106::setSubLevel(float level) {
    _subLevel = level;
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        const float vel = 0.35f + 0.65f * _voices[i].velocity;
        _voiceMix[i].gain(1, _subLevel * vel);
    }
}

void Juno106::setNoiseLevel(float level) {
    _noiseLevel = level;
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        const float vel = 0.35f + 0.65f * _voices[i].velocity;
        _voiceMix[i].gain(2, _noiseLevel * 0.5f * vel);
    }
}

void Juno106::setFilterCutoff(float hz) {
    _filterCutoff = hz;
}

void Juno106::setFilterResonance(float q) {
    _filterResonance = q;
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _filter[i].resonance(q);
    }
}

void Juno106::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

void Juno106::setDrive(float drive) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _filter[i].set_drive(drive);
    }
}

void Juno106::setHpf(uint8_t pos) {
    if (pos > 3) pos = 3;
    _hpfPos = pos;
    _chorus.set_hpf(pos);
}

void Juno106::setChorusMode(uint8_t mode) {
    if (mode > 3) mode = 3;
    _chorusMode = mode;
    _chorus.set_mode(mode);
}

void Juno106::setChorus(bool enabled, uint8_t mode) {
    if (!enabled) {
        setChorusMode(0);
    } else {
        setChorusMode(mode < 1 ? 1 : (mode > 3 ? 3 : mode));
    }
}

void Juno106::setAttack(float ms) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _filterEnv[i].setAttack(ms);
        _ampEnv[i].attack(ms);
    }
}

void Juno106::setDecay(float ms) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _filterEnv[i].setDecay(ms);
        _ampEnv[i].decay(ms);
    }
}

void Juno106::setSustain(float level) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _filterEnv[i].setSustain(level);
        _ampEnv[i].sustain(level);
    }
}

void Juno106::setRelease(float ms) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _filterEnv[i].setRelease(ms);
        _ampEnv[i].release(ms);
    }
}

void Juno106::set_dsp_enabled(bool on) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        _dco[i].enable(on);
        _sub[i].enable(on);
        _filter[i].set_enabled(on);
    }
    _chorus.set_enabled(on);
}

void Juno106::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    const float glide_k = _glideMs >= 1.0f ? (1.0f - expf(-float(dt) / _glideMs)) : 1.0f;

    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        const float envLevel = _filterEnv[i].update((float)dt);
        const float velBright = 0.50f + 0.50f * _voices[i].velocity;

        float modulated = (_filterCutoff + _filterEnvAmt * velBright * envLevel * 8000.0f) * _lfo.cutoff_mul * _spread[i].cutoff;
        if (modulated < 20.0f)    modulated = 20.0f;
        if (modulated > 20000.0f) modulated = 20000.0f;
        _filter[i].frequency(modulated);

        _voices[i].currentHz += (_voices[i].targetHz - _voices[i].currentHz) * glide_k;
        const float f = _voices[i].currentHz * _lfo.pitch_ratio * _spread[i].pitch;
        _dco[i].frequency(f);
        _sub[i].frequency(f * 0.5f);

        _dco[i].pulseWidth(_pulseWidth + 0.42f * _lfo.pwm);
    }
}

void Juno106::_triggerVoice(uint8_t voice_idx, uint8_t midi_note, float velocity) {
    const float freq = _midiToFreq(midi_note);
    _voices[voice_idx].targetHz = freq;
    if (_glideMs < 1.0f || _voices[voice_idx].midi_note == 255) {
        _voices[voice_idx].currentHz = freq;
    }
    _voices[voice_idx].velocity = velocity;

    const float velGain = 0.35f + 0.65f * velocity;
    _voiceMix[voice_idx].gain(0, 0.60f * velGain);
    _voiceMix[voice_idx].gain(1, _subLevel * velGain);
    _voiceMix[voice_idx].gain(2, _noiseLevel * 0.5f * velGain);

    _dco[voice_idx].frequency(_voices[voice_idx].currentHz);
    _sub[voice_idx].frequency(_voices[voice_idx].currentHz * 0.5f);

    _ampEnv[voice_idx].noteOn();
    _filterEnv[voice_idx].noteOn(false);

    _voices[voice_idx].midi_note = midi_note;
    _voices[voice_idx].active    = true;
    _voices[voice_idx].age       = ++_ageCounter;
}

void Juno106::_releaseVoice(uint8_t voice_idx) {
    _ampEnv[voice_idx].noteOff();
    _filterEnv[voice_idx].noteOff();
    _voices[voice_idx].midi_note = 255;
    _voices[voice_idx].active    = false;
}

int8_t Juno106::_findFreeVoice() {
    // Rotativa, como los polys analógicos: dos notas seguidas caen en voces (y voice cards) distintas
    for (uint8_t k = 0; k < JUNO_VOICES; k++) {
        const uint8_t i = uint8_t((_rr + k) % JUNO_VOICES);
        if (_voices[i].midi_note == 255) {
            _rr = uint8_t((i + 1) % JUNO_VOICES);
            return (int8_t)i;
        }
    }
    return -1;
}

int8_t Juno106::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < JUNO_VOICES; i++) {
        if (_voices[i].midi_note == midi_note) return (int8_t)i;
    }
    return -1;
}

int8_t Juno106::_stealOldestVoice() {
    uint8_t  oldest_idx = 0;
    uint32_t oldest_age = _voices[0].age;
    for (uint8_t i = 1; i < JUNO_VOICES; i++) {
        if (_voices[i].age < oldest_age) {
            oldest_age = _voices[i].age;
            oldest_idx = i;
        }
    }
    return (int8_t)oldest_idx;
}

float Juno106::_midiToFreq(uint8_t midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}
