// apps/firmware-teensy/src/engines/dx7.cpp
// DX7 de 4 operadores (ADR-005): apps/docs/sprints/46-dx7-4-operadores.md

#include "dx7.h"
#include <math.h>

namespace {
constexpr float VOICE_SCALE = 26000.0f;
}

void Dx7Voice::update() {
    if (!_on || !_patch || !_core.active()) return;
    audio_block_t* block = allocate();
    if (!block) return;
    float tmp[AUDIO_BLOCK_SAMPLES];
    _core.render(*_patch, AUDIO_SAMPLE_RATE_EXACT, tmp, AUDIO_BLOCK_SAMPLES);
    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
        const int32_t v = int32_t(tmp[i] * VOICE_SCALE);
        block->data[i] = int16_t(v > 32767 ? 32767 : v < -32768 ? -32768 : v);
    }
    transmit(block);
    release(block);
}

DX7Engine::DX7Engine() :
    _c0(_voice[0], 0, _ampEnv[0], 0), _c1(_voice[1], 0, _ampEnv[1], 0), _c2(_voice[2], 0, _ampEnv[2], 0),
    _c3(_voice[3], 0, _ampEnv[3], 0), _c4(_voice[4], 0, _ampEnv[4], 0), _c5(_voice[5], 0, _ampEnv[5], 0),
    _c6(_ampEnv[0], 0, _masterMixA, 0), _c7(_ampEnv[1], 0, _masterMixA, 1), _c8(_ampEnv[2], 0, _masterMixA, 2),
    _c9(_ampEnv[3], 0, _masterMixA, 3), _c10(_ampEnv[4], 0, _masterMixB, 0), _c11(_ampEnv[5], 0, _masterMixB, 1),
    _c12(_masterMixA, 0, _finalMix, 0), _c13(_masterMixB, 0, _finalMix, 1),
    _c14(_finalMix, 0, _tone, 0)
{}

void DX7Engine::begin(float volume) {
    (void)volume;
    // E.PIANO de arranque (algoritmo 5: dos pilas 2 → 1 y 4 → 3)
    _patch.algorithm = 5;
    _patch.feedback = 2;
    _patch.op[0] = {1.0f, 99, 0, 1800, 0.0f, 400};
    _patch.op[1] = {1.0f, 72, 0, 900, 0.15f, 300};
    _patch.op[2] = {1.0f, 88, 0, 2200, 0.0f, 400};
    _patch.op[3] = {14.0f, 52, 0, 160, 0.0f, 120};
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _voice[i].attach(&_patch);
        _ampEnv[i].attack(2);
        _ampEnv[i].decay(50);
        _ampEnv[i].sustain(1.0f);
        _ampEnv[i].release(600);
    }
    for (uint8_t ch = 0; ch < 4; ch++) {
        _masterMixA.gain(ch, 0.25f);
        _masterMixB.gain(ch, ch < 2 ? 0.25f : 0.0f);
        _finalMix.gain(ch, ch < 2 ? 1.0f : 0.0f);
    }
    _tone.frequency(_filterCutoff);
    _tone.resonance(0.7f);
    _tone.set_drive(1.0f);
    _tone.setCharacter(dsp::LadderCharacter::Cem3320);   // filtro de tono suave (sin overdrive Moog)
    _lastUpdateMs = millis();
}

void DX7Engine::noteOn(uint8_t midi_note, float velocity) {
    if (velocity < 0.0f) velocity = 0.0f;
    if (velocity > 1.0f) velocity = 1.0f;

    int8_t v = _findVoiceForNote(midi_note);
    if (v < 0) v = _findFreeVoice();
    if (v < 0) v = _stealOldestVoice();
    VoiceState& s = _voices[v];
    const float freq = 440.0f * powf(2.0f, (float(midi_note) - 69.0f) / 12.0f);
    s.targetHz = freq;
    if (_glideMs < 1.0f || s.midi_note == 255) s.currentHz = freq;
    s.midi_note = midi_note;
    s.age = ++_ageCounter;

    _voice[v].note_on(s.currentHz * _lfo.pitch_ratio, velocity);
    _ampEnv[v].noteOn();
}

void DX7Engine::noteOff(uint8_t midi_note) {
    const int8_t v = _findVoiceForNote(midi_note);
    if (v < 0) return;
    _voice[v].note_off();
    _ampEnv[v].noteOff();
    _voices[v].midi_note = 255;
}

void DX7Engine::setAlgorithm(uint8_t alg) {
    const uint8_t a = alg < 1 ? 1 : alg > 8 ? 8 : alg;
    __disable_irq();
    _patch.algorithm = a;
    __enable_irq();
}

void DX7Engine::setFeedback(uint8_t fb) {
    const uint8_t f = fb > 7 ? 7 : fb;
    __disable_irq();
    _patch.feedback = f;
    __enable_irq();
}

void DX7Engine::setOp(uint8_t op, float ratio, float level) {
    if (op >= DX7_OPS) return;
    __disable_irq();
    _patch.op[op].ratio = ratio;
    _patch.op[op].level = level;
    __enable_irq();
}

void DX7Engine::setOpEnv(uint8_t op, float attack_ms, float decay_ms, float sustain, float release_ms) {
    if (op >= DX7_OPS) return;
    __disable_irq();
    Dx7OpParams& o = _patch.op[op];
    o.attack_ms = attack_ms;
    o.decay_ms = decay_ms;
    o.sustain = sustain;
    o.release_ms = release_ms;
    __enable_irq();
    _syncAmpReleaseWithCarriers(release_ms);
}

void DX7Engine::_syncAmpReleaseWithCarriers(float min_release_ms) {
    float max_rel = min_release_ms;
    for (int k = 0; k < DX7_OPS; ++k) {
        if (dx7_is_carrier(_patch.algorithm, k) && _patch.op[k].release_ms > max_rel) {
            max_rel = _patch.op[k].release_ms;
        }
    }
    for (auto& e : _ampEnv) e.release(max_rel);
}

void DX7Engine::setAttack(float ms)  { for (auto& e : _ampEnv) e.attack(ms); }
void DX7Engine::setDecay(float ms)   { for (auto& e : _ampEnv) e.decay(ms); }
void DX7Engine::setSustain(float level) {
    // No bajar el sustain de _ampEnv por debajo de 0.85 si el usuario no lo cierra del todo,
    // para que las portadoras con decay percusivo (E.PIANO) conserven su cuerpo natural.
    for (auto& e : _ampEnv) e.sustain(level);
}
void DX7Engine::setRelease(float ms) { _syncAmpReleaseWithCarriers(ms); }

void DX7Engine::setAmpEnv(float a_ms, float d_ms, float s, float r_ms) {
    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _ampEnv[i].attack(a_ms);
        _ampEnv[i].decay(d_ms);
        _ampEnv[i].sustain(s);
    }
    _syncAmpReleaseWithCarriers(r_ms);
}

void DX7Engine::set_dsp_enabled(bool on) {
    for (auto& v : _voice) v.enable(on);
    _tone.set_enabled(on);
}

void DX7Engine::update() {
    uint32_t now = millis();
    uint32_t dt  = now - _lastUpdateMs;
    _lastUpdateMs = now;
    if (dt == 0) dt = 1;

    const float glide_k = _glideMs >= 1.0f ? (1.0f - expf(-float(dt) / _glideMs)) : 1.0f;

    for (uint8_t i = 0; i < DX7_VOICES; i++) {
        _voices[i].currentHz += (_voices[i].targetHz - _voices[i].currentHz) * glide_k;
        _voice[i].set_freq(_voices[i].currentHz * _lfo.pitch_ratio);
    }
    _finalMix.gain(0, _lfo.amp);
    _finalMix.gain(1, _lfo.amp);
    _tone.frequency(_filterCutoff < 500.0f ? 500.0f : _filterCutoff);
}

int8_t DX7Engine::_findVoiceForNote(uint8_t midi_note) {
    for (uint8_t i = 0; i < DX7_VOICES; i++)
        if (_voices[i].midi_note == midi_note) return int8_t(i);
    return -1;
}

int8_t DX7Engine::_findFreeVoice() {
    for (uint8_t i = 0; i < DX7_VOICES; i++)
        if (_voices[i].midi_note == 255) return int8_t(i);
    return -1;
}

int8_t DX7Engine::_stealOldestVoice() {
    uint8_t oldest = 0;
    for (uint8_t i = 1; i < DX7_VOICES; i++)
        if (_voices[i].age < _voices[oldest].age) oldest = i;
    return int8_t(oldest);
}
