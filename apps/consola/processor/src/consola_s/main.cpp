#include <Arduino.h>
#include <Wire.h>
#include <Encoder.h>
#include <IntervalTimer.h>
#include <cstring>
#include "consola_s/FsrVelocity.h"
#include "consola_s/AnalogChange.h"
#include "consola_s/RelativeEncoder.h"
#include "consola_s/AdsScanner.h"
#include "consola_s/HandSensor.h"
#include "shared/BinaryProtocol.h"
#include "MidiCommands.h"

namespace {
constexpr uint8_t MIDI_CHANNEL = 1;
constexpr uint8_t FSR_PINS[] = {14, 15, 16, 17};
constexpr uint8_t FSR_ADC_CHANNELS[] = {7, 8, 12, 11};
constexpr uint8_t POT_PINS[] = {23, 38, 39};
constexpr uint8_t POT_ADC2_CHANNELS[] = {1, 2};
constexpr uint8_t MCP_PINS[] = {0,1,2,3,4,5,6,8,9,10,11,12,13,14};
constexpr uint8_t QUEUE_SIZE = 64;
FsrVelocity pads[] = {
    FsrVelocity({160,3000,100}), FsrVelocity({160,3000,100}),
    FsrVelocity({160,3000,100}), FsrVelocity({160,3000,100})
};
// 8 encoders del panel + rueda jog EC11 (29/36, cable directo en la protoboard).
Encoder encoders[] = {
    {2,3}, {4,5}, {9,10}, {11,12}, {22,24}, {26,30}, {31,32}, {34,35}, {29,36}
};
constexpr uint8_t ENCODER_COUNT = sizeof(encoders) / sizeof(encoders[0]);
constexpr uint8_t ENCODER_CC[ENCODER_COUNT] = {16,17,18,19,20,21,22,23,35};
RelativeEncoder relative[ENCODER_COUNT];
int32_t encoder_positions[ENCODER_COUNT] = {};
AnalogChange pots[3];
AnalogChange ads_controls[] = {
    AnalogChange(26400), AnalogChange(26400), AnalogChange(26400),
    AnalogChange(26400), AnalogChange(26400), AnalogChange(26400)
};
AdsScanner ads[] = {{Wire,0x48,4}, {Wire,0x49,4}};
HandSensor hands[2];
IntervalTimer fsr_timer;
uint8_t m4_tx_memory[256];
struct PadEvent { uint8_t pad; bool on; uint8_t velocity; };
volatile PadEvent queue[QUEUE_SIZE];
volatile uint8_t queue_head = 0, queue_tail = 0;
volatile uint32_t scans = 0, scan_gap_max = 0, queue_drops = 0;
uint32_t last_scan = 0;
uint32_t stats_start = 0, loop_max = 0, midi_events = 0, ads_reads[2] = {};
uint32_t ads_error_base[2] = {}, mcp_errors = 0;
uint32_t last_buttons = 0, last_pots = 0, last_handshake = 0;
uint8_t next_ads = 0, next_mcp = 0;
bool mcp_ready[2] = {};
uint32_t mcp_retry[2] = {};
uint16_t button_candidate[2] = {0xffff,0xffff}, button_stable[2] = {0xffff,0xffff};
uint32_t button_since[2][14] = {};
bool m4_held[32] = {}, legacy_pulse[32] = {};
uint32_t legacy_since[32] = {};
bool m4_connected = false;
uint32_t m4_last_rx = 0;
uint8_t m4_buffer[259];
uint16_t m4_size = 0;
uint32_t m4_byte_time = 0;
char command[32];
size_t command_size = 0;
bool command_overflow = false;
char response[256];
size_t response_size = 0, response_sent = 0;

uint16_t read_adc1(uint8_t channel) {
    // PJRC analogRead calls yield while waiting; ISR must never run host callbacks.
    ADC1_HC0 = channel;
    while (!(ADC1_HS & ADC_HS_COCO0)) {}
    return ADC1_R0;
}
uint16_t read_adc2(uint8_t channel) {
    ADC2_HC0 = channel;
    while (!(ADC2_HS & ADC_HS_COCO0)) {}
    return ADC2_R0;
}
void scan_fsr() {
    const uint32_t now = micros();
    if (scans && now - last_scan > scan_gap_max) scan_gap_max = now - last_scan;
    last_scan = now;
    ++scans;
    for (uint8_t i = 0; i < 4; ++i) {
        const auto event = pads[i].sample(read_adc1(FSR_ADC_CHANNELS[i]), now);
        if (!event.changed) continue;
        const uint8_t next = (queue_head + 1) % QUEUE_SIZE;
        if (next == queue_tail) { ++queue_drops; continue; }
        queue[queue_head].pad = i;
        queue[queue_head].on = event.on;
        queue[queue_head].velocity = event.velocity;
        __asm__ volatile("dmb" ::: "memory");
        queue_head = next;
    }
}
void send_note(uint8_t note, bool on, uint8_t velocity) {
    if (on) usbMIDI.sendNoteOn(note, velocity, MIDI_CHANNEL);
    else usbMIDI.sendNoteOff(note, 0, MIDI_CHANNEL);
    ++midi_events;
}
void send_cc(uint8_t cc, uint8_t value) {
    usbMIDI.sendControlChange(cc, value, MIDI_CHANNEL);
    ++midi_events;
}
void send_hand(uint8_t index, const HandSensor::Event& event) {
    if (event.presence_changed) send_note(108+index,event.present,127);
    if (event.cc_changed) send_cc(33+index,event.value);
}
void drain_pads() {
    // Bound work so a disconnected/slow host cannot monopolize a loop iteration.
    for (uint8_t n = 0; n < 8 && queue_tail != queue_head; ++n) {
        const uint8_t index = queue_tail;
        __asm__ volatile("dmb" ::: "memory");
        const PadEvent event{queue[index].pad, queue[index].on, queue[index].velocity};
        queue_tail = (index + 1) % QUEUE_SIZE;
        send_note(36 + event.pad, event.on, event.velocity);
    }
}
bool mcp_write(uint8_t chip, uint8_t reg, uint16_t value) {
    Wire.beginTransmission(0x20 + chip);
    Wire.write(reg);
    Wire.write(uint8_t(value));
    Wire.write(uint8_t(value >> 8));
    return Wire.endTransmission() == 0;
}
void poll_buttons(uint32_t now) {
    if (now - last_buttons < 1000) return;
    last_buttons = now;
    const uint8_t chip = next_mcp;
    next_mcp ^= 1;
    if (!mcp_ready[chip]) {
        if (now - mcp_retry[chip] < 1000000) return;
        mcp_retry[chip] = now;
        // BANK=0 power-on register layout; output-only A7/B7 stay outputs.
        if (!mcp_write(chip,0x00,0x7f7f) || !mcp_write(chip,0x0c,0x7f7f)) {
            ++mcp_errors;
            return;
        }
        mcp_ready[chip] = true;
        return;
    }
    Wire.beginTransmission(0x20 + chip);
    Wire.write(0x12);
    if (Wire.endTransmission(false) || Wire.requestFrom(uint8_t(0x20 + chip),uint8_t(2)) != 2) {
        ++mcp_errors;
        mcp_ready[chip] = false;
        mcp_retry[chip] = now;
        return;
    }
    uint16_t value = Wire.read();
    value |= uint16_t(Wire.read()) << 8;
    for (uint8_t i = 0; i < 14; ++i) {
        const uint16_t mask = 1u << MCP_PINS[i];
        if ((value ^ button_candidate[chip]) & mask) {
            button_candidate[chip] ^= mask;
            button_since[chip][i] = now;
        }
        if (((value ^ button_stable[chip]) & mask) && now - button_since[chip][i] >= 5000) {
            button_stable[chip] ^= mask;
            send_note(80 + chip * 14 + i, !(value & mask), 127);
        }
    }
}
void m4_send(uint8_t cmd, const uint8_t* data, uint8_t size) {
    uint8_t frame[100];
    const auto length = BinaryProtocol::buildMessage(cmd,data,size,frame,sizeof(frame));
    if (length && Serial5.availableForWrite() >= length) Serial5.write(frame,length);
}
void m4_note(uint8_t pad, bool on) {
    if (pad >= 32 || m4_held[pad] == on) return;
    m4_held[pad] = on;
    send_note(48 + pad, on, 100);
}
void m4_frame() {
    uint8_t cmd, length;
    const uint8_t* data;
    if (!BinaryProtocol::parseMessage(m4_buffer,m4_size,cmd,data,length)) return;
    m4_last_rx = millis();
    if (cmd == CMD_HANDSHAKE_REPLY) {
        m4_connected = true;
        uint8_t black[96] = {};
        m4_send(CMD_LED_GRID_UPDATE,black,sizeof(black));
        m4_send(CMD_ENABLE_KEYS,nullptr,0);
    } else if ((cmd == CMD_GRID_PAD_PRESS && length == 3) ||
               (cmd == CMD_CLIP_TRIGGER && length == 2)) {
        if (data[0] >= 8 || data[1] >= 4) return;
        const uint8_t pad = data[1] * 8 + data[0];
        const bool on = length == 2 || data[2] != 0;
        if (length == 2 && m4_held[pad]) m4_note(pad,false);
        m4_note(pad,on);
        legacy_pulse[pad] = length == 2;
        legacy_since[pad] = millis();
    } else if (cmd == CMD_DISCONNECT) {
        m4_connected = false;
        for (uint8_t i = 0; i < 32; ++i) m4_note(i,false);
    }
}
void poll_m4() {
    const uint32_t now = millis();
    if (m4_size && now - m4_byte_time > 10) m4_size = 0;
    for (uint8_t n = 0; n < 32 && Serial5.available(); ++n) {
        const uint8_t byte = Serial5.read();
        m4_byte_time = now;
        if (!m4_size && byte != 0xaa) continue;
        m4_buffer[m4_size++] = byte;
        if (m4_size >= 3 && m4_size == uint16_t(m4_buffer[2]) + 4) {
            m4_frame();
            m4_size = 0;
        }
    }
    for (uint8_t i = 0; i < 32; ++i)
        if (legacy_pulse[i] && now - legacy_since[i] >= 10) {
            m4_note(i,false);
            legacy_pulse[i] = false;
        }
    if (m4_connected && now - m4_last_rx > 5000) {
        for (uint8_t i = 0; i < 32; ++i) m4_note(i,false);
        m4_connected = false;
    }
    if (now - last_handshake >= 1000) {
        last_handshake = now;
        if (m4_connected) m4_send(CMD_PING,nullptr,0);
        else {
            const uint8_t name[] = {'P','U','S','H','C','L','O','N','E'};
            m4_send(CMD_HANDSHAKE,name,sizeof(name));
        }
    }
}
void reset_stats() {
    noInterrupts();
    scans = scan_gap_max = queue_drops = 0;
    interrupts();
    stats_start = millis();
    loop_max = midi_events = ads_reads[0] = ads_reads[1] = mcp_errors = 0;
    for (uint8_t i = 0; i < 2; ++i) ads_error_base[i] = ads[i].errors();
}
void prepare_stats() {
    noInterrupts();
    const uint32_t count = scans, gap = scan_gap_max, drops = queue_drops;
    interrupts();
    const uint32_t elapsed = max(uint32_t(1), millis() - stats_start);
    response_size = snprintf(response,sizeof(response),
        "@S PROTOTYPE loop_max_us=%lu fsr_hz=%lu scan_gap_max_us=%lu ads0_sps=%lu ads1_sps=%lu events_s=%lu i2c_errors=%lu drops=%lu\n",
        (unsigned long)loop_max, (unsigned long)(uint64_t(count)*1000/elapsed),
        (unsigned long)gap, (unsigned long)(uint64_t(ads_reads[0])*1000/elapsed),
        (unsigned long)(uint64_t(ads_reads[1])*1000/elapsed),
        (unsigned long)(uint64_t(midi_events)*1000/elapsed),
        (unsigned long)(mcp_errors + ads[0].errors()-ads_error_base[0] + ads[1].errors()-ads_error_base[1]),
        (unsigned long)drops);
    if (response_size >= sizeof(response)) response_size = sizeof(response)-1;
    response_sent = 0;
}
void poll_serial() {
    for (uint8_t n = 0; n < 16 && Serial.available() && response_sent == response_size; ++n) {
        const char ch = Serial.read();
        if (ch == '\r') continue;
        if (ch == '\n') {
            command[command_size] = 0;
            if (!command_overflow && !strcmp(command,"@S STATS")) prepare_stats();
            else if (!command_overflow && !strcmp(command,"@S RESET")) {
                reset_stats();
                strcpy(response,"@S PROTOTYPE RESET OK\n");
                response_size = strlen(response);
                response_sent = 0;
            }
            command_size = 0;
            command_overflow = false;
        } else if (command_size < sizeof(command)-1) command[command_size++] = ch;
        else command_overflow = true;
    }
    if (response_sent < response_size && Serial) {
        const size_t space = Serial.availableForWrite();
        const size_t amount = min(size_t(32),min(space,response_size-response_sent));
        if (amount) response_sent += Serial.write(reinterpret_cast<uint8_t*>(response+response_sent),amount);
    }
}
} // namespace

void setup() {
    Serial.begin(115200);
    Serial5.addMemoryForWrite(m4_tx_memory,sizeof(m4_tx_memory));
    Serial5.begin(1000000);
    Wire.begin();
    Wire.setClock(400000);
    analogReadResolution(12);
    analogReadAveraging(1);
    for (const auto pin : FSR_PINS) pinMode(pin,INPUT_DISABLE);
    for (const auto pin : POT_PINS) pinMode(pin,INPUT_DISABLE);
    for (uint8_t i = 0; i < ENCODER_COUNT; ++i) encoder_positions[i] = encoders[i].read();
    analogRead(FSR_PINS[0]); // Finish PJRC ADC calibration before enabling ISR.
    reset_stats();
    fsr_timer.begin(scan_fsr,400);
    fsr_timer.priority(64);
}
void loop() {
    const uint32_t start = micros();
    drain_pads();
    poll_m4();
    for (uint8_t i = 0; i < ENCODER_COUNT; ++i) {
        const int32_t position = encoders[i].read();
        const int32_t delta = int32_t(uint32_t(position)-uint32_t(encoder_positions[i]));
        encoder_positions[i] = position;
        const uint8_t cc = relative[i].update(delta);
        if (cc) send_cc(ENCODER_CC[i],cc);
    }
    if (start - last_pots >= 1000) {
        last_pots = start;
        for (uint8_t i = 0; i < sizeof(POT_PINS); ++i) {
            int raw;
            if (i == 0) {
                // MASTER shares ADC1 with FSR; CUE/IN use independent ADC2.
                noInterrupts();
                raw = read_adc1(14);
                interrupts();
            } else raw = read_adc2(POT_ADC2_CHANNELS[i-1]);
            uint8_t value;
            if (pots[i].sample(raw,value)) send_cc(30+i,value);
        }
    }
    uint8_t channel;
    int16_t raw;
    const uint8_t chip = next_ads;
    next_ads ^= 1;
    if (ads[chip].poll(micros(),channel,raw)) {
        ++ads_reads[chip];
        if (chip == 1 && channel >= 2) send_hand(channel-2,hands[channel-2].sample(raw,millis()));
        else {
            const uint8_t control = chip ? 4+channel : channel;
            uint8_t value;
            if (ads_controls[control].sample(raw,value)) send_cc(24+control,value);
        }
    }
    for (uint8_t i = 0; i < 2; ++i) send_hand(i,hands[i].expire(millis()));
    poll_buttons(micros());
    poll_serial();
    for (uint8_t n = 0; n < 8 && usbMIDI.read(); ++n) {}
    usbMIDI.send_now();
    const uint32_t duration = micros()-start;
    if (duration > loop_max) loop_max = duration;
}
