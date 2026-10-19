// Tests del LFO (theory/lfo-modulacion.md §4): rango, formas, SYNC y S&H determinista.
#include <math.h>
#include <unity.h>

#include "dsp/lfo.h"

using dsp::Lfo;
using dsp::LfoShape;

void setUp() {}
void tearDown() {}

void test_rate_se_recorta_al_rango() {
    Lfo l;
    l.set_rate_hz(100.0f);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, l.rate_hz());
    l.set_rate_hz(0.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.05f, l.rate_hz());
}

void test_salida_bipolar_en_todas_las_formas() {
    for (uint8_t s = 0; s <= uint8_t(LfoShape::SampleHold); ++s) {
        Lfo l;
        l.set_shape(LfoShape(s));
        l.set_rate_hz(7.3f);
        float lo = 2.0f, hi = -2.0f;
        for (int i = 0; i < 5000; ++i) {
            const float v = l.tick(0.001f);
            TEST_ASSERT_FALSE(isnan(v));
            lo = v < lo ? v : lo;
            hi = v > hi ? v : hi;
        }
        TEST_ASSERT_TRUE(lo >= -1.0f && hi <= 1.0f);
        TEST_ASSERT_TRUE(hi - lo > 1.0f);    // recorre buena parte del rango
    }
}

void test_triangulo_pasa_por_los_extremos() {
    Lfo l;
    l.set_shape(LfoShape::Tri);
    l.set_rate_hz(1.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, l.tick(0.25f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -1.0f, l.tick(0.5f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, l.tick(0.25f));
}

void test_sync_sigue_el_bpm() {
    Lfo l;
    l.set_sync(true, 120.0f, 0.5f);          // 1/8 a 120 BPM = 4 Hz
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 4.0f, l.rate_hz());
    l.set_sync(false, 120.0f, 0.5f);
    l.set_rate_hz(2.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, l.rate_hz());
}

void test_sample_hold_cambia_solo_al_cerrar_el_ciclo() {
    Lfo a, b;
    a.set_shape(LfoShape::SampleHold);
    b.set_shape(LfoShape::SampleHold);
    a.set_rate_hz(1.0f);
    b.set_rate_hz(1.0f);
    const float v1 = a.tick(1.01f);          // cerró el primer ciclo: valor nuevo
    TEST_ASSERT_EQUAL_FLOAT(v1, a.tick(0.3f));   // dentro del ciclo se mantiene
    TEST_ASSERT_EQUAL_FLOAT(v1, b.tick(1.01f));  // determinista
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_rate_se_recorta_al_rango);
    RUN_TEST(test_salida_bipolar_en_todas_las_formas);
    RUN_TEST(test_triangulo_pasa_por_los_extremos);
    RUN_TEST(test_sync_sigue_el_bpm);
    RUN_TEST(test_sample_hold_cambia_solo_al_cerrar_el_ciclo);
    return UNITY_END();
}
