// Tests de la máquina de páginas de Studio (theory/04 §3 y §6): secuencias de controles → estado de UI.
// El comportamiento esperado es el de App::handle de apps/studio-head.
#include <unity.h>

#include "ui/studio_ui.h"

using namespace studio;

void setUp() {}
void tearDown() {}

void test_arranque_en_perform() {
    StudioUi ui;
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().mode);
    TEST_ASSERT_TRUE(ui.take_dirty());      // el primer UI_STATE sale al arrancar
    TEST_ASSERT_FALSE(ui.take_dirty());
}

void test_nav_recorre_las_vistas() {
    StudioUi ui;
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(AI, ui.state().page);
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(SPECTRUM, ui.state().page);
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page);
    ui.handle(Control::Nav, -1);
    TEST_ASSERT_EQUAL_UINT8(SPECTRUM, ui.state().page);
}

void test_b1_abre_osc_y_vuelve_a_la_ultima_vista() {
    StudioUi ui;
    ui.handle(Control::Nav, 1);              // AI
    ui.handle(Control::Key1);
    TEST_ASSERT_EQUAL_UINT8(OSC, ui.state().page);
    ui.handle(Control::Key1);
    TEST_ASSERT_EQUAL_UINT8(AI, ui.state().page);
    ui.handle(Control::Key2);
    TEST_ASSERT_EQUAL_UINT8(ENV, ui.state().page);
    ui.handle(Control::Key3);                  // de ENV a LFO directo
    TEST_ASSERT_EQUAL_UINT8(LFO, ui.state().page);
}

void open_presets(StudioUi& ui) {                        // SHIFT + B4 (theory/05 de Studio)
    ui.handle(Control::B6Down);
    ui.handle(Control::Key4);
    ui.handle(Control::B6Up);
}

void test_b4_presets_engine_perform_y_carga_engine() {
    StudioUi ui;
    open_presets(ui);
    TEST_ASSERT_EQUAL_UINT8(PRESETS, ui.state().page);
    ui.handle(Control::Key4);
    TEST_ASSERT_EQUAL_UINT8(ENGINE, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().cursor);
    ui.handle(Control::Nav, 1);
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(2, ui.state().cursor);
    Result r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(Action::LoadEngine, r.action);
    TEST_ASSERT_EQUAL_UINT8(2, r.arg);
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(2, ui.engine());
}

void test_engine_cursor_da_la_vuelta() {
    StudioUi ui;
    ui.handle(Control::EngineCard);
    ui.handle(Control::Nav, -1);
    TEST_ASSERT_EQUAL_UINT8(5, ui.state().cursor);
}

void test_presets_save_y_load() {
    StudioUi ui;
    ui.set_preset_count(3);
    open_presets(ui);
    Result r = ui.handle(Control::NavPush);                  // fila 0 = SAVE
    TEST_ASSERT_EQUAL(Action::SavePreset, r.action);
    TEST_ASSERT_EQUAL_UINT8(3, r.arg);
    ui.handle(Control::Nav, 2);
    r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(Action::LoadPreset, r.action);
    TEST_ASSERT_EQUAL_UINT8(1, r.arg);
    ui.handle(Control::Nav, 100);                            // se queda en la última fila
    TEST_ASSERT_EQUAL_UINT8(4, ui.state().cursor);
}

void test_osc_recorre_los_osciladores_del_engine() {
    StudioUi ui;                                            // Moog: 3 VCO
    ui.handle(Control::Key1);
    ui.handle(Control::Nav, 1);
    ui.handle(Control::Nav, 1);
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().cursor);
    ui.set_engine(1);                                       // Juno: 1 DCO, NAV no hace nada
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().cursor);
}

void test_shift_momentaneo_y_bypass() {
    StudioUi ui;
    ui.handle(Control::B6Down);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().shift);
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().shift);
    Result r = ui.handle(Control::EncLPush);
    TEST_ASSERT_EQUAL(Action::FilterBypass, r.action);
    TEST_ASSERT_BITS(GF_UI_FLAG_FILTER_BYPASS, GF_UI_FLAG_FILTER_BYPASS, ui.state().flags);
}

void test_lfo_push_alterna_sync() {
    StudioUi ui;
    ui.handle(Control::Key3);
    Result r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(Action::LfoSync, r.action);
    TEST_ASSERT_EQUAL_UINT8(1, r.arg);
}

void test_mode_fx_y_vuelta() {
    StudioUi ui;
    ui.handle(Control::Nav, 1);              // AI
    ui.handle(Control::B6Down);              // SHIFT apretado
    ui.handle(Control::Mode);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().mode);
    TEST_ASSERT_EQUAL_UINT8(FX, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().shift);
    ui.handle(Control::Mode);
    TEST_ASSERT_EQUAL_UINT8(AI, ui.state().page);
}

void test_fx_slots_ruteo_hold_y_favorito() {
    StudioUi ui;
    ui.handle(Control::Mode);
    TEST_ASSERT_EQUAL_UINT8(2, ui.state().fx_slot);
    ui.handle(Control::Nav, 1);              // salta SPECTRAL SMEAR (3, no disponible)
    TEST_ASSERT_EQUAL_UINT8(4, ui.state().fx_slot);
    Result r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(Action::FxToggle, r.action);
    TEST_ASSERT_TRUE(ui.state().fx_on & (1u << 4));
    ui.handle(Control::Key2);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().fx_route[4]);
    ui.handle(Control::B6Down);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().fx_hold);
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().fx_hold);
    ui.handle(Control::Key4);
    TEST_ASSERT_EQUAL_UINT8(FAV_FX_SLOT, ui.state().fx_slot);
}

void test_ui_state_roundtrip() {
    StudioUi ui;
    ui.handle(Control::Mode);
    ui.handle(Control::Nav, 3);
    uint8_t buf[GF_UI_STATE_LEN];
    TEST_ASSERT_EQUAL(GF_UI_STATE_LEN, gf_ui_state_pack(&ui.state(), buf));
    GF_UiState back{};
    TEST_ASSERT_TRUE(gf_ui_state_unpack(buf, sizeof(buf), &back));
    TEST_ASSERT_EQUAL_UINT8(ui.state().fx_slot, back.fx_slot);
    TEST_ASSERT_EQUAL_UINT16(ui.state().fx_on, back.fx_on);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(ui.state().fx_route, back.fx_route, GF_UI_FX_SLOTS);
    TEST_ASSERT_FALSE(gf_ui_state_unpack(buf, 21, &back));   // LEN distinto → NACK INVALID_LEN
}

void test_lfo_nav_cambia_la_forma() {     // desde el Prophet completo: NAV elige la parada (LFO · POLY · WHEEL)
    StudioUi ui;
    ui.handle(Control::Key3);                // LFO del Moog: solo la parada LFO
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().cursor);
    ui.set_engine(2);                        // Prophet: LFO · POLY-MOD · WHEEL-MOD
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().cursor);
    ui.handle(Control::Nav, 5);
    TEST_ASSERT_EQUAL_UINT8(2, ui.state().cursor);
    const Result r = ui.handle(Control::NavPush);   // en una parada de MOD, NAV push no alterna SYNC
    TEST_ASSERT_EQUAL(Action::None, r.action);
    TEST_ASSERT_EQUAL_UINT8(LFO, ui.state().page);
}

void test_b4_abre_synth_fx_cursor_y_encendido() {
    StudioUi ui;
    ui.handle(Control::Nav, 1);              // AI
    ui.handle(Control::Key4);
    TEST_ASSERT_EQUAL_UINT8(SYNTH_FX, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().cursor);
    ui.handle(Control::Nav, 2);
    TEST_ASSERT_EQUAL_UINT8(2, ui.state().cursor);
    ui.handle(Control::Nav, 9);              // no pasa del último FX
    TEST_ASSERT_EQUAL_UINT8(3, ui.state().cursor);
    const Result r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(Action::SfxToggle, r.action);
    TEST_ASSERT_EQUAL_UINT8(3, r.arg);
    ui.handle(Control::Key4);                // B4 otra vez: vuelve a la última vista
    TEST_ASSERT_EQUAL_UINT8(AI, ui.state().page);
}

void test_shift_nav_push_abre_engine_select() {
    StudioUi ui;
    ui.set_engine(3);
    ui.handle(Control::Key3);                // desde LFO, donde NAV push solo es SYNC
    ui.handle(Control::B6Down);
    const Result r = ui.handle(Control::NavPush);
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL(Action::None, r.action);
    TEST_ASSERT_EQUAL_UINT8(ENGINE, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(3, ui.state().cursor);      // el cursor arranca en el engine actual
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().flags & GF_UI_FLAG_LFO_SYNC);
}

void test_nav_largo_vuelve_a_home() {
    StudioUi ui;
    ui.handle(Control::Nav, 2);              // SPECTRUM
    ui.handle(Control::Key2);                // ENV
    ui.handle(Control::Key3);                // LFO
    ui.handle(Control::NavLong);
    TEST_ASSERT_EQUAL_UINT8(SPECTRUM, ui.state().page);
    ui.handle(Control::Mode);                // en FX se queda en FX
    ui.handle(Control::NavLong);
    TEST_ASSERT_EQUAL_UINT8(FX, ui.state().page);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_arranque_en_perform);
    RUN_TEST(test_nav_recorre_las_vistas);
    RUN_TEST(test_b1_abre_osc_y_vuelve_a_la_ultima_vista);
    RUN_TEST(test_b4_presets_engine_perform_y_carga_engine);
    RUN_TEST(test_engine_cursor_da_la_vuelta);
    RUN_TEST(test_presets_save_y_load);
    RUN_TEST(test_osc_recorre_los_osciladores_del_engine);
    RUN_TEST(test_shift_momentaneo_y_bypass);
    RUN_TEST(test_lfo_push_alterna_sync);
    RUN_TEST(test_lfo_nav_cambia_la_forma);
    RUN_TEST(test_b4_abre_synth_fx_cursor_y_encendido);
    RUN_TEST(test_shift_nav_push_abre_engine_select);
    RUN_TEST(test_nav_largo_vuelve_a_home);
    RUN_TEST(test_mode_fx_y_vuelta);
    RUN_TEST(test_fx_slots_ruteo_hold_y_favorito);
    RUN_TEST(test_ui_state_roundtrip);
    return UNITY_END();
}
