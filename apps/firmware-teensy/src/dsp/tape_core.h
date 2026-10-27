#pragma once

#include <math.h>

#include "fast_math.h"

/**
 * @file tape_core.h
 * @brief Saturación de cinta en coma flotante (theory/fase3-fx.md, TAPE rev. 2), lógica pura. Código custom.
 *
 * Reemplaza el camino mojado del TAPE (waveshaper de tabla int16 + dos AudioFilterStateVariable en punto fijo), que
 * metía ruido con cada nota. tanh(d·x)/√d (Zölzer, "DAFX" cap. 5.1) → bloqueador de DC de un polo (~20 Hz) →
 * pasabajos de un polo con el corte suavizado por muestra (la "edad" de la cinta y el azimut del wow, sin zipper).
 */
namespace dsp {

class TapeCore {
public:
    void set_drive(float d) {
        d = d < 1.0f ? 1.0f : d > 10.0f ? 10.0f : d;
        _drive = d;
        _norm = 1.0f / sqrtf(d);
    }
    /// Corte del pasabajos de cinta en Hz (la edad y el azimut); se llega a él en ~1 ms.
    void set_cutoff(float hz, float fs) {
        hz = hz < 2500.0f ? 2500.0f : hz > 20000.0f ? 20000.0f : hz;
        _g_target = 1.0f - expf(-6.2831853f * hz / fs);
        if (_g < 0.0f) _g = _g_target;
        _k_smooth = 1.0f - expf(-1.0f / (0.001f * fs));
        _r_dc = 1.0f - 6.2831853f * 20.0f / fs;
    }
    void set_mix(float m) { _mix = m < 0.0f ? 0.0f : m > 1.0f ? 1.0f : m; }

    void render(const float* in, float* out, int n) {
        for (int i = 0; i < n; ++i) {
            _g += _k_smooth * (_g_target - _g);
            const float x = in[i];
            const float s = fast_tanh(_drive * x) * _norm;
            _dc = s - _s_prev + _r_dc * _dc;                  // bloqueador de DC
            _s_prev = s;
            _lp += _g * (_dc - _lp);
            out[i] = x + _mix * (_lp - x);
        }
    }

private:
    float _drive = 3.25f, _norm = 0.5547f, _mix = 0.6f;
    float _g = -1.0f, _g_target = 1.0f, _k_smooth = 0.02f, _r_dc = 0.99715f;
    float _dc = 0.0f, _s_prev = 0.0f, _lp = 0.0f;
};

}  // namespace dsp
