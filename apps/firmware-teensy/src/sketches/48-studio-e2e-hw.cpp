/**
 * 48-studio-e2e-hw.cpp — Prueba e2e del hardware de Studio antes del pedido de la portadora.
 *
 * Valida la cadena completa con piezas mínimas, sin engines, ML ni bridge:
 *   teclado MIDI en el USB host → voz (Teensy Audio Library oficial) → I2S → SGTL5000 → LINE OUT
 *   + ENC L / ENC R / NAV y B1–B4 cambiando parámetros en vivo.
 * Si esto suena y el sketch 28 no, el problema es de firmware, no de la placa.
 *
 * Voz (Teensy Audio Library oficial; el LFO y el mapeo de P1/P2 son custom, theory/lfo-modulacion.md):
 *   AudioSynthWaveformModulated (FM = vibrato, forma = PWM) → AudioFilterStateVariable (LP, entrada de
 *   control = envelope del filtro + LFO) → AudioEffectEnvelope → AudioEffectMultiply (LFO → AMP)
 *   → AudioMixer4 → AudioOutputI2S (L y R). Un solo ADSR para amplificador y filtro, como el Juno-106.
 *   El LFO (dsp::Lfo) corre a 1 kHz y entra por AudioSynthWaveformDc con rampa de 1 ms.
 *
 * Controles (pines del ADR-004, los mismos de la portadora):
 *   ENC L (10/11) cutoff · ENC R (13/14) resonancia
 *   P1 (30/31) y P2 (34/35): lo que diga la página (studio::edit_encoder): PERFORM FLT ENV AMT / LFO→CUTOFF,
 *   ENV A/D (SHIFT S/R), LFO RATE / →CUTOFF (SHIFT →PITCH / →PWM o AMP). En LFO, NAV elige la forma.
 *   Navegación de Studio con studio::StudioUi (theory/04): NAV (16/17, push 26), B1–B4 (2–5),
 *   B5 MODE (40), B6 SHIFT/HOLD (41), ENC L push (12) bypass del filtro. El estado de UI sale entero en
 *   UI_STATE (0x44) y la pantalla de la Pi lo obedece. Este sketch tiene una sola voz: cargar un engine
 *   cambia la forma de onda y lo anuncia por ENGINE_CHANGED (los engines reales viven en el sketch 28).
 *
 * Al arrancar suena un La de 440 Hz durante 0.5 s (sin teclado) para probar el LINE OUT.
 *
 * Bridge con la cabeza de la Pi (Serial1, pines 0/1, 921600; 02-bridge-protocol.md): el Teensy es el
 * master. HEARTBEAT cada 1 s, ENGINE_CHANGED al arrancar, al cambiar y cada 5 s, PARAM_CHANGED del filtro,
 * del ENV y del LFO del engine activo (ids y normalización de gf_params.h), BEAT_DETECTED con el BPM del MIDI
 * clock, UI_STATE y NOTE_ON con cada nota. La cabeza contesta ACK.
 *
 *   cd apps/firmware-teensy
 *   pio run -e sketch48 -t upload
 *   pio device monitor -b 115200
 */
#include <Arduino.h>
#include <Audio.h>
#include <Bounce2.h>
#include <Encoder.h>
#include <USBHost_t36.h>
#include <protocol.h>

#include <gf_params.h>

#include "dsp/lfo.h"
#include "ui/page_params.h"
#include "ui/studio_ui.h"

AudioSynthWaveformDc pitch_dc, shape_dc, fenv_src, lfo_cut_dc, amp_dc;
AudioSynthWaveformModulated osc;
AudioEffectEnvelope fenv, env;
AudioMixer4 fmix, mix;
AudioFilterStateVariable filt;
AudioEffectMultiply trem;
AudioOutputI2S i2s_out;
AudioControlSGTL5000 codec;
AudioConnection c1(pitch_dc, 0, osc, 0);
AudioConnection c2(shape_dc, 0, osc, 1);
AudioConnection c3(osc, 0, filt, 0);
AudioConnection c4(fenv_src, 0, fenv, 0);
AudioConnection c5(fenv, 0, fmix, 0);
AudioConnection c6(lfo_cut_dc, 0, fmix, 1);
AudioConnection c7(fmix, 0, filt, 1);    // control de frecuencia: ±7 octavas a escala completa
AudioConnection c8(filt, 0, env, 0);     // salida 0 = low-pass
AudioConnection c9(env, 0, trem, 0);
AudioConnection c10(amp_dc, 0, trem, 1);
AudioConnection c11(trem, 0, mix, 0);
AudioConnection c12(mix, 0, i2s_out, 0);
AudioConnection c13(mix, 0, i2s_out, 1);
constexpr float FILT_OCT = 7.0f;         // octaveControl: la entrada ±1 mueve ±7 octavas

USBHost usb_host;
MIDIDevice midi(usb_host);

Encoder enc_l(10, 11), enc_r(6, 14), enc_nav(16, 17), enc_p1(30, 31), enc_p2(34, 35);
// B1–B4, B5 MODE, B6 SHIFT/HOLD, NAV push, ENC L push
constexpr uint8_t BTN_PINS[8] = {2, 3, 4, 5, 40, 41, 26, 12};
Bounce btn[8];
studio::StudioUi ui;
uint32_t last_ui_tx = 0, last_ui_beat = 0;

// Una forma de onda por engine para que el cambio se oiga (sketch de prueba, una sola voz). Los engines con
// PWM usan PULSE o TRIANGLE_VARIABLE para que el LFO → PWM se escuche; PWM_BASE es el ancho de reposo
// (entrada de forma: −1 = 0 %, 0 = 50 %, +1 = 100 %).
constexpr short ENGINE_WAVE[6] = {WAVEFORM_SAWTOOTH, WAVEFORM_PULSE, WAVEFORM_PULSE, WAVEFORM_PULSE,
                                   WAVEFORM_SINE, WAVEFORM_TRIANGLE_VARIABLE};
constexpr float PWM_BASE[6] = {0.0f, 0.0f, -0.5f, 0.0f, 0.0f, 0.0f};
const char* const ENGINE_NAME[6] = {"MOOG MODEL D", "JUNO-106", "PROPHET-5", "OB-6", "DX7", "ARP 2600"};

float cutoff = 2000.0f, resonance = 1.2f;
uint8_t held_note = 255;
int32_t last_l = 0, last_r = 0, last_nav = 0, last_p1 = 0, last_p2 = 0;

studio::Patch patches[GF_ENGINE_COUNT];  // uno por engine: al volver a un engine vuelve su LFO y su ENV
dsp::Lfo lfo;
float wheel = 0.0f;                      // CC 1, 0–1: suma vibrato como en los originales
float bpm = 120.0f;                      // MIDI clock del USB host; sin clock, 120
uint32_t last_clock_us = 0, last_bpm_tx = 0;
float clock_us_avg = 0.0f;

studio::Patch& patch() { return patches[ui.engine()]; }
uint32_t last_stats = 0;
bool host_seen = false;

// ── Bridge (master) ────────────────────────────────────────────────────────────────────────────
uint8_t tx_seq = 0;
uint32_t last_hb = 0, last_engine = 0, last_ack_ms = 0, acks = 0;
bool link_up = false;
uint8_t rx_buf[GF_PAYLOAD_MAX + GF_FRAME_MIN_SIZE];
size_t rx_len = 0;

void bridge_send(GF_Cmd cmd, const uint8_t* payload, uint8_t len) {
    GF_Frame f;
    gf_frame_build(&f, cmd, tx_seq++, payload, len);
    uint8_t wire[GF_PAYLOAD_MAX + GF_FRAME_MIN_SIZE];
    size_t n = gf_frame_serialize(&f, wire, sizeof(wire));
    Serial1.write(wire, n);
}

void bridge_param(uint16_t id, float norm) {
    norm = constrain(norm, 0.0f, 1.0f);
    uint8_t p[6];
    memcpy(p, &id, 2);
    memcpy(p + 2, &norm, 4);
    bridge_send(GF_CMD_PARAM_CHANGED, p, 6);
}

void bridge_engine() {
    uint8_t p[24] = {ui.engine()};
    const size_t n = strlen(ENGINE_NAME[ui.engine()]);
    memcpy(p + 1, ENGINE_NAME[ui.engine()], n + 1);
    bridge_send(GF_CMD_ENGINE_CHANGED, p, uint8_t(n + 2));
}

void bridge_ui_state() {
    uint8_t p[GF_UI_STATE_LEN];
    gf_ui_state_pack(&ui.state(), p);
    bridge_send(GF_CMD_UI_STATE, p, GF_UI_STATE_LEN);
    last_ui_tx = last_ui_beat = millis();
}

// Filtro del engine activo (grupo 2): cutoff y resonancia en los rangos de gf_params.h
void bridge_filter() {
    const uint8_t e = ui.engine();
    bridge_param(gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_CUTOFF), gf_norm(GF_CUTOFF_RANGE[e], cutoff));
    bridge_param(gf_param_id(e, GF_GROUP_FILTER, GF_FILTER_RESO), (resonance - 0.7f) / (5.0f - 0.7f));
}

void bridge_patch() {
    studio::ParamChange c[studio::PATCH_PARAM_COUNT];
    const uint8_t n = studio::patch_params(patch(), ui.engine(), c);
    for (uint8_t i = 0; i < n; ++i) bridge_param(c[i].id, c[i].norm);
}

void bridge_bpm() {
    const uint16_t x10 = uint16_t(lroundf(bpm * 10.0f));
    const uint8_t p[3] = {uint8_t(x10 & 0xFF), uint8_t(x10 >> 8), 0};
    bridge_send(GF_CMD_BEAT_DETECTED, p, 3);
    last_bpm_tx = millis();
}

/// Lleva al motor lo que no se modula: ADSR (amplificador y filtro) y FLT ENV AMT.
void apply_patch() {
    const studio::Patch& p = patch();
    for (AudioEffectEnvelope* e : {&env, &fenv}) {
        e->attack(p.attack_ms);
        e->decay(p.decay_ms);
        e->sustain(p.sustain);
        e->release(p.release_ms);
    }
    fmix.gain(0, p.env_amt * GF_FLT_ENV_OCT / FILT_OCT);
}

/// LFO a 1 kHz: una evaluación y tres cantidades (theory/lfo-modulacion.md §5).
void mod_tick() {
    static uint32_t last_us = micros();
    const uint32_t now = micros();
    if (now - last_us < 1000) return;
    const float dt = (now - last_us) * 1e-6f;
    last_us = now;

    const studio::Patch& p = patch();
    const uint8_t e = ui.engine();
    lfo.set_shape(dsp::LfoShape(p.lfo_shape));
    lfo.set_rate_hz(p.lfo_rate_hz);
    lfo.set_sync(ui.state().flags & GF_UI_FLAG_LFO_SYNC, bpm, GF_LFO_DIV_BEATS[p.lfo_div]);
    const float v = lfo.tick(dt);

    auto amt = [&](uint8_t d) { return gf_lfo_dest_available(e, d) ? p.lfo_amt[d] : 0.0f; };
    lfo_cut_dc.amplitude(amt(GF_DEST_CUTOFF) * GF_LFO_CUT_OCT * v / FILT_OCT, 1);
    const float a_pitch = fminf(1.0f, amt(GF_DEST_PITCH) + 0.3f * wheel);
    pitch_dc.amplitude(gf_lfo_pitch_st(a_pitch) * v / 12.0f, 1);   // frequencyModulation(1): ±1 = ±1 oct
    shape_dc.amplitude(constrain(PWM_BASE[e] + 0.9f * amt(GF_DEST_PWM) * v, -0.95f, 0.95f), 1);
    amp_dc.amplitude(1.0f - amt(GF_DEST_AMP) * (1.0f + v) * 0.5f, 1);
}

/// P1 (0) o P2 (1) giró: edita lo que diga la página y avisa a la cabeza.
void page_encoder(uint8_t enc, int32_t steps) {
    studio::ParamChange c[1];
    if (!studio::edit_encoder(patch(), ui.state(), ui.engine(), enc, int8_t(constrain(steps, -8, 8)), c)) return;
    bridge_param(c[0].id, c[0].norm);
    apply_patch();
    Serial.printf("[P%u] param 0x%04X = %.3f\n", enc + 1, c[0].id, c[0].norm);
}

/// Ejecuta en el sonido lo que pidió la navegación.
void run_action(studio::Result r) {
    switch (r.action) {
        case studio::Action::LoadEngine:
            osc.begin(ENGINE_WAVE[r.arg]);
            apply_patch();
            bridge_engine();
            bridge_filter();
            bridge_patch();
            Serial.printf("[UI] engine %s\n", ENGINE_NAME[r.arg]);
            break;
        case studio::Action::LfoShape: {
            studio::ParamChange c[1];
            studio::step_lfo_shape(patch(), ui.engine(), int8_t(r.arg), c);
            bridge_param(c[0].id, c[0].norm);
            Serial.printf("[UI] forma del LFO %u\n", patch().lfo_shape);
            break;
        }
        case studio::Action::LfoSync:
            Serial.printf("[UI] LFO SYNC %s (%.1f BPM)\n", r.arg ? "ON" : "OFF", bpm);
            break;
        case studio::Action::FilterBypass:
            filt.frequency(r.arg ? 18000.0f : cutoff);
            Serial.printf("[UI] bypass del filtro %s\n", r.arg ? "ON" : "OFF");
            break;
        case studio::Action::None: break;
        default: Serial.printf("[UI] acción %u (arg %u) sin efecto en este sketch\n", unsigned(r.action), r.arg); break;
    }
}

void ui_control(studio::Control c, int8_t v = 1) {
    run_action(ui.handle(c, v));
    const GF_UiState& u = ui.state();
    Serial.printf("[UI] modo %s · página %u · cursor %u · shift %u · hold %u · fx %u\n", u.mode ? "FX" : "SYNTH",
                  u.page, u.cursor, u.shift, u.fx_hold, u.fx_slot);
}

/// Lee las respuestas de la cabeza: cuenta ACKs (frames de 4 bytes sin payload) para saber si está viva.
void bridge_poll() {
    while (Serial1.available()) {
        if (rx_len == sizeof(rx_buf)) rx_len = 0;
        rx_buf[rx_len++] = Serial1.read();
        while (rx_len >= GF_FRAME_MIN_SIZE) {
            size_t total = GF_FRAME_MIN_SIZE + rx_buf[1];
            if (rx_len < total) break;
            if (gf_crc8(rx_buf, total - 1) == rx_buf[total - 1]) {
                if (rx_buf[0] == GF_CMD_ACK || rx_buf[0] == GF_CMD_VERSION) { acks++; last_ack_ms = millis(); }
                memmove(rx_buf, rx_buf + total, rx_len - total);
                rx_len -= total;
            } else {
                memmove(rx_buf, rx_buf + 1, --rx_len);   // resync de a un byte
            }
        }
    }
    bool up = last_ack_ms && millis() - last_ack_ms < 3000;
    if (up != link_up) {
        link_up = up;
        Serial.printf("[bridge] cabeza %s\n", up ? "CONECTADA (ACKs)" : "sin respuesta");
        if (up) { bridge_engine(); bridge_filter(); bridge_patch(); bridge_bpm(); bridge_ui_state(); }   // al conectar, todo
    }
}

float note_hz(uint8_t note) { return 440.0f * powf(2.0f, (note - 69) / 12.0f); }

void note_on(byte ch, byte note, byte vel) {
    if (vel == 0) return;
    held_note = note;
    osc.frequency(note_hz(note));
    osc.amplitude(0.25f + 0.5f * vel / 127.0f);
    env.noteOn();
    fenv.noteOn();
    Serial.printf("[MIDI-ON] ch=%u note=%u vel=%u  cpu=%.1f%%\n", ch, note, vel, AudioProcessorUsage());
    const uint8_t p[2] = {note, vel};
    bridge_send(GF_CMD_NOTE_ON, p, 2);
}

void note_off(byte ch, byte note, byte vel) {
    if (note == held_note) { env.noteOff(); fenv.noteOff(); }
}

/// MIDI clock (24 ppq): promedio móvil del intervalo → BPM para el SYNC del LFO.
void clock_tick() {
    const uint32_t now = micros();
    const uint32_t d = now - last_clock_us;
    last_clock_us = now;
    if (d > 100000) return;                               // primer pulso o reanudación: no promediar
    clock_us_avg = clock_us_avg == 0.0f ? d : clock_us_avg + 0.05f * (d - clock_us_avg);
    bpm = constrain(60e6f / (clock_us_avg * 24.0f), 30.0f, 300.0f);
}

void control_change(byte ch, byte cc, byte val) {
    if (cc == 74) { cutoff = 60.0f * powf(2.0f, val / 127.0f * 8.0f); filt.frequency(cutoff); bridge_filter(); }
    if (cc == 71) { resonance = 0.7f + val / 127.0f * 4.3f; filt.resonance(resonance); bridge_filter(); }
    if (cc == 1) wheel = val / 127.0f;
    Serial.printf("[CC] %u = %u\n", cc, val);
}

void test_tone(uint32_t ms) {
    osc.frequency(440.0f);
    osc.amplitude(0.6f);
    env.noteOn();
    fenv.noteOn();
    delay(ms);
    env.noteOff();
    fenv.noteOff();
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 1500) {
    }
    Serial.println("\n=== sketch48 — e2e de hardware: USB host MIDI → voz → I2S → LINE OUT ===");
    if (CrashReport) Serial.print(CrashReport);

    AudioMemory(20);
    bool ok = codec.enable();
    codec.volume(0.6f);
    codec.lineOutLevel(13);                 // el más alto (3.16 Vpp); la entrada de la interfaz puede bajar
    Serial.printf("[codec] SGTL5000 enable: %s\n", ok ? "OK" : "FALLA (revisar I2C 18/19 y 3.3 V)");

    osc.begin(ENGINE_WAVE[0]);
    osc.frequencyModulation(1);
    filt.frequency(cutoff);
    filt.resonance(resonance);
    filt.octaveControl(FILT_OCT);
    fenv_src.amplitude(1.0f);
    fmix.gain(1, 1.0f);
    amp_dc.amplitude(1.0f);
    apply_patch();
    mix.gain(0, 0.8f);

    Serial.println("[test] La 440 Hz durante 0.5 s por LINE OUT");
    test_tone(500);
    Serial.printf("[test] cpu=%.2f%% (>0 = interrupción de audio corriendo)\n", AudioProcessorUsageMax());

    for (int i = 0; i < 8; i++) btn[i].attach(BTN_PINS[i], INPUT_PULLUP), btn[i].interval(5);

    Serial1.begin(921600);                  // bridge con la Pi: TX1 (1) → GPIO15, RX1 (0) ← GPIO14
    usb_host.begin();                       // enciende el 5 V del puerto host y empieza a enumerar
    midi.setHandleNoteOn(note_on);
    midi.setHandleNoteOff(note_off);
    midi.setHandleControlChange(control_change);
    midi.setHandleClock(clock_tick);
    Serial.println("[host] USB host iniciado: conectar el teclado MIDI");
}

void loop() {
    usb_host.Task();
    midi.read();
    bridge_poll();
    if (millis() - last_hb >= 1000) { last_hb = millis(); bridge_send(GF_CMD_HEARTBEAT, nullptr, 0); }
    if (millis() - last_engine >= 5000) { last_engine = millis(); bridge_engine(); }
    if (last_clock_us && millis() - last_bpm_tx >= 1000) bridge_bpm();   // con clock, el BPM 1 vez por segundo
    mod_tick();

    if (bool(midi) != host_seen) {
        host_seen = bool(midi);
        Serial.printf("[host] teclado MIDI %s\n", host_seen ? "conectado" : "desconectado");
    }

    int32_t l = (enc_l.read() >> 2), r = (enc_r.read() >> 2), n = (enc_nav.read() >> 2);
    if (l != last_l) {
        cutoff = constrain(cutoff * powf(1.06f, (float)(l - last_l)), 60.0f, 15000.0f);
        filt.frequency(cutoff);
        bridge_filter();
        last_l = l;
        Serial.printf("[ENC L] cutoff=%.0f Hz\n", cutoff);
    }
    if (r != last_r) {
        resonance = constrain(resonance + 0.1f * (r - last_r), 0.7f, 5.0f);
        filt.resonance(resonance);
        bridge_filter();
        last_r = r;
        Serial.printf("[ENC R] resonancia=%.1f\n", resonance);
    }
    const int32_t p1 = (enc_p1.read() >> 2), p2 = (enc_p2.read() >> 2);
    if (p1 != last_p1) { page_encoder(0, p1 - last_p1); last_p1 = p1; }
    if (p2 != last_p2) { page_encoder(1, p2 - last_p2); last_p2 = p2; }
    if (n != last_nav) {
        ui_control(studio::Control::Nav, int8_t(constrain(n - last_nav, -8, 8)));
        last_nav = n;
    }
    using studio::Control;
    static const Control PRESS[8] = {Control::Key1, Control::Key2, Control::Key3, Control::Key4, Control::Mode,
                                     Control::B6Down, Control::NavPush, Control::EncLPush};
    for (int i = 0; i < 8; i++) {
        btn[i].update();
        if (btn[i].fell()) ui_control(PRESS[i]);
        if (i == 5 && btn[i].rose()) ui_control(Control::B6Up);   // SHIFT / HOLD momentáneos
    }
    // UI_STATE: en cada cambio (como máximo cada 20 ms, coalescido) y 1 Hz para converger (theory/04 §5.1)
    if ((millis() - last_ui_tx >= 20 && ui.take_dirty()) || millis() - last_ui_beat >= 1000) bridge_ui_state();

    if (millis() - last_stats > 5000) {
        last_stats = millis();
        Serial.printf("-- cpu %.1f%% (máx %.1f%%) · memoria de audio %d (máx %d) · teclado %s · cabeza %s (%lu ACKs)\n",
                      AudioProcessorUsage(), AudioProcessorUsageMax(), AudioMemoryUsage(), AudioMemoryUsageMax(),
                      host_seen ? "sí" : "no", link_up ? "sí" : "no", (unsigned long)acks);
    }
}
