/**
 * 48-studio-e2e-hw.cpp — Prueba e2e del hardware de Studio antes del pedido de la portadora.
 *
 * Valida la cadena completa con piezas mínimas, sin engines, ML ni bridge:
 *   teclado MIDI en el USB host → voz (Teensy Audio Library oficial) → I2S → SGTL5000 → LINE OUT
 *   + ENC L / ENC R / NAV y B1–B4 cambiando parámetros en vivo.
 * Si esto suena y el sketch 28 no, el problema es de firmware, no de la placa.
 *
 * Voz (todo de la Teensy Audio Library, nada custom):
 *   AudioSynthWaveform (saw/square/triangle/sine) → AudioFilterStateVariable (LP)
 *   → AudioEffectEnvelope → AudioMixer4 → AudioOutputI2S (L y R)
 *
 * Controles (pines del ADR-004, los mismos de la portadora):
 *   ENC L (10/11) cutoff · ENC R (13/14) resonancia · NAV (16/17) octava
 *   B1–B4 (2–5) forma de onda · ENC L push (12) tono de prueba La 440
 *
 * Al arrancar suena un La de 440 Hz durante 0.5 s (sin teclado) para probar el LINE OUT.
 *
 * Bridge con la cabeza de la Pi (Serial1, pines 0/1, 921600; 02-bridge-protocol.md): el Teensy es el
 * master. HEARTBEAT cada 1 s, ENGINE_CHANGED (Moog) al arrancar y cada 5 s, PARAM_CHANGED de cutoff y
 * resonancia con el mismo param_id y la misma normalización que el firmware real (grupo FILTER del
 * Moog: cutoff 20–8000 Hz, resonancia 0.1–5) y NOTE_ON con cada nota. La cabeza contesta ACK.
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

AudioSynthWaveform osc;
AudioFilterStateVariable filt;
AudioEffectEnvelope env;
AudioMixer4 mix;
AudioOutputI2S i2s_out;
AudioControlSGTL5000 codec;
AudioConnection c1(osc, 0, filt, 0);
AudioConnection c2(filt, 0, env, 0);     // salida 0 = low-pass
AudioConnection c3(env, 0, mix, 0);
AudioConnection c4(mix, 0, i2s_out, 0);
AudioConnection c5(mix, 0, i2s_out, 1);

USBHost usb_host;
MIDIDevice midi(usb_host);

Encoder enc_l(10, 11), enc_r(13, 14), enc_nav(16, 17);
Bounce btn[4], push_l;
constexpr uint8_t BTN_PINS[4] = {2, 3, 4, 5};
constexpr short WAVES[4] = {WAVEFORM_SAWTOOTH, WAVEFORM_SQUARE, WAVEFORM_TRIANGLE, WAVEFORM_SINE};
const char* WAVE_NAMES[4] = {"saw", "square", "triangle", "sine"};

float cutoff = 2000.0f, resonance = 1.2f;
int octave = 0;
uint8_t held_note = 255;
int32_t last_l = 0, last_r = 0, last_nav = 0;
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
    const uint8_t p[] = {0, 'M', 'O', 'O', 'G', ' ', 'M', 'O', 'D', 'E', 'L', ' ', 'D', 0};
    bridge_send(GF_CMD_ENGINE_CHANGED, p, sizeof(p));
}

// Filtro del Moog (engine 0, grupo 2): param_id 0x1020 cutoff, 0x1021 resonancia
void bridge_filter() {
    bridge_param(0x1020, (cutoff - 20.0f) / (8000.0f - 20.0f));
    bridge_param(0x1021, (resonance - 0.1f) / (5.0f - 0.1f));
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
        if (up) { bridge_engine(); bridge_filter(); }   // al conectar, mandar el estado completo
    }
}

float note_hz(uint8_t note) { return 440.0f * powf(2.0f, (note + 12 * octave - 69) / 12.0f); }

void note_on(byte ch, byte note, byte vel) {
    if (vel == 0) return;
    held_note = note;
    osc.frequency(note_hz(note));
    osc.amplitude(0.25f + 0.5f * vel / 127.0f);
    env.noteOn();
    Serial.printf("[MIDI-ON] ch=%u note=%u vel=%u  cpu=%.1f%%\n", ch, note, vel, AudioProcessorUsage());
    const uint8_t p[2] = {note, vel};
    bridge_send(GF_CMD_NOTE_ON, p, 2);
}

void note_off(byte ch, byte note, byte vel) {
    if (note == held_note) env.noteOff();
}

void control_change(byte ch, byte cc, byte val) {
    if (cc == 74) { cutoff = 60.0f * powf(2.0f, val / 127.0f * 8.0f); filt.frequency(cutoff); bridge_filter(); }
    if (cc == 71) { resonance = 0.7f + val / 127.0f * 4.3f; filt.resonance(resonance); bridge_filter(); }
    Serial.printf("[CC] %u = %u\n", cc, val);
}

void test_tone(uint32_t ms) {
    osc.frequency(440.0f);
    osc.amplitude(0.6f);
    env.noteOn();
    delay(ms);
    env.noteOff();
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

    osc.begin(WAVEFORM_SAWTOOTH);
    filt.frequency(cutoff);
    filt.resonance(resonance);
    env.attack(5);
    env.decay(120);
    env.sustain(0.7f);
    env.release(250);
    mix.gain(0, 0.8f);

    Serial.println("[test] La 440 Hz durante 0.5 s por LINE OUT");
    test_tone(500);
    Serial.printf("[test] cpu=%.2f%% (>0 = interrupción de audio corriendo)\n", AudioProcessorUsageMax());

    for (int i = 0; i < 4; i++) btn[i].attach(BTN_PINS[i], INPUT_PULLUP), btn[i].interval(5);
    push_l.attach(12, INPUT_PULLUP);
    push_l.interval(5);

    Serial1.begin(921600);                  // bridge con la Pi: TX1 (1) → GPIO15, RX1 (0) ← GPIO14
    usb_host.begin();                       // enciende el 5 V del puerto host y empieza a enumerar
    midi.setHandleNoteOn(note_on);
    midi.setHandleNoteOff(note_off);
    midi.setHandleControlChange(control_change);
    Serial.println("[host] USB host iniciado: conectar el teclado MIDI");
}

void loop() {
    usb_host.Task();
    midi.read();
    bridge_poll();
    if (millis() - last_hb >= 1000) { last_hb = millis(); bridge_send(GF_CMD_HEARTBEAT, nullptr, 0); }
    if (millis() - last_engine >= 5000) { last_engine = millis(); bridge_engine(); }

    if (bool(midi) != host_seen) {
        host_seen = bool(midi);
        Serial.printf("[host] teclado MIDI %s\n", host_seen ? "conectado" : "desconectado");
    }

    int32_t l = enc_l.read() / 4, r = enc_r.read() / 4, n = enc_nav.read() / 4;
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
    if (n != last_nav) {
        octave = constrain(octave + (n - last_nav), -2, 2);
        last_nav = n;
        Serial.printf("[NAV] octava=%+d\n", octave);
    }
    for (int i = 0; i < 4; i++) {
        btn[i].update();
        if (btn[i].fell()) {
            osc.begin(WAVES[i]);
            Serial.printf("[B%d] forma de onda: %s\n", i + 1, WAVE_NAMES[i]);
        }
    }
    push_l.update();
    if (push_l.fell()) {
        Serial.println("[ENC L push] tono de prueba");
        test_tone(300);
    }

    if (millis() - last_stats > 5000) {
        last_stats = millis();
        Serial.printf("-- cpu %.1f%% (máx %.1f%%) · memoria de audio %d (máx %d) · teclado %s · cabeza %s (%lu ACKs)\n",
                      AudioProcessorUsage(), AudioProcessorUsageMax(), AudioMemoryUsage(), AudioMemoryUsageMax(),
                      host_seen ? "sí" : "no", link_up ? "sí" : "no", (unsigned long)acks);
    }
}
