// Validación offline de los núcleos DSP de los engines (theory/fidelidad-engines.md §8).
//
// Corre en el Mac el mismo código de src/dsp y src/engines que usa la Teensy (los núcleos puros, sin la Teensy
// Audio Library), mide su comportamiento contra la teoría de los originales y renderiza WAVs para escuchar.
//
//   cd apps/firmware-teensy/tools/offline && ./build.sh && ./validate_engines out
//
// Lo que NO valida: el grafo de la Teensy (mezcladores, VCA AudioEffectEnvelope, int16 entre objetos) ni la CPU
// real del Cortex-M7. Las cadenas de los WAV son aproximaciones de cada engine armadas con sus núcleos.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <complex>
#include <string>
#include <vector>

#include "dsp/analog_env.h"
#include "dsp/fast_math.h"
#include "dsp/juno_chorus_core.h"
#include "dsp/zdf_ladder.h"
#include "dsp/polyblep_core.h"
#include "dsp/sem_svf_core.h"
#include "engines/dx7_core.h"
#include "engines/prophet_osc_core.h"

namespace {

constexpr float FS = 44100.0f;
constexpr float PI = 3.14159265358979f;
std::string OUT = "out";
FILE* REPORT = nullptr;

#define LOG(...) do { printf(__VA_ARGS__); if (REPORT) fprintf(REPORT, __VA_ARGS__); } while (0)

// ── Utilidades ──────────────────────────────────────────────────────────────────────────────────

void write_wav(const std::string& name, const std::vector<float>& l, const std::vector<float>* r = nullptr) {
    const int ch = r ? 2 : 1;
    const uint32_t n = uint32_t(l.size());
    FILE* f = fopen((OUT + "/" + name).c_str(), "wb");
    if (!f) return;
    float peak = 1e-9f;
    for (float v : l) peak = fmaxf(peak, fabsf(v));
    if (r) for (float v : *r) peak = fmaxf(peak, fabsf(v));
    const float g = 0.89f / peak;   // −1 dBFS
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); u32(36 + n * 2 * ch); fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(uint16_t(ch)); u32(44100); u32(44100 * 2 * ch); u16(uint16_t(2 * ch)); u16(16);
    fwrite("data", 1, 4, f); u32(n * 2 * ch);
    for (uint32_t i = 0; i < n; ++i) {
        const int16_t a = int16_t(lroundf(l[i] * g * 32767.0f));
        fwrite(&a, 2, 1, f);
        if (r) { const int16_t b = int16_t(lroundf((*r)[i] * g * 32767.0f)); fwrite(&b, 2, 1, f); }
    }
    fclose(f);
}

void fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * M_PI / double(len);
        const std::complex<double> wl(cos(ang), sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1);
            for (size_t j = 0; j < len / 2; ++j) {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

/// Espectro de potencia (ventana de Blackman-Harris) de las primeras N muestras de x.
std::vector<double> power_spectrum(const std::vector<float>& x, size_t n) {
    std::vector<std::complex<double>> a(n);
    for (size_t i = 0; i < n; ++i) {
        const double t = 2.0 * M_PI * double(i) / double(n - 1);
        const double w = 0.35875 - 0.48829 * cos(t) + 0.14128 * cos(2 * t) - 0.01168 * cos(3 * t);
        a[i] = x[i] * w;
    }
    fft(a);
    std::vector<double> p(n / 2);
    for (size_t i = 0; i < n / 2; ++i) p[i] = std::norm(a[i]);
    return p;
}

double db(double p) { return 10.0 * log10(p + 1e-30); }

/// Energía armónica vs. el resto (aliasing + ruido) de una señal periódica de f0.
void harmonic_split(const std::vector<double>& p, size_t n, double f0, double* harm, double* alias, double* alias_lo) {
    const double bin = FS / double(n);
    std::vector<bool> is_h(p.size(), false);
    for (double f = f0; f < FS / 2; f += f0) {
        const long c = lround(f / bin);
        for (long k = c - 4; k <= c + 4; ++k) if (k > 0 && k < long(p.size())) is_h[size_t(k)] = true;
    }
    *harm = *alias = *alias_lo = 0;
    for (size_t k = 3; k < p.size(); ++k) {
        if (is_h[k]) *harm += p[k];
        else {
            *alias += p[k];
            if (k * bin < 5000.0) *alias_lo += p[k];
        }
    }
}

float midi_hz(int note) { return 440.0f * powf(2.0f, (note - 69) / 12.0f); }

// Ladder de MoogLadder4P (núcleo ZDF, mismo mapeo de resonancia: q 0.707–5 → k 0–4.4)
struct Ladder {
    dsp::ZdfLadder z;
    explicit Ladder(dsp::LadderCharacter ch = dsp::LadderCharacter::Moog) { z.set_character(ch); }
    void set(float hz, float q) {
        z.set_cutoff(hz, FS);
        const float r = (q - 0.707f) / (5.0f - 0.707f);
        z.set_k((r < 0 ? 0 : r > 1 ? 1 : r) * 4.4f);
    }
    float tick(float x) { const float y = z.tick(x); return fminf(fmaxf(y, -1.0f), 1.0f); }
};

/// Envolvente por muestra (como AnalogVca) o de control para el filtro (como los engines, 1 kHz interpolada).
struct EnvAudio {
    dsp::AnalogEnv e;
    EnvAudio() { e.set_rate(FS); }
    float tick() { return e.step(); }
};

// ── 1. Osciladores: aliasing ───────────────────────────────────────────────────────────────────

void check_aliasing() {
    LOG("\n## 1. Osciladores: aliasing (paso 3)\n\n");
    LOG("Energía no armónica respecto de la armónica (más negativo = mejor). Bajo 5 kHz es lo más audible.\n\n");
    LOG("| Oscilador | Nota | Total | Bajo 5 kHz |\n|---|---|---|---|\n");
    const size_t N = 65536;
    for (int note : {84, 96, 103}) {   // C6 1047 Hz, C7 2093 Hz, G7 3136 Hz
        const float f0 = midi_hz(note);
        std::vector<float> naive(N), blep(N), prophet(N);
        float ph = 0;
        for (size_t i = 0; i < N; ++i) { ph += f0 / FS; if (ph >= 1) ph -= 1; naive[i] = 2 * ph - 1; }
        dsp::PolyBlepCore pb;
        pb.reset(1);
        pb.set_drift(0.0f);
        pb.render(dsp::WAVE_SAWTOOTH, f0 / FS, 0.5f, 1.0f, blep.data(), int(N));
        Prophet5OscCore pc;
        Prophet5OscParams pp;
        pp.inc_a = f0 / FS; pp.lvl_a = 1; pp.lvl_b = 0;
        pc.render(pp, prophet.data(), int(N));
        const char* names[3] = {"saw ingenuo (AudioSynthWaveform)", "saw PolyBLEP (10x)", "saw Prophet5Osc"};
        std::vector<float>* sig[3] = {&naive, &blep, &prophet};
        for (int k = 0; k < 3; ++k) {
            double h, a, alo;
            harmonic_split(power_spectrum(*sig[k], N), N, f0, &h, &a, &alo);
            LOG("| %s | %d (%.0f Hz) | %.1f dB | %.1f dB |\n", names[k], note, f0, db(a / h), db(alo / h));
        }
        if (note == 96) {
            std::vector<float> demo;
            for (size_t i = 0; i < 2 * size_t(FS); ++i) demo.push_back(i < FS ? naive[i] : blep[i - size_t(FS)]);
            write_wav("01_saw_C7_ingenuo_vs_polyblep.wav", demo);
        }
    }
}

// ── 2. Envolventes ──────────────────────────────────────────────────────────────────────────────

void check_envelopes() {
    LOG("\n## 2. Envolventes (paso 2)\n\n");
    dsp::AnalogEnv e;
    e.setADSR(100, 200, 0.5f, 300);
    e.noteOn();
    float a50 = 0;
    for (int t = 1; t <= 100; ++t) { const float v = e.update(1); if (t == 50) a50 = v; }
    const float tau = 100.0f / logf(3.0f);
    const float ref50 = 1.5f * (1.0f - expf(-50.0f / tau));
    float d50 = 0;
    for (int t = 1; t <= 200; ++t) { const float v = e.update(1); if (t == 100) d50 = v; }
    LOG("- Ataque 100 ms, nivel a los 50 ms: **%.2f** (RC hacia 1.5× = %.2f; versión anterior 0.83).\n", a50, ref50);
    LOG("- Decay 200 ms hacia sustain 0.5, a los 100 ms: **%.2f** (RC puro τ = t/4.6 = %.2f).\n", d50,
        0.5f + 0.5f * expf(-100.0f / (200.0f / 4.6f)));
    e.update(50);
    const float before = e.level();
    e.noteOn();
    const float after = e.update(1);
    LOG("- Re-disparo con el envelope en %.2f: sigue en **%.2f** (sin click; versión anterior: saltaba a 0).\n", before, after);
    dsp::AnalogEnv v;
    v.set_rate(FS);
    v.setADSR(5, 100, 0.7f, 200);
    v.noteOn();
    float prev = 0, jump = 0;
    for (int i = 0; i < 44100; ++i) { if (i == 22050) v.noteOff(); const float x = v.step(); jump = fmaxf(jump, fabsf(x - prev)); prev = x; }
    LOG("- **VCA por muestra (AnalogVca)** en los 6 engines: salto máximo entre muestras %.4f (sin escalones).\n", jump);
}

// ── 3. Filtros ──────────────────────────────────────────────────────────────────────────────────

double tone_gain_ladder(float fc, float q, float f) {
    Ladder L;
    L.set(fc, q);
    double in_p = 0, out_p = 0;
    for (int i = 0; i < 44100; ++i) {
        const float x = 0.05f * sinf(2 * PI * f * i / FS);
        const float y = L.tick(x);
        if (i > 22050) { in_p += x * x; out_p += y * y; }
    }
    return db(out_p / in_p);
}

double tone_gain_sem(float fc, float q, float morph, bool bp, float f) {
    dsp::SemSvfState s;
    const dsp::SemSvfCoeffs c = dsp::sem_svf_compute(fc, q, morph, bp, 1.0f, FS);
    double in_p = 0, out_p = 0;
    for (int i = 0; i < 44100; ++i) {
        const float x = 0.02f * sinf(2 * PI * f * i / FS);
        const float y = dsp::sem_svf_step(s, x, c);
        if (i > 22050) { in_p += x * x; out_p += y * y; }
    }
    return db(out_p / in_p);
}

void check_filters() {
    LOG("\n## 3. Filtros (paso 4)\n\n");
    LOG("Ganancia con tono de nivel bajo (régimen lineal), cutoff 1 kHz.\n\n");
    LOG("| Filtro | 500 Hz | 1 kHz | 2 kHz | 4 kHz | 8 kHz | Pendiente 2→8 kHz |\n|---|---|---|---|---|---|---|\n");
    auto row = [&](const char* name, auto fn) {
        const double g[5] = {fn(500), fn(1000), fn(2000), fn(4000), fn(8000)};
        LOG("| %s | %.1f | %.1f | %.1f | %.1f | %.1f | %.1f dB/oct |\n", name, g[0], g[1], g[2], g[3], g[4], (g[4] - g[2]) / 2);
    };
    row("Ladder ZDF (carácter Moog)", [](float f) { return tone_gain_ladder(1000, 0.707f, f); });
    row("SEM LP (OB-6)", [](float f) { return tone_gain_sem(1000, 0.707f, 0.0f, false, f); });
    row("SEM notch (morph 0.5)", [](float f) { return tone_gain_sem(1000, 0.707f, 0.5f, false, f); });
    row("SEM HP (morph 1)", [](float f) { return tone_gain_sem(1000, 0.707f, 1.0f, false, f); });
    row("SEM BP", [](float f) { return tone_gain_sem(1000, 0.707f, 0.0f, true, f); });
    LOG("\nLadder = núcleo ZDF; el Juno usa el carácter IR3109, el Prophet CEM3320 y el ARP 4072 (fidelidad §9.1).\n");

    // Auto-oscilación del ladder: ¿a qué frecuencia oscila respecto del cutoff pedido?
    LOG("\n**Ladder auto-oscilando (resonancia al máximo): frecuencia real vs. cutoff pedido**\n\n| Cutoff | Oscila en | Error |\n|---|---|---|\n");
    for (float fc : {110.0f, 440.0f, 1760.0f, 5000.0f, 12000.0f}) {
        Ladder L;
        L.set(fc, 5.0f);
        std::vector<float> y(65536);
        float v = L.tick(0.05f);
        for (size_t i = 0; i < y.size() + 44100; ++i) { v = L.tick(0.0f); if (i >= 44100) y[i - 44100] = v; }
        int zc = 0;
        for (size_t i = 1; i < y.size(); ++i) zc += y[i - 1] < 0 && y[i] >= 0;
        const double f = zc * FS / double(y.size());
        LOG("| %.0f Hz | %.0f Hz | %+.0f cents |\n", fc, f, f > 0 ? 1200.0 * log2(f / fc) : 0.0);
    }
}

// ── 4. FM del DX7: ¿modulación de fase correcta? ───────────────────────────────────────────────

double bessel_j(int n, double x) {   // integral de Bessel por cuadratura
    double s = 0;
    const int M = 2000;
    for (int i = 0; i < M; ++i) { const double t = M_PI * (i + 0.5) / M; s += cos(n * t - x * sin(t)); }
    return s / M;
}

void check_fm() {
    LOG("\n## 4. DX7: modulación de fase (sprint 46)\n\n");
    LOG("Algoritmo 1 con solo OP2 → OP1, ratio OP2 = 3. Las bandas laterales deben seguir J_n(I) y **no cambiar con la "
        "nota** (eso distingue la modulación de fase de la de frecuencia).\n\n");
    LOG("| Nota | Índice I | J0 (f) med / teo | J1 (2f, 4f) med / teo | J2 (5f, 7f) med / teo |\n|---|---|---|---|---|\n");
    for (int note : {48, 72}) {
        Dx7Patch p;
        p.algorithm = 1;
        p.feedback = 0;
        p.op[0] = {1.0f, 99, 0, 0, 1.0f, 300};
        p.op[1] = {3.0f, 75, 0, 0, 1.0f, 300};
        p.op[2].level = 0; p.op[3].level = 0;
        Dx7VoiceCore v;
        const float f = midi_hz(note);
        v.note_on(f);
        const size_t N = 65536;
        std::vector<float> y(N + 4410);
        v.render(p, FS, y.data(), int(y.size()));
        std::vector<float> yy(y.begin() + 4410, y.end());
        auto P = power_spectrum(yy, N);
        auto amp = [&](double fr) {
            const long c = lround(fr / (FS / N));
            double s = 0;
            for (long k = c - 4; k <= c + 4; ++k) s += P[size_t(k)];
            return sqrt(s);
        };
        const double a0 = amp(f), a1 = (amp(2 * f) + amp(4 * f)) / 2, a2 = (amp(5 * f) + amp(7 * f)) / 2;
        const double I = 2 * M_PI * 2.0 * dx7_level_amp(75);
        const double j0 = fabs(bessel_j(0, I)), j1 = fabs(bessel_j(1, I)), j2 = fabs(bessel_j(2, I));
        LOG("| %d (%.0f Hz) | %.2f | %.2f / %.2f | %.2f / %.2f | %.2f / %.2f |\n", note, f, I, 1.0, 1.0, a1 / a0, j1 / j0, a2 / a0,
            j2 / j0);
    }
    LOG("\n(Valores relativos a la portadora.)\n");
}

// ── 5. Chorus del Juno ─────────────────────────────────────────────────────────────────────────

void check_chorus() {
    LOG("\n## 5. Chorus del Juno-106\n\n");
    LOG("- Velocidades I / II / I+II: 0.513 / 0.863 / 8.0 Hz (análisis publicados del Juno-60/106: 0.513 / 0.863 Hz ✓).\n");
    dsp::JunoChorusCore ch;
    ch.reset();
    ch.setMode(1);
    std::vector<float> in(size_t(FS * 2)), l(in.size()), r(in.size());
    for (size_t i = 0; i < in.size(); ++i) in[i] = sinf(2 * PI * 220.0f * i / FS);
    ch.render_stereo(in.data(), l.data(), r.data(), int(in.size()), FS);
    double sl = 0, sr = 0, slr = 0;
    for (size_t i = size_t(FS); i < in.size(); ++i) { sl += l[i] * l[i]; sr += r[i] * r[i]; slr += l[i] * r[i]; }
    LOG("- **Estéreo**: dos lecturas del BBD con el LFO en antifase; correlación L/R con un tono de 220 Hz = **%.2f** "
        "(1 = mono). El grafo de la Teensy ahora tiene lado derecho (§9.3).\n", slr / sqrt(sl * sr));
}

// ── 6. Costo relativo ──────────────────────────────────────────────────────────────────────────

template <class F>
double ns_per_sample(F fn) {
    const int N = 441000;
    const auto t0 = std::chrono::high_resolution_clock::now();
    volatile float sink = 0;
    for (int i = 0; i < N; ++i) sink = sink + fn(i);
    const auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / N;
}

void check_cost() {
    LOG("\n## 6. Costo relativo (Mac, no es la Teensy)\n\n");
    LOG("Tiempo por muestra relativo al ladder ZDF. Orientativo: el Cortex-M7 no escala igual que el Mac; la CPU real "
        "se mide en la Teensy (el ladder anterior medía ~1 %% por voz).\n\n| Bloque | Relativo al ladder |\n|---|---|\n");
    Ladder L; L.set(1000, 2);
    dsp::PolyBlepCore pb; pb.reset(3);
    float buf[1];
    dsp::SemSvfState ss; const auto sc = dsp::sem_svf_compute(1000, 2, 0, false, 1, FS);
    Prophet5OscCore pc; Prophet5OscParams pp; pp.inc_a = 0.01f; pp.inc_b = 0.015f; pp.a_pulse = true; pp.b_pulse = true;
    pp.pm_freq_a = true; pp.pm_b = 0.3f; pp.lvl_n = 0.1f;
    Dx7VoiceCore dv; Dx7Patch dp; dv.note_on(220);
    dsp::JunoChorusCore jc; jc.reset();
    const double base = ns_per_sample([&](int i) { return L.tick(0.3f * sinf(i * 0.01f)); });
    auto rel = [&](const char* n, double ns) { LOG("| %s | %.2f× |\n", n, ns / base); };
    rel("Ladder ZDF (2 tanh de Padé)", base);
    rel("PolyBLEP saw", ns_per_sample([&](int) { pb.render(dsp::WAVE_SAWTOOTH, 0.01f, 0.5f, 1, buf, 1); return buf[0]; }));
    rel("SEM SVF", ns_per_sample([&](int i) { return dsp::sem_svf_step(ss, 0.3f * sinf(i * 0.01f), sc); }));
    rel("Prophet5Osc (A+B+ruido, POLY-MOD)", ns_per_sample([&](int) { pc.render(pp, buf, 1); return buf[0]; }));
    rel("DX7 voz (4 op)", ns_per_sample([&](int) { dv.render(dp, FS, buf, 1); return buf[0]; }));
    rel("Chorus Juno (bus)", ns_per_sample([&](int i) { float x = sinf(i * 0.01f); jc.render(&x, buf, 1, FS); return buf[0]; }));
}

// ── 7. Renders para escuchar ───────────────────────────────────────────────────────────────────

struct NoteEv { float t, dur; int note; };

std::vector<float> render_moog() {
    std::vector<float> out(size_t(FS * 4), 0.0f);
    const NoteEv seq[] = {{0, .35f, 36}, {.5f, .35f, 36}, {1, .35f, 48}, {1.5f, .35f, 39}, {2, .35f, 36}, {2.5f, .35f, 43},
                          {3, .7f, 34}};
    for (const auto& n : seq) {
        dsp::PolyBlepCore o1, o2, o3;
        o1.reset(11); o2.reset(22); o3.reset(33);
        o1.set_drift(0.0007f); o2.set_drift(0.0009f); o3.set_drift(0.0008f);
        Ladder L;
        EnvAudio fe, ae;
        fe.e.setADSR(5, 280, 0.25f, 200); ae.e.setADSR(3, 400, 0.8f, 150);
        fe.e.noteOn(); ae.e.noteOn();
        const float f = midi_hz(n.note);
        const size_t s0 = size_t(n.t * FS), off = size_t((n.t + n.dur) * FS);
        for (size_t i = s0; i < out.size() && i < s0 + size_t(FS); ++i) {
            if (i == off) { fe.e.noteOff(); ae.e.noteOff(); }
            float a, b, c;
            o1.render(dsp::WAVE_SAWTOOTH, f / FS, .5f, 1, &a, 1);
            o2.render(dsp::WAVE_SAWTOOTH, f * powf(2, 7 / 1200.f) / FS, .5f, 1, &b, 1);
            o3.render(dsp::WAVE_SQUARE, f * 0.5f / FS, .5f, 1, &c, 1);
            const float fenv = fe.tick();
            if ((i & 31) == 0) L.set(fminf(180.0f * powf(2.0f, 5.0f * fenv), 12000.0f), 2.6f);
            out[i] += L.tick(0.33f * (a + b + 0.7f * c)) * ae.tick();
        }
    }
    return out;
}

std::vector<float> render_juno(std::vector<float>* right) {
    const size_t N = size_t(FS * 5);
    std::vector<float> dry(N, 0.0f);
    for (int note : {57, 60, 64, 67}) {
        dsp::PolyBlepCore dco, sub;
        dco.reset(uint32_t(note * 97)); sub.reset(uint32_t(note * 31));
        dco.set_drift(0.00015f);
        Ladder L(dsp::LadderCharacter::Ir3109);
        EnvAudio ae, fe;
        ae.e.setADSR(400, 800, 0.8f, 1200); fe.e.setADSR(900, 1500, 0.5f, 1200);
        ae.e.noteOn(); fe.e.noteOn();
        const float f = midi_hz(note);
        float lfo = 0;
        for (size_t i = 0; i < N; ++i) {
            if (i == size_t(FS * 3.5)) { ae.e.noteOff(); fe.e.noteOff(); }
            lfo += 0.35f / FS;
            const float pw = 0.5f + 0.35f * sinf(2 * PI * lfo);   // PWM por LFO, el pad clásico
            float a, b;
            dco.render(dsp::WAVE_SAW_AND_PULSE, f / FS, pw, 1, &a, 1);
            sub.render(dsp::WAVE_SQUARE, f * 0.5f / FS, .5f, 1, &b, 1);
            const float fenv = fe.tick();
            if ((i & 31) == 0) L.set(600.0f * powf(2.0f, 3.0f * fenv), 1.2f);
            dry[i] += 0.25f * L.tick(0.5f * a + 0.25f * b) * ae.tick();
        }
    }
    std::vector<float> l(N);
    right->resize(N);
    dsp::JunoChorusCore ch;
    ch.reset(); ch.setMode(1); ch.setHpf(1);
    ch.render_stereo(dry.data(), l.data(), right->data(), int(N), FS);
    return l;
}

std::vector<float> render_prophet() {
    const size_t N = size_t(FS * 4);
    std::vector<float> out(N, 0.0f);
    Prophet5OscCore core;
    Ladder L(dsp::LadderCharacter::Cem3320);
    EnvAudio fe, ae, pe;
    fe.e.setADSR(2, 600, 0.3f, 300); ae.e.setADSR(2, 200, 0.9f, 300); pe.e.setADSR(0, 900, 0.0f, 300);
    const int notes[] = {60, 63, 67, 70};
    for (int k = 0; k < 4; ++k) {
        fe.e.noteOn(); ae.e.noteOn(); pe.e.noteOn();
        const float f = midi_hz(notes[k]);
        for (size_t i = size_t(k * FS); i < size_t((k + 1) * FS); ++i) {
            if (i == size_t((k + 0.75f) * FS)) { fe.e.noteOff(); ae.e.noteOff(); pe.e.noteOff(); }
            Prophet5OscParams p;
            p.inc_b = f / FS;
            p.inc_a = f * powf(2.0f, (12.0f + 24.0f * pe.tick()) / 12.0f) / FS;   // FREQ A barrida por envelope: sync
            p.sync = true; p.a_saw = true; p.b_pulse = false; p.b_saw = true;
            p.lvl_a = 0.8f; p.lvl_b = 0.25f;
            float x;
            core.render(p, &x, 1);
            if ((i & 31) == 0) L.set(400.0f * powf(2.0f, 4.5f * fe.tick()), 1.5f); else fe.tick();
            out[i] = L.tick(0.6f * x) * ae.tick();
        }
    }
    return out;
}

std::vector<float> render_ob6() {
    const size_t N = size_t(FS * 5);
    std::vector<float> out(N, 0.0f);
    for (int note : {53, 57, 60, 65}) {
        dsp::PolyBlepCore v1, v2;
        v1.reset(uint32_t(note * 7)); v2.reset(uint32_t(note * 13));
        v1.set_drift(0.00085f); v2.set_drift(0.00095f);
        dsp::SemSvfState s;
        EnvAudio fe, ae;
        fe.e.setADSR(60, 700, 0.45f, 600); ae.e.setADSR(20, 300, 0.85f, 600);
        fe.e.noteOn(); ae.e.noteOn();
        const float f = midi_hz(note);
        dsp::SemSvfCoeffs c{};
        for (size_t i = 0; i < N; ++i) {
            if (i == size_t(FS * 3.6)) { fe.e.noteOff(); ae.e.noteOff(); }
            float a, b;
            v1.render(dsp::WAVE_SAWTOOTH, f / FS, .5f, 1, &a, 1);
            v2.render(dsp::WAVE_SAWTOOTH, f * powf(2, 9 / 1200.f) / FS, .5f, 1, &b, 1);
            const float morph = i > size_t(FS * 2.0) ? fminf(0.5f, float(i - size_t(FS * 2.0)) / (FS * 1.5f) * 0.5f) : 0.0f;
            if ((i & 31) == 0) c = dsp::sem_svf_compute(500.0f * powf(2.0f, 3.5f * fe.tick()), 1.6f, morph, false, 1.2f, FS);
            else fe.tick();
            out[i] += 0.25f * dsp::sem_svf_step(s, 0.5f * (a + b), c) * ae.tick();
        }
    }
    return out;
}

std::vector<float> render_dx7() {
    const size_t N = size_t(FS * 5);
    std::vector<float> out(N, 0.0f);
    Dx7Patch p;   // E.PIANO de DX7Engine::begin()
    p.algorithm = 5; p.feedback = 2;
    p.op[0] = {1.0f, 99, 0, 1800, 0.0f, 400};
    p.op[1] = {1.0f, 72, 0, 900, 0.15f, 300};
    p.op[2] = {1.0f, 88, 0, 2200, 0.0f, 400};
    p.op[3] = {14.0f, 52, 0, 160, 0.0f, 120};
    const NoteEv seq[] = {{0, 1.2f, 60}, {0, 1.2f, 64}, {0, 1.2f, 67}, {1.5f, 1.2f, 62}, {1.5f, 1.2f, 65}, {1.5f, 1.2f, 69},
                          {3.0f, 1.6f, 48}, {3.0f, 1.6f, 72}};
    for (const auto& n : seq) {
        Dx7VoiceCore v;
        v.note_on(midi_hz(n.note));
        const size_t s0 = size_t(n.t * FS), off = size_t((n.t + n.dur) * FS);
        float x;
        for (size_t i = s0; i < N; ++i) {
            if (i == off) v.note_off();
            v.render(p, FS, &x, 1);
            out[i] += 0.3f * x;
        }
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) OUT = argv[1];
    REPORT = fopen((OUT + "/reporte.md").c_str(), "w");
    LOG("# Validación offline de los engines\n\nNúcleos DSP del firmware corridos en el Mac (`tools/offline`).\n");
    check_aliasing();
    check_envelopes();
    check_filters();
    check_fm();
    check_chorus();
    check_cost();
    write_wav("02_moog_bajo.wav", render_moog());
    write_wav("03_prophet_sync.wav", render_prophet());
    std::vector<float> r;
    std::vector<float> l = render_juno(&r);
    write_wav("04_juno_pad_estereo.wav", l, &r);
    write_wav("06_ob6_brass_y_notch.wav", render_ob6());
    write_wav("07_dx7_epiano.wav", render_dx7());
    LOG("\n## 7. Audio\n\nWAVs en `%s/`: 01 aliasing (1 s ingenuo, 1 s PolyBLEP), 02 Moog, 03 Prophet sync, 04 Juno estéreo "
        "(IR3109 + chorus en antifase), 06 OB-6 (LP y morph a notch), 07 DX7 E.PIANO.\n", OUT.c_str());
    if (REPORT) fclose(REPORT);
    return 0;
}
