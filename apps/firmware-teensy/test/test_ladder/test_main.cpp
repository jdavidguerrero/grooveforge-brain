// Tests del paso 1 de theory/fidelidad-engines.md: tanh de Padé y ladder equivalente a la versión con tanhf.
#include <math.h>
#include <initializer_list>
#include <unity.h>

#include "dsp/fast_math.h"
#include "dsp/ladder_core.h"

void setUp() {}
void tearDown() {}

namespace {
float ref_tanh(float x) { return tanhf(x); }
constexpr float FS = 44100.0f, VT = 0.026f, INV2VT = 1.0f / (2.0f * VT), DRIVE = 0.1f;

float coef_g(float hz) {
    const float pi_fc = 3.14159265f * hz / FS;
    return 4.0f * FS * (pi_fc * (1.0f - pi_fc) / (1.0f + pi_fc)) * VT;
}
}  // namespace

void test_pade_contra_tanhf() {
    float max_err = 0.0f;
    for (float x = -8.0f; x <= 8.0f; x += 0.001f) max_err = fmaxf(max_err, fabsf(dsp::fast_tanh(x) - tanhf(x)));
    TEST_ASSERT_TRUE(max_err < 3e-4f);   // máximo en la zona del recorte (|x| ≈ 5)
    for (float x = -4.0f; x <= 4.0f; x += 0.001f) TEST_ASSERT_FLOAT_WITHIN(2e-5f, tanhf(x), dsp::fast_tanh(x));
}

void test_ladder_rapido_igual_al_de_referencia() {
    for (float k : {0.0f, 2.0f, 3.6f}) {
        dsp::LadderState a, b;
        const float g = coef_g(1200.0f);
        double err = 0.0, sig = 0.0;
        float ph = 0.0f;
        for (int i = 0; i < 44100; ++i) {                 // 1 s de saw de 110 Hz
            ph += 110.0f / FS;
            if (ph >= 1.0f) ph -= 1.0f;
            const float vin = (2.0f * ph - 1.0f) * DRIVE;
            const float ya = dsp::ladder_step<dsp::fast_tanh>(a, vin, g, k, INV2VT, 0.5f / FS);
            const float yb = dsp::ladder_step<ref_tanh>(b, vin, g, k, INV2VT, 0.5f / FS);
            err += double(ya - yb) * double(ya - yb);
            sig += double(yb) * double(yb);
        }
        TEST_ASSERT_TRUE(10.0 * log10(err / sig) < -60.0);   // diferencia < −60 dB
    }
}

void test_auto_oscila_con_k4() {
    dsp::LadderState s;
    const float g = coef_g(800.0f);
    float peak = 0.0f;
    float y = dsp::ladder_step<dsp::fast_tanh>(s, 0.01f * DRIVE, g, 4.0f, INV2VT, 0.5f / FS);   // impulso chico
    for (int i = 0; i < 44100; ++i) {
        y = dsp::ladder_step<dsp::fast_tanh>(s, 0.0f, g, 4.0f, INV2VT, 0.5f / FS);
        if (i > 22050) peak = fmaxf(peak, fabsf(y));
    }
    TEST_ASSERT_FALSE(isnan(y));
    TEST_ASSERT_TRUE(peak > 0.01f);   // sigue oscilando sin entrada
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pade_contra_tanhf);
    RUN_TEST(test_ladder_rapido_igual_al_de_referencia);
    RUN_TEST(test_auto_oscila_con_k4);
    return UNITY_END();
}
