#pragma once

#include <math.h>
#include <stdint.h>

/**
 * @file analog_env.h
 * @brief Generador de envolvente ADSR con curvatura RC analógica y RELEASE continuo sin saltos.
 *
 * Resuelve el bug histórico de los 5 motores sustractivos donde soltar la tecla durante
 * ATTACK o DECAY saltaba abruptamente a `sustain * (1 - t)`, y modela la carga/descarga
 * exponencial de un circuito RC real (CEM3310 / Minimoog contour).
 */

namespace dsp {

class AnalogEnv {
public:
    enum class Stage : uint8_t { IDLE = 0, ATTACK, DECAY, SUSTAIN, RELEASE };

    void setAttack(float ms)  { _attackMs  = ms < 0.0f ? 0.0f : ms; }
    void setDecay(float ms)   { _decayMs   = ms < 1.0f ? 1.0f : ms; }
    void setSustain(float s)  { _sustain   = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    void setRelease(float ms) { _releaseMs = ms < 1.0f ? 1.0f : ms; }

    void setADSR(float a_ms, float d_ms, float s, float r_ms) {
        setAttack(a_ms);
        setDecay(d_ms);
        setSustain(s);
        setRelease(r_ms);
    }

    float attackMs() const  { return _attackMs; }
    float decayMs() const   { return _decayMs; }
    float sustain() const   { return _sustain; }
    float releaseMs() const { return _releaseMs; }

    /// Dispara la envolvente. Si `legato == true` y ya estaba activa, continúa desde el nivel actual.
    void noteOn(bool legato = false) {
        if (!legato || _stage == Stage::IDLE) {
            _stage = Stage::ATTACK;
            _stageMs = 0;
            _attackStart = legato ? _level : 0.0f;
            if (!legato) _level = 0.0f;
        }
    }

    /// Captura el nivel exacto actual como inicio de RELEASE (cero discontinuidad).
    void noteOff() {
        if (_stage != Stage::IDLE) {
            _releaseStart = _level;
            _stage = Stage::RELEASE;
            _stageMs = 0;
        }
    }

    void reset() {
        _stage = Stage::IDLE;
        _stageMs = 0;
        _level = 0.0f;
        _releaseStart = 0.0f;
    }

    float update(uint32_t dt_ms) {
        _stageMs += dt_ms;
        switch (_stage) {
            case Stage::IDLE:
                _level = 0.0f;
                break;

            case Stage::ATTACK:
                if (_attackMs < 1.0f) {
                    _level = 1.0f;
                    _stage = Stage::DECAY;
                    _stageMs = 0;
                } else {
                    const float t = float(_stageMs) / _attackMs;
                    if (t >= 1.0f) {
                        _level = 1.0f;
                        _stage = Stage::DECAY;
                        _stageMs = 0;
                    } else {
                        // Curva RC convexa de carga hacia 1.0 (más rápida al inicio, natural al oído)
                        const float shaped = 1.0f - expf(-3.2f * t);
                        const float norm = 1.0f / (1.0f - expf(-3.2f));
                        _level = _attackStart + (1.0f - _attackStart) * (shaped * norm);
                    }
                }
                break;

            case Stage::DECAY:
                if (_decayMs < 1.0f) {
                    _level = _sustain;
                    _stage = Stage::SUSTAIN;
                } else {
                    const float t = float(_stageMs) / _decayMs;
                    if (t >= 1.0f) {
                        _level = _sustain;
                        _stage = Stage::SUSTAIN;
                    } else {
                        // Caída exponencial RC con "snap" analógico que llega exactamente a _sustain en t=1
                        const float k = expf(-4.2f * t) * (1.0f - t);
                        _level = _sustain + (1.0f - _sustain) * k;
                    }
                }
                break;

            case Stage::SUSTAIN:
                _level = _sustain;
                break;

            case Stage::RELEASE:
                if (_releaseMs < 1.0f || _releaseStart <= 1e-4f) {
                    _level = 0.0f;
                    _stage = Stage::IDLE;
                } else {
                    const float t = float(_stageMs) / _releaseMs;
                    if (t >= 1.0f) {
                        _level = 0.0f;
                        _stage = Stage::IDLE;
                    } else {
                        // Descarga exponencial continua desde _releaseStart hasta 0.0 en t=1
                        const float k = expf(-3.5f * t) * (1.0f - t);
                        _level = _releaseStart * k;
                    }
                }
                break;
        }
        return _level;
    }

    float level() const { return _level; }
    Stage stage() const { return _stage; }
    bool active() const { return _stage != Stage::IDLE; }

private:
    float _attackMs = 10.0f;
    float _decayMs = 300.0f;
    float _sustain = 0.7f;
    float _releaseMs = 400.0f;
    float _level = 0.0f;
    float _attackStart = 0.0f;
    float _releaseStart = 0.0f;
    Stage _stage = Stage::IDLE;
    uint32_t _stageMs = 0;
};

}  // namespace dsp
