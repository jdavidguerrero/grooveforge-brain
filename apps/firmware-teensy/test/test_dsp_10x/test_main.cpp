// Tests nativos de los núcleos DSP 10/10: PolyBlepCore, AnalogEnv, SemSvfCore, JunoChorusCore y Dx7VoiceCore.
#include <math.h>
#include <unity.h>

#include "dsp/analog_env.h"
#include "dsp/juno_chorus_core.h"
#include "dsp/polyblep_core.h"
#include "dsp/sem_svf_core.h"
#include "engines/dx7_core.h"

void setUp() {}
void tearDown() {}

void test_polyblep_waves_bounded_and_zero_dc() {
    dsp::PolyBlepCore osc;

    const uint16_t waves[5] = {
        dsp::WAVE_SAWTOOTH,
        dsp::WAVE_SQUARE,
        dsp::WAVE_TRIANGLE,
        dsp::WAVE_SINE,
        dsp::WAVE_SAW_AND_PULSE
    };

    for (int w = 0; w < 5; ++w) {
        osc.reset();
        float sum = 0.0f;
        float peak = 0.0f;
        constexpr int BLOCK = 128;
        constexpr int BLOCKS = 300;
        float buf[BLOCK];
        for (int b = 0; b < BLOCKS; ++b) {
            osc.render(waves[w], 440.0f / 44100.0f, 0.5f, 1.0f, buf, BLOCK);
            for (int i = 0; i < BLOCK; ++i) {
                sum += buf[i];
                if (fabsf(buf[i]) > peak) peak = fabsf(buf[i]);
            }
        }
        // Debe producir señal activa acotada y sin DC offset excesivo
        TEST_ASSERT_TRUE(peak > 0.5f && peak <= 1.15f);
        TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, sum / (BLOCK * BLOCKS));
    }
}

void test_analog_env_release_sin_salto_y_legato() {
    dsp::AnalogEnv env;
    env.setADSR(10.0f, 500.0f, 0.2f, 300.0f);

    // Disparar nota y avanzar 20 ms (entra en DECAY cerca de 0.90, muy por encima de sustain = 0.2)
    env.noteOn(false);
    float lvl_before = 0.0f;
    for (int i = 0; i < 20; ++i) lvl_before = env.update(1);
    TEST_ASSERT_TRUE(lvl_before > 0.8f);

    // Soltar nota antes de llegar a sustain: NO debe saltar a 0.20, debe caer suavemente desde lvl_before
    env.noteOff();
    float lvl_after = env.update(1);
    TEST_ASSERT_FLOAT_WITHIN(0.03f, lvl_before, lvl_after);

    // Single-trigger legato: si ya está activa, noteOn(true) no reinicia a 0
    env.noteOn(false);
    for (int i = 0; i < 15; ++i) lvl_before = env.update(1);
    env.noteOn(true);
    lvl_after = env.update(1);
    TEST_ASSERT_TRUE(lvl_after > 0.7f);
}

void test_sem_svf_lp_notch_hp_bp() {
    auto measure_rms = [&](float freq_hz, float morph, bool bp) {
        dsp::SemSvfState st{};
        const dsp::SemSvfCoeffs c = dsp::sem_svf_compute(1000.0f, 1.4f, morph, bp, 1.0f, 44100.0f);
        float sum_sq = 0.0f;
        constexpr int WARM = 1000, N = 2000;
        for (int i = 0; i < WARM + N; ++i) {
            float x = sinf(2.0f * 3.14159265f * freq_hz * i / 44100.0f) * 0.5f;
            float y = dsp::sem_svf_step(st, x, c);
            if (i >= WARM) sum_sq += y * y;
        }
        return sqrtf(sum_sq / N);
    };

    // En LP (morph = 0.0): 200 Hz pasa fuerte, 8000 Hz se atenúa (12 dB/oct)
    float lp_low = measure_rms(200.0f, 0.0f, false);
    float lp_high = measure_rms(8000.0f, 0.0f, false);
    TEST_ASSERT_TRUE(lp_low > lp_high * 8.0f);

    // En HP (morph = 1.0): 8000 Hz pasa fuerte, 200 Hz se atenúa
    float hp_low = measure_rms(200.0f, 1.0f, false);
    float hp_high = measure_rms(8000.0f, 1.0f, false);
    TEST_ASSERT_TRUE(hp_high > hp_low * 8.0f);

    // En NOTCH (morph = 0.5): 1000 Hz (= fc) se rechaza fuertemente respecto a 200 Hz
    float notch_fc = measure_rms(1000.0f, 0.5f, false);
    float notch_low = measure_rms(200.0f, 0.5f, false);
    TEST_ASSERT_TRUE(notch_fc < notch_low * 0.25f);

    // En BP (bp_mode = true): 1000 Hz (= fc) domina sobre 200 Hz
    float bp_fc = measure_rms(1000.0f, 0.0f, true);
    float bp_low = measure_rms(200.0f, 0.0f, true);
    TEST_ASSERT_TRUE(bp_fc > bp_low * 2.5f);
}

void test_juno_chorus_bbd_and_hpf() {
    dsp::JunoChorusCore ch;
    ch.reset();

    // Modo I (0.513 Hz) produce modulación BBD limpia sobre un tono de 440 Hz
    ch.setMode(1);
    ch.setHpf(1); // FLAT
    float in_buf[128], out_buf[128];
    float diff_sum = 0.0f;
    for (int b = 0; b < 32; ++b) {
        for (int i = 0; i < 128; ++i) {
            int idx = b * 128 + i;
            in_buf[i] = sinf(2.0f * 3.14159265f * 440.0f * idx / 44100.0f) * 0.4f;
        }
        ch.render(in_buf, out_buf, 128, 44100.0f);
        for (int i = 0; i < 128; ++i) diff_sum += fabsf(out_buf[i] - in_buf[i]);
    }
    TEST_ASSERT_TRUE(diff_sum / 4096.0f > 0.02f);

    // HPF posición 3 (720 Hz) atenúa un grave de 100 Hz respecto a posición 0 (Bass Boost)
    auto measure_hpf = [&](uint8_t hpf_mode) {
        ch.reset();
        ch.setMode(0); // Chorus OFF para medir solo el HPF
        ch.setHpf(hpf_mode);
        float sum_sq = 0.0f;
        for (int b = 0; b < 16; ++b) {
            for (int i = 0; i < 128; ++i) {
                int idx = b * 128 + i;
                in_buf[i] = sinf(2.0f * 3.14159265f * 100.0f * idx / 44100.0f) * 0.4f;
            }
            ch.render(in_buf, out_buf, 128, 44100.0f);
            if (b >= 4) {
                for (int i = 0; i < 128; ++i) sum_sq += out_buf[i] * out_buf[i];
            }
        }
        return sqrtf(sum_sq / (12 * 128));
    };
    float rms_boost = measure_hpf(0);
    float rms_hpf3 = measure_hpf(3);
    TEST_ASSERT_TRUE(rms_boost > rms_hpf3 * 3.0f);
}

void test_dx7_velocity_mod_index() {
    Dx7Patch patch{};
    patch.algorithm = 0; // 4 -> 3 -> 2 -> 1
    patch.feedback = 0;
    for (int k = 0; k < 4; ++k) {
        patch.op[k] = {1.0f, (k < 2 ? 0.9f : 0.0f), 0.1f, 100.0f, 1.0f, 300.0f};
    }

    auto measure_peak = [&](float vel) {
        Dx7VoiceCore v;
        v.note_on(440.0f, vel);
        float buf[128];
        v.render(patch, 44100.0f, buf, 128);
        float pk = 0.0f;
        for (int i = 0; i < 128; ++i) {
            float a = fabsf(buf[i]);
            if (a > pk) pk = a;
        }
        return pk;
    };

    float pk_loud = measure_peak(1.0f);
    float pk_soft = measure_peak(0.25f);
    TEST_ASSERT_TRUE(pk_loud > pk_soft * 1.8f);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_polyblep_waves_bounded_and_zero_dc);
    RUN_TEST(test_analog_env_release_sin_salto_y_legato);
    RUN_TEST(test_sem_svf_lp_notch_hp_bp);
    RUN_TEST(test_juno_chorus_bbd_and_hpf);
    RUN_TEST(test_dx7_velocity_mod_index);
    return UNITY_END();
}
