// Tests nativos de la página GROOVE (theory/09 de Studio): reloj común, PUMP, ARP y CHORD.
#include <math.h>
#include <unity.h>

#include "dsp/arp_core.h"
#include "dsp/chord_memory.h"
#include "dsp/groove_clock.h"
#include "dsp/pump.h"

using dsp::GrooveClock;

void setUp() {}
void tearDown() {}

static constexpr uint32_t TICK_120 = 20833;   // 60e6 / (120 · 24) µs

// ── Reloj ─────────────────────────────────────────────────────────────────────

/// Manda `n` ticks a partir de `t` con período `dt`; devuelve el instante del último.
static uint32_t send_ticks(GrooveClock& c, uint32_t t, uint32_t dt, int n) {
    for (int i = 0; i < n; ++i) c.midi_clock(t + uint32_t(i) * dt);
    return t + uint32_t(n - 1) * dt;
}

void test_clock_midi_bpm_and_position() {
    GrooveClock c;
    c.midi_start();
    const uint32_t t = send_ticks(c, 1000, TICK_120, 48);
    TEST_ASSERT_EQUAL(int(GrooveClock::Source::Midi), int(c.source(t)));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 120.0f, c.bpm(t));
    // 48 ticks contados = 2 beats; START cuenta desde el primer tick
    TEST_ASSERT_FLOAT_WITHIN(0.01, 2.0, c.position(t));
    // Medio tick después: interpola
    TEST_ASSERT_FLOAT_WITHIN(0.005, 2.0 + 0.5 / 24.0, c.position(t + TICK_120 / 2));
}

void test_clock_midi_start_stop_spp() {
    GrooveClock c;
    c.midi_start();
    uint32_t t = send_ticks(c, 1000, TICK_120, 30);
    c.midi_stop();
    const double frozen = c.position(t);
    t = send_ticks(c, t + TICK_120, TICK_120, 10);   // clock sigue llegando, transporte parado
    TEST_ASSERT_FLOAT_WITHIN(0.001, frozen, c.position(t));
    TEST_ASSERT_FALSE(c.running(t));
    c.midi_song_position(8);   // 8 semicorcheas = 2 beats
    TEST_ASSERT_FLOAT_WITHIN(0.001, 2.0, c.position(t));
    c.midi_start();
    TEST_ASSERT_FLOAT_WITHIN(0.06, 0.0, c.position(t));
}

void test_clock_tap_sets_bpm_and_phase() {
    GrooveClock c;
    c.force_source(GrooveClock::Source::Internal);
    const uint32_t t0 = 10000000;
    for (int i = 0; i < 4; ++i) c.tap(t0 + uint32_t(i) * 500000);   // 120 BPM
    const uint32_t last = t0 + 3 * 500000;
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 120.0f, c.bpm(last));
    const double p = c.position(last);
    TEST_ASSERT_FLOAT_WITHIN(1e-6, floor(p + 0.5), p);   // el toque cae en un beat entero
    // Un toque aislado (después de más de 2 s) solo re-fasa
    c.tap(last + 3300000);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 120.0f, c.bpm(last + 3300000));
}

void test_clock_source_priority() {
    GrooveClock c;
    const uint32_t t = 5000000;
    TEST_ASSERT_EQUAL(int(GrooveClock::Source::Internal), int(c.source(t)));
    c.ai_beat(t, 124.0f, 0.0f);
    TEST_ASSERT_EQUAL(int(GrooveClock::Source::Ai), int(c.source(t)));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 124.0f, c.bpm(t));
    c.midi_clock(t);
    TEST_ASSERT_EQUAL(int(GrooveClock::Source::Midi), int(c.source(t + 1000)));
    // MIDI cae a los 300 ms, IA a los 2 s
    TEST_ASSERT_EQUAL(int(GrooveClock::Source::Ai), int(c.source(t + 400000)));
    TEST_ASSERT_EQUAL(int(GrooveClock::Source::Internal), int(c.source(t + 2500000)));
    c.force_source(GrooveClock::Source::Internal);
    TEST_ASSERT_EQUAL(int(GrooveClock::Source::Internal), int(c.source(t + 1000)));
}

void test_clock_ai_position_continuous() {
    GrooveClock c;
    const uint32_t t = 1000000;
    c.ai_beat(t, 120.0f, 0.0f);
    const uint32_t t2 = t + 1500000;   // 3 beats después (dentro del timeout de la IA)
    const double before = c.position(t2);
    c.ai_beat(t2, 120.0f, 0.02f);       // pequeña corrección de fase
    TEST_ASSERT_FLOAT_WITHIN(0.05, before, c.position(t2));
}

// ── PUMP ──────────────────────────────────────────────────────────────────────

void test_pump_kick_curve() {
    dsp::PumpParams p;
    p.depth = 0.6f;
    p.release_ms = 200.0f;
    // 120 BPM: 1 beat = 500 ms; 10 ms después del beat está abajo del todo
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.4f, dsp::pump_gain(p, 3.0 + 10.0 / 500.0, 120.0f));
    // Recuperado al final del ciclo
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 1.0f, dsp::pump_gain(p, 3.0 + 450.0 / 500.0, 120.0f));
    p.depth = 0.0f;
    TEST_ASSERT_EQUAL_FLOAT(1.0f, dsp::pump_gain(p, 3.01, 120.0f));
}

void test_pump_offset_and_shapes() {
    dsp::PumpParams p;
    p.depth = 1.0f;
    p.offset_ms = 50.0f;
    // Con offset de 50 ms, en el beat todavía no cayó (fin del ciclo anterior)
    TEST_ASSERT_TRUE(dsp::pump_gain(p, 2.0, 120.0f) > 0.9f);
    TEST_ASSERT_TRUE(dsp::pump_gain(p, 2.0 + 60.0 / 500.0, 120.0f) < 0.1f);
    p.offset_ms = 0.0f;
    const dsp::PumpShape shapes[3] = {dsp::PumpShape::Smooth, dsp::PumpShape::Exp, dsp::PumpShape::Kick};
    for (auto s : shapes) {
        p.shape = s;
        float prev = -1.0f;
        for (int ms = 30; ms < 450; ms += 20) {   // monótona creciente después de la caída
            const float g = dsp::pump_gain(p, 1.0 + ms / 500.0, 120.0f);
            TEST_ASSERT_TRUE(g >= prev - 1e-5f);
            TEST_ASSERT_TRUE(g >= 0.0f && g <= 1.0f);
            prev = g;
        }
    }
}

// ── ARP ───────────────────────────────────────────────────────────────────────

struct ArpLog {
    uint8_t on[64];
    double on_pos[64];
    int n_on = 0, n_off = 0;
    double pos = 0;
};
static void log_on(uint8_t note, uint8_t, void* c) {
    auto* l = static_cast<ArpLog*>(c);
    if (l->n_on < 64) {
        l->on_pos[l->n_on] = l->pos;
        l->on[l->n_on++] = note;
    }
}
static void log_off(uint8_t, void* c) { ++static_cast<ArpLog*>(c)->n_off; }

/// Corre el arp de `from` a `to` beats en pasos de 1 ms a 120 BPM.
static void run_arp(dsp::ArpCore& a, ArpLog& l, double from, double to) {
    for (double p = from; p < to; p += 0.002) {
        l.pos = p;
        a.update(p);
    }
}

void test_arp_up_and_division() {
    dsp::ArpCore a;
    ArpLog l;
    a.set_callbacks(log_on, log_off, &l);
    a.set_step_beats(0.25f);
    a.note_on(64, 100);
    a.note_on(60, 100);
    a.note_on(67, 100);
    run_arp(a, l, 0.0, 1.999);
    TEST_ASSERT_EQUAL(8, l.n_on);   // 8 semicorcheas en 2 beats
    const uint8_t expect[4] = {60, 64, 67, 60};
    for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL(expect[i], l.on[i]);
    TEST_ASSERT_FLOAT_WITHIN(0.003, 0.25, l.on_pos[1]);
    TEST_ASSERT_TRUE(l.n_off >= 7);   // gate 60 %: cada nota se apaga antes del paso siguiente
}

void test_arp_triplets_and_dotted() {
    dsp::ArpCore a;
    ArpLog l;
    a.set_callbacks(log_on, log_off, &l);
    a.note_on(60, 100);
    a.set_step_beats(1.0f / 3.0f);
    run_arp(a, l, 0.0, 1.999);
    TEST_ASSERT_EQUAL(6, l.n_on);
    a.clear();
    a.note_on(60, 100);
    l.n_on = 0;
    a.set_step_beats(0.75f);
    run_arp(a, l, 0.0, 2.999);
    TEST_ASSERT_EQUAL(4, l.n_on);   // 3 beats / 0.75
}

void test_arp_swing_delays_odd_steps() {
    dsp::ArpCore a;
    ArpLog l;
    a.set_callbacks(log_on, log_off, &l);
    a.set_step_beats(0.25f);
    a.set_swing(0.66f);
    a.note_on(60, 100);
    run_arp(a, l, 0.0, 0.999);
    TEST_ASSERT_EQUAL(4, l.n_on);
    TEST_ASSERT_FLOAT_WITHIN(0.004, 0.33, l.on_pos[1]);   // 0.66 · 0.5
    TEST_ASSERT_FLOAT_WITHIN(0.004, 0.5, l.on_pos[2]);
}

void test_arp_octaves_and_updown() {
    dsp::ArpCore a;
    ArpLog l;
    a.set_callbacks(log_on, log_off, &l);
    a.set_step_beats(0.25f);
    a.set_octaves(2);
    a.note_on(60, 100);
    a.note_on(64, 100);
    run_arp(a, l, 0.0, 0.999);
    const uint8_t up[4] = {60, 64, 72, 76};
    for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL(up[i], l.on[i]);

    dsp::ArpCore b;
    ArpLog m;
    b.set_callbacks(log_on, log_off, &m);
    b.set_step_beats(0.25f);
    b.set_mode(dsp::ArpCore::Mode::UpDown);
    b.note_on(60, 100);
    b.note_on(64, 100);
    b.note_on(67, 100);
    run_arp(b, m, 0.0, 1.499);
    const uint8_t ud[6] = {60, 64, 67, 64, 60, 64};
    for (int i = 0; i < 6; ++i) TEST_ASSERT_EQUAL(ud[i], m.on[i]);
}

void test_arp_smart_never_repeats() {
    dsp::ArpCore a;
    ArpLog l;
    a.set_callbacks(log_on, log_off, &l);
    a.set_step_beats(0.25f);
    a.set_mode(dsp::ArpCore::Mode::Smart);
    a.note_on(60, 100);
    a.note_on(63, 100);
    a.note_on(67, 100);
    a.note_on(70, 100);
    run_arp(a, l, 0.0, 7.999);
    TEST_ASSERT_EQUAL(32, l.n_on);
    for (int i = 1; i < l.n_on; ++i) TEST_ASSERT_NOT_EQUAL(l.on[i - 1], l.on[i]);
}

void test_arp_latch_and_release() {
    dsp::ArpCore a;
    ArpLog l;
    a.set_callbacks(log_on, log_off, &l);
    a.set_latch(true);
    a.note_on(60, 100);
    a.note_on(64, 100);
    a.note_off(60);
    a.note_off(64);
    TEST_ASSERT_EQUAL(2, a.held());
    a.note_on(62, 100);   // todo suelto: reemplaza
    TEST_ASSERT_EQUAL(1, a.held());
    a.set_latch(false);   // 62 sigue apretada
    TEST_ASSERT_EQUAL(1, a.held());
    a.note_off(62);
    TEST_ASSERT_EQUAL(0, a.held());
    run_arp(a, l, 0.0, 0.5);
    TEST_ASSERT_EQUAL(0, l.n_on);
}

// ── CHORD ─────────────────────────────────────────────────────────────────────

void test_chord_default_learn_and_transpose() {
    dsp::ChordMemory c;
    uint8_t out[6];
    TEST_ASSERT_EQUAL(4, c.notes(57, out));
    const uint8_t def[4] = {57, 60, 64, 69};
    for (int i = 0; i < 4; ++i) TEST_ASSERT_EQUAL(def[i], out[i]);

    const uint8_t played[3] = {67, 60, 64};   // C mayor, desordenado
    TEST_ASSERT_TRUE(c.learn(played, 3));
    TEST_ASSERT_EQUAL(3, c.notes(62, out));
    const uint8_t d[3] = {62, 66, 69};
    for (int i = 0; i < 3; ++i) TEST_ASSERT_EQUAL(d[i], out[i]);
    TEST_ASSERT_FALSE(c.learn(played, 1));
}

void test_chord_inversion() {
    dsp::ChordMemory c;
    const uint8_t played[3] = {60, 64, 67};
    c.learn(played, 3);
    c.set_inversion(1);
    uint8_t out[6];
    c.notes(60, out);
    TEST_ASSERT_EQUAL(64, out[0]);
    TEST_ASSERT_EQUAL(67, out[1]);
    TEST_ASSERT_EQUAL(72, out[2]);
}

void test_chord_key_triads() {
    dsp::ChordMemory c;
    c.set_type(dsp::ChordMemory::Type::Key);
    c.set_key(0);   // C mayor
    uint8_t out[6];
    TEST_ASSERT_EQUAL(3, c.notes(62, out));   // D → Dm
    TEST_ASSERT_EQUAL(62, out[0]);
    TEST_ASSERT_EQUAL(65, out[1]);
    TEST_ASSERT_EQUAL(69, out[2]);
    c.notes(71, out);   // B → B dim
    TEST_ASSERT_EQUAL(74, out[1]);
    TEST_ASSERT_EQUAL(77, out[2]);
    c.set_key(12 + 9);   // A menor
    c.notes(57, out);
    TEST_ASSERT_EQUAL(60, out[1]);
    TEST_ASSERT_EQUAL(64, out[2]);
    c.notes(61, out);   // C# fuera de la escala → C
    TEST_ASSERT_EQUAL(60, out[0]);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_clock_midi_bpm_and_position);
    RUN_TEST(test_clock_midi_start_stop_spp);
    RUN_TEST(test_clock_tap_sets_bpm_and_phase);
    RUN_TEST(test_clock_source_priority);
    RUN_TEST(test_clock_ai_position_continuous);
    RUN_TEST(test_pump_kick_curve);
    RUN_TEST(test_pump_offset_and_shapes);
    RUN_TEST(test_arp_up_and_division);
    RUN_TEST(test_arp_triplets_and_dotted);
    RUN_TEST(test_arp_swing_delays_odd_steps);
    RUN_TEST(test_arp_octaves_and_updown);
    RUN_TEST(test_arp_smart_never_repeats);
    RUN_TEST(test_arp_latch_and_release);
    RUN_TEST(test_chord_default_learn_and_transpose);
    RUN_TEST(test_chord_inversion);
    RUN_TEST(test_chord_key_triads);
    return UNITY_END();
}
