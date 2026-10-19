// Tercera vuelta de sonido (theory/fidelidad-engines.md §11.8): forma continua, hard sync, ruido rosa, pila mono.
#include <math.h>
#include <unity.h>

#include "dsp/color_noise_core.h"
#include "dsp/note_stack.h"
#include "dsp/output_stage_core.h"
#include "dsp/polyblep_core.h"

using namespace dsp;

void setUp() {}
void tearDown() {}

namespace {
constexpr int N = 4096;
float a[N], b[N], c[N];
}  // namespace

void test_forma_continua_extremos() {
    const float inc = 220.0f / 44100.0f;
    PolyBlepCore s, m, p;
    s.set_drift(0); m.set_drift(0); p.set_drift(0);
    s.render(WAVE_SAWTOOTH, inc, 0.3f, 1.0f, a, N);
    m.render(WAVE_SHAPE_MORPH, inc, 0.3f, 1.0f, b, N, 0.0f);
    for (int i = 0; i < N; ++i) TEST_ASSERT_FLOAT_WITHIN(1e-5f, a[i], b[i]);
    PolyBlepCore m1;
    m1.set_drift(0);
    p.render(WAVE_PULSE, inc, 0.3f, 1.0f, a, N);
    m1.render(WAVE_SHAPE_MORPH, inc, 0.3f, 1.0f, c, N, 1.0f);
    for (int i = 0; i < N; ++i) TEST_ASSERT_FLOAT_WITHIN(1e-5f, a[i], c[i]);
}

// El esclavo reinicia la fase en cada vuelta del maestro: su período pasa a ser el del maestro.
void test_hard_sync_sigue_al_maestro() {
    PolyBlepCore master, slave;
    master.set_drift(0);
    slave.set_drift(0);
    SyncEvents ev;
    int resets = 0, wraps = 0;
    for (int blk = 0; blk < 32; ++blk) {
        master.render(WAVE_SAWTOOTH, 100.0f / 44100.0f, 0.5f, 1.0f, a, 128, 0.0f, &ev);
        wraps += ev.n;
        slave.render(WAVE_SAWTOOTH, 270.0f / 44100.0f, 0.5f, 1.0f, b, 128, 0.0f, nullptr, &ev);
        resets += ev.n;
        for (int i = 0; i < 128; ++i) TEST_ASSERT_TRUE(fabsf(b[i]) <= 1.6f);
    }
    // 32 bloques de 128 muestras a 100 Hz ≈ 9.3 vueltas
    TEST_ASSERT_INT_WITHIN(1, 9, wraps);
    TEST_ASSERT_EQUAL_INT(wraps, resets);   // el esclavo recibe todas las vueltas
    // Con sync la fase del esclavo justo después de una vuelta es chica (fase desde el reinicio)
    PolyBlepCore m2, s2;
    m2.set_drift(0);
    s2.set_drift(0);
    SyncEvents e2;
    for (int blk = 0; blk < 4; ++blk) {
        m2.render(WAVE_SAWTOOTH, 100.0f / 44100.0f, 0.5f, 1.0f, a, 128, 0.0f, &e2);
        s2.render(WAVE_SAWTOOTH, 270.0f / 44100.0f, 0.5f, 1.0f, b, 128, 0.0f, nullptr, &e2);
        if (e2.n) {
            // el último reinicio pasó hace (127 − index) muestras: fase ≈ ese tiempo × inc del esclavo
            const float expect = (float(127 - e2.index[e2.n - 1]) + e2.frac[e2.n - 1]) * 270.0f / 44100.0f;
            TEST_ASSERT_FLOAT_WITHIN(0.02f, fmodf(expect, 1.0f), s2.phase());
        }
    }
}

// Rosa: menos energía en el agudo que el blanco, RMS parecido.
void test_ruido_rosa_inclinado() {
    ColorNoiseCore w, p;
    w.reset(42);
    p.reset(42);
    p.set_color(1.0f);
    w.render(a, N);
    p.render(b, N);
    double rw = 0, rp = 0, dw = 0, dp = 0;
    for (int i = 1; i < N; ++i) {
        rw += a[i] * a[i];
        rp += b[i] * b[i];
        dw += (a[i] - a[i - 1]) * (a[i] - a[i - 1]);   // energía de la diferencia ≈ agudos
        dp += (b[i] - b[i - 1]) * (b[i] - b[i - 1]);
    }
    TEST_ASSERT_TRUE(rp > 0.3 * rw && rp < 3.0 * rw);
    TEST_ASSERT_TRUE(dp / rp < 0.5 * (dw / rw));
}

void test_pila_mono_ultima_nota() {
    NoteStack s;
    TEST_ASSERT_FALSE(s.push(60));
    TEST_ASSERT_TRUE(s.push(64));
    TEST_ASSERT_TRUE(s.push(67));
    TEST_ASSERT_EQUAL_UINT8(67, s.top());
    TEST_ASSERT_EQUAL_UINT8(67, s.remove(64));            // soltar una de abajo no cambia la que suena
    TEST_ASSERT_EQUAL_UINT8(60, s.remove(67));            // soltar la de arriba vuelve a la anterior
    TEST_ASSERT_EQUAL_UINT8(NoteStack::NONE, s.remove(60));
    for (uint8_t n = 0; n < 12; ++n) s.push(uint8_t(40 + n));   // se llena: descarta la más vieja
    TEST_ASSERT_EQUAL_UINT8(NoteStack::MAX, s.size());
    TEST_ASSERT_EQUAL_UINT8(51, s.top());
}

// Etapa de salida (§11.6): quita el DC y no cambia la ganancia de señal chica.
void test_etapa_de_salida_quita_dc() {
    OutputStageCore o;
    o.set(8.0f, 1.2f, 44100.0f);
    float y = 0.0f;
    for (int i = 0; i < 44100; ++i) y = o.tick(0.5f);              // escalón de DC: a 1 s ya decayó
    TEST_ASSERT_TRUE(fabsf(y) < 0.01f);
    OutputStageCore s;
    s.set(8.0f, 1.2f, 44100.0f);
    float peak = 0.0f;
    for (int i = 0; i < 44100; ++i) {
        const float v = s.tick(0.01f * sinf(6.2831853f * 1000.0f * i / 44100.0f));
        if (i > 4410) peak = fmaxf(peak, fabsf(v));
    }
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 0.01f, peak);                // 1 kHz chico: pasa entero
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_forma_continua_extremos);
    RUN_TEST(test_hard_sync_sigue_al_maestro);
    RUN_TEST(test_ruido_rosa_inclinado);
    RUN_TEST(test_pila_mono_ultima_nota);
    RUN_TEST(test_etapa_de_salida_quita_dc);
    return UNITY_END();
}
