// Tests del ladder ZDF (theory/fidelidad-engines.md §9.1): afinación de la resonancia, pendiente, carácter y
// estabilidad. Reemplazan a los del modelo D'Angelo, que no podía auto-oscilar sobre ~1.65 kHz.
#include <initializer_list>
#include <math.h>
#include <unity.h>

#include "dsp/fast_math.h"
#include "dsp/zdf_ladder.h"

void setUp() {}
void tearDown() {}

namespace {
constexpr float FS = 44100.0f;

double self_osc_hz(float fc, dsp::LadderCharacter ch, float* amp) {
    dsp::ZdfLadder f;
    f.set_character(ch);
    f.set_cutoff(fc, FS);
    f.set_k(4.4f);
    float y = f.tick(0.01f), prev = y, pk = 0;
    int zc = 0, first = -1, last = -1;
    for (int i = 0; i < int(3 * FS); ++i) {
        y = f.tick(0.0f);
        if (i > int(2 * FS)) {
            pk = fmaxf(pk, fabsf(y));
            if (prev < 0 && y >= 0) { if (first < 0) first = i; last = i; ++zc; }
        }
        prev = y;
    }
    *amp = pk;
    return zc > 1 ? (zc - 1) * FS / double(last - first) : 0.0;
}

double tone_db(float fc, float k, float f, dsp::LadderCharacter ch = dsp::LadderCharacter::Moog) {
    dsp::ZdfLadder L;
    L.set_character(ch);
    L.set_cutoff(fc, FS);
    L.set_k(k);
    double a = 0, b = 0;
    for (int i = 0; i < int(FS); ++i) {
        const float x = 0.01f * sinf(6.2831853f * f * i / FS);
        const float y = L.tick(x);
        if (i > int(FS / 2)) { a += x * x; b += y * y; }
    }
    return 10.0 * log10(b / a);
}
}  // namespace

void test_pade_contra_tanhf() {
    for (float x = -4.0f; x <= 4.0f; x += 0.001f) TEST_ASSERT_FLOAT_WITHIN(2e-5f, tanhf(x), dsp::fast_tanh(x));
}

void test_resonancia_afinada_en_todo_el_rango() {
    for (auto ch : {dsp::LadderCharacter::Moog, dsp::LadderCharacter::Ir3109, dsp::LadderCharacter::Cem3320})
        for (float fc : {110.0f, 440.0f, 1760.0f, 5000.0f, 12000.0f}) {
            float amp;
            const double f = self_osc_hz(fc, ch, &amp);
            TEST_ASSERT_TRUE(f > 0);
            // ±6 cents hasta 6 kHz (el modelo anterior: −1913 a 5 kHz); la saturación por etapa corre el tono unos cents
            // hacia arriba con la amplitud, como en el original (±12 a 12 kHz)
            TEST_ASSERT_TRUE(fabs(1200.0 * log2(f / fc)) < (fc > 6000.0f ? 12.0 : 6.0));
            TEST_ASSERT_TRUE(amp > 0.25f && amp < 1.0f);           // canta con amplitud útil y acotada
        }
}

void test_pendiente_24db_y_corte() {
    TEST_ASSERT_FLOAT_WITHIN(0.5, -12.0, tone_db(1000, 0, 1000));   // 4 polos: −3 dB cada uno en fc
    const double slope = (tone_db(1000, 0, 8000) - tone_db(1000, 0, 2000)) / 2.0;
    TEST_ASSERT_TRUE(slope < -20.0 && slope > -25.0);
}

void test_caracter_compensa_graves() {
    // Con resonancia, el Moog pierde graves (1 / (1 + k)); el IR3109 y el CEM3320 compensan en parte
    const double moog = tone_db(1000, 3.5f, 100, dsp::LadderCharacter::Moog);
    const double juno = tone_db(1000, 3.5f, 100, dsp::LadderCharacter::Ir3109);
    const double prophet = tone_db(1000, 3.5f, 100, dsp::LadderCharacter::Cem3320);
    TEST_ASSERT_TRUE(juno > moog + 3.0);
    TEST_ASSERT_TRUE(prophet > juno);
}

void test_estable_con_barrido_y_senal_fuerte() {
    dsp::ZdfLadder L;
    float mx = 0;
    for (int i = 0; i < int(5 * FS); ++i) {
        L.set_cutoff(50.0f + 18000.0f * (0.5f + 0.5f * sinf(i * 0.0003f)), FS);
        L.set_k(4.4f * (0.5f + 0.5f * sinf(i * 0.00007f)));
        const float y = L.tick((i % 100) < 50 ? 1.5f : -1.5f);
        TEST_ASSERT_FALSE(isnan(y) || isinf(y));
        mx = fmaxf(mx, fabsf(y));
    }
    TEST_ASSERT_TRUE(mx < 2.0f);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pade_contra_tanhf);
    RUN_TEST(test_resonancia_afinada_en_todo_el_rango);
    RUN_TEST(test_pendiente_24db_y_corte);
    RUN_TEST(test_caracter_compensa_graves);
    RUN_TEST(test_estable_con_barrido_y_senal_fuerte);
    return UNITY_END();
}
