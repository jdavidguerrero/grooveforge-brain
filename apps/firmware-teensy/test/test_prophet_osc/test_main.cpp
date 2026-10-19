// Tests del oscilador del Prophet-5 (theory/prophet-5-completo.md §6): salida sana, polyBLEP, sync y POLY-MOD.
#include <math.h>
#include <unity.h>

#include "engines/prophet_osc_core.h"

void setUp() {}
void tearDown() {}

namespace {
constexpr float FS = 44100.0f;
constexpr int N = 4410;   // 100 ms
float buf[N];

int rising_zero_crossings(const float* x, int n) {
    int c = 0;
    for (int i = 1; i < n; ++i) c += x[i - 1] < 0.0f && x[i] >= 0.0f;
    return c;
}
}  // namespace

void test_exp2_rapido() {
    for (float x = -6.0f; x <= 6.0f; x += 0.37f) TEST_ASSERT_FLOAT_WITHIN(powf(2.0f, x) * 1e-3f, powf(2.0f, x), prophet_fast_exp2(x));
}

void test_salida_finita_en_todas_las_combinaciones() {
    for (int mask = 0; mask < 64; ++mask) {
        Prophet5OscCore core;
        Prophet5OscParams p;
        p.inc_a = 440.0f / FS;
        p.inc_b = 663.0f / FS;
        p.a_saw = mask & 1; p.a_pulse = mask & 2; p.b_saw = mask & 4; p.b_tri = mask & 8; p.b_pulse = mask & 16;
        p.sync = mask & 32;
        p.pm_b = 1.0f; p.pm_env = 1.0f; p.env = 1.0f; p.pm_freq_a = true; p.pm_pw_a = true;
        p.lvl_a = 1.0f; p.lvl_b = 1.0f; p.lvl_n = 1.0f;
        core.render(p, buf, N);
        for (int i = 0; i < N; ++i) {
            TEST_ASSERT_FALSE(isnan(buf[i]) || isinf(buf[i]));
            TEST_ASSERT_TRUE(fabsf(buf[i]) <= 3.5f);
        }
    }
}

void test_saw_con_polyblep_en_frecuencia() {
    Prophet5OscCore core;
    Prophet5OscParams p;
    p.inc_a = 441.0f / FS;
    p.lvl_a = 1.0f; p.lvl_b = 0.0f;
    core.render(p, buf, N);
    const int zc = rising_zero_crossings(buf, N);
    TEST_ASSERT_INT_WITHIN(2, 44, zc);       // 441 Hz en 100 ms
    float maxv = 0.0f;
    for (int i = 0; i < N; ++i) maxv = fmaxf(maxv, fabsf(buf[i]));
    TEST_ASSERT_TRUE(maxv <= 1.05f);          // el BLEP no desborda
}

void test_poly_mod_env_sube_la_frecuencia_de_a() {
    Prophet5OscCore core;
    Prophet5OscParams p;
    p.inc_a = 220.0f / FS;
    p.lvl_a = 1.0f; p.lvl_b = 0.0f;
    p.pm_env = 0.25f; p.env = 1.0f; p.pm_freq_a = true;   // +1 octava (0.25 · 4)
    core.render(p, buf, N);
    TEST_ASSERT_INT_WITHIN(2, 44, rising_zero_crossings(buf, N));
}

void test_sync_reinicia_a_con_b() {
    Prophet5OscCore core;
    Prophet5OscParams p;
    p.inc_a = 1000.0f / FS;
    p.inc_b = 100.0f / FS;
    p.sync = true;
    p.lvl_a = 1.0f; p.lvl_b = 0.0f;
    float prev_b = 0.0f;
    for (int k = 0; k < 2000; ++k) {
        core.render(p, buf, 1);
        if (core.phase_b() < prev_b) TEST_ASSERT_TRUE(core.phase_a() < 0.05f);   // B dio la vuelta → A casi en 0
        prev_b = core.phase_b();
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_exp2_rapido);
    RUN_TEST(test_salida_finita_en_todas_las_combinaciones);
    RUN_TEST(test_saw_con_polyblep_en_frecuencia);
    RUN_TEST(test_poly_mod_env_sube_la_frecuencia_de_a);
    RUN_TEST(test_sync_reinicia_a_con_b);
    return UNITY_END();
}
