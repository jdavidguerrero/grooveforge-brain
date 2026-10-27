#pragma once

#include <math.h>
#include <stdint.h>

/**
 * @file pingpong_delay_core.h
 * @brief Delay estéreo ping-pong sincronizado (theory/fase3-fx.md §2), lógica pura. Código custom.
 *
 * La suma L + R entra a la línea izquierda; la salida izquierda alimenta la derecha y la derecha vuelve a la izquierda
 * con el feedback: los ecos se alternan L, R, L… TONE filtra lo que entra a la línea (señal + realimentación), como la
 * cabeza de una cinta: el primer eco pasa una vez y cada repetición se oscurece un poco más. Las líneas
 * son int16 en un buffer externo (RAM2 en la Teensy).
 */
namespace dsp {

class PingPongDelayCore {
public:
    /// `buf_l` y `buf_r` con `len` muestras cada uno (len ≥ 2).
    void init(int16_t* buf_l, int16_t* buf_r, uint32_t len, float fs) {
        _l = buf_l;
        _r = buf_r;
        _len = len;
        _fs = fs;
        clear();
    }
    void clear() {
        for (uint32_t i = 0; i < _len; ++i) _l[i] = _r[i] = 0;
        _lp = _hp_l = _hp_x = 0.0f;
    }

    /// Tiempo en segundos (se recorta al buffer). El tiempo de lectura se desliza hacia el nuevo (sin clicks).
    void set_time(float seconds) {
        float s = seconds * _fs;
        const float maxs = float(_len - 2);
        _target = s < 1.0f ? 1.0f : s > maxs ? maxs : s;
        if (_cur <= 0.0f) _cur = _target;
    }
    void set_feedback(float fb) { _fb = fb < 0.0f ? 0.0f : fb > 0.9f ? 0.9f : fb; }
    /// TONE 0–1: < 0.5 oscurece (pasabajos 18 kHz → 1.5 kHz), 0.5 plano, > 0.5 adelgaza (pasaaltos 20 → 700 Hz).
    void set_tone(float t) {
        t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
        const float dark = t < 0.5f ? (0.5f - t) * 2.0f : 0.0f, thin = t > 0.5f ? (t - 0.5f) * 2.0f : 0.0f;
        const float lp_hz = 18000.0f * powf(1500.0f / 18000.0f, dark);
        const float hp_hz = 20.0f * powf(700.0f / 20.0f, thin);
        _g_lp = dark > 0.0f ? 1.0f - expf(-6.2831853f * lp_hz / _fs) : 1.0f;   // plano: sin filtro
        _g_hp = thin > 0.0f ? 1.0f - expf(-6.2831853f * hp_hz / _fs) : 0.0f;
    }
    /// Nivel del eco sobre la señal seca (que no se atenúa).
    void set_mix(float m) { _mix = m < 0.0f ? 0.0f : m > 1.0f ? 1.0f : m; }

    void render(const float* in_l, const float* in_r, float* out_l, float* out_r, int n) {
        for (int i = 0; i < n; ++i) {
            _cur += (_target - _cur) > 1.0f ? 1.0f : (_target - _cur) < -1.0f ? -1.0f : (_target - _cur);
            const float dl = read(_l, _cur), dr = read(_r, _cur);
            // TONE a la entrada de la línea: la señal y lo que vuelve de la derecha pasan por el pasabajos y el pasaaltos
            const float x = 0.5f * (in_l[i] + in_r[i]) + _fb * dr;
            _lp += _g_lp * (x - _lp);
            _hp_x += _g_hp * (_lp - _hp_x);
            write(_l, _lp - _hp_x);
            write(_r, dl);                      // la izquierda alimenta la derecha
            _w = (_w + 1) % _len;
            out_l[i] = in_l[i] + _mix * dl;
            out_r[i] = in_r[i] + _mix * dr;
        }
    }

private:
    float read(const int16_t* b, float delay) const {
        float rp = float(_w) - delay;
        while (rp < 0.0f) rp += float(_len);
        const uint32_t i0 = uint32_t(rp) % _len, i1 = (i0 + 1) % _len;
        const float f = rp - floorf(rp);
        return (b[i0] + f * (b[i1] - b[i0])) * (1.0f / 32767.0f);
    }
    void write(int16_t* b, float v) {
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        b[_w] = int16_t(v * 32767.0f);
    }

    int16_t* _l = nullptr;
    int16_t* _r = nullptr;
    uint32_t _len = 0, _w = 0;
    float _fs = 44100.0f, _target = 1.0f, _cur = 0.0f, _fb = 0.4f, _mix = 0.3f;
    float _g_lp = 1.0f, _g_hp = 0.0f, _lp = 0.0f, _hp_l = 0.0f, _hp_x = 0.0f;
};

/// Beats de cada TIME del DELAY: 1/16 · 1/8 · 1/8. · 1/4 · 1/4.
constexpr float PINGPONG_DIV_BEATS[5] = {0.25f, 0.5f, 0.75f, 1.0f, 1.5f};

}  // namespace dsp
