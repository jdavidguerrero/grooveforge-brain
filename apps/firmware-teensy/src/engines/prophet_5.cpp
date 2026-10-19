// apps/firmware-teensy/src/engines/prophet_5.cpp
// Prophet-5 Engine: 5-voice polyphony with Poly-Mod (including OSC B -> Filter), AnalogEnv RC & Drive.

#include "prophet_5.h"
#include <math.h>

namespace {
constexpr float SAMPLE_RATE = AUDIO_SAMPLE_RATE_EXACT;
constexpr float C4_HZ = 261.63f;
constexpr float OSC_SCALE = 20000.0f;
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
inline float st_ratio(float st) { return prophet_fast_exp2(st / 12.0f); }
}  // namespace

void Prophet5Osc::update() {
    if (!_on) return;
    audio_block_t* block = allocate();
    if (!block) return;
    float tmp[AUDIO_BLOCK_SAMPLES];
    _core.render(_p, tmp, AUDIO_BLOCK_SAMPLES);
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
        const int32_t v = int32_t(tmp[i] * OSC_SCALE);
        block->data[i] = int16_t(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    transmit(block);
    release(block);
}

Prophet5::Prophet5() :
    _c0(_osc[0], 0, _vcf[0], 0), _c1(_vcf[0], 0, _vcaEnv[0], 0),
    _c2(_osc[1], 0, _vcf[1], 0), _c3(_vcf[1], 0, _vcaEnv[1], 0),
    _c4(_osc[2], 0, _vcf[2], 0), _c5(_vcf[2], 0, _vcaEnv[2], 0),
    _c6(_osc[3], 0, _vcf[3], 0), _c7(_vcf[3], 0, _vcaEnv[3], 0),
    _c8(_osc[4], 0, _vcf[4], 0), _c9(_vcf[4], 0, _vcaEnv[4], 0),
    _c10(_vcaEnv[0], 0, _masterMixA, 0),
    _c11(_vcaEnv[1], 0, _masterMixA, 1),
    _c12(_vcaEnv[2], 0, _masterMixA, 2),
    _c13(_vcaEnv[3], 0, _masterMixA, 3),
    _c14(_vcaEnv[4], 0, _masterMixB, 0),
    _c15(_masterMixA, 0, _finalMix, 0),
    _c16(_masterMixB, 0, _finalMix, 1)
{}

void Prophet5::begin(float volume) {
    (void)volume;
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _vcf[i].frequency(_filterCutoff);
        _vcf[i].setCharacter(dsp::LadderCharacter::Cem3320);   // CEM3320 / SSI2140 (fidelidad §9.1)
        _vcf[i].resonance(_filterResonance);
        _vcf[i].set_drive(1.2f);
        _vcf[i].octaveControl(1.0f);

        _filterEnv[i].setAttack(10.0f);
        _filterEnv[i].setDecay(300.0f);
        _filterEnv[i].setSustain(0.7f);
        _filterEnv[i].setRelease(500.0f);

        _vcaEnv[i].attack(10.0f);
        _vcaEnv[i].decay(300.0f);
        _vcaEnv[i].sustain(0.7f);
        _vcaEnv[i].release(500.0f);

        _targetHz[i] = _currentHz[i] = 440.0f;
    }
    for (uint8_t ch = 0; ch < 4; ch++) {
        _masterMixA.gain(ch, 0.25f);
        _masterMixB.gain(ch, ch == 0 ? 0.25f : 0.0f);
        _finalMix.gain(ch, ch < 2 ? 1.0f : 0.0f);
    }
    _lastUpdateMs = millis();
}

void Prophet5::noteOn(uint8_t midi_note, float velocity) {
    if (velocity < 0.0f) velocity = 0.0f;
    if (velocity > 1.0f) velocity = 1.0f;

    int8_t voice = _findVoiceForNote(midi_note);
    if (voice < 0) voice = _findFreeVoice();
    if (voice < 0) voice = _stealOldestVoice();
    _triggerVoice((uint8_t)voice, midi_note, velocity);
}

void Prophet5::noteOff(uint8_t midi_note) {
    int8_t voice = _findVoiceForNote(midi_note);
    if (voice >= 0) _releaseVoice((uint8_t)voice);
}

void Prophet5::setOscAWaveform(uint16_t waveform) {
    const bool pulse = waveform == WAVEFORM_PULSE || waveform == WAVEFORM_SQUARE;
    setOscAShape(!pulse, pulse);
}

void Prophet5::setOscBWaveform(uint16_t waveform) {
    const bool pulse = waveform == WAVEFORM_PULSE || waveform == WAVEFORM_SQUARE;
    const bool tri = waveform == WAVEFORM_TRIANGLE;
    setOscBShape(!pulse && !tri, tri, pulse);
}

void Prophet5::setFilterResonance(float q) {
    _filterResonance = q;
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) _vcf[i].resonance(q);
}

void Prophet5::setDrive(float drive) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) _vcf[i].set_drive(drive);
}

void Prophet5::setAttack(float ms) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _filterEnv[i].setAttack(ms);
        _vcaEnv[i].attack(ms);
    }
}
void Prophet5::setDecay(float ms) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _filterEnv[i].setDecay(ms);
        _vcaEnv[i].decay(ms);
    }
}
void Prophet5::setSustain(float level) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _filterEnv[i].setSustain(level);
        _vcaEnv[i].sustain(level);
    }
}
void Prophet5::setRelease(float ms) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _filterEnv[i].setRelease(ms);
        _vcaEnv[i].release(ms);
    }
}

void Prophet5::set_dsp_enabled(bool on) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        _vcf[i].set_enabled(on);
        _osc[i].enable(on);
    }
}

void Prophet5::update() {
    const uint32_t now = millis();
    const uint32_t dt = now - _lastUpdateMs;
    _lastUpdateMs = now;
    if (dt == 0) return;

    // WHEEL-MOD: fuente = LFO global ↔ ruido de control
    _rng ^= _rng << 13;
    _rng ^= _rng >> 17;
    _rng ^= _rng << 5;
    _wmNoise = float(int32_t(_rng)) * (1.0f / 2147483648.0f);
    const float wm = ((1.0f - _wmMix) * _lfo.lfo + _wmMix * _wmNoise) * _wheel;

    // Glide exponencial: el 63 % del camino en _glideMs
    const float glide_k = _glideMs >= 1.0f ? 1.0f - expf(-float(dt) / _glideMs) : 1.0f;

    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        const float env = _filterEnv[i].update((float)dt);
        const float velGain = 0.35f + 0.65f * _voices[i].velocity;
        const float velBright = 0.50f + 0.50f * _voices[i].velocity;

        _currentHz[i] += (_targetHz[i] - _currentHz[i]) * glide_k;
        const float f = _currentHz[i] * _lfo.pitch_ratio;

        // Osciladores
        Prophet5OscParams p;
        p.inc_a = f * st_ratio(_oscASemitones + (_wmFreqA ? 12.0f * wm : 0.0f)) / SAMPLE_RATE;
        float fb = (_bKeyboard ? f : C4_HZ) * st_ratio(_oscBSemitones + _oscBFine / 100.0f + (_wmFreqB ? 12.0f * wm : 0.0f));
        if (_bLoFreq) fb *= 0.01f;
        p.inc_b = fb / SAMPLE_RATE;
        p.a_saw = _aSaw; p.a_pulse = _aPulse; p.sync = _sync;
        p.b_saw = _bSaw; p.b_tri = _bTri; p.b_pulse = _bPulse;
        p.pw_a = clampf(_pwA + 0.45f * _lfo.pwm + (_wmPwA ? 0.45f * wm : 0.0f), 0.05f, 0.95f);
        p.pw_b = clampf(_pwB + 0.45f * _lfo.pwm + (_wmPwB ? 0.45f * wm : 0.0f), 0.05f, 0.95f);
        p.pm_b = _pmB; p.pm_env = _pmEnv; p.env = env;
        p.pm_freq_a = _pmFreqA; p.pm_pw_a = _pmPwA;
        p.lvl_a = _lvlA * velGain;
        p.lvl_b = _lvlB * velGain;
        p.lvl_n = _lvlN * velGain;
        _osc[i].set(p);

        // Señal instantánea del OSC B para POLY-MOD → FILTER (especialmente expresiva cuando B está en LO FREQ)
        const float pb = _osc[i].phase_b();
        const float oscBMod = _bTri ? (1.0f - 4.0f * fabsf(pb - 0.5f)) : (2.0f * pb - 1.0f);

        // Filtro: base + envelope, × LFO, × POLY-MOD (envelope + OSC B), × WHEEL-MOD, × keyboard tracking
        float cutoff = (_filterCutoff + _filterEnvAmt * velBright * env * 6000.0f) * _lfo.cutoff_mul;
        if (_pmFilter) {
            cutoff *= prophet_fast_exp2(PROPHET_PM_OCT * (_pmEnv * env + 0.5f * _pmB * oscBMod));
        }
        if (_wmFilter) cutoff *= prophet_fast_exp2(2.0f * wm);
        if (_filterKbd > 0.0f) cutoff *= powf(_currentHz[i] / C4_HZ, _filterKbd);
        _vcf[i].frequency(clampf(cutoff, 20.0f, 20000.0f));
    }
}

void Prophet5::_triggerVoice(uint8_t voice_idx, uint8_t midi_note, float velocity) {
    _targetHz[voice_idx] = _midiToFreq(midi_note);
    if (_glideMs < 1.0f || _voices[voice_idx].midi_note == 255) _currentHz[voice_idx] = _targetHz[voice_idx];
    _voices[voice_idx].velocity = velocity;
    _vcaEnv[voice_idx].noteOn();
    _filterEnv[voice_idx].noteOn(false);
    _voices[voice_idx].midi_note = midi_note;
    _voices[voice_idx].active    = true;
    _voices[voice_idx].age       = ++_ageCounter;
}

void Prophet5::_releaseVoice(uint8_t voice_idx) {
    _vcaEnv[voice_idx].noteOff();
    _filterEnv[voice_idx].noteOff();
    _voices[voice_idx].midi_note = 255;
    _voices[voice_idx].active    = false;
}

int8_t Prophet5::_findFreeVoice() {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        if (_voices[i].midi_note == 255) return (int8_t)i;
    }
    return -1;
}

int8_t Prophet5::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < PROPHET_VOICES; i++) {
        if (_voices[i].midi_note == midi_note) return (int8_t)i;
    }
    return -1;
}

int8_t Prophet5::_stealOldestVoice() {
    uint8_t oldest_idx = 0;
    uint32_t oldest_age = _voices[0].age;
    for (uint8_t i = 1; i < PROPHET_VOICES; i++) {
        if (_voices[i].age < oldest_age) {
            oldest_age = _voices[i].age;
            oldest_idx = i;
        }
    }
    return (int8_t)oldest_idx;
}

float Prophet5::_midiToFreq(uint8_t midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69) / 12.0f);
}
