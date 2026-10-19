#pragma once

#include "../dsp/sem_svf_core.h"
#include <Arduino.h>
#include <Audio.h>

/**
 * @file sem_svf.h
 * @brief Filtro Oberheim SEM 12 dB/oct (State-Variable ZDF) como AudioStream para el motor OB-6.
 */

class OberheimSemSvf2P : public AudioStream {
public:
    OberheimSemSvf2P() : AudioStream(1, _inputQueueArray) {
        recompute();
    }

    void frequency(float hz) {
        if (fabsf(hz - _hz) < 0.25f) return;
        _hz = hz;
        recompute();
    }

    void resonance(float q) {
        if (fabsf(q - _q) < 0.005f) return;
        _q = q;
        recompute();
    }

    /// Morph continuo del OB-6: 0.0 = LP, 0.5 = Notch, 1.0 = HP.
    void set_morph(float m) {
        if (fabsf(m - _morph) < 0.005f) return;
        _morph = m;
        recompute();
    }

    void set_bp_mode(bool bp) {
        if (bp == _bp) return;
        _bp = bp;
        recompute();
    }

    void set_drive(float drive) {
        if (fabsf(drive - _drive) < 0.01f) return;
        _drive = drive;
        recompute();
    }

    void octaveControl(float n) { (void)n; }

    void set_enabled(bool on) {
        if (on == _enabled) return;
        if (on) {
            __disable_irq();
            _state = dsp::SemSvfState{};
            _enabled = true;
            __enable_irq();
        } else {
            _enabled = false;
        }
    }

    void update() override {
        audio_block_t* in_block = receiveReadOnly(0);
        if (!in_block) return;
        if (!_enabled) {
            release(in_block);
            return;
        }
        audio_block_t* out_block = allocate();
        if (!out_block) {
            release(in_block);
            return;
        }

        dsp::SemSvfCoeffs c;
        __disable_irq();
        c = _coeffs;
        __enable_irq();

        const int16_t* src = in_block->data;
        int16_t* dst = out_block->data;
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            const float in_f = float(src[i]) * (1.0f / 32768.0f);
            float out_f = dsp::sem_svf_step(_state, in_f, c);
            if (!isfinite(out_f)) {
                _state = dsp::SemSvfState{};
                out_f = 0.0f;
            }
            if (out_f > 1.0f) out_f = 1.0f;
            if (out_f < -1.0f) out_f = -1.0f;
            dst[i] = int16_t(out_f * 32767.0f);
        }

        release(in_block);
        transmit(out_block, 0);
        release(out_block);
    }

private:
    void recompute() {
        const dsp::SemSvfCoeffs c =
            dsp::sem_svf_compute(_hz, _q, _morph, _bp, _drive, AUDIO_SAMPLE_RATE_EXACT);
        __disable_irq();
        _coeffs = c;
        __enable_irq();
    }

    audio_block_t* _inputQueueArray[1];
    dsp::SemSvfState _state;
    dsp::SemSvfCoeffs _coeffs;
    float _hz = 2000.0f;
    float _q = 1.0f;
    float _morph = 0.0f;
    float _drive = 1.0f;
    bool _bp = false;
    volatile bool _enabled = true;
};
