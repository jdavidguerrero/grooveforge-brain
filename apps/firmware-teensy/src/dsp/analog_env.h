#pragma once

#include <math.h>
#include <stdint.h>

/**
 * @file analog_env.h
 * @brief ADSR con carga y descarga RC (un polo por etapa), evaluable por muestra o a tasa de control.
 *
 * Modela el contorno de un CEM3310 / Minimoog: el ataque carga hacia un objetivo por encima del pico (1 + overshoot)
 * y se corta en 1.0; decay y release descargan exponencialmente (τ = t / 4.6, llegan al 1 % en el tiempo pedido).
 * El re-disparo arranca desde el nivel actual (sin click) y el release desde cualquier etapa es continuo.
 * Teoría: theory/fidelidad-engines.md §9.2 · medición de la versión anterior: §8.
 *
 * Dos usos: update(dt_ms) a tasa de control (envelope del filtro, base de 1 kHz) y step() por muestra
 * (AnalogVca, con set_rate(44100)).
 */

namespace dsp {

class AnalogEnv {
public:
    enum class Stage : uint8_t { IDLE = 0, ATTACK, DECAY, SUSTAIN, RELEASE };

    void setAttack(float ms)  { _attackMs  = ms < 0.0f ? 0.0f : ms; _dirty = true; }
    void setDecay(float ms)   { _decayMs   = ms < 1.0f ? 1.0f : ms; _dirty = true; }
    void setSustain(float s)  { _sustain   = s < 0.0f ? 0.0f : (s > 1.0f ? 1.0f : s); }
    void setRelease(float ms) { _releaseMs = ms < 1.0f ? 1.0f : ms; _dirty = true; }

    void setADSR(float a_ms, float d_ms, float s, float r_ms) {
        setAttack(a_ms);
        setDecay(d_ms);
        setSustain(s);
        setRelease(r_ms);
    }

    /// Sobrepaso del objetivo del ataque (0.5 = carga hacia 1.5 × el pico, como el CEM3310). Menor = más curvo.
    void setAttackOvershoot(float os) { _overshoot = os < 0.05f ? 0.05f : os; _dirty = true; }

    /// Pasos por segundo de step(): 1000 para update(dt_ms), la frecuencia de muestreo para uso por muestra.
    void set_rate(float steps_per_second) { _rate = steps_per_second; _dirty = true; }

    float attackMs() const  { return _attackMs; }
    float decayMs() const   { return _decayMs; }
    float sustain() const   { return _sustain; }
    float releaseMs() const { return _releaseMs; }

    /// Dispara la envolvente desde el nivel actual. Con `legato` y la envolvente activa, no re-dispara.
    void noteOn(bool legato = false) {
        if (legato && _stage != Stage::IDLE && _stage != Stage::RELEASE) return;
        _stage = Stage::ATTACK;
    }

    /// Release desde el nivel actual, en cualquier etapa.
    void noteOff() {
        if (_stage != Stage::IDLE) _stage = Stage::RELEASE;
    }

    void reset() {
        _stage = Stage::IDLE;
        _level = 0.0f;
    }

    /// Un paso de 1 / rate segundos.
    float step() {
        if (_dirty) recompute();
        switch (_stage) {
            case Stage::IDLE:
                _level = 0.0f;
                break;
            case Stage::ATTACK:
                _level = _attackTarget + (_level - _attackTarget) * _ca;
                if (_level >= 1.0f) {
                    _level = 1.0f;
                    _stage = Stage::DECAY;
                }
                break;
            case Stage::DECAY:
                _level = _sustain + (_level - _sustain) * _cd;
                if (fabsf(_level - _sustain) < 1e-4f) {
                    _level = _sustain;
                    _stage = Stage::SUSTAIN;
                }
                break;
            case Stage::SUSTAIN:
                _level = _sustain;   // sigue al knob de sustain en vivo
                break;
            case Stage::RELEASE:
                _level *= _cr;
                if (_level < 1e-4f) {   // −80 dB
                    _level = 0.0f;
                    _stage = Stage::IDLE;
                }
                break;
        }
        return _level;
    }

    /// Avanza dt_ms milisegundos a tasa de control (rate = 1000, el valor por defecto).
    float update(uint32_t dt_ms) {
        for (uint32_t i = 0; i < dt_ms; ++i) step();
        return _level;
    }

    float level() const { return _level; }
    Stage stage() const { return _stage; }
    bool active() const { return _stage != Stage::IDLE; }

private:
    void recompute() {
        const float a_steps = _attackMs * 0.001f * _rate;
        _attackTarget = 1.0f + _overshoot;
        // Carga de 0 a 1 en a_steps pasos hacia (1 + os): c^n = os / (1 + os)
        _ca = a_steps < 1.0f ? 0.0f : expf(logf(_overshoot / (1.0f + _overshoot)) / a_steps);
        _cd = expf(-4.6f / fmaxf(_decayMs * 0.001f * _rate, 1.0f));
        _cr = expf(-4.6f / fmaxf(_releaseMs * 0.001f * _rate, 1.0f));
        _dirty = false;
    }

    float _attackMs = 10.0f, _decayMs = 300.0f, _sustain = 0.7f, _releaseMs = 400.0f;
    float _overshoot = 0.5f;
    float _rate = 1000.0f;
    float _attackTarget = 1.5f, _ca = 0.0f, _cd = 0.0f, _cr = 0.0f;
    bool _dirty = true;
    float _level = 0.0f;
    Stage _stage = Stage::IDLE;
};

}  // namespace dsp
