// Tests del mapeo de P1/P2 por página (theory/lfo-modulacion.md §6–7) y de las conversiones de gf_params.h.
#include <unity.h>

#include "ui/page_params.h"
#include "ui/studio_ui.h"

using namespace studio;

void setUp() {}
void tearDown() {}

namespace {
GF_UiState page(uint8_t p, bool shift = false, uint8_t flags = 0) {
    GF_UiState u{};
    u.page = p;
    u.shift = shift;
    u.flags = flags;
    return u;
}
}  // namespace

void test_perform_p1_env_amt_p2_lfo_cutoff() {
    Patch p;
    ParamChange c[1];
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, page(PERFORM), 1, 0, 5, c));
    TEST_ASSERT_EQUAL_HEX16(0x1122, c[0].id);                       // Juno, FILTER, ENV AMT
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.4f, c[0].norm);
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, page(PERFORM), 1, 1, 10, c));
    TEST_ASSERT_EQUAL_HEX16(0x1133, c[0].id);                       // LFO, cantidad CUTOFF
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.2f, p.lfo_amt[GF_DEST_CUTOFF]);
}

void test_lfo_cutoff_es_el_mismo_valor_en_perform_y_lfo() {
    Patch p;
    ParamChange c[1];
    edit_encoder(p, page(PERFORM), 0, 1, 10, c);
    edit_encoder(p, page(LFO), 0, 1, 5, c);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.3f, p.lfo_amt[GF_DEST_CUTOFF]);
}

void test_lfo_shift_pitch_y_tercer_destino_por_engine() {
    Patch p;
    ParamChange c[1];
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, page(LFO), 1, 3, 3, c));         // ENC R: Juno → PWM
    TEST_ASSERT_EQUAL_HEX16(0x1136, c[0].id);
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, page(LFO), 4, 3, 3, c));         // DX7 → AMP
    TEST_ASSERT_EQUAL_HEX16(0x1435, c[0].id);
    TEST_ASSERT_EQUAL_UINT8(0, edit_encoder(p, page(LFO), 0, 3, 3, c));         // Moog: sin 3.er destino
    TEST_ASSERT_EQUAL_UINT8(0, edit_encoder(p, page(LFO), 4, 1, 3, c));         // DX7: sin CUTOFF
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, page(LFO), 0, 2, 2, c));         // ENC L: PITCH en todos
    TEST_ASSERT_EQUAL_UINT8(0, edit_encoder(p, page(LFO, true), 0, 2, 2, c));   // capa SHIFT vacía
    TEST_ASSERT_EQUAL_HEX16(0x1034, c[0].id);
}

void test_lfo_p1_rate_libre_o_division_con_sync() {
    Patch p;
    ParamChange c[1];
    edit_encoder(p, page(LFO), 0, 0, 12, c);
    TEST_ASSERT_TRUE(p.lfo_rate_hz > 3.9f && p.lfo_rate_hz < 4.1f);  // 2 Hz · 1.06^12 ≈ 4.02
    TEST_ASSERT_EQUAL_HEX16(0x1030, c[0].id);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, p.lfo_rate_hz, gf_lfo_rate_hz(c[0].norm));
    edit_encoder(p, page(LFO, false, GF_UI_FLAG_LFO_SYNC), 0, 0, 1, c);
    TEST_ASSERT_EQUAL_UINT8(2, p.lfo_div);
    TEST_ASSERT_EQUAL_HEX16(0x1032, c[0].id);
    edit_encoder(p, page(LFO, false, GF_UI_FLAG_LFO_SYNC), 0, 0, 9, c);
    TEST_ASSERT_EQUAL_UINT8(3, p.lfo_div);                          // no se pasa de 1/8T
}

void test_env_cuatro_celdas_y_rangos() {
    Patch p;
    ParamChange c[1];
    edit_encoder(p, page(ENV), 2, 0, -100, c);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, p.attack_ms);
    TEST_ASSERT_EQUAL_HEX16(0x1210, c[0].id);
    edit_encoder(p, page(ENV), 2, 3, 100, c);                        // ENC R = RELEASE
    TEST_ASSERT_EQUAL_FLOAT(5000.0f, p.release_ms);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, c[0].norm);
    edit_encoder(p, page(ENV), 2, 2, -5, c);                         // ENC L = SUSTAIN
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.6f, p.sustain);
}

void test_fx_y_paginas_sin_encoders_no_editan() {
    Patch p;
    ParamChange c[1];
    GF_UiState fx = page(FX);
    fx.mode = 1;
    TEST_ASSERT_EQUAL_UINT8(0, edit_encoder(p, fx, 0, 0, 3, c));
    TEST_ASSERT_EQUAL_UINT8(0, edit_encoder(p, page(AI), 0, 0, 3, c));   // ARP RATE: fase 2
    TEST_ASSERT_EQUAL_UINT8(0, edit_encoder(p, page(PERFORM), 0, 2, 3, c));      // en home ENC L = filtro
    TEST_ASSERT_EQUAL_UINT8(0, edit_encoder(p, page(PERFORM, true), 0, 2, 3, c));
}

void test_perform_shift_glide_y_drive() {
    Patch p;
    patch_init(p, 0);
    ParamChange c[1];
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, page(PERFORM, true), 0, 0, 10, c));   // SHIFT + P1 = GLIDE
    TEST_ASSERT_EQUAL_HEX16(0x1023, c[0].id);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.2f, p.glide);
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, page(SPECTRUM, true), 0, 1, 5, c));   // SHIFT + P2 = DRIVE
    TEST_ASSERT_EQUAL_HEX16(0x1024, c[0].id);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.3f, p.drive);
}

void test_forma_sin_dar_la_vuelta_y_patch_completo() {
    Patch p;
    ParamChange c[PATCH_PARAM_COUNT];
    step_lfo_shape(p, 0, 10, c);
    TEST_ASSERT_EQUAL_UINT8(4, p.lfo_shape);
    TEST_ASSERT_EQUAL_UINT8(4, gf_index_from_norm(c[0].norm, GF_LFO_SHAPES));
    step_lfo_shape(p, 0, -10, c);
    TEST_ASSERT_EQUAL_UINT8(0, p.lfo_shape);
    const uint8_t n = patch_params(p, 3, c);
    TEST_ASSERT_EQUAL_UINT8(38 + 12 + 5, n);                         // OB-6: 38 base + 12 OSC + 5 X-MOD
    for (uint8_t i = 0; i < n; ++i) TEST_ASSERT_EQUAL_HEX8(0x13, c[i].id >> 8);
    TEST_ASSERT_TRUE(patch_params(p, 2, c) <= PATCH_PARAM_COUNT);    // Prophet: 3 paradas × 8 + MOD
}

void test_synth_fx_mix_principal_y_discretos() {
    Patch p;
    ParamChange c[1];
    GF_UiState u = page(SYNTH_FX);
    u.cursor = GF_SFX_TAPE;
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, u, 1, 2, 5, c));      // ENC L = MIX del TAPE (como el modo FX)
    TEST_ASSERT_EQUAL_HEX16(0x1145, c[0].id);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.7f, c[0].norm);
    edit_encoder(p, u, 1, 3, 50, c);                                 // ENC R = DRIVE al máximo
    TEST_ASSERT_EQUAL_HEX16(0x1153, c[0].id);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 10.0f, gf_sfx_drive(c[0].norm));
    u.cursor = GF_SFX_MODAL;
    edit_encoder(p, u, 1, 0, 1, c);                                  // P1 = MATERIAL: GUADUA → CRISTAL
    TEST_ASSERT_EQUAL_HEX16(0x115A, c[0].id);
    TEST_ASSERT_EQUAL_UINT8(2, gf_index_from_norm(c[0].norm, 4));
    u.cursor = GF_SFX_BIT;
    edit_encoder(p, u, 1, 3, -100, c);                               // ENC R = BITS: un valor por paso
    TEST_ASSERT_EQUAL_UINT8(16 - 6 + 1, gf_sfx_bits(c[0].norm));
}

void test_env_filter_y_osc() {
    Patch p;
    patch_init(p, 0);
    ParamChange c[1];
    GF_UiState u = page(ENV);
    u.cursor = GF_ENV_FILTER;
    edit_encoder(p, u, 0, 1, 3, c);                                  // P2 = DECAY del filtro
    TEST_ASSERT_EQUAL_HEX16(0x1015, c[0].id);
    TEST_ASSERT_EQUAL_FLOAT(300.0f, p.decay_ms);                     // el del amplificador no cambia
    TEST_ASSERT_TRUE(p.f_decay_ms > 400.0f);
    u = page(OSC);
    u.cursor = 1;                                                    // Moog VCO 2
    edit_encoder(p, u, 0, 1, 5, c);                                  // P2 = DETUNE +5 CT
    TEST_ASSERT_EQUAL_HEX16(0x1005, c[0].id);
    TEST_ASSERT_EQUAL_FLOAT(12.0f, p.osc[1][1]);
    edit_encoder(p, u, 0, 3, -6, c);                                 // ENC R = NOISE (del engine, oscilador 0)
    TEST_ASSERT_EQUAL_HEX16(0x1003, c[0].id);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, p.osc[0][3]);
    u.cursor = 0;
    TEST_ASSERT_EQUAL_UINT8(0, edit_encoder(p, u, 0, 1, 1, c));      // VCO 1 no tiene P2
    edit_encoder(p, u, 0, 0, 9, c);                                  // WAVE avanza de a uno
    TEST_ASSERT_EQUAL_FLOAT(1.0f, p.osc[0][0]);
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, u, 4, 0, 1, c));      // DX7: P1 = RATIO de OP 1 (sprint 46)
}

void test_prophet_osc_shift_mod_y_shape() {
    Patch p;
    patch_init(p, 2);
    ParamChange c[1];
    GF_UiState u = page(OSC);
    u.cursor = 0;                                                    // OSC A
    u.shift = 1;
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, u, 2, 0, 1, c));      // SHIFT P1 = SYNC → ON
    TEST_ASSERT_EQUAL_HEX16(0x1260, c[0].id);                        // grupo OSC_SHIFT, parada 0, slot 4
    TEST_ASSERT_EQUAL_FLOAT(1.0f, c[0].norm);
    u.cursor = 2;                                                    // MIXER
    edit_encoder(p, u, 2, 0, 1, c);                                  // SHIFT P1 = FILTER KBD: OFF → HALF
    TEST_ASSERT_EQUAL_FLOAT(1.0f, p.osc[2][4]);
    u = page(LFO);
    u.cursor = 1;                                                    // POLY-MOD
    edit_encoder(p, u, 2, 2, 1, c);                                  // ENC L = FREQ A → ON
    TEST_ASSERT_EQUAL_HEX16(0x1272, c[0].id);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, p.mod[0][2]);
    u.cursor = 0;
    u.shift = 1;
    edit_encoder(p, u, 2, 0, 1, c);                                  // SHIFT P1 en la parada LFO = SHAPE
    TEST_ASSERT_EQUAL_UINT8(2, p.lfo_shape);
    u.cursor = 1;
    u.shift = 0;
    TEST_ASSERT_EQUAL_UINT8(0, edit_encoder(p, u, 0, 0, 1, c));      // el Moog no tiene paradas de MOD
}

void test_nuevos_slots_juno_ob6_arp() {
    Patch p;
    ParamChange c[1];
    // Juno-106: WAVE hasta SAW+PLS (índice 3) y SHIFT P1 = CHORUS (0..3)
    patch_init(p, 1);
    GF_UiState u = page(OSC);
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, u, 1, 0, 1, c));      // PULSE (2) → SAW+PLS (3)
    TEST_ASSERT_EQUAL_FLOAT(3.0f, p.osc[0][0]);
    u.shift = 1;
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, u, 1, 0, 2, c));      // CHORUS I (1) → II (2)
    TEST_ASSERT_EQUAL_FLOAT(2.0f, p.osc[0][4]);
    // OB-6: SHIFT ENC L = FLT MORPH (global, slot 6) y X-MOD en LFO cursor 1
    patch_init(p, 3);
    u.cursor = 1;                                                    // VCO 2
    u.shift = 1;
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, u, 3, 2, 25, c));     // SHIFT ENC L = FLT MORPH +50%
    TEST_ASSERT_EQUAL_FLOAT(50.0f, p.osc[0][6]);                     // guardado en oscilador 0 (global)
    u = page(LFO);
    u.cursor = 1;                                                    // OB-6 X-MOD
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, u, 3, 3, 1, c));      // ENC R = FLT MODE → ON
    TEST_ASSERT_EQUAL_FLOAT(1.0f, p.mod[0][3]);
    // ARP 2600: P2 en VCO 1 = PW, SHIFT P1 = VCO 1 LEVEL
    patch_init(p, 5);
    u = page(OSC);
    u.cursor = 0;
    TEST_ASSERT_EQUAL_UINT8(1, edit_encoder(p, u, 5, 1, 5, c));      // P2 = PW 50 → 60%
    TEST_ASSERT_EQUAL_FLOAT(60.0f, p.osc[0][1]);
}

void test_synth_fx_encendido() {
    Patch p;
    ParamChange c[1];
    TEST_ASSERT_EQUAL_UINT8(1, toggle_sfx(p, 2, GF_SFX_CHORUS, c));
    TEST_ASSERT_EQUAL_HEX16(0x1242, c[0].id);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, c[0].norm);
    toggle_sfx(p, 2, GF_SFX_CHORUS, c);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, c[0].norm);
    TEST_ASSERT_EQUAL_UINT8(0, toggle_sfx(p, 2, 9, c));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_perform_p1_env_amt_p2_lfo_cutoff);
    RUN_TEST(test_perform_shift_glide_y_drive);
    RUN_TEST(test_lfo_cutoff_es_el_mismo_valor_en_perform_y_lfo);
    RUN_TEST(test_lfo_shift_pitch_y_tercer_destino_por_engine);
    RUN_TEST(test_lfo_p1_rate_libre_o_division_con_sync);
    RUN_TEST(test_env_cuatro_celdas_y_rangos);
    RUN_TEST(test_fx_y_paginas_sin_encoders_no_editan);
    RUN_TEST(test_forma_sin_dar_la_vuelta_y_patch_completo);
    RUN_TEST(test_synth_fx_mix_principal_y_discretos);
    RUN_TEST(test_synth_fx_encendido);
    RUN_TEST(test_env_filter_y_osc);
    RUN_TEST(test_prophet_osc_shift_mod_y_shape);
    RUN_TEST(test_nuevos_slots_juno_ob6_arp);
    return UNITY_END();
}
