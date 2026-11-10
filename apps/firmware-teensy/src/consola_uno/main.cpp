#include <Arduino.h>
#include <Wire.h>
#include "../audio/audio_graph.h"
#include "../ui/preset_link.h"
#include "consola_s/Surface.h"
#include "consola_s/RetryGate.h"
#ifdef GF_CONSOLA_BRAIN
#include "../engines/moog_model_d.h"
#endif

namespace {
RetryGate codec, dac;
uint8_t input_gain = 5, hp_volume = 64;
uint8_t applied_input = 255, applied_hp = 255;
uint16_t cv = 0, applied_cv = 65535;
uint32_t codec_probe = 0, dac_probe = 0;
#ifdef GF_CONSOLA_BRAIN
MoogModelD brain;
AudioConnection brain_left(brain.getOutput(),0,g_engine_mix_a,0);
AudioConnection brain_right(brain.getOutput(),0,g_engine_mix_ra,0);
void note_on(uint8_t channel, uint8_t note, uint8_t velocity) {
    if (channel == 1) brain.noteOn(note,velocity/127.0f);
}
void note_off(uint8_t channel, uint8_t note, uint8_t) {
    if (channel == 1) brain.noteOff(note);
}
void control_change(uint8_t channel, uint8_t cc, uint8_t) {
    if (channel == 1 && (cc == 120 || cc == 123)) brain.noteOff();
}
#endif
bool probe(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}
// SGTL5000 is configured at boot/recovery; changes coalesce to the latest pot value.
void poll_codec(uint32_t now) {
    if (!codec.due(now)) return;
    if (!codec.online()) {
        bool ok = probe(0x0a) && g_codec.enable();
        Wire.setClock(400000); // enable() calls Wire.begin() internally.
        if (ok) ok = g_codec.inputSelect(AUDIO_INPUT_LINEIN);
        if (ok) ok = g_codec.lineInLevel(input_gain) && g_codec.volume(hp_volume/127.0f);
        if (!ok) { codec.failed(micros()); return; }
        codec.succeeded();
        applied_input = input_gain;
        applied_hp = hp_volume;
        codec_probe = micros();
    } else if (applied_input != input_gain || applied_hp != hp_volume) {
        const bool ok = (applied_input == input_gain || g_codec.lineInLevel(input_gain)) &&
                        (applied_hp == hp_volume || g_codec.volume(hp_volume/127.0f));
        if (!ok) { codec.failed(micros()); return; }
        applied_input = input_gain;
        applied_hp = hp_volume;
    } else if (now - codec_probe >= 1000000) {
        codec_probe = now;
        if (!probe(0x0a)) codec.failed(micros());
    }
}
void poll_cv(uint32_t now) {
    if (!dac.due(now)) return;
    if (!dac.online() || applied_cv != cv) {
        // Fast mode writes the DAC register only; never program its EEPROM.
        Wire.beginTransmission(0x60);
        Wire.write(uint8_t(cv >> 8));
        Wire.write(uint8_t(cv));
        if (Wire.endTransmission()) { dac.failed(micros()); return; }
        dac.succeeded();
        applied_cv = cv;
        dac_probe = now;
    } else if (now - dac_probe >= 1000000) {
        dac_probe = now;
        if (!probe(0x60)) dac.failed(micros());
    }
}
void pot_changed(uint8_t pot, uint8_t value) {
    if (pot == 1) hp_volume = value;
    if (pot == 2) input_gain = uint16_t(value)*15/127;
}
void stats(char* out, size_t size) {
    snprintf(out,size," codec=%u cv=%u codec_errors=%lu cv_errors=%lu audio_cpu_max_x100=%lu audio_blocks_max=%u",
        unsigned(codec.online()),unsigned(dac.online()),(unsigned long)codec.errors(),(unsigned long)dac.errors(),
        (unsigned long)(AudioProcessorUsageMax()*100),unsigned(AudioMemoryUsageMax()));
}
bool command(char* line, char* out, size_t size) {
    if (strncmp(line,"@GF ",4)) return false;
    const auto c = studio::parse_link_cmd(line);
    using Cmd = studio::LinkCmd;
    switch (c.kind) {
    case Cmd::Ruta:
        audio_graph::set_usb_out_source(c.valor);
        snprintf(out,size,"@GF OK RUTA %s\n",c.valor ? "LINEA" : "SINTE");
        break;
    case Cmd::Cue:
        audio_graph::set_usb_passthrough(c.valor);
        snprintf(out,size,"@GF OK CUE %u\n",c.valor);
        break;
    case Cmd::In:
        input_gain = c.valor;
        snprintf(out,size,"@GF OK IN %u\n",c.valor);
        break;
    case Cmd::Hp:
        hp_volume = c.valor;
        snprintf(out,size,"@GF OK HP %u\n",c.valor);
        break;
    case Cmd::Cv:
        cv = c.cv;
        snprintf(out,size,"@GF OK CV %u\n",cv);
        break;
    case Cmd::Usb: {
        const auto rx = g_usb_in.getStatus();
        const auto tx = g_usb_out.getStatus();
        snprintf(out,size,"@GF USB RX_UND %lu RX_OVR %lu RX_MEM %lu TX_UND %lu TX_OVR %lu "
            "TX_SKIP %lu TX_PAD %lu TX_LESS %lu TX_MORE %lu RX_ACTIVE %u TX_ACTIVE %u\n",
            (unsigned long)rx.usb_audio_underrun_count,(unsigned long)rx.usb_audio_overrun_count,
            (unsigned long)rx.audio_memory_underrun_count,(unsigned long)tx.usb_audio_underrun_count,
            (unsigned long)tx.usb_audio_overrun_count,(unsigned long)tx.num_skipped_Samples,
            (unsigned long)tx.num_padded_Samples,(unsigned long)tx.num_send_one_less,
            (unsigned long)tx.num_send_one_more,unsigned(rx.receivingData),unsigned(tx.transmittingData));
        break;
    }
    default: snprintf(out,size,"@GF ERR COMANDO\n"); break;
    }
    return true;
}
}
#ifdef GF_DIAG_NO_FSR
#define DIAG(msg) do { Serial.println(msg); Serial.send_now(); } while (0)
#else
#define DIAG(msg) do {} while (0)
#endif
void setup() {
#ifdef GF_DIAG_NO_FSR
    Serial.begin(115200);
    while (!Serial && millis() < 4000) {}
    DIAG("diag: setup inicio");
#endif
    surface::begin(command,pot_changed,stats);
    DIAG("diag: surface::begin listo");
    // Reuse the existing graph, with no engine/FX objects unless explicitly enabled.
    if (audio_graph::init(hp_volume/127.0f)) {
        codec.succeeded();
        applied_input = input_gain;
        applied_hp = hp_volume;
    } else codec.failed(micros());
    DIAG("diag: audio_graph::init listo");
    Wire.setClock(400000);
    audio_graph::set_usb_out_source(1);
    DIAG("diag: setup fin");
#ifdef GF_CONSOLA_BRAIN
    brain.begin();
    usbMIDI.setHandleNoteOn(note_on);
    usbMIDI.setHandleNoteOff(note_off);
    usbMIDI.setHandleControlChange(control_change);
#endif
}
void loop() {
    const uint32_t start = micros();
#ifdef GF_DIAG_NO_FSR
    static uint32_t beat = 0, stage = 0;
    if (millis() - beat > 1000) { beat = millis(); Serial.printf("diag: latido etapa %lu\n", (unsigned long)stage); Serial.send_now(); }
    stage = 1;
#endif
    surface::poll();
#ifdef GF_DIAG_NO_FSR
    stage = 2;
#endif
#ifdef GF_CONSOLA_BRAIN
    brain.update();
#endif
    poll_codec(micros());
#ifdef GF_DIAG_NO_FSR
    stage = 3;
#endif
    poll_cv(micros());
    surface::record_loop_duration(micros()-start);
}
