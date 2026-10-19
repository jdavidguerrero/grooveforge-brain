// Tests de la envolvente RC y del chorus estéreo (theory/fidelidad-engines.md §9.2–9.3).
#include <math.h>
#include <unity.h>

#include "dsp/analog_env.h"
#include "dsp/juno_chorus_core.h"

void setUp() {}
void tearDown() {}

void test_ataque_carga_rc_hacia_1_5() {
    dsp::AnalogEnv e;
    e.setADSR(100, 200, 0.5f, 300);
    e.noteOn();
    float a50 = 0, a100 = 0;
    for (int t = 1; t <= 101; ++t) { const float v = e.update(1); if (t == 50) a50 = v; if (t == 100) a100 = v; }
    TEST_ASSERT_FLOAT_WITHIN(0.03f, 0.634f, a50);    // RC hacia 1.5: 1.5·(1 − 3^(−1/2))
    TEST_ASSERT_TRUE(a100 > 0.97f);                  // llega al pico en el tiempo pedido
}

void test_decay_y_release_exponenciales() {
    dsp::AnalogEnv e;
    e.setADSR(0, 200, 0.5f, 300);
    e.noteOn();
    e.update(1);
    float d = 0;
    for (int t = 1; t <= 100; ++t) d = e.update(1);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 0.5f + 0.5f * expf(-100.0f / (200.0f / 4.6f)), d);
    e.update(400);
    e.noteOff();
    float r = 0;
    for (int t = 1; t <= 300; ++t) r = e.update(1);
    TEST_ASSERT_TRUE(r < 0.5f * 0.012f);   // al 1 % en el tiempo de release
}

void test_redisparo_sin_salto() {
    dsp::AnalogEnv e;
    e.setADSR(50, 200, 0.6f, 300);
    e.noteOn();
    e.update(400);                      // en sustain (0.6)
    const float before = e.level();
    e.noteOn();
    const float after = e.update(1);
    TEST_ASSERT_TRUE(after >= before);  // sigue subiendo desde donde estaba, no vuelve a 0
    TEST_ASSERT_TRUE(after - before < 0.05f);
}

void test_vca_por_muestra_continuo() {
    dsp::AnalogEnv e;
    e.set_rate(44100.0f);
    e.setADSR(5, 100, 0.7f, 200);
    e.noteOn();
    float prev = 0, max_jump = 0;
    for (int i = 0; i < 44100; ++i) {
        if (i == 22050) e.noteOff();
        const float v = e.step();
        max_jump = fmaxf(max_jump, fabsf(v - prev));
        prev = v;
    }
    TEST_ASSERT_TRUE(max_jump < 0.01f);   // sin escalones audibles
}

void test_chorus_estereo_en_antifase() {
    dsp::JunoChorusCore c;
    c.reset();
    c.setMode(1);
    static float in[44100], l[44100], r[44100];
    for (int i = 0; i < 44100; ++i) in[i] = sinf(6.2831853f * 440.0f * i / 44100.0f);
    c.render_stereo(in, l, r, 44100, 44100.0f);
    double diff = 0;
    for (int i = 22050; i < 44100; ++i) diff += fabs(l[i] - r[i]);
    TEST_ASSERT_TRUE(diff / 22050.0 > 0.05);   // L y R distintos: hay ancho estéreo
    c.reset();
    c.setMode(0);
    c.render_stereo(in, l, r, 1000, 44100.0f);
    for (int i = 0; i < 1000; ++i) TEST_ASSERT_EQUAL_FLOAT(l[i], r[i]);   // chorus apagado: mono
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ataque_carga_rc_hacia_1_5);
    RUN_TEST(test_decay_y_release_exponenciales);
    RUN_TEST(test_redisparo_sin_salto);
    RUN_TEST(test_vca_por_muestra_continuo);
    RUN_TEST(test_chorus_estereo_en_antifase);
    return UNITY_END();
}
