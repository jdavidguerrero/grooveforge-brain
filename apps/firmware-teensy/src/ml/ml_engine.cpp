#ifndef NATIVE_TEST

#include "ml_engine.h"
#include <Arduino.h>
#include <string.h>
#include <math.h>

MLEngine::MLEngine()
    : _last_decay_ms(0)
    , _last_inference_ms(0)
    , _prev_key_index(0xFF)
    , _prev_chord_index(0xFF)
    , _prev_bpm_int(0)
    , _initialized(false)
{
    memset(_pitch_count,    0, sizeof(_pitch_count));
    memset(_pitch_hist,     0, sizeof(_pitch_hist));
    memset(_pitch_activity, 0, sizeof(_pitch_activity));
    memset(&_key_result,   0, sizeof(_key_result));
    memset(&_chord_result, 0, sizeof(_chord_result));
    memset(&_beat_result,  0, sizeof(_beat_result));
}

bool MLEngine::init() {
    bool key_ok   = _key.init();
    bool chord_ok = _chord.init();
    bool beat_ok  = _beat.init();

    Serial.printf("[MLEngine] key=%u chord=%u beat=%u bytes\n",
                  (unsigned)_key.arena_used_bytes(),
                  (unsigned)_chord.arena_used_bytes(),
                  (unsigned)_beat.arena_used_bytes());

    _initialized = key_ok && chord_ok && beat_ok;
    return _initialized;
}

void MLEngine::on_onset(uint32_t now_ms) {
    _beat.on_onset(now_ms);
}

void MLEngine::update_pitch(uint8_t midi_note, uint8_t velocity) {
    uint8_t pc = midi_note % 12;
    _pitch_count[pc]++;
    // Acumular actividad EMA saturada a 1024 para que el decay sea estable.
    // El valor 1024 como techo impide que ráfagas de notas iguales rompan la escala.
    float new_act = _pitch_activity[pc] + (float)velocity;
    _pitch_activity[pc] = (new_act > 1024.0f) ? 1024.0f : new_act;
}

uint8_t MLEngine::get_pitch_activity_u8(uint8_t pc) const {
    if (pc >= 12) return 0;
    // Mapear [0, 1024] → [0, 255] con clamp
    float val = _pitch_activity[pc] * (255.0f / 1024.0f);
    if (val < 0.0f)   return 0;
    if (val > 255.0f) return 255;
    return (uint8_t)val;
}

bool MLEngine::_compute_histogram() {
    uint64_t total = 0;
    for (uint8_t i = 0; i < 12; i++) total += _pitch_count[i];
    if (total == 0) return false;
    float ftotal = (float)total;
    for (uint8_t i = 0; i < 12; i++) {
        _pitch_hist[i] = (float)_pitch_count[i] / ftotal;
    }
    return true;
}

uint8_t MLEngine::tick(uint32_t now_ms) {
    if (!_initialized) return 0;

    // Decay EMA del pitch_activity — se aplica en cada tick() independientemente
    // del intervalo de inferencia. tau = 2s según spec 33-ai-hero-viz.md §Theory.
    if (_last_decay_ms == 0) {
        _last_decay_ms = now_ms;
    } else {
        uint32_t dt_ms = now_ms - _last_decay_ms;
        _last_decay_ms = now_ms;
        // decay = exp(-dt / tau_ms). Calculado en float; el branch de dt==0 es
        // imposible porque tick() no corre dos veces en el mismo milisegundo.
        float decay = expf(-(float)dt_ms / 2000.0f);
        for (uint8_t i = 0; i < 12; i++) {
            _pitch_activity[i] *= decay;
        }
    }

    if ((now_ms - _last_inference_ms) < INFERENCE_INTERVAL_MS) return 0;
    _last_inference_ms = now_ms;

    uint8_t changed = 0;

    if (_compute_histogram()) {
        if (_key.inference(_pitch_hist, _key_result)) {
            if (_key_result.key_index != _prev_key_index) {
                changed |= KEY_CHANGED;
                _prev_key_index = _key_result.key_index;
            }
        }
        if (_chord.inference(_pitch_hist, _chord_result)) {
            if (_chord_result.chord_index != _prev_chord_index) {
                changed |= CHORD_CHANGED;
                _prev_chord_index = _chord_result.chord_index;
            }
        }
    }

    BeatResult br = _beat.infer();
    if (br.valid) {
        _beat_result = br;
        uint16_t bpm_int = (uint16_t)(br.bpm + 0.5f);
        if (bpm_int != _prev_bpm_int) {
            changed |= BEAT_CHANGED;
            _prev_bpm_int = bpm_int;
        }
    }

    return changed;
}

#endif // NATIVE_TEST
