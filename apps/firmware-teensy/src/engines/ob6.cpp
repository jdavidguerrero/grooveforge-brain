// apps/firmware-teensy/src/engines/ob6.cpp
// Sequential / Oberheim OB-6 Engine: 6-voice polyphony with Oberheim SEM 12dB/oct SVF (LP↔Notch↔HP),
// PolyBLEP VCOs, VCO2 Fine Detune, X-MOD & AnalogEnv RC.

#include "ob6.h"
#include <math.h>

OB6::OB6() :
    // Voz 0
    _c0 (_vco1[0],      0, _voiceMix[0], 0),
    _c1 (_vco2[0],      0, _voiceMix[0], 1),
    _c2 (_sub[0],       0, _voiceMix[0], 2),
    _c3 (_noise[0],     0, _voiceMix[0], 3),
    _c4 (_voiceMix[0],  0, _filter[0],   0),
    _c5 (_filter[0],    0, _ampEnv[0],   0),
    // Voz 1
    _c6 (_vco1[1],      0, _voiceMix[1], 0),
    _c7 (_vco2[1],      0, _voiceMix[1], 1),
    _c8 (_sub[1],       0, _voiceMix[1], 2),
    _c9 (_noise[1],     0, _voiceMix[1], 3),
    _c10(_voiceMix[1],  0, _filter[1],   0),
    _c11(_filter[1],    0, _ampEnv[1],   0),
    // Voz 2
    _c12(_vco1[2],      0, _voiceMix[2], 0),
    _c13(_vco2[2],      0, _voiceMix[2], 1),
    _c14(_sub[2],       0, _voiceMix[2], 2),
    _c15(_noise[2],     0, _voiceMix[2], 3),
    _c16(_voiceMix[2],  0, _filter[2],   0),
    _c17(_filter[2],    0, _ampEnv[2],   0),
    // Voz 3
    _c18(_vco1[3],      0, _voiceMix[3], 0),
    _c19(_vco2[3],      0, _voiceMix[3], 1),
    _c20(_sub[3],       0, _voiceMix[3], 2),
    _c21(_noise[3],     0, _voiceMix[3], 3),
    _c22(_voiceMix[3],  0, _filter[3],   0),
    _c23(_filter[3],    0, _ampEnv[3],   0),
    // Voz 4
    _c24(_vco1[4],      0, _voiceMix[4], 0),
    _c25(_vco2[4],      0, _voiceMix[4], 1),
    _c26(_sub[4],       0, _voiceMix[4], 2),
    _c27(_noise[4],     0, _voiceMix[4], 3),
    _c28(_voiceMix[4],  0, _filter[4],   0),
    _c29(_filter[4],    0, _ampEnv[4],   0),
    // Voz 5
    _c30(_vco1[5],      0, _voiceMix[5], 0),
    _c31(_vco2[5],      0, _voiceMix[5], 1),
    _c32(_sub[5],       0, _voiceMix[5], 2),
    _c33(_noise[5],     0, _voiceMix[5], 3),
    _c34(_voiceMix[5],  0, _filter[5],   0),
    _c35(_filter[5],    0, _ampEnv[5],   0),
    // ampEnv → master mixers
    _c36(_ampEnv[0],    0, _masterMixA,  0),
    _c37(_ampEnv[1],    0, _masterMixA,  1),
    _c38(_ampEnv[2],    0, _masterMixA,  2),
    _c39(_ampEnv[3],    0, _masterMixA,  3),
    _c40(_ampEnv[4],    0, _masterMixB,  0),
    _c41(_ampEnv[5],    0, _masterMixB,  1),
    // master path
    _c42(_masterMixA,   0, _finalMix,    0),
    _c43(_masterMixB,   0, _finalMix,    1)
{}

void OB6::begin(float volume) {
    (void)volume;

    for (uint8_t i = 0; i < OB6_VOICES; i++) {

        // Voice card y curva del envelope del chip (fidelidad §10)

        _spread[i] = dsp::voice_spread(0xB6B6u + i * 7919u, 2.0f, 0.03f);

        _filterEnv[i].setAttackOvershoot(0.5f);

        _ampEnv[i].attackOvershoot(0.5f);
        // VCOs discretos estilo Tom Oberheim SEM: drift orgánico un poco más suelto que DCO
        _vco1[i].setSeed(0x0B601001u + i * 131u);
        _vco2[i].setSeed(0x0B602001u + i * 131u);
        _sub[i].setSeed(0x0B603001u + i * 131u);
        _vco1[i].setDrift(0.00085f);
        _vco2[i].setDrift(0.00095f);
        _sub[i].setDrift(0.00050f);

        _vco1[i].begin(WAVEFORM_SAWTOOTH);
        _vco1[i].amplitude(1.0f);
        _vco1[i].frequency(440.0f);

        _vco2[i].begin(WAVEFORM_SAWTOOTH);
        _vco2[i].amplitude(1.0f);
        _vco2[i].frequency(440.0f);

        _sub[i].begin(WAVEFORM_SQUARE);
        _sub[i].amplitude(1.0f);
        _sub[i].frequency(220.0f);

        // Importante: noise en 1.0f para que voiceMix ch3 pueda mezclarlo cuando el usuario suba NOISE
        _noise[i].amplitude(1.0f);

        _voiceMix[i].gain(0, _lvl1);
        _voiceMix[i].gain(1, _lvl2);
        _voiceMix[i].gain(2, _lvlSub);
        _voiceMix[i].gain(3, _lvlNoise * 0.5f);

        // Filtro Oberheim SEM 12 dB/oct State-Variable
        _filter[i].frequency(_filterCutoff);
        _filter[i].resonance(_filterResonance);
        _filter[i].set_morph(_filterMorph);
        _filter[i].set_bp_mode(_filterBp);
        _filter[i].set_drive(1.3f);
        _filter[i].octaveControl(1.0f);

        _filterEnv[i].setAttack(10.0f);
        _filterEnv[i].setDecay(300.0f);
        _filterEnv[i].setSustain(0.6f);
        _filterEnv[i].setRelease(500.0f);

        _ampEnv[i].attack(10.0f);
        _ampEnv[i].decay(300.0f);
        _ampEnv[i].sustain(0.6f);
        _ampEnv[i].release(500.0f);
    }

    for (uint8_t ch = 0; ch < 4; ch++) {
        _masterMixA.gain(ch, 0.25f);
        _masterMixB.gain(ch, ch < 2 ? 0.25f : 0.0f);
        _finalMix.gain(ch, ch < 2 ? 1.0f : 0.0f);
    }

    _lastUpdateMs = millis();
}

void OB6::noteOn(uint8_t midi_note, float velocity) {
    if (velocity < 0.0f) velocity = 0.0f;
    if (velocity > 1.0f) velocity = 1.0f;

    int8_t voice = _findVoiceForNote(midi_note);
    if (voice < 0) voice = _findFreeVoice();
    if (voice < 0) voice = _stealOldestVoice();

    _triggerVoice((uint8_t)voice, midi_note, velocity);
}

void OB6::noteOff(uint8_t midi_note) {
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice >= 0) _releaseVoice((uint8_t)voice);
}

void OB6::setVco1Waveform(uint16_t waveform) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _vco1[i].begin(waveform);
    }
}

void OB6::setVco2Waveform(uint16_t waveform) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _vco2[i].begin(waveform);
    }
}

void OB6::setVco2Interval(float semitones) {
    _vco2Semitones = semitones;
}

void OB6::setVco2Detune(float cents) {
    _vco2DetuneCents = cents;
}

void OB6::setPulseWidth(float pw1, float pw2) {
    if (pw1 < 0.05f) pw1 = 0.05f;
    if (pw1 > 0.95f) pw1 = 0.95f;
    if (pw2 < 0.05f) pw2 = 0.05f;
    if (pw2 > 0.95f) pw2 = 0.95f;
    _pw1 = pw1;
    _pw2 = pw2;
}

void OB6::setOscMix(float vco1, float vco2, float sub, float noise) {
    _lvl1     = vco1;
    _lvl2     = vco2;
    _lvlSub   = sub;
    _lvlNoise = noise;
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        const float vel = 0.35f + 0.65f * _voices[i].velocity;
        _voiceMix[i].gain(0, _lvl1 * vel);
        _voiceMix[i].gain(1, _lvl2 * vel);
        _voiceMix[i].gain(2, _lvlSub * vel);
        _voiceMix[i].gain(3, _lvlNoise * 0.5f * vel);
    }
}

void OB6::setFilterCutoff(float hz) {
    _filterCutoff = hz;
}

void OB6::setFilterResonance(float q) {
    _filterResonance = q;
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _filter[i].resonance(q);
    }
}

void OB6::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

void OB6::setFilterMorph(float morph) {
    if (morph < 0.0f) morph = 0.0f;
    if (morph > 1.0f) morph = 1.0f;
    _filterMorph = morph;
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _filter[i].set_morph(morph);
    }
}

void OB6::setFilterBpMode(bool bp) {
    _filterBp = bp;
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _filter[i].set_bp_mode(bp);
    }
}

void OB6::setFilterMode(uint8_t mode) {
    _filterMode = mode;
    switch (mode) {
        case 0: // LP
            setFilterBpMode(false);
            setFilterMorph(0.0f);
            break;
        case 1: // BP
            setFilterBpMode(true);
            break;
        case 2: // HP
            setFilterBpMode(false);
            setFilterMorph(1.0f);
            break;
        case 3: // Notch
        default:
            setFilterBpMode(false);
            setFilterMorph(0.5f);
            break;
    }
}

void OB6::setDrive(float drive) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _filter[i].set_drive(drive);
    }
}

void OB6::setAttack(float ms) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _filterEnv[i].setAttack(ms);
        _ampEnv[i].attack(ms);
    }
}

void OB6::setDecay(float ms) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _filterEnv[i].setDecay(ms);
        _ampEnv[i].decay(ms);
    }
}

void OB6::setSustain(float level) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _filterEnv[i].setSustain(level);
        _ampEnv[i].sustain(level);
    }
}

void OB6::setRelease(float ms) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _filterEnv[i].setRelease(ms);
        _ampEnv[i].release(ms);
    }
}

void OB6::set_dsp_enabled(bool on) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        _vco1[i].enable(on);
        _vco2[i].enable(on);
        _sub[i].enable(on);
        _filter[i].set_enabled(on);
    }
}

void OB6::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    const float glide_k = _glideMs >= 1.0f ? (1.0f - expf(-float(dt) / _glideMs)) : 1.0f;

    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        const float envLevel  = _filterEnv[i].update((float)dt);
        const float velBright = 0.50f + 0.50f * _voices[i].velocity;

        // X-MOD combinada: Filter Env (unipolar) + VCO2/LFO
        const float xmodSig = _xmEnv * envLevel + _xmVco2 * _lfo.lfo;

        // 1. Cutoff del filtro SEM 12 dB/oct
        float modulated = (_filterCutoff + _filterEnvAmt * velBright * envLevel * 7500.0f) * _lfo.cutoff_mul * _spread[i].cutoff;
        if (modulated < 20.0f)    modulated = 20.0f;
        if (modulated > 20000.0f) modulated = 20000.0f;
        _filter[i].frequency(modulated);

        // 2. X-MOD → Filter Mode Morph (barrido continuo LP ↔ Notch ↔ HP típico de Oberheim)
        if (_xmMorph) {
            float m = _filterMorph + 0.75f * xmodSig;
            if (m < 0.0f) m = 0.0f;
            if (m > 1.0f) m = 1.0f;
            _filter[i].set_morph(m);
        }

        // 3. Glide + Pitch + VCO2 Fine Detune + X-MOD → Freq VCO1
        _voices[i].currentHz += (_voices[i].targetHz - _voices[i].currentHz) * glide_k;
        const float fBase = _voices[i].currentHz * _lfo.pitch_ratio * _spread[i].pitch;
        const float f1 = _xmFreq1 ? (fBase * _semitonesToRatio(24.0f * xmodSig)) : fBase;
        const float f2 = fBase * _semitonesToRatio(_vco2Semitones + _vco2DetuneCents / 100.0f);

        _vco1[i].frequency(f1);
        _vco2[i].frequency(f2);
        _sub[i].frequency(f1 * 0.5f);

        // 4. PWM (LFO + X-MOD sobre VCO1)
        const float xmPw = _xmPw1 ? (0.40f * xmodSig) : 0.0f;
        _vco1[i].pulseWidth(_pw1 + 0.42f * _lfo.pwm + xmPw);
        _vco2[i].pulseWidth(_pw2 + 0.42f * _lfo.pwm);
    }
}

void OB6::_triggerVoice(uint8_t voice_idx, uint8_t midi_note, float velocity) {
    float freq = _midiToFreq(midi_note);
    _voices[voice_idx].targetHz = freq;
    if (_glideMs < 1.0f || _voices[voice_idx].midi_note == 255) {
        _voices[voice_idx].currentHz = freq;
    }
    _voices[voice_idx].velocity = velocity;

    const float vel = 0.35f + 0.65f * velocity;
    _voiceMix[voice_idx].gain(0, _lvl1 * vel);
    _voiceMix[voice_idx].gain(1, _lvl2 * vel);
    _voiceMix[voice_idx].gain(2, _lvlSub * vel);
    _voiceMix[voice_idx].gain(3, _lvlNoise * 0.5f * vel);

    const float f = _voices[voice_idx].currentHz;
    _vco1[voice_idx].frequency(f);
    _vco2[voice_idx].frequency(f * _semitonesToRatio(_vco2Semitones + _vco2DetuneCents / 100.0f));
    _sub[voice_idx].frequency(f * 0.5f);

    _ampEnv[voice_idx].noteOn();
    _filterEnv[voice_idx].noteOn(false);

    _voices[voice_idx].midi_note = midi_note;
    _voices[voice_idx].active    = true;
    _voices[voice_idx].age       = ++_ageCounter;
}

void OB6::_releaseVoice(uint8_t voice_idx) {
    _ampEnv[voice_idx].noteOff();
    _filterEnv[voice_idx].noteOff();
    _voices[voice_idx].midi_note = 255;
    _voices[voice_idx].active    = false;
}

int8_t OB6::_findFreeVoice() {
    // Rotativa, como los polys analógicos: dos notas seguidas caen en voces (y voice cards) distintas
    for (uint8_t k = 0; k < OB6_VOICES; k++) {
        const uint8_t i = uint8_t((_rr + k) % OB6_VOICES);
        if (_voices[i].midi_note == 255) {
            _rr = uint8_t((i + 1) % OB6_VOICES);
            return (int8_t)i;
        }
    }
    return -1;
}

int8_t OB6::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < OB6_VOICES; i++) {
        if (_voices[i].midi_note == midi_note) return (int8_t)i;
    }
    return -1;
}

int8_t OB6::_stealOldestVoice() {
    uint8_t  oldest_idx = 0;
    uint32_t oldest_age = _voices[0].age;
    for (uint8_t i = 1; i < OB6_VOICES; i++) {
        if (_voices[i].age < oldest_age) {
            oldest_age = _voices[i].age;
            oldest_idx = i;
        }
    }
    return (int8_t)oldest_idx;
}

float OB6::_midiToFreq(uint8_t midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}

float OB6::_semitonesToRatio(float semitones) {
    return powf(2.0f, semitones / 12.0f);
}
