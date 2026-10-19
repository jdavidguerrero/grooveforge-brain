// apps/firmware-teensy/src/engines/arp2600.cpp
// ARP 2600 Engine: 4-voice polyphony with PolyBLEP VCO1 + VCO2 direct + Ring Modulator + AnalogEnv RC.

#include "arp2600.h"
#include <math.h>

ARP2600::ARP2600() :
    // Voz 0
    _c0 (_vco1[0],     0, _ringMod[0],    0),
    _c1 (_vco2[0],     0, _ringMod[0],    1),
    _c2 (_ringMod[0],  0, _voiceMix[0],   0),   // ch0 = Ring Mod (VCO1 × VCO2)
    _c3 (_vco1[0],     0, _voiceMix[0],   1),   // ch1 = VCO1 directo
    _c4 (_noise[0],    0, _voiceMix[0],   2),   // ch2 = Noise
    _c5 (_voiceMix[0], 0, _filter[0],     0),
    _c6 (_filter[0],   0, _ampEnv[0],     0),
    _c0b(_vco2[0],     0, _voiceMix[0],   3),   // ch3 = VCO2 directo
    // Voz 1
    _c7 (_vco1[1],     0, _ringMod[1],    0),
    _c8 (_vco2[1],     0, _ringMod[1],    1),
    _c9 (_ringMod[1],  0, _voiceMix[1],   0),
    _c10(_vco1[1],     0, _voiceMix[1],   1),
    _c11(_noise[1],    0, _voiceMix[1],   2),
    _c12(_voiceMix[1], 0, _filter[1],     0),
    _c13(_filter[1],   0, _ampEnv[1],     0),
    _c1b(_vco2[1],     0, _voiceMix[1],   3),
    // Voz 2
    _c14(_vco1[2],     0, _ringMod[2],    0),
    _c15(_vco2[2],     0, _ringMod[2],    1),
    _c16(_ringMod[2],  0, _voiceMix[2],   0),
    _c17(_vco1[2],     0, _voiceMix[2],   1),
    _c18(_noise[2],    0, _voiceMix[2],   2),
    _c19(_voiceMix[2], 0, _filter[2],     0),
    _c20(_filter[2],   0, _ampEnv[2],     0),
    _c2b(_vco2[2],     0, _voiceMix[2],   3),
    // Voz 3
    _c21(_vco1[3],     0, _ringMod[3],    0),
    _c22(_vco2[3],     0, _ringMod[3],    1),
    _c23(_ringMod[3],  0, _voiceMix[3],   0),
    _c24(_vco1[3],     0, _voiceMix[3],   1),
    _c25(_noise[3],    0, _voiceMix[3],   2),
    _c26(_voiceMix[3], 0, _filter[3],     0),
    _c27(_filter[3],   0, _ampEnv[3],     0),
    _c3b(_vco2[3],     0, _voiceMix[3],   3),
    // ampEnv → masterMixA
    _c28(_ampEnv[0],   0, _masterMixA,    0),
    _c29(_ampEnv[1],   0, _masterMixA,    1),
    _c30(_ampEnv[2],   0, _masterMixA,    2),
    _c31(_ampEnv[3],   0, _masterMixA,    3),
    // master path
    _c32(_masterMixA,  0, _finalMix,      0)
{}

void ARP2600::begin(float volume) {
    (void)volume;

    for (uint8_t i = 0; i < ARP_VOICES; i++) {

        // Voice card y curva del envelope del chip (fidelidad §10)

        _spread[i] = dsp::voice_spread(0x26002600u + i * 7919u, 2.0f, 0.03f);

        _filterEnv[i].setAttackOvershoot(0.6f);

        _ampEnv[i].attackOvershoot(0.6f);
        _vco1[i].setSeed(0x26001001u + i * 173u);
        _vco2[i].setSeed(0x26002001u + i * 173u);
        _vco1[i].setDrift(0.0008f);
        _vco2[i].setDrift(0.0009f);

        _vco1[i].begin(WAVEFORM_SAWTOOTH);
        _vco1[i].amplitude(1.0f);
        _vco1[i].frequency(440.0f);

        _vco2[i].begin(WAVEFORM_SINE);
        _vco2[i].amplitude(1.0f);
        _vco2[i].frequency(440.0f);

        _noise[i].amplitude(1.0f);
        _applyVoiceMix(i);

        _filter[i].frequency(_filterCutoff);
        _filter[i].resonance(_filterResonance);
        _filter[i].set_drive(1.4f);   // carácter cálido del filtro ARP 4012
        _filter[i].setCharacter(dsp::LadderCharacter::Arp4072);
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
        _finalMix.gain(ch, ch == 0 ? 1.0f : 0.0f);
    }

    _lastUpdateMs = millis();
}

void ARP2600::noteOn(uint8_t midi_note, float velocity) {
    if (velocity < 0.0f) velocity = 0.0f;
    if (velocity > 1.0f) velocity = 1.0f;

    if (_mono) {   // una voz, prioridad a la última nota; legato (sin redisparo) solo con glide
        const bool legato = _stack.push(midi_note);
        if (legato && _glideMs >= 1.0f && _voices[0].active) {
            _voices[0].targetHz = _midiToFreq(midi_note);
            _voices[0].midi_note = midi_note;
        } else {
            _triggerVoice(0, midi_note, velocity);
        }
        return;
    }

    int8_t voice = _findVoiceForNote(midi_note);
    if (voice < 0) voice = _findFreeVoice();
    if (voice < 0) voice = _stealOldestVoice();

    _triggerVoice((uint8_t)voice, midi_note, velocity);
}

void ARP2600::noteOff(uint8_t midi_note) {
    if (_mono) {
        const uint8_t top = _stack.remove(midi_note);
        if (top == dsp::NoteStack::NONE) {
            if (_voices[0].active) _releaseVoice(0);
        } else if (top != _voices[0].midi_note) {     // vuelve a la nota anterior que sigue apretada
            _voices[0].targetHz = _midiToFreq(top);
            if (_glideMs < 1.0f) _voices[0].currentHz = _voices[0].targetHz;
            _voices[0].midi_note = top;
        }
        return;
    }
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice >= 0) _releaseVoice((uint8_t)voice);
}

void ARP2600::setVco1Waveform(uint16_t waveform) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _vco1[i].begin(waveform);
    }
}

void ARP2600::setVco2Waveform(uint16_t waveform) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _vco2[i].begin(waveform);
    }
}

void ARP2600::setVco2Ratio(float ratio) {
    _vco2Ratio = ratio;
}

void ARP2600::setPulseWidth(float pw) {
    if (pw < 0.05f) pw = 0.05f;
    if (pw > 0.95f) pw = 0.95f;
    _pulseWidth = pw;
}

void ARP2600::setRingMix(float mix) {
    if (mix < 0.0f) mix = 0.0f;
    if (mix > 1.0f) mix = 1.0f;
    _ringMixAmt = mix;
    _vco1Level  = 1.0f - 0.5f * mix;
    for (uint8_t i = 0; i < ARP_VOICES; i++) _applyVoiceMix(i);
}

void ARP2600::setVco1Level(float level) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    _vco1Level = level;
    for (uint8_t i = 0; i < ARP_VOICES; i++) _applyVoiceMix(i);
}

void ARP2600::setVco2Level(float level) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    _vco2Level = level;
    for (uint8_t i = 0; i < ARP_VOICES; i++) _applyVoiceMix(i);
}

void ARP2600::setNoiseLevel(float level) {
    if (level < 0.0f) level = 0.0f;
    if (level > 1.0f) level = 1.0f;
    _noiseLevel = level;
    for (uint8_t i = 0; i < ARP_VOICES; i++) _applyVoiceMix(i);
}

void ARP2600::setMono(bool on) {
    if (on == _mono) return;
    _mono = on;
    _stack.clear();
    for (uint8_t i = 0; i < ARP_VOICES; i++)
        if (_voices[i].active) _releaseVoice(i);
}

void ARP2600::setSync(bool on) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) _vco2[i].syncTo(on ? &_vco1[i] : nullptr);
}

void ARP2600::setVco3(float rate_hz, float to_pitch, float to_cutoff) {
    _v3Rate = rate_hz < 0.05f ? 0.05f : rate_hz;
    _v3Pitch = to_pitch < 0.0f ? 0.0f : to_pitch > 1.0f ? 1.0f : to_pitch;
    _v3Cutoff = to_cutoff < 0.0f ? 0.0f : to_cutoff > 1.0f ? 1.0f : to_cutoff;
}

void ARP2600::setNoiseColor(float color) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) _noise[i].color(color);
}

void ARP2600::_applyVoiceMix(uint8_t v) {
    const float vel = 0.35f + 0.65f * _voices[v].velocity;
    const float sum = _ringMixAmt + _vco1Level + _vco2Level + _noiseLevel * 0.5f;
    const float norm = (sum > 1.15f) ? (1.15f / sum) : 1.0f;
    const float g = vel * norm;
    _voiceMix[v].gain(0, _ringMixAmt * g);
    _voiceMix[v].gain(1, _vco1Level  * g);
    _voiceMix[v].gain(2, _noiseLevel * 0.5f * g);
    _voiceMix[v].gain(3, _vco2Level  * g);
}

void ARP2600::setFilterCutoff(float hz) {
    _filterCutoff = hz;
}

void ARP2600::setFilterResonance(float q) {
    _filterResonance = q;
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _filter[i].resonance(q);
    }
}

void ARP2600::setFilterEnvAmount(float amount) {
    _filterEnvAmt = amount;
}

void ARP2600::setDrive(float drive) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _filter[i].set_drive(drive);
    }
}

void ARP2600::setAttack(float ms) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _filterEnv[i].setAttack(ms);
        _ampEnv[i].attack(ms);
    }
}

void ARP2600::setDecay(float ms) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _filterEnv[i].setDecay(ms);
        _ampEnv[i].decay(ms);
    }
}

void ARP2600::setSustain(float level) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _filterEnv[i].setSustain(level);
        _ampEnv[i].sustain(level);
    }
}

void ARP2600::setRelease(float ms) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _filterEnv[i].setRelease(ms);
        _ampEnv[i].release(ms);
    }
}

void ARP2600::set_dsp_enabled(bool on) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        _vco1[i].enable(on);
        _vco2[i].enable(on);
        _noise[i].enable(on);
        _filter[i].set_enabled(on);
    }
}

void ARP2600::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;

    if (dt == 0) return;

    const float glide_k = _glideMs >= 1.0f ? (1.0f - expf(-float(dt) / _glideMs)) : 1.0f;

    // VCO 3 en rango bajo como modulador (triángulo), a tasa de control
    _v3Phase += _v3Rate * float(dt) * 0.001f;
    _v3Phase -= floorf(_v3Phase);
    const float v3 = 1.0f - 4.0f * fabsf(_v3Phase - 0.5f);
    const float v3_pitch = _v3Pitch > 0.0f ? exp2f(_v3Pitch * v3) : 1.0f;          // ±12 st a fondo
    const float v3_cut = _v3Cutoff > 0.0f ? exp2f(4.0f * _v3Cutoff * v3) : 1.0f;   // ±4 oct a fondo

    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        const float envLevel  = _filterEnv[i].update((float)dt);
        const float velBright = 0.50f + 0.50f * _voices[i].velocity;

        float modulated = (_filterCutoff + _filterEnvAmt * velBright * envLevel * 6000.0f) * _lfo.cutoff_mul *
                          _spread[i].cutoff * v3_cut;
        if (modulated < 20.0f)    modulated = 20.0f;
        if (modulated > 20000.0f) modulated = 20000.0f;
        _filter[i].frequency(modulated);

        _voices[i].currentHz += (_voices[i].targetHz - _voices[i].currentHz) * glide_k;
        const float f = _voices[i].currentHz * _lfo.pitch_ratio * _spread[i].pitch * v3_pitch;
        _vco1[i].frequency(f);
        _vco2[i].frequency(f * _vco2Ratio);

        const float pw = _pulseWidth + 0.42f * _lfo.pwm;
        _vco1[i].pulseWidth(pw);
        _vco2[i].pulseWidth(pw);
    }
}

void ARP2600::_triggerVoice(uint8_t v, uint8_t midi_note, float velocity) {
    float freq = _midiToFreq(midi_note);
    _voices[v].targetHz = freq;
    if (_glideMs < 1.0f || _voices[v].midi_note == 255) {
        _voices[v].currentHz = freq;
    }
    _voices[v].velocity = velocity;
    _applyVoiceMix(v);

    _vco1[v].frequency(_voices[v].currentHz);
    _vco2[v].frequency(_voices[v].currentHz * _vco2Ratio);

    _ampEnv[v].noteOn();
    _filterEnv[v].noteOn(false);

    _voices[v].midi_note = midi_note;
    _voices[v].active    = true;
    _voices[v].age       = ++_ageCounter;
}

void ARP2600::_releaseVoice(uint8_t v) {
    _ampEnv[v].noteOff();
    _filterEnv[v].noteOff();

    _voices[v].midi_note = 255;
    _voices[v].active    = false;
}

int8_t ARP2600::_findFreeVoice() {
    // Rotativa, como los polys analógicos: dos notas seguidas caen en voces (y voice cards) distintas
    for (uint8_t k = 0; k < ARP_VOICES; k++) {
        const uint8_t i = uint8_t((_rr + k) % ARP_VOICES);
        if (_voices[i].midi_note == 255) {
            _rr = uint8_t((i + 1) % ARP_VOICES);
            return (int8_t)i;
        }
    }
    return -1;
}

int8_t ARP2600::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < ARP_VOICES; i++) {
        if (_voices[i].midi_note == midi_note) return (int8_t)i;
    }
    return -1;
}

int8_t ARP2600::_stealOldestVoice() {
    uint8_t  oldest_idx = 0;
    uint32_t oldest_age = _voices[0].age;
    for (uint8_t i = 1; i < ARP_VOICES; i++) {
        if (_voices[i].age < oldest_age) {
            oldest_age = _voices[i].age;
            oldest_idx = i;
        }
    }
    return (int8_t)oldest_idx;
}

float ARP2600::_midiToFreq(uint8_t midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}
