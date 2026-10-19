#include "dx7_core.h"

#include <math.h>

namespace {

constexpr int SINE_N = 1024;
constexpr float MOD_CYCLES = 2.0f;   // salida de un modulador a escala completa = 2 ciclos de fase (≈ 12.6 rad)

/// Tabla de seno de un ciclo (+1 punto para interpolar sin módulo), creada la primera vez.
const float* sine_table() {
    static float t[SINE_N + 1];
    static bool ready = false;
    if (!ready) {
        for (int i = 0; i <= SINE_N; ++i) t[i] = sinf(6.28318530718f * float(i) / SINE_N);
        ready = true;
    }
    return t;
}

inline float sine(const float* t, float ph) {
    ph -= floorf(ph);
    const float x = ph * SINE_N;
    const int i = int(x);
    const float f = x - float(i);
    return t[i] + f * (t[i + 1] - t[i]);
}

// Moduladores de cada operador por algoritmo (máscara de bits de OP1..OP4 = bits 0..3), tabla `ALG` de la cabeza.
constexpr uint8_t MODS[DX7_ALGORITHMS][DX7_OPS] = {
    {1 << 1, 1 << 2, 1 << 3, 0},                  // 1: 4 → 3 → 2 → 1
    {1 << 1, (1 << 2) | (1 << 3), 0, 0},          // 2: 3 → 2, 4 → 2, 2 → 1
    {(1 << 1) | (1 << 3), 1 << 2, 0, 0},          // 3: 3 → 2 → 1, 4 → 1
    {(1 << 1) | (1 << 2), 0, 1 << 3, 0},          // 4: 4 → 3 → 1, 2 → 1
    {1 << 1, 0, 1 << 3, 0},                       // 5: 2 → 1, 4 → 3
    {1 << 3, 1 << 3, 1 << 3, 0},                  // 6: 4 → 1, 2, 3
    {0, 0, 1 << 3, 0},                            // 7: 4 → 3
    {0, 0, 0, 0},                                 // 8: sin modulación
};
constexpr uint8_t CARRIERS[DX7_ALGORITHMS] = {0x1, 0x1, 0x1, 0x1, 0x5, 0x7, 0x7, 0xF};

/// 2^x para x ≤ 0 (amplitud de la envolvente): exponente por bits, fracción por polinomio de 3.er grado (< 0.1 %).
inline float fast_exp2(float x) {
    if (x < -30.0f) return 0.0f;
    const float fl = floorf(x);
    const float f = x - fl;
    const float p = 1.0f + f * (0.6951786f + f * (0.2261868f + f * 0.0781021f));
    union { float v; int32_t i; } u{p};
    u.i += int32_t(fl) << 23;
    return u.v;
}

}  // namespace

float dx7_level_amp(float level) {
    if (level <= 0.0f) return 0.0f;
    return powf(2.0f, (level - 99.0f) / 8.0f);
}

bool dx7_is_carrier(int alg, int op) {
    if (alg < 1 || alg > DX7_ALGORITHMS || op < 0 || op >= DX7_OPS) return false;
    return CARRIERS[alg - 1] & (1 << op);
}

float dx7_wave(int wave, float ph) {
    const float* t = sine_table();
    ph -= floorf(ph);
    if (wave <= 0) return sine(t, ph);
    if (wave == 1) { const float s = sine(t, ph); return s * fabsf(s); }
    if (ph >= 0.5f) return 0.0f;                     // W3–W8: la segunda mitad del ciclo en cero
    switch (wave) {
        case 2: return sine(t, ph);
        case 3: { const float s = sine(t, ph); return s * s; }
        case 4: return sine(t, 2.0f * ph);
        case 5: { const float s = sine(t, 2.0f * ph); return s * fabsf(s); }
        case 6: return fabsf(sine(t, 2.0f * ph));
        default: { const float s = sine(t, 2.0f * ph); return s * s; }
    }
}

void Dx7VoiceCore::note_on(float hz, float velocity) {
    (void)sine_table();   // la tabla se arma acá (contexto de loop), no en la primera ISR de audio
    hz_ = hz;
    note_ = hz > 1.0f ? 69.0f + 12.0f * log2f(hz / 440.0f) : 0.0f;
    if (velocity < 0.0f) velocity = 0.0f;
    if (velocity > 1.0f) velocity = 1.0f;
    vel_ = velocity;
    for (int k = 0; k < DX7_OPS; ++k) {
        phase_[k] = 0.0f;
        att_[k] = DX7_EG_FLOOR_DB;
        stage_[k] = ATTACK;
    }
    fb_[0] = fb_[1] = 0.0f;
    active_ = true;
}

void Dx7VoiceCore::note_off() {
    for (int k = 0; k < DX7_OPS; ++k)
        if (stage_[k] != IDLE) stage_[k] = RELEASE;
}

void Dx7VoiceCore::render(const Dx7Patch& p, float fs, float* out, int n) {
    if (!active_) {
        for (int i = 0; i < n; ++i) out[i] = 0.0f;
        return;
    }
    const float* tab = sine_table();
    const int alg = (p.algorithm < 1 || p.algorithm > DX7_ALGORITHMS) ? 0 : p.algorithm - 1;
    const uint8_t carriers = CARRIERS[alg];
    int ncar = 0;
    for (int k = 0; k < DX7_OPS; ++k) ncar += (carriers >> k) & 1;
    const float out_gain = 1.0f / float(ncar);
    const float fb = p.feedback ? powf(2.0f, float(p.feedback) - 7.0f) * 0.5f : 0.0f;

    // Sensibilidad a la velocidad clásica DX7:
    // -En portadoras controla el volumen de la nota (1.0 cuando vel_==1.0).
    // -En moduladores controla el índice de modulación FM (timbre suave a baja velocidad, metálico/rico a alta).
    const float car_vel = (vel_ >= 0.999f) ? 1.0f : (0.35f + 0.65f * vel_);
    const float mod_vel = (vel_ >= 0.999f) ? 1.0f : (0.30f + 0.70f * vel_ * vel_);

    // Escalado de teclado (§11.1): octavas sobre C3 (nota 48)
    const float oct_up = (note_ - 48.0f) / 12.0f;
    const float kbd_att_db = p.kbd_level > 0.0f && oct_up > 0.0f ? p.kbd_level / 99.0f * 12.0f * oct_up : 0.0f;
    const float rate_mul = p.kbd_rate ? exp2f(oct_up * float(p.kbd_rate) / 3.0f) : 1.0f;   // > 1 = más rápido

    // Por bloque: incrementos de fase, amplitudes y pendientes de las envolventes en dB por muestra
    float inc[DX7_OPS], amp[DX7_OPS], ka[DX7_OPS], d_slope[DX7_OPS], r_slope[DX7_OPS], sus_att[DX7_OPS];
    const float ms_to_samples = fs / 1000.0f / rate_mul;
    const float d2_slope = p.d2_ms > 0.5f ? 60.0f / (p.d2_ms * ms_to_samples) : 0.0f;
    for (int k = 0; k < DX7_OPS; ++k) {
        const Dx7OpParams& o = p.op[k];
        const bool is_car = ((carriers >> k) & 1) != 0;
        inc[k] = hz_ * o.ratio * exp2f(float(o.detune) * DX7_DETUNE_CENTS / 1200.0f) / fs;
        amp[k] = dx7_level_amp(o.level) * (is_car ? car_vel : mod_vel);
        if (!is_car && kbd_att_db > 0.0f) amp[k] *= exp2f(-kbd_att_db / 6.0206f);
        const float s = o.sustain < 0.0f ? 0.0f : o.sustain > 1.0f ? 1.0f : o.sustain;
        sus_att[k] = s <= 1.6e-5f ? DX7_EG_FLOOR_DB : fminf(DX7_EG_FLOOR_DB, -20.0f * log10f(s));
        // Ataque: att ← att·ka, de 96 dB a 0.1 dB en attack_ms
        ka[k] = o.attack_ms > 0.5f ? expf(logf(0.1f / DX7_EG_FLOOR_DB) / (o.attack_ms * ms_to_samples)) : 0.0f;
        d_slope[k] = o.decay_ms > 0.5f ? 60.0f / (o.decay_ms * ms_to_samples) : DX7_EG_FLOOR_DB;
        r_slope[k] = o.release_ms > 0.5f ? 60.0f / (o.release_ms * ms_to_samples) : DX7_EG_FLOOR_DB;
    }

    for (int i = 0; i < n; ++i) {
        float y[DX7_OPS];
        float sum = 0.0f;
        for (int k = DX7_OPS - 1; k >= 0; --k) {          // OP4 → OP1: los moduladores tienen número mayor
            float& a = att_[k];
            switch (stage_[k]) {
                case ATTACK:
                    a *= ka[k];
                    if (a < 0.1f) { a = 0.0f; stage_[k] = DECAY; }
                    break;
                case DECAY:
                    a += d_slope[k];
                    if (a >= sus_att[k]) { a = sus_att[k]; stage_[k] = SUSTAIN; }
                    break;
                case SUSTAIN:
                    if (d2_slope > 0.0f) a = fminf(DX7_EG_FLOOR_DB, a + d2_slope);
                    else a = sus_att[k];                   // sigue al SUSTAIN del panel en vivo
                    break;
                case RELEASE:
                    a += r_slope[k];
                    if (a >= DX7_EG_FLOOR_DB) { a = DX7_EG_FLOOR_DB; stage_[k] = IDLE; }
                    break;
                case IDLE:
                    a = DX7_EG_FLOOR_DB;
                    break;
            }
            const float e = a >= DX7_EG_FLOOR_DB ? 0.0f : fast_exp2(-a * (1.0f / 6.0206f));
            float m = 0.0f;
            const uint8_t mods = MODS[alg][k];
            for (int j = k + 1; j < DX7_OPS; ++j)
                if (mods & (1 << j)) m += y[j] * MOD_CYCLES;
            if (k == DX7_OPS - 1) m += fb * (fb_[0] + fb_[1]) * 0.5f;
            const float ph = phase_[k] + m;
            y[k] = (p.op[k].wave ? dx7_wave(p.op[k].wave, ph) : sine(tab, ph)) * amp[k] * e;
            phase_[k] += inc[k];
            if (phase_[k] >= 1.0f) phase_[k] -= 1.0f;
            if ((carriers >> k) & 1) sum += y[k];
        }
        fb_[1] = fb_[0];
        fb_[0] = y[DX7_OPS - 1];
        out[i] = sum * out_gain;
    }

    bool any = false;
    for (int k = 0; k < DX7_OPS; ++k)
        if (((carriers >> k) & 1) && stage_[k] != IDLE) any = true;
    active_ = any;
}
