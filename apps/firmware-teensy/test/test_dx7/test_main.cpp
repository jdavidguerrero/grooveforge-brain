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

// fidelidad §11.1: el decay baja lineal en dB y cae 60 dB en el tiempo pedido.
void test_envolvente_lineal_en_db() {
    Dx7Patch p;
    p.algorithm = 8;
    p.op[0] = {1.0f, 99, 0.0f, 500.0f, 0.0f, 300.0f};
    Dx7VoiceCore v;
    v.note_on(220.0f);
    v.render(p, FS, buf, 2);                                   // ataque instantáneo
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, v.eg_db(0));
    const int half = int(0.25f * FS);                          // 250 ms = mitad del decay
    for (int done = 0; done < half; done += N) v.render(p, FS, buf, half - done < N ? half - done : N);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 30.0f, v.eg_db(0));         // 30 dB a mitad de camino: lineal en dB
    for (int done = 0; done < half; done += N) v.render(p, FS, buf, half - done < N ? half - done : N);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 60.0f, v.eg_db(0));
}

void test_ataque_y_d2_rate() {
    Dx7Patch p;
    p.algorithm = 8;
    p.op[0] = {1.0f, 99, 100.0f, 50.0f, 1.0f, 300.0f};
    p.d2_ms = 1000.0f;
    Dx7VoiceCore v;
    v.note_on(220.0f);
    v.render(p, FS, buf, int(0.05f * FS));                    // mitad del ataque: ya muy arriba en dB
    TEST_ASSERT_TRUE(v.eg_db(0) < 50.0f && v.eg_db(0) > 0.0f);
    for (int k = 0; k < 2; ++k) v.render(p, FS, buf, N);      // ataque terminado, sustain 1 (0 dB)
    const float before = v.eg_db(0);
    for (int k = 0; k < 5; ++k) v.render(p, FS, buf, N);      // D2: 60 dB por segundo, 0.5 s → +30 dB
    TEST_ASSERT_FLOAT_WITHIN(2.0f, before + 30.0f, v.eg_db(0));
}

// W1–W8 del TX81Z: W1 y W2 simétricas; W3–W8 en cero en la segunda mitad del ciclo.
void test_ocho_formas_tx81z() {
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, dx7_wave(0, 0.25f));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -1.0f, dx7_wave(1, 0.75f));
    TEST_ASSERT_FLOAT_WITHIN(2e-3f, 0.25f, dx7_wave(1, 1.0f / 12.0f));  // W2: sin(30°)·|sin(30°)| = 0.25
    for (int w = 2; w < DX7_WAVES; ++w) {
        TEST_ASSERT_EQUAL_FLOAT(0.0f, dx7_wave(w, 0.6f));
        TEST_ASSERT_EQUAL_FLOAT(0.0f, dx7_wave(w, 0.95f));
    }
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, dx7_wave(4, 0.125f));          // W5: seno ×2 en la primera mitad
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1.0f, dx7_wave(6, 0.375f));          // W7: |seno ×2|
    TEST_ASSERT_TRUE(dx7_wave(7, 0.375f) > 0.0f);                       // W8: siempre ≥ 0
}

void test_detune_y_escalado_de_teclado() {
    Dx7Patch p;
    p.algorithm = 8;
    for (int k = 1; k < DX7_OPS; ++k) p.op[k].level = 0;
    p.op[0].detune = 3;                                        // +6 ¢
    Dx7VoiceCore v;
    v.note_on(1000.0f);
    v.render(p, FS, buf, N);
    // 0.1 s a 1000 Hz · 2^(6/1200) ≈ 100.35 ciclos
    const int zc = rising_zero_crossings(buf, N);
    TEST_ASSERT_TRUE(zc >= 99 && zc <= 101);

    // KBD LEVEL atenúa al modulador en el agudo: menos índice → menos energía fuera de la fundamental
    Dx7Patch fm;
    fm.algorithm = 1;
    fm.feedback = 0;
    fm.op[2].level = fm.op[3].level = 0;
    fm.op[1] = {1.0f, 99, 0.0f, 0.0f, 1.0f, 300.0f};
    Dx7Patch scaled = fm;
    scaled.kbd_level = 99;
    Dx7VoiceCore a, b;
    a.note_on(1760.0f);                                         // 3 octavas sobre C3
    b.note_on(1760.0f);
    a.render(fm, FS, buf, N);
    b.render(scaled, FS, ref, N);
    // con −36 dB en el modulador la salida es casi un seno: mismos cruces que la portadora sola (1760 / 10)
    TEST_ASSERT_INT_WITHIN(2, 176, rising_zero_crossings(ref, N));
    TEST_ASSERT_TRUE(rising_zero_crossings(buf, N) != rising_zero_crossings(ref, N));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ocho_algoritmos_finitos_con_feedback_maximo);
    RUN_TEST(test_algoritmo_8_op1_solo_es_un_seno_a_la_nota);
    RUN_TEST(test_modulador_en_cero_no_cambia_la_portadora);
    RUN_TEST(test_release_apaga_la_voz);
    RUN_TEST(test_portadoras_por_algoritmo);
    RUN_TEST(test_envolvente_lineal_en_db);
    RUN_TEST(test_ataque_y_d2_rate);
    RUN_TEST(test_ocho_formas_tx81z);
    RUN_TEST(test_detune_y_escalado_de_teclado);
    return UNITY_END();
}
