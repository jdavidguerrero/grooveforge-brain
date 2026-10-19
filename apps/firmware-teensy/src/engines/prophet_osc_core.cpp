#include "prophet_osc_core.h"

#include <math.h>
#include <string.h>

namespace {

/// Corrección polyBLEP para un salto de +2 en t = 0 (t en ciclos, dt = incremento por muestra).
inline float poly_blep(float t, float dt) {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

inline float frac(float x) { return x - floorf(x); }

inline float saw(float t, float dt) { return 2.0f * t - 1.0f - poly_blep(t, dt); }

inline float pulse(float t, float dt, float pw) {
    const float v = t < pw ? 1.0f : -1.0f;
    return v + poly_blep(t, dt) - poly_blep(frac(t - pw + 1.0f), dt);
}

inline float tri(float t) { return 1.0f - 4.0f * fabsf(t - 0.5f); }

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

}  // namespace

float prophet_fast_exp2(float x) {
    const float fl = floorf(x);
    const float f = x - fl;
    // 2^f en [0, 1): polinomio minimax de 3.er grado
    const float p = 1.0f + f * (0.6951786f + f * (0.2261513f + f * 0.0784947f));
    int32_t e = int32_t(fl);
    if (e < -126) return 0.0f;
    if (e > 127) e = 127;
    uint32_t bits;
    memcpy(&bits, &p, 4);
    bits += uint32_t(e) << 23;
    float r;
    memcpy(&r, &bits, 4);
    return r;
}

void Prophet5OscCore::render(const Prophet5OscParams& p, float* out, int n) {
    // Varias formas encendidas se promedian para que el nivel no dependa de cuántas son
    const int na = int(p.a_saw) + int(p.a_pulse);
    const int nb = int(p.b_saw) + int(p.b_tri) + int(p.b_pulse);
    const float pm_env = p.pm_env * p.env;
    const float inc_b = clampf(p.inc_b, 0.0f, 0.45f);

    for (int i = 0; i < n; ++i) {
        // OSC B
        pb_ += inc_b;
        bool wrapped = false;
        if (pb_ >= 1.0f) { pb_ -= 1.0f; wrapped = true; }
        float b = 0.0f;
        if (p.b_saw) b += saw(pb_, inc_b);
        if (p.b_tri) b += tri(pb_);
        if (p.b_pulse) b += pulse(pb_, inc_b, p.pw_b);
        if (nb) b /= float(nb);

        // POLY-MOD: OSC B (audio) + envelope del filtro (bloque)
        const float m = p.pm_b * b + pm_env;
        float inc_a = p.inc_a;
        if (p.pm_freq_a) inc_a *= prophet_fast_exp2(PROPHET_PM_OCT * m);
        inc_a = clampf(inc_a, 0.0f, 0.45f);
        const float pw_a = p.pm_pw_a ? clampf(p.pw_a + 0.45f * m, 0.05f, 0.95f) : p.pw_a;

        // OSC A (sync: vuelve a 0 cuando B da la vuelta)
        pa_ += inc_a;
        if (p.sync && wrapped) pa_ = pb_ * (inc_b > 0.0f ? inc_a / inc_b : 0.0f);
        if (pa_ >= 1.0f) pa_ -= floorf(pa_);
        float a = 0.0f;
        if (p.a_saw) a += saw(pa_, inc_a);
        if (p.a_pulse) a += pulse(pa_, inc_a, pw_a);
        if (na) a /= float(na);

        // Ruido blanco (xorshift32)
        rng_ ^= rng_ << 13;
        rng_ ^= rng_ >> 17;
        rng_ ^= rng_ << 5;
        const float noise = float(int32_t(rng_)) * (1.0f / 2147483648.0f);

        out[i] = a * p.lvl_a + b * p.lvl_b + noise * p.lvl_n;
    }
}
