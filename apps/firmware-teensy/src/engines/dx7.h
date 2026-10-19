#pragma once

// Teensy Audio Library (oficial — PaulStoffregen/Audio, incluida con Teensyduino)
#include <Audio.h>
// Código custom — filtro digital Moog ladder D'Angelo & Välimäki (ICASSP 2013), aquí como filtro de tono
#include "../audio/moog_ladder.h"
#include "dx7_core.h"
#include "lfo_mod.h"

static constexpr uint8_t DX7_VOICES = 6;

/**
 * @brief Una voz FM de 4 operadores como objeto de audio (código custom, subclase de AudioStream).
 */
class Dx7Voice : public AudioStream {
public:
    Dx7Voice() : AudioStream(0, nullptr) {}

    void attach(const Dx7Patch* patch) { _patch = patch; }
    void enable(bool on) { _on = on; }
    void note_on(float hz, float velocity = 1.0f) {
        __disable_irq();
        _core.note_on(hz, velocity);
        __enable_irq();
    }
    void note_off() {
        __disable_irq();
        _core.note_off();
        __enable_irq();
    }
    void set_freq(float hz) {
        __disable_irq();
        _core.set_freq(hz);
        __enable_irq();
    }

    void update() override;

private:
    Dx7VoiceCore _core;
    const Dx7Patch* _patch = nullptr;
    volatile bool _on = true;
};

/**
 * @brief Engine DX7: 6 voces × 4 operadores FM (ADR-005), con sincronización atómica ISR,
 *        sensibilidad a la velocidad en moduladores/portadoras, Glide, Drive y filtro de tono.
 */
class DX7Engine {
public:
    DX7Engine();

    void begin(float volume = 0.5f);

    void noteOn(uint8_t midi_note, float velocity = 1.0f);
    void noteOff(uint8_t midi_note);

    // ── FM de 4 operadores (con protección atómica contra ISR) ─────────────────
    void setAlgorithm(uint8_t alg);
    void setFeedback(uint8_t fb);
    void setOp(uint8_t op, float ratio, float level);
    void setOpEnv(uint8_t op, float attack_ms, float decay_ms, float sustain, float release_ms);

    // ── Compatibilidad con el sketch 28 (engine de 2 operadores) ───────────────
    void setFmRatio(float ratio) {
        __disable_irq();
        _patch.op[1].ratio = ratio;
        __enable_irq();
    }
    void setFmDepth(float depth) {
        __disable_irq();
        _patch.op[1].level = depth * 99.0f;
        __enable_irq();
    }
    void setCarrierWaveform(uint16_t w) { (void)w; }
    void setModWaveform(uint16_t w) { (void)w; }

    // ── Filtro de tono, Glide, Drive y envelopes ───────────────────────────────
    void setFilterCutoff(float hz) { _filterCutoff = hz; }
    void setFilterResonance(float q) { _tone.resonance(q); }
    void setFilterEnvAmount(float amount) { (void)amount; }
    void setGlide(float ms) { _glideMs = ms; }
    void setDrive(float drive) { _tone.set_drive(drive); }

    void setAttack(float ms);
    void setDecay(float ms);
    void setSustain(float level);
    void setRelease(float ms);

    void setLfoMod(const LfoMod& m) { _lfo = m; }

    void setFilterEnv(float a_ms, float d_ms, float s, float r_ms) {
        (void)a_ms; (void)d_ms; (void)s; (void)r_ms;
    }

    /**
     * @brief En el DX7, la envolvente de cada voz la generan las portadoras FM de Dx7VoiceCore.
     *        Aseguramos que _ampEnv no trunque prematuramente las colas de release de las portadoras.
     */
    void setAmpEnv(float a_ms, float d_ms, float s, float r_ms);

    void update();
    void set_dsp_enabled(bool on);

    AudioStream& getOutput() { return _tone; }

private:
    LfoMod _lfo;
    Dx7Patch _patch;

    Dx7Voice            _voice[DX7_VOICES];
    AudioEffectEnvelope _ampEnv[DX7_VOICES];
    AudioMixer4         _masterMixA, _masterMixB, _finalMix;
    MoogLadder4P        _tone;

    AudioConnection _c0, _c1, _c2, _c3, _c4, _c5;
    AudioConnection _c6, _c7, _c8, _c9, _c10, _c11;
    AudioConnection _c12, _c13, _c14;

    struct VoiceState {
        uint8_t  midi_note = 255;
        uint32_t age       = 0;
        float    targetHz  = 440.0f;
        float    currentHz = 440.0f;
    };
    VoiceState _voices[DX7_VOICES];
    uint32_t   _ageCounter   = 0;
    uint32_t   _lastUpdateMs = 0;
    float      _filterCutoff = 20000.0f;
    float      _glideMs      = 0.0f;

    int8_t _findVoiceForNote(uint8_t midi_note);
    int8_t _findFreeVoice();
    int8_t _stealOldestVoice();
    void   _syncAmpReleaseWithCarriers(float min_release_ms);
};
