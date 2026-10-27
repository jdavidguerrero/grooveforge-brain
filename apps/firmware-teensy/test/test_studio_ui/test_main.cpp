#include <string.h>
// Tests de la máquina de páginas de Studio (theory/04 §3 y §6): secuencias de controles → estado de UI.
// El comportamiento esperado es el de App::handle de apps/studio-head.
#include <unity.h>

#include "ui/detent.h"
#include <gf_params.h>

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

void test_nav_recorre_las_vistas() {   // home de theory/08: PERFORM · PANEL
    StudioUi ui;
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(PANEL, ui.state().page);
    ui.handle(Control::Nav, -1);                           // antes de la primera tarjeta: vuelve a PERFORM
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page);
    ui.handle(Control::Nav, -1);                           // en círculo: de PERFORM hacia atrás, PANEL
    TEST_ASSERT_EQUAL_UINT8(PANEL, ui.state().page);
    ui.handle(Control::Key1);                              // B1 desde PANEL y vuelta: regresa a PANEL
    ui.handle(Control::Key1);
    TEST_ASSERT_EQUAL_UINT8(PANEL, ui.state().page);
}

// PANEL: NAV recorre las tarjetas, NAV push abre su página con la parada elegida, pasada la última → PERFORM.
void test_panel_tarjetas_navegables() {
    StudioUi ui;                                            // Moog: 4 OSC + 1 MOD + 5 = 10 tarjetas
    ui.handle(Control::Nav, -1);                            // PERFORM → PANEL
    TEST_ASSERT_EQUAL_UINT8(PANEL, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(10, panel_card_count(0));
    ui.handle(Control::Nav, 3);                             // tarjeta 3 = NOISE
    TEST_ASSERT_EQUAL_UINT8(3, ui.state().cursor);
    ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL_UINT8(OSC, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(3, ui.state().cursor);
    ui.handle(Control::Key1);                               // vuelve a PANEL, en la misma tarjeta
    TEST_ASSERT_EQUAL_UINT8(PANEL, ui.state().page);
    ui.handle(Control::Nav, 1);                             // 4 = MODULATION
    ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL_UINT8(LFO, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().cursor);
    ui.handle(Control::Key3);
    ui.handle(Control::Nav, 2);                             // 6 = ENV FILTER
    ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL_UINT8(ENV, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().cursor);
    ui.handle(Control::Key2);
    ui.handle(Control::Nav, 8);                             // pasada la última tarjeta: PERFORM
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page);
}

// P1 push en la parada LFO: siguiente forma.
void test_p1_push_forma_del_lfo() {
    StudioUi ui;
    ui.handle(Control::Key3);
    const Result r = ui.handle(Control::P1Push);
    TEST_ASSERT_EQUAL(Action::LfoShape, r.action);
    ui.handle(Control::Nav, 1);                             // en una parada de MOD no hace nada
    TEST_ASSERT_EQUAL(Action::None, ui.handle(Control::P1Push).action);
}

void test_b1_abre_osc_y_vuelve_a_la_ultima_vista() {
    StudioUi ui;
    ui.handle(Control::Nav, 1);              // PANEL
    ui.handle(Control::Key1);
    TEST_ASSERT_EQUAL_UINT8(OSC, ui.state().page);
    ui.handle(Control::Key1);
    TEST_ASSERT_EQUAL_UINT8(PANEL, ui.state().page);
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
    ui.handle(Control::B6Down);                           // desde PRESETS (B1–B4 son bancos, theory/13): SHIFT + NAV push
    ui.handle(Control::NavPush);
    ui.handle(Control::B6Up);
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
    StudioUi ui;                                            // Moog: 3 VCO + NOISE
    ui.handle(Control::Key1);
    ui.handle(Control::Nav, 1);
    ui.handle(Control::Nav, 1);
    ui.handle(Control::Nav, 1);
    TEST_ASSERT_EQUAL_UINT8(3, ui.state().cursor);
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
    ui.handle(Control::Nav, 1);              // PANEL
    ui.handle(Control::B6Down);              // SHIFT apretado
    ui.handle(Control::Mode);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().mode);
    TEST_ASSERT_EQUAL_UINT8(FX, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().shift);
    ui.handle(Control::Mode);                // FX → HYBRID SYNTH (theory/12 §3d)
    TEST_ASSERT_EQUAL_UINT8(2, ui.state().mode);
    ui.handle(Control::Mode);                // → SYNTH, en la vista que tenía
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().mode);
    TEST_ASSERT_EQUAL_UINT8(PANEL, ui.state().page);
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
    ui.handle(Control::Key3);                // LFO del Moog: LFO · MODULATION
    ui.handle(Control::Nav, 3);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().cursor);
    ui.handle(Control::Nav, -3);
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
    ui.handle(Control::Nav, 1);              // PANEL
    ui.handle(Control::Key4);
    TEST_ASSERT_EQUAL_UINT8(SYNTH_FX, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().cursor);
    ui.handle(Control::Nav, 2);
    TEST_ASSERT_EQUAL_UINT8(2, ui.state().cursor);
    ui.handle(Control::Nav, 9);              // no pasa del último FX (REVERB, 5 lugares desde la fase 3)
    TEST_ASSERT_EQUAL_UINT8(4, ui.state().cursor);
    const Result r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(Action::SfxToggle, r.action);
    TEST_ASSERT_EQUAL_UINT8(4, r.arg);
    ui.handle(Control::Key4);                // B4 otra vez: vuelve a la última vista
    TEST_ASSERT_EQUAL_UINT8(PANEL, ui.state().page);
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
    ui.handle(Control::Nav, 1);              // PANEL
    ui.handle(Control::Key2);                // ENV
    ui.handle(Control::Key3);                // LFO
    ui.handle(Control::NavLong);
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page); // home = PERFORM
    ui.handle(Control::NavPush);                       // NAV push en PERFORM: entra a PANEL
    TEST_ASSERT_EQUAL_UINT8(PANEL, ui.state().page);
    ui.handle(Control::Nav, 3);
    ui.handle(Control::NavLong);                       // desde PANEL, NAV largo sale a PERFORM
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page);
    ui.handle(Control::Mode);                // en FX se queda en FX
    ui.handle(Control::NavLong);
    TEST_ASSERT_EQUAL_UINT8(FX, ui.state().page);
}

// Encoders de 2 pulsos por detent con el vaivén de ±1 medido en el hardware (2026-10-19): no rebota.
void test_detent_con_histeresis() {
    DetentCounter d(2);
    const int32_t seq[] = {1, 2, 3, 4, 5, 4, 5, 4, 5, 6};          // dos detents y vaivén en 4/5 (borde de >> 1)
    int32_t pos = 0, changes = 0, last = 0;
    for (int32_t raw : seq) {
        pos = d.update(raw);
        if (pos != last) ++changes;
        last = pos;
    }
    TEST_ASSERT_EQUAL_INT32(3, pos);                                // 6 pulsos = 3 detents
    TEST_ASSERT_EQUAL_INT32(3, changes);                            // sin idas y vueltas
    TEST_ASSERT_EQUAL_INT32(3, d.update(5));                        // el vaivén no baja
    TEST_ASSERT_EQUAL_INT32(2, d.update(4));                        // 2 pulsos atrás sí
    TEST_ASSERT_EQUAL_INT32(-1, d.update(-2));
}

void test_groove_shift_b3_nav_push_y_tap() {
    StudioUi ui;
    ui.handle(Control::B6Down);
    ui.handle(Control::Key3);                                   // SHIFT + B3 = GROOVE
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL_UINT8(GROOVE, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(0, ui.state().cursor);
    ui.handle(Control::Nav, 2);
    TEST_ASSERT_EQUAL_UINT8(2, ui.state().cursor);              // CHORD
    ui.handle(Control::Nav, 9);
    TEST_ASSERT_EQUAL_UINT8(4, ui.state().cursor);              // MACRO, sin dar la vuelta
    Result r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(int(Action::GroovePush), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(4, r.arg);
    r = ui.handle(Control::Key4);
    TEST_ASSERT_EQUAL(int(Action::Tap), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(GROOVE, ui.state().page);
    ui.handle(Control::NavLong);
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page);
    ui.handle(Control::Key3);                                   // B3 solo sigue siendo LFO
    TEST_ASSERT_EQUAL_UINT8(LFO, ui.state().page);
}

void test_estado_del_reloj_en_flags() {
    StudioUi ui;
    ui.take_dirty();
    ui.set_clock(true, true);
    TEST_ASSERT_TRUE(ui.take_dirty());
    TEST_ASSERT_EQUAL_HEX8(GF_UI_FLAG_CLOCK_MIDI | GF_UI_FLAG_CLOCK_RUN, ui.state().flags);
    ui.set_clock(true, true);
    TEST_ASSERT_FALSE(ui.take_dirty());                       // sin cambio, no se reenvía
    ui.handle(Control::EncLPush);                             // el bypass convive con los bits del reloj
    ui.set_clock(true, false);                                // STOP
    TEST_ASSERT_EQUAL_HEX8(GF_UI_FLAG_FILTER_BYPASS | GF_UI_FLAG_CLOCK_MIDI, ui.state().flags);
}

void test_volumen_desde_la_cabeza() {
    StudioUi ui;
    Result r = ui.handle(Control::VolUp);
    TEST_ASSERT_EQUAL(int(Action::VolStep), int(r.action));
    TEST_ASSERT_EQUAL_INT8(1, int8_t(r.arg));
    ui.handle(Control::Mode);                                 // también en FX
    r = ui.handle(Control::VolDown);
    TEST_ASSERT_EQUAL(int(Action::VolStep), int(r.action));
    TEST_ASSERT_EQUAL_INT8(-1, int8_t(r.arg));
    TEST_ASSERT_EQUAL_FLOAT(-60.0f, gf_vol_db(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.9f, gf_vol_norm(-6.0f));
}

void test_modo_hybrid() {
    StudioUi ui;
    ui.handle(Control::B6Down);
    Result r = ui.handle(Control::Key2);                        // SHIFT + B2 ya no entra (theory/12 §3d)
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL(int(Action::None), int(r.action));
    TEST_ASSERT_FALSE(ui.hybrid());
    ui.handle(Control::NavLong);
    ui.handle(Control::Mode);                                   // SYNTH → FX
    TEST_ASSERT_EQUAL_UINT8(MODE_FX, ui.state().mode);
    r = ui.handle(Control::Mode);                               // FX → HYBRID SYNTH
    TEST_ASSERT_EQUAL(int(Action::HybridEnter), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(MODE_HYBRID, ui.state().mode);
    TEST_ASSERT_EQUAL_UINT8(HYBRID, ui.state().page);
    TEST_ASSERT_TRUE(ui.hybrid());
    ui.handle(Control::Nav, 3);                                 // NAV recorre los presets de capa
    TEST_ASSERT_EQUAL_UINT8(3, ui.state().cursor);
    ui.handle(Control::Nav, 20);                                // no pasa de la fila + CUSTOM
    TEST_ASSERT_EQUAL_UINT8(8, ui.state().cursor);
    ui.handle(Control::Nav, -5);
    r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(int(Action::LayerLoad), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(3, r.arg);

    ui.handle(Control::Key1);                                   // B1 = OSC (teclas de SYNTH, rev. 5)
    TEST_ASSERT_EQUAL_UINT8(OSC, ui.state().page);
    r = ui.handle(Control::Key1);                               // otra vez: cambia a la capa B, sigue en OSC
    TEST_ASSERT_EQUAL(int(Action::LayerFocus), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(1, r.arg);
    TEST_ASSERT_EQUAL_UINT8(OSC, ui.state().page);
    TEST_ASSERT_TRUE(ui.state().flags & GF_UI_FLAG_FOCUS_B);
    r = ui.handle(Control::Key1);                               // y vuelve a A
    TEST_ASSERT_EQUAL_UINT8(0, r.arg);
    r = ui.handle(Control::Key2);                               // B2 = ENV de la capa en foco (A)
    TEST_ASSERT_EQUAL(int(Action::None), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(ENV, ui.state().page);
    r = ui.handle(Control::Key2);
    TEST_ASSERT_EQUAL_UINT8(1, r.arg);                          // ENV de B
    ui.handle(Control::Key3);
    TEST_ASSERT_EQUAL_UINT8(LFO, ui.state().page);
    ui.handle(Control::NavLong);                                // home con NAV largo
    TEST_ASSERT_EQUAL_UINT8(HYBRID, ui.state().page);

    ui.handle(Control::B6Down);                                 // SHIFT + B3: GROOVE, foco en A
    r = ui.handle(Control::Key3);
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL_UINT8(GROOVE, ui.state().page);
    TEST_ASSERT_EQUAL(int(Action::LayerFocus), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(0, r.arg);
    ui.handle(Control::Key4);                                   // B4: SYNTH FX · otra vez: home
    TEST_ASSERT_EQUAL_UINT8(SYNTH_FX, ui.state().page);
    ui.handle(Control::Key4);
    TEST_ASSERT_EQUAL_UINT8(HYBRID, ui.state().page);

    ui.handle(Control::B6Down);                                 // SHIFT + B4: presets de capa (cursor en el cargado)
    ui.handle(Control::Key4);
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL_UINT8(LAYER_PRESETS, ui.state().page);
    TEST_ASSERT_EQUAL_UINT8(4, ui.state().cursor);
    ui.handle(Control::Key1);                                   // banco A = fila 1
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().cursor);
    ui.handle(Control::Key2);                                   // banco B sin presets: el último
    TEST_ASSERT_EQUAL_UINT8(8, ui.state().cursor);
    ui.handle(Control::Nav, -20);                               // SAVE
    r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(int(Action::LayerSave), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(8, r.arg);
    TEST_ASSERT_EQUAL_UINT8(9, ui.layer_count());
    ui.handle(Control::NavLong);

    ui.handle(Control::B6Down);                                 // SHIFT + NAV push: ENGINE de la capa en foco
    ui.handle(Control::NavPush);
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL_UINT8(ENGINE, ui.state().page);
    r = ui.handle(Control::NavPush);
    TEST_ASSERT_EQUAL(int(Action::LoadEngine), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(HYBRID, ui.state().page);
    ui.handle(Control::Key1);
    ui.handle(Control::NavLong);                                // NAV largo = home del modo
    TEST_ASSERT_EQUAL_UINT8(HYBRID, ui.state().page);
    r = ui.handle(Control::Mode);                               // HYBRID SYNTH → SYNTH
    TEST_ASSERT_EQUAL(int(Action::HybridExit), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(MODE_SYNTH, ui.state().mode);
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page);
    ui.set_hybrid(true);                                        // CC 18
    TEST_ASSERT_EQUAL_UINT8(HYBRID, ui.state().page);
    ui.set_layer(true);
    TEST_ASSERT_TRUE(ui.state().flags & GF_UI_FLAG_LAYER);
}

void test_hybrid_custom() {
    StudioUi ui;
    ui.set_hybrid(true);
    ui.handle(Control::Nav, 20);                                // la última fila: + CUSTOM
    TEST_ASSERT_EQUAL_UINT8(8, ui.state().cursor);
    Result r = ui.handle(Control::NavPush);                     // ENGINE SELECT de A
    TEST_ASSERT_EQUAL(int(Action::None), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(ENGINE, ui.state().page);
    TEST_ASSERT_TRUE(ui.state().flags & GF_UI_FLAG_PICK);
    TEST_ASSERT_FALSE(ui.state().flags & GF_UI_FLAG_FOCUS_B);
    ui.handle(Control::Pick4);                                  // A = DX7 → ahora B
    TEST_ASSERT_EQUAL_UINT8(2, ui.custom_step());
    TEST_ASSERT_TRUE(ui.state().flags & GF_UI_FLAG_FOCUS_B);
    r = ui.handle(Control::Pick4);                              // B no puede repetir el engine de A
    TEST_ASSERT_EQUAL(int(Action::None), int(r.action));
    r = ui.handle(Control::Pick1);                              // B = Juno: arma la capa y abre OSC de A
    TEST_ASSERT_EQUAL(int(Action::LayerCustom), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(4 | (1 << 4), r.arg);
    TEST_ASSERT_EQUAL_UINT8(OSC, ui.state().page);
    TEST_ASSERT_FALSE(ui.state().flags & (GF_UI_FLAG_PICK | GF_UI_FLAG_FOCUS_B));
    ui.handle(Control::NavLong);                                // NAV largo cancela a mitad
    ui.handle(Control::Nav, 20);
    ui.handle(Control::NavPush);
    ui.handle(Control::NavLong);
    TEST_ASSERT_EQUAL_UINT8(0, ui.custom_step());
    TEST_ASSERT_EQUAL_UINT8(HYBRID, ui.state().page);
    TEST_ASSERT_FALSE(ui.state().flags & GF_UI_FLAG_PICK);
    ui.handle(Control::Mode);                                   // entrar al modo nunca carga la fila CUSTOM
    ui.handle(Control::Mode);
    r = ui.handle(Control::Mode);
    TEST_ASSERT_EQUAL(int(Action::HybridEnter), int(r.action));
    TEST_ASSERT_EQUAL_UINT8(0, r.arg);
}

void test_shift_b4_sale_de_presets() {
    StudioUi ui;
    ui.handle(Control::B6Down);
    ui.handle(Control::Key4);                                   // SHIFT + B4: PRESETS
    TEST_ASSERT_EQUAL_UINT8(PRESETS, ui.state().page);
    ui.handle(Control::Key4);                                   // otra vez (SHIFT sigue apretado): PERFORM
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL_UINT8(PERFORM, ui.state().page);
    ui.handle(Control::B6Down);
    ui.handle(Control::Key4);
    ui.handle(Control::B6Up);
    ui.handle(Control::Key4);                                   // B4 sin SHIFT: banco D (no sale)
    TEST_ASSERT_EQUAL_UINT8(PRESETS, ui.state().page);
}

void test_layer_state_ida_y_vuelta() {
    GF_LayerState l{};
    l.layer = 1;
    l.cursor = 1;
    l.engine_a = 4;
    l.engine_b = 3;
    l.focus = 1;
    l.morph = 50;
    l.delay_b_ms = 10;
    l.transpose_b = -12;
    strcpy(l.name, "KALIMBA");
    strcpy(l.preset_a, "HY KALIMBA A");
    strcpy(l.preset_b, "HY KALIMBA B");
    uint8_t buf[GF_LAYER_STATE_LEN];
    TEST_ASSERT_EQUAL_UINT32(GF_LAYER_STATE_LEN, gf_layer_state_pack(&l, buf));
    GF_LayerState r{};
    TEST_ASSERT_TRUE(gf_layer_state_unpack(buf, GF_LAYER_STATE_LEN, &r));
    TEST_ASSERT_EQUAL_INT8(1, r.layer);
    TEST_ASSERT_EQUAL_INT8(-12, r.transpose_b);
    TEST_ASSERT_EQUAL_UINT8(1, r.focus);
    TEST_ASSERT_EQUAL_STRING("HY KALIMBA B", r.preset_b);
    TEST_ASSERT_FALSE(gf_layer_state_unpack(buf, GF_LAYER_STATE_LEN - 1, &r));
    l.layer = -1;
    gf_layer_state_pack(&l, buf);
    gf_layer_state_unpack(buf, GF_LAYER_STATE_LEN, &r);
    TEST_ASSERT_EQUAL_INT8(-1, r.layer);
}

void test_presets_saltos_de_banco() {
    StudioUi ui;
    ui.set_preset_count(40);                                   // bancos A y B llenos, C con 8
    ui.handle(Control::B6Down);
    ui.handle(Control::Key4);                                  // SHIFT + B4 = PRESETS
    ui.handle(Control::B6Up);
    TEST_ASSERT_EQUAL_UINT8(PRESETS, ui.state().page);
    ui.handle(Control::Key2);                                  // B2 = banco B: fila 17
    TEST_ASSERT_EQUAL_UINT8(17, ui.state().cursor);
    ui.handle(Control::Key3);                                  // banco C: fila 33
    TEST_ASSERT_EQUAL_UINT8(33, ui.state().cursor);
    ui.handle(Control::Key4);                                  // banco D no existe todavía: se queda en el último
    TEST_ASSERT_EQUAL_UINT8(40, ui.state().cursor);
    ui.handle(Control::Key1);
    TEST_ASSERT_EQUAL_UINT8(1, ui.state().cursor);
    TEST_ASSERT_EQUAL_UINT8(PRESETS, ui.state().page);         // sigue en PRESETS
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_arranque_en_perform);
    RUN_TEST(test_nav_recorre_las_vistas);
    RUN_TEST(test_panel_tarjetas_navegables);
    RUN_TEST(test_p1_push_forma_del_lfo);
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
    RUN_TEST(test_detent_con_histeresis);
    RUN_TEST(test_groove_shift_b3_nav_push_y_tap);
    RUN_TEST(test_estado_del_reloj_en_flags);
    RUN_TEST(test_volumen_desde_la_cabeza);
    RUN_TEST(test_modo_hybrid);
    RUN_TEST(test_hybrid_custom);
    RUN_TEST(test_shift_b4_sale_de_presets);
    RUN_TEST(test_layer_state_ida_y_vuelta);
    RUN_TEST(test_presets_saltos_de_banco);
    return UNITY_END();
}
