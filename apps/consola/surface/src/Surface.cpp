#ifdef ARDUINO
#include <Arduino.h>
#include <Wire.h>
#include <Encoder.h>
#include <IntervalTimer.h>
#include <cstring>
#include "consola_s/Surface.h"
#include "consola_s/AdcScan.h"
#include "consola_s/CcCadence.h"
#include "consola_s/ConsolaMap.h"
#include "consola_s/PadSelector.h"
#include "consola_s/McpButtons.h"
#include "consola_s/FsrVelocity.h"
#include "consola_s/AnalogChange.h"
#include "consola_s/RelativeEncoder.h"
#include "consola_s/AdsScanner.h"
#include "consola_s/HandSensor.h"
#include "shared/BinaryProtocol.h"
#include "MidiCommands.h"

namespace {
constexpr uint8_t MIDI_CHANNEL = consola::MIDI_CHANNEL;
constexpr uint8_t FSR_PINS[] = {14, 15, 16, 17};
constexpr uint8_t ADC1_CHANNELS[] = {7, 8, 12, 11, 1, 10};
#ifdef GF_CONSOLA_UNO
constexpr uint8_t POT_PINS[] = {22,24,26};
constexpr auto PROFILE = GF_CONSOLA_MINIMAL ? consola::MINIMAL : consola::FULL;
constexpr uint8_t MCP_COUNT = 3;
#define M4_SERIAL Serial7
#else
constexpr uint8_t POT_PINS[] = {23,38,39};
constexpr consola::Profile PROFILE{0x1ff,0x0f,0x07,{0x0f,0x0f},{0x7f7f,0x7f7f,0},false};
constexpr uint8_t MCP_COUNT = 2;
#define M4_SERIAL Serial5
#endif
surface::CommandHandler command_handler = nullptr;
surface::PotHandler pot_handler = nullptr;
surface::StatsHandler stats_handler = nullptr;
PadSelector selector;
#ifdef GF_CONSOLA_UNO
constexpr uint8_t ADC1_MASK = PROFILE.fsrs | ((PROFILE.pots & 2) ? 0x10 : 0) | (PROFILE.selector ? 0x20 : 0);
#else
constexpr uint8_t ADC1_MASK = PROFILE.fsrs;
#endif
AdcScan adc_scan(ADC1_MASK);
volatile uint16_t slow_adc[2] = {};
volatile uint8_t slow_valid = 0;
volatile uint32_t adc1_timeouts = 0;
uint32_t adc2_timeouts = 0;
bool fsr_timer_ok = false;
CcCadence cc_cadence[128];
uint8_t cc_latest[128] = {};
bool cc_pending[128] = {};
constexpr uint8_t QUEUE_SIZE = 64;
FsrVelocity pads[] = {
    // 12-bit ADC: 600 ≈ 0.48 V on, 400 ≈ 0.32 V off. 160/100 (0.13/0.08 V) false-triggered on the breadboard with the
    // Sharp pulsing on the shared ground (2026-11-10, ~2800 notes/min untouched); per-pad calibration still pending.
    FsrVelocity({600,3000,400}), FsrVelocity({600,3000,400}),
    FsrVelocity({600,3000,400}), FsrVelocity({600,3000,400})
};

Encoder encoders[] = {
#ifdef GF_CONSOLA_UNO
#define GF_ENCODER(n) {consola::ENCODER_PINS[n][0],consola::ENCODER_PINS[n][1]}
    GF_ENCODER(0),GF_ENCODER(1),GF_ENCODER(2),GF_ENCODER(3),GF_ENCODER(4),
    GF_ENCODER(5),GF_ENCODER(6),GF_ENCODER(7),GF_ENCODER(8)
#undef GF_ENCODER
#else
    {2,3}, {4,5}, {9,10}, {11,12}, {22,24}, {26,30}, {31,32}, {34,35}, {29,36}
#endif
};
constexpr uint8_t ENCODER_COUNT = sizeof(encoders) / sizeof(encoders[0]);
constexpr auto& ENCODER_CC = consola::ENCODER_CC;
RelativeEncoder relative[ENCODER_COUNT];
int32_t encoder_positions[ENCODER_COUNT] = {};
AnalogChange pots[3];
AnalogChange ads_controls[] = {
    AnalogChange(26400), AnalogChange(26400), AnalogChange(26400),
    AnalogChange(26400), AnalogChange(26400), AnalogChange(26400)
};
AdsScanner ads[] = {{Wire,0x48,4,PROFILE.ads_channels[0]}, {Wire,0x49,4,PROFILE.ads_channels[1]}};
HandSensor hands[2];
IntervalTimer fsr_timer;
uint8_t m4_tx_memory[256];
struct PadEvent { uint8_t pad; bool on; uint8_t velocity; };
volatile PadEvent queue[QUEUE_SIZE];
volatile uint8_t queue_head = 0, queue_tail = 0;
volatile uint32_t scans = 0, scan_gap_max = 0, queue_drops = 0, fsr_samples[4] = {};
uint32_t last_scan = 0;
uint32_t stats_start = 0, loop_max = 0, midi_events = 0, ads_reads[2] = {};
uint32_t ads_error_base[2] = {}, mcp_errors = 0;
uint32_t last_buttons = 0, last_pots = 0, last_handshake = 0;
uint8_t next_ads = 0, next_mcp = 0;
#ifdef GF_CONSOLA_UNO
uint32_t last_selector = 0;
#endif
RetryGate mcp[MCP_COUNT];
bool mcp_configured[MCP_COUNT] = {};
McpButtons buttons[] = {McpButtons(0,PROFILE.mcp_inputs[0]), McpButtons(1,PROFILE.mcp_inputs[1])
#ifdef GF_CONSOLA_UNO
    ,McpButtons(2,PROFILE.mcp_inputs[2])
#endif
};
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
char response[1024];
size_t response_size = 0, response_sent = 0;

// ADC2 belongs to loop only. 20 us is well above a 12-bit single conversion.
// DWT keeps the bound independent of SysTick priority; no yield/callbacks here.
bool read_adc2(uint8_t channel, uint16_t& value) {
    const uint32_t began = ARM_DWT_CYCCNT;
    ADC2_HC0 = channel;
    while (!(ADC2_HS & ADC_HS_COCO0)) {
        if (uint32_t(ARM_DWT_CYCCNT - began) >= F_CPU_ACTUAL / 50000) {
            ADC2_HC0 = 31;
            ++adc2_timeouts;
            return false;
        }
    }
    value = ADC2_R0;
    return true;
}
void scan_fsr() {
    const uint32_t now = micros();
    const auto step = adc_scan.tick(now, ADC1_HS & ADC_HS_COCO0);
    if (step.timeout) { ADC1_HC0 = 31; ++adc1_timeouts; }
    if (step.frame) {
        if (scans && now - last_scan > scan_gap_max) scan_gap_max = now - last_scan;
        last_scan = now;
        ++scans;
    }
    if (step.sample >= 0) {
        const uint16_t raw = ADC1_R0;
        const uint8_t i = step.sample;
        if (i >= 4) {
            slow_adc[i-4] = raw;
            slow_valid |= 1u << (i-4);
        } else {
            ++fsr_samples[i];
            const auto event = pads[i].sample(raw,now);
            if (event.changed) {
                const uint8_t next = (queue_head + 1) % QUEUE_SIZE;
                if (next == queue_tail) ++queue_drops;
                else {
                    queue[queue_head].pad = i;
                    queue[queue_head].on = event.on;
                    queue[queue_head].velocity = event.velocity;
                    __asm__ volatile("dmb" ::: "memory");
                    queue_head = next;
                }
            }
        }
    }
    if (step.start >= 0) ADC1_HC0 = ADC1_CHANNELS[step.start];
}
void send_note(uint8_t note, bool on, uint8_t velocity) {
    if (on) usbMIDI.sendNoteOn(note, velocity, MIDI_CHANNEL);
    else usbMIDI.sendNoteOff(note, 0, MIDI_CHANNEL);
    ++midi_events;
}
void send_cc(uint8_t cc, uint8_t value) {
    cc_latest[cc] = value;
    cc_pending[cc] = true;
}
void flush_cc() {
    const uint32_t now = micros();
    for (uint8_t cc = 0; cc < 128; ++cc) {
        if (!cc_pending[cc] || !cc_cadence[cc].due(now)) continue;
        usbMIDI.sendControlChange(cc,cc_latest[cc],MIDI_CHANNEL);
        ++midi_events;
        if (pot_handler && cc >= 30 && cc <= 32) pot_handler(cc-30,cc_latest[cc]);
        cc_cadence[cc].sent(now);
        cc_pending[cc] = false;
    }
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
void send_buttons(const McpButtons::Event* events, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) send_note(events[i].note,events[i].on,127);
}
void poll_buttons(uint32_t now) {
    if (now - last_buttons < 1000) return;
    last_buttons = now;
    const uint8_t chip = next_mcp;
    next_mcp = (next_mcp + 1) % MCP_COUNT;
    if (!PROFILE.mcp_inputs[chip] || !mcp[chip].due(now)) return;
    if (!mcp_configured[chip]) {
        // BANK=0 power-on register layout; output-only A7/B7 stay outputs.
        if (!mcp_write(chip,0x00,0x7f7f) || !mcp_write(chip,0x0c,0x7f7f)) {
            ++mcp_errors;
            mcp[chip].failed(now);
            return;
        }
        mcp_configured[chip] = true; // Reset backoff only after GPIO read succeeds.
        return;
    }
    Wire.beginTransmission(0x20 + chip);
    Wire.write(0x12);
    if (Wire.endTransmission(false) || Wire.requestFrom(uint8_t(0x20 + chip),uint8_t(2)) != 2) {
        ++mcp_errors;
        mcp[chip].failed(now);
        mcp_configured[chip] = false;
        McpButtons::Event events[14];
        const uint8_t count = buttons[chip].disconnect(events);
        send_buttons(events,count);
        return;
    }
    mcp[chip].succeeded();
    uint16_t value = Wire.read();
    value |= uint16_t(Wire.read()) << 8;
    McpButtons::Event events[14];
    const uint8_t count = buttons[chip].sample(value,now,events);
    send_buttons(events,count);
}
void m4_send(uint8_t cmd, const uint8_t* data, uint8_t size) {
    uint8_t frame[100];
    const auto length = BinaryProtocol::buildMessage(cmd,data,size,frame,sizeof(frame));
    if (length && M4_SERIAL.availableForWrite() >= length) M4_SERIAL.write(frame,length);
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
    for (uint8_t n = 0; n < 32 && M4_SERIAL.available(); ++n) {
        const uint8_t byte = M4_SERIAL.read();
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
    scans = scan_gap_max = queue_drops = adc1_timeouts = 0;
    adc2_timeouts = 0;
    for (auto& samples : fsr_samples) samples = 0;
    interrupts();
    stats_start = millis();
    loop_max = midi_events = ads_reads[0] = ads_reads[1] = mcp_errors = 0;
    for (uint8_t i = 0; i < 2; ++i) ads_error_base[i] = ads[i].errors();
}
void prepare_stats() {
    noInterrupts();
    const uint32_t count = scans, gap = scan_gap_max, drops = queue_drops, adc_errors = adc1_timeouts;
    uint32_t samples[4];
    for (uint8_t i=0;i<4;++i) samples[i] = fsr_samples[i];
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
    if (response_size && response[response_size-1] == '\n') --response_size;
    uint8_t mcp_mask = 0;
    uint32_t retry_max = max(ads[0].retry_us(),ads[1].retry_us());
    for (uint8_t i = 0; i < MCP_COUNT; ++i) {
        if (mcp[i].online()) mcp_mask |= 1u << i;
        retry_max = max(retry_max,mcp[i].retry_us());
    }
    const int n = snprintf(response+response_size,sizeof(response)-response_size,
        " mcp_mask=%u ads_mask=%u m4=%u encoder_mask=%u fsr_mask=%u pot_mask=%u ads0_channels=%u ads1_channels=%u selector=%u mcp0_inputs=%u mcp1_inputs=%u mcp2_inputs=%u adc1_timeouts=%lu adc2_timeouts=%lu fsr_timer=%u wire_timeout_us=%u wire_timeouts=%lu retry_max_us=%lu fsr0_sps=%lu fsr1_sps=%lu fsr2_sps=%lu fsr3_sps=%lu adc1_cfg=%lx adc1_gc=%lx pit_priority=%u",
        mcp_mask, unsigned(ads[0].online() | (ads[1].online() << 1)), unsigned(m4_connected),
        PROFILE.encoders, PROFILE.fsrs, PROFILE.pots, PROFILE.ads_channels[0], PROFILE.ads_channels[1], unsigned(PROFILE.selector), PROFILE.mcp_inputs[0], PROFILE.mcp_inputs[1], PROFILE.mcp_inputs[2],
        (unsigned long)adc_errors,(unsigned long)adc2_timeouts,unsigned(fsr_timer_ok),
        unsigned(GF_WIRE_TIMEOUT_US),(unsigned long)Wire.getWireTimeoutCount(),
        (unsigned long)retry_max,
        (unsigned long)(uint64_t(samples[0])*1000/elapsed),(unsigned long)(uint64_t(samples[1])*1000/elapsed),
        (unsigned long)(uint64_t(samples[2])*1000/elapsed),(unsigned long)(uint64_t(samples[3])*1000/elapsed),
        (unsigned long)ADC1_CFG,(unsigned long)ADC1_GC,unsigned(NVIC_GET_PRIORITY(IRQ_PIT)));
    response_size += min(size_t(max(0,n)),sizeof(response)-response_size-1);
    if (stats_handler) {
        stats_handler(response+response_size,sizeof(response)-response_size);
        response_size = strlen(response);
    }
    if (response_size < sizeof(response)-1) response[response_size++] = '\n';
    response[response_size] = 0;
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
            else if (!command_overflow && command_handler) {
                response[0] = 0;
                if (command_handler(command,response,sizeof(response))) {
                    response_size = strlen(response);
                    response_sent = 0;
                }
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

void surface::begin(CommandHandler commands, PotHandler pots_callback, StatsHandler stats_callback) {
    command_handler = commands;
    pot_handler = pots_callback;
    stats_handler = stats_callback;
    Serial.begin(115200);
    M4_SERIAL.addMemoryForWrite(m4_tx_memory,sizeof(m4_tx_memory));
    M4_SERIAL.begin(1000000);
    Wire.begin();
    Wire.setClock(400000);
    analogReadResolution(12);
    analogReadAveraging(1);
    for (const auto pin : FSR_PINS) pinMode(pin,INPUT_DISABLE);
    for (const auto pin : POT_PINS) pinMode(pin,INPUT_DISABLE);
    for (uint8_t i = 0; i < ENCODER_COUNT; ++i) encoder_positions[i] = encoders[i].read();
#ifdef GF_CONSOLA_UNO
    pinMode(13,OUTPUT);
    digitalWrite(13,LOW);
    if (PROFILE.selector) pinMode(41,INPUT_DISABLE);
#endif
    // PJRC startup analog_init has already calibrated both ADCs synchronously.
    // Keep its async clock setup; software trigger, no continuous conversion,
    // comparison, DMA or ADC interrupts. No analogRead after taking ownership.
    ADC1_CFG &= ~ADC_CFG_ADTRG;
    ADC2_CFG &= ~ADC_CFG_ADTRG;
    ADC1_GC = 0;
    ADC2_GC = 0;
    ADC1_HC0 = ADC2_HC0 = 31;
    Wire.setWireTimeout(GF_WIRE_TIMEOUT_US);
    reset_stats();
    fsr_timer.priority(160); // Set BEFORE begin, which enables the shared PIT IRQ.
#ifdef GF_CONSOLA_UNO
    NVIC_SET_PRIORITY(IRQ_USB1,112);
#endif
#ifndef GF_DIAG_NO_FSR
    fsr_timer_ok = fsr_timer.begin(scan_fsr,50);
#endif
}
void surface::poll() {
    const uint32_t start = micros();
    drain_pads();
    poll_m4();
    for (uint8_t i = 0; i < ENCODER_COUNT; ++i) {
        if (!(PROFILE.encoders & (1u << i))) continue;
        const int32_t position = encoders[i].read();
        const int32_t delta = int32_t(uint32_t(position)-uint32_t(encoder_positions[i]));
        encoder_positions[i] = position;
        const uint8_t cc = ENCODER_CC[i];
        // Keep detents in RelativeEncoder until this control's next send slot.
        if (cc_cadence[cc].due(start)) {
            const uint8_t value = relative[i].update(delta);
            if (value) send_cc(cc,value);
        } else relative[i].accumulate(delta);
    }
    if (start - last_pots >= 1000) {
        last_pots = start;
        for (uint8_t i = 0; i < sizeof(POT_PINS); ++i) {
            if (!(PROFILE.pots & (1u << i))) continue;
            uint16_t raw;
#ifdef GF_CONSOLA_UNO
            if (i == 1) {
                if (!(slow_valid & 1)) continue;
                raw = slow_adc[0]; // ISR mailbox; ADC1 never touched by loop.
            } else if (!read_adc2(i == 0 ? 13 : 3,raw)) continue;
#else
            if (!read_adc2(i == 0 ? 14 : i,raw)) continue;
#endif
            uint8_t value;
            if (pots[i].sample(raw,value)) {
                send_cc(30+i,value);
            }
        }
    }
#ifdef GF_CONSOLA_UNO
    if (PROFILE.selector && start - last_selector >= 10000) {
        last_selector = start;
        uint8_t value;
        if ((slow_valid & 2) && selector.sample(slow_adc[1],value)) send_cc(36,value);
    }
#endif
    uint8_t channel;
    int16_t raw;
    const uint8_t chip = next_ads;
    next_ads ^= 1;
    if (ads[chip].poll(micros(),channel,raw)) {
        ++ads_reads[chip];
#ifdef GF_CONSOLA_UNO
        const int8_t hand = consola::hand_index(GF_CONSOLA_MINIMAL,chip,channel);
#else
        const int8_t hand = consola::hand_index(false,chip,channel);
#endif
        if (hand >= 0) send_hand(hand,hands[hand].sample(raw,millis()));
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
    flush_cc();
    usbMIDI.send_now();
    const uint32_t duration = micros()-start;
    record_loop_duration(duration);
}

void surface::record_loop_duration(uint32_t duration) {
    if (duration > loop_max) loop_max = duration;
}

#endif
