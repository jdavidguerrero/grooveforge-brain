// Tests del FM de 4 operadores (sprints/46-dx7-4-operadores.md §5).
#include <math.h>
#include <unity.h>

#include "engines/dx7_core.h"

void setUp() {}
void tearDown() {}

namespace {
constexpr float FS = 44100.0f;
constexpr int N = 4410;
float buf[N], ref[N];

int rising_zero_crossings(const float* x, int n) {
    int c = 0;
    for (int i = 1; i < n; ++i) c += x[i - 1] < 0.0f && x[i] >= 0.0f;
    return c;
}
}  // namespace

void test_ocho_algoritmos_finitos_con_feedback_maximo() {
    for (int alg = 1; alg <= 8; ++alg) {
        Dx7Patch p;
        p.algorithm = uint8_t(alg);
        p.feedback = 7;
        Dx7VoiceCore v;
        v.note_on(220.0f);
        v.render(p, FS, buf, N);
        for (int i = 0; i < N; ++i) {
            TEST_ASSERT_FALSE(isnan(buf[i]) || isinf(buf[i]));
            TEST_ASSERT_TRUE(fabsf(buf[i]) <= 1.01f);
        }
    }
}

void test_algoritmo_8_op1_solo_es_un_seno_a_la_nota() {
    Dx7Patch p;
    p.algorithm = 8;
    for (int k = 1; k < DX7_OPS; ++k) p.op[k].level = 0;
    Dx7VoiceCore v;
    v.note_on(441.0f);
    v.render(p, FS, buf, N);
    TEST_ASSERT_INT_WITHIN(1, 44, rising_zero_crossings(buf, N));
}

void test_modulador_en_cero_no_cambia_la_portadora() {
    Dx7Patch a, b;
    a.algorithm = b.algorithm = 1;
    for (int k = 1; k < DX7_OPS; ++k) a.op[k].level = 0;
    b = a;
    b.op[1].ratio = 3.0f;                 // cambia el modulador apagado: no debe oírse
    Dx7VoiceCore va, vb;
    va.note_on(330.0f);
    vb.note_on(330.0f);
    va.render(a, FS, buf, N);
    vb.render(b, FS, ref, N);
    for (int i = 0; i < N; ++i) TEST_ASSERT_FLOAT_WITHIN(1e-6f, buf[i], ref[i]);
}

void test_release_apaga_la_voz() {
    Dx7Patch p;
    for (auto& o : p.op) o.release_ms = 20.0f;
    Dx7VoiceCore v;
    v.note_on(220.0f);
    v.render(p, FS, buf, 441);
    TEST_ASSERT_TRUE(v.active());
    v.note_off();
    v.render(p, FS, buf, N);              // 100 ms > 20 ms de release
    TEST_ASSERT_FALSE(v.active());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, buf[N - 1]);
}

void test_portadoras_por_algoritmo() {
    TEST_ASSERT_TRUE(dx7_is_carrier(5, 0));
    TEST_ASSERT_TRUE(dx7_is_carrier(5, 2));
    TEST_ASSERT_FALSE(dx7_is_carrier(5, 1));
    TEST_ASSERT_TRUE(dx7_is_carrier(8, 3));
    TEST_ASSERT_FALSE(dx7_is_carrier(1, 3));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ocho_algoritmos_finitos_con_feedback_maximo);
    RUN_TEST(test_algoritmo_8_op1_solo_es_un_seno_a_la_nota);
    RUN_TEST(test_modulador_en_cero_no_cambia_la_portadora);
    RUN_TEST(test_release_apaga_la_voz);
    RUN_TEST(test_portadoras_por_algoritmo);
    return UNITY_END();
}
