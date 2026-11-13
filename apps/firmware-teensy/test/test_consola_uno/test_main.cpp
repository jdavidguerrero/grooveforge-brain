#include <unity.h>
#include <cstring>
#include <cstdio>
#include <climits>
#include "consola_s/ConsolaMap.h"
#include "consola_s/AdcScan.h"
#include "consola_s/CcCadence.h"
#include "consola_s/FsrVelocity.h"
#include "consola_s/RelativeEncoder.h"
#include "consola_s/McpButtons.h"
#include "consola_s/PadSelector.h"
#include "consola_s/PotInput.h"
#include "consola_s/RetryGate.h"
#include "ui/preset_link.h"
// Compile the production I2C adapter against a fault-injection Wire in this TU.
#define ARDUINO 1
#include "../../../consola/surface/src/AdsScanner.cpp"
#undef ARDUINO
void setUp() {}
void tearDown() {}

void mcp_full_map_and_reserved_pins() {
    constexpr uint8_t map[4][14] = {
        {84,85,86,87,255,255,255,80,81,82,83,255,255,255},
        {89,90,91,92,255,255,255,101,97,96,95,94,255,255},
        {98,102,103,104,105,106,100,107,110,111,99,255,255,255},
        {112,113,114,115,116,117,88,122,255,255,255,255,255,255}};
    bool used[128] = {};
    McpButtons::Event events[14];
    for (uint8_t chip = 0; chip < 4; ++chip) {
        for (uint8_t input = 0; input < 14; ++input) {
            McpButtons buttons(chip);
            const uint8_t expected = map[chip][input];
            TEST_ASSERT_EQUAL_UINT8(expected,consola::mcp_note(chip,input));
            TEST_ASSERT_EQUAL(expected != 0xff, bool(consola::FULL.mcp_inputs[chip] & (1u << consola::MCP_PINS[input])));
            const uint16_t gpio = 0xffff ^ (1u << consola::MCP_PINS[input]);
            TEST_ASSERT_EQUAL_UINT8(0,buttons.sample(gpio,100,events));
            TEST_ASSERT_EQUAL_UINT8(0,buttons.sample(gpio,5099,events));
            if (expected == 0xff) {
                TEST_ASSERT_EQUAL_UINT8(0,buttons.sample(gpio,5100,events));
                continue;
            }
            TEST_ASSERT_EQUAL_UINT8(1,buttons.sample(gpio,5100,events));
            TEST_ASSERT_EQUAL_UINT8(expected,events[0].note);
            TEST_ASSERT_TRUE(events[0].on);
            TEST_ASSERT_FALSE(used[expected]);
            used[expected] = true;
            TEST_ASSERT_EQUAL_UINT8(0,buttons.sample(gpio,20000,events));
            TEST_ASSERT_EQUAL_UINT8(0,buttons.sample(0xffff,21000,events));
            TEST_ASSERT_EQUAL_UINT8(1,buttons.sample(0xffff,26000,events));
            TEST_ASSERT_FALSE(events[0].on);
            TEST_ASSERT_EQUAL_UINT8(expected,events[0].note);
        }
        McpButtons buttons(chip);
        buttons.sample(0x7f7f,0,events); // GPA7/GPB7 are never inputs.
        TEST_ASSERT_EQUAL_UINT8(0,buttons.sample(0x7f7f,6000,events));
    }
    unsigned count = 0;
    for (unsigned i=0; i<128; ++i) if (used[i]) { ++count; TEST_ASSERT_TRUE(i>=80 && i<=122); }
    TEST_ASSERT_EQUAL_UINT(36,count);
    TEST_ASSERT_FALSE(used[108]);
    TEST_ASSERT_FALSE(used[109]);
    TEST_ASSERT_TRUE(used[88]); // jog click
    TEST_ASSERT_TRUE(used[122]); // pedal, chip 4 physical GPB0
    TEST_ASSERT_EQUAL_UINT8(0xff,consola::mcp_note(4,0));
    TEST_ASSERT_EQUAL_UINT8(0xff,consola::mcp_note(0,14));
}
void mcp_noise_wrap_unplug_and_minimal_port_a() {
    McpButtons buttons(3);
    McpButtons::Event events[14];
    constexpr uint16_t PEDAL = 0xffff ^ (1u << 8);
    buttons.sample(PEDAL,UINT32_MAX-2000,events);
    TEST_ASSERT_EQUAL_UINT8(0,buttons.sample(PEDAL,2998,events));
    TEST_ASSERT_EQUAL_UINT8(1,buttons.sample(PEDAL,2999,events));
    TEST_ASSERT_EQUAL_UINT8(122,events[0].note);
    TEST_ASSERT_EQUAL_UINT8(1,buttons.disconnect(events));
    TEST_ASSERT_FALSE(events[0].on);
    TEST_ASSERT_EQUAL_UINT8(0,buttons.disconnect(events));
    buttons.sample(PEDAL,4000,events);
    buttons.sample(0xffff,5000,events); // contact bounce restarts debounce
    buttons.sample(PEDAL,6000,events);
    TEST_ASSERT_EQUAL_UINT8(0,buttons.sample(PEDAL,10999,events));
    TEST_ASSERT_EQUAL_UINT8(1,buttons.sample(PEDAL,11000,events));
    for (uint8_t chip = 0; chip < 4; ++chip) {
        McpButtons absent(chip);
        TEST_ASSERT_EQUAL_UINT8(0,absent.disconnect(events));
        TEST_ASSERT_EQUAL_UINT8(0,absent.disconnect(events));
        McpButtons disabled(chip,0);
        disabled.sample(0,0,events);
        TEST_ASSERT_EQUAL_UINT8(0,disabled.sample(0,6000,events));
    }
    for (uint8_t chip = 0; chip < 2; ++chip) {
        McpButtons minimal(chip,consola::MINIMAL.mcp_inputs[chip]);
        minimal.sample(0x00ff,0,events); // floating/unwired port B is ignored
        TEST_ASSERT_EQUAL_UINT8(0,minimal.sample(0x00ff,6000,events));
        minimal.sample(0xfffe,7000,events);
        TEST_ASSERT_EQUAL_UINT8(1,minimal.sample(0xfffe,12000,events));
        TEST_ASSERT_EQUAL_UINT8(chip == 0 ? 84 : 89,events[0].note);
    }
}
void selector_initial_thresholds_and_jumps() {
    const uint16_t raw[] = {0,1364,1365,2729,2730,4095};
    const uint8_t expected[] = {0,0,64,64,127,127};
    for (uint8_t i=0; i<6; ++i) {
        PadSelector pad;
        uint8_t value = 255;
        TEST_ASSERT_TRUE(pad.sample(raw[i],value));
        TEST_ASSERT_EQUAL_UINT8(expected[i],value);
        TEST_ASSERT_FALSE(pad.sample(raw[i],value));
    }
    PadSelector pad;
    uint8_t value;
    pad.sample(0,value);
    TEST_ASSERT_TRUE(pad.sample(4095,value));
    TEST_ASSERT_EQUAL_UINT8(127,value);
    TEST_ASSERT_TRUE(pad.sample(0,value));
    TEST_ASSERT_EQUAL_UINT8(0,value);
}
void selector_hysteresis_both_directions() {
    PadSelector pad;
    uint8_t value;
    pad.sample(0,value);
    for (uint16_t raw=1285;raw<1445;++raw) TEST_ASSERT_FALSE(pad.sample(raw,value));
    TEST_ASSERT_TRUE(pad.sample(1445,value));
    TEST_ASSERT_EQUAL_UINT8(64,value);
    TEST_ASSERT_FALSE(pad.sample(1285,value));
    TEST_ASSERT_TRUE(pad.sample(1284,value));
    TEST_ASSERT_EQUAL_UINT8(0,value);
    pad.sample(2048,value);
    for (uint16_t raw=2650;raw<2810;++raw) TEST_ASSERT_FALSE(pad.sample(raw,value));
    TEST_ASSERT_TRUE(pad.sample(2810,value));
    TEST_ASSERT_EQUAL_UINT8(127,value);
    TEST_ASSERT_FALSE(pad.sample(2650,value));
    TEST_ASSERT_TRUE(pad.sample(2649,value));
    TEST_ASSERT_EQUAL_UINT8(64,value);
}
void prototype_masks_midi_and_pins() {
    TEST_ASSERT_EQUAL_UINT16(0x10f,consola::MINIMAL.encoders);
    TEST_ASSERT_EQUAL_UINT8(3,consola::MINIMAL.fsrs);
    TEST_ASSERT_EQUAL_UINT8(0,consola::MINIMAL.pots);
    TEST_ASSERT_EQUAL_UINT8(5,consola::MINIMAL.ads_channels[0]);
    TEST_ASSERT_EQUAL_UINT8(0,consola::MINIMAL.ads_channels[1]);
    TEST_ASSERT_EQUAL_UINT16(0x7f,consola::MINIMAL.mcp_inputs[0]);
    TEST_ASSERT_EQUAL_UINT16(0x7f,consola::MINIMAL.mcp_inputs[1]);
    TEST_ASSERT_EQUAL_UINT16(0,consola::MINIMAL.mcp_inputs[2]);
    TEST_ASSERT_FALSE(consola::MINIMAL.selector);
    TEST_ASSERT_EQUAL_INT8(0,consola::hand_index(true,0,2));
    TEST_ASSERT_EQUAL_INT8(-1,consola::hand_index(true,0,0));
    TEST_ASSERT_EQUAL_INT8(-1,consola::hand_index(true,1,2));
    TEST_ASSERT_EQUAL_INT8(0,consola::hand_index(false,1,2));
    TEST_ASSERT_EQUAL_INT8(1,consola::hand_index(false,1,3));
    TEST_ASSERT_EQUAL_UINT8(1,consola::MIDI_CHANNEL);
    const uint8_t cc[] = {16,17,18,19,20,21,22,23,35};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(cc,consola::ENCODER_CC,9);
    const uint8_t pins[][2] = {{2,3},{4,5},{6,9},{10,11},{12,30},{31,32},{33,34},{35,36},{37,40}};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(pins,consola::ENCODER_PINS,18);
}
void usb_product_name_matches_descriptor() {
    TEST_ASSERT_EQUAL_STRING("GF Consola",consola::USB_PRODUCT_TEXT);
    const uint16_t descriptor_chars[] = GF_CONSOLA_PRODUCT_CHARS;
    TEST_ASSERT_EQUAL_UINT(10,sizeof(descriptor_chars)/sizeof(descriptor_chars[0]));
    TEST_ASSERT_EQUAL_UINT(11,sizeof(consola::USB_PRODUCT_TEXT));
    for (uint8_t i=0;i<10;++i) TEST_ASSERT_EQUAL_UINT16(consola::USB_PRODUCT_TEXT[i],descriptor_chars[i]);
}
void retry_backoff_reconnect_and_wrap() {
    RetryGate gate;
    TEST_ASSERT_FALSE(gate.online());
    TEST_ASSERT_TRUE(gate.due(0));
    gate.failed(UINT32_MAX-200000);
    TEST_ASSERT_FALSE(gate.online());
    TEST_ASSERT_FALSE(gate.due(799998));
    TEST_ASSERT_TRUE(gate.due(799999));
    TEST_ASSERT_EQUAL_UINT32(1,gate.errors());
    gate.succeeded();
    TEST_ASSERT_TRUE(gate.online());
    TEST_ASSERT_TRUE(gate.due(800000));
    gate.failed(800000);
    TEST_ASSERT_FALSE(gate.due(1799999));
    TEST_ASSERT_TRUE(gate.due(1800000));
    TEST_ASSERT_EQUAL_UINT32(2,gate.errors());
}

void adc_frames_slow_mailboxes_and_missing_coco() {
    // Full panel: 4 pads at 2.5 kHz, CUE/selector at 100 Hz. COCO on next tick.
    AdcScan scan(0x3f);
    unsigned samples[6] = {}, frames = 0;
    bool converting = false;
    for (uint32_t now=0; now<20000; now+=50) {
        const auto step = scan.tick(now,converting);
        TEST_ASSERT_FALSE(step.timeout);
        if (step.sample >= 0) ++samples[step.sample];
        if (step.frame) ++frames;
        converting = step.start >= 0;
    }
    TEST_ASSERT_EQUAL_UINT(50,frames);
    for (unsigned i=0;i<4;++i) TEST_ASSERT_EQUAL_UINT(50,samples[i]);
    TEST_ASSERT_EQUAL_UINT(2,samples[4]);
    TEST_ASSERT_EQUAL_UINT(2,samples[5]);
    // A permanently silent ADC cannot block USB/loop; skip faults and retry.
    AdcScan broken(3);
    unsigned timeouts=0, starts=0;
    for (uint32_t now=0;now<2000;now+=50) {
        const auto step = broken.tick(now,false);
        TEST_ASSERT_EQUAL_INT8(-1,step.sample);
        timeouts += step.timeout;
        starts += step.start >= 0;
    }
    TEST_ASSERT_EQUAL_UINT(10,timeouts);
    TEST_ASSERT_EQUAL_UINT(10,starts);
}
void adc_wrap_velocity_and_recovery() {
    AdcScan scan(3);
    FsrVelocity pad({100,1100,60});
    unsigned hits=0, releases=0;
    bool converting=false;
    uint32_t began=UINT32_MAX-1000;
    for (uint32_t t=0;t<10000;t+=50) {
        const auto step=scan.tick(began+t,converting);
        TEST_ASSERT_FALSE(step.timeout);
        if (step.sample == 0) {
            const auto event=pad.sample(t<6000 ? 1100 : 0,began+t);
            if (event.changed && event.on) { ++hits; TEST_ASSERT_EQUAL_UINT8(127,event.velocity); }
            if (event.changed && !event.on) ++releases;
        }
        TEST_ASSERT_TRUE(step.start < 2); // Disabled slots never scheduled.
        converting=step.start>=0;
    }
    TEST_ASSERT_EQUAL_UINT(1,hits);
    TEST_ASSERT_EQUAL_UINT(1,releases);
    AdcScan stalled(3);
    TEST_ASSERT_EQUAL_INT8(0,stalled.tick(UINT32_MAX-20,false).start);
    TEST_ASSERT_FALSE(stalled.tick(28,false).timeout);
    TEST_ASSERT_TRUE(stalled.tick(79,false).timeout);
    const auto recovered=stalled.tick(129,true);
    TEST_ASSERT_EQUAL_INT8(1,recovered.sample); // Never relabel late/missing FSR0.
}
void cc_flood_is_bounded_and_relative_steps_survive() {
    CcCadence rate;
    RelativeEncoder encoder;
    unsigned sends=0, detents=0;
    for (uint32_t now=0;now<1000000;now+=100) {
        if (rate.due(now)) {
            detents += encoder.update(4);
            rate.sent(now);
            ++sends;
        } else encoder.accumulate(4);
    }
    TEST_ASSERT_EQUAL_UINT(100,sends);
    // Saturation retains the untransmitted detents in RelativeEncoder as well.
    for (unsigned n=0;n<100;++n) detents += encoder.update(0);
    TEST_ASSERT_EQUAL_UINT(10000,detents);
    rate.sent(UINT32_MAX-5000);
    TEST_ASSERT_FALSE(rate.due(4998));
    TEST_ASSERT_TRUE(rate.due(4999));
}
void retry_exponential_caps_and_success_resets() {
    RetryGate gate;
    uint32_t now=UINT32_MAX-2000;
    uint32_t delay=1000000;
    for (unsigned n=0;n<12;++n) {
        gate.failed(now);
        TEST_ASSERT_EQUAL_UINT32(delay,gate.retry_us());
        TEST_ASSERT_FALSE(gate.due(now+delay-1));
        TEST_ASSERT_TRUE(gate.due(now+delay));
        now += delay;
        if (delay<32000000) delay*=2;
    }
    gate.succeeded();
    TEST_ASSERT_EQUAL_UINT32(0,gate.retry_us());
    gate.failed(now);
    TEST_ASSERT_EQUAL_UINT32(1000000,gate.retry_us());
}

void ads_disabled_has_no_bus_traffic_and_recovers_after_backoff() {
    TwoWire bus;
    AdsScanner absent(bus,0x49,4,0);
    uint8_t channel=255; int16_t sample=0;
    for (uint32_t t=0;t<40000000;t+=1000) absent.poll(t,channel,sample);
    TEST_ASSERT_EQUAL_UINT(0,bus.transactions);
    AdsScanner sharp(bus,0x48,4,5);
    bus.nack=true;
    TEST_ASSERT_FALSE(sharp.poll(0,channel,sample));
    TEST_ASSERT_EQUAL_UINT(1,bus.transactions);
    TEST_ASSERT_FALSE(sharp.poll(999999,channel,sample));
    TEST_ASSERT_EQUAL_UINT(1,bus.transactions);
    TEST_ASSERT_FALSE(sharp.poll(1000000,channel,sample));
    TEST_ASSERT_EQUAL_UINT32(2000000,sharp.retry_us());
    TEST_ASSERT_EQUAL_UINT(2,bus.transactions);
    bus.nack=false;
    bool got_fader=false, got_hand=false;
    for (uint32_t t=3000000;t<3010000;t+=500) {
        if (!sharp.poll(t,channel,sample)) continue;
        TEST_ASSERT_EQUAL_INT16(8192,sample);
        TEST_ASSERT_TRUE(channel==0 || channel==2);
        if (channel==0) got_fader=true;
        if (channel==2) got_hand=true;
    }
    TEST_ASSERT_TRUE(got_fader);
    TEST_ASSERT_TRUE(got_hand);
    TEST_ASSERT_TRUE(sharp.online());
    TEST_ASSERT_EQUAL_UINT32(0,sharp.retry_us());
    // ACKing config does not reset backoff when reads are broken.
    bus.read_fault=true;
    for (uint32_t t=3010000;t<3025000;t+=500) sharp.poll(t,channel,sample);
    TEST_ASSERT_FALSE(sharp.online());
    const auto count=bus.transactions;
    sharp.poll(3500000,channel,sample);
    TEST_ASSERT_EQUAL_UINT(count,bus.transactions);
    for (uint32_t t=4025000;t<4040000;t+=500) sharp.poll(t,channel,sample);
    TEST_ASSERT_EQUAL_UINT32(2000000,sharp.retry_us());
}
studio::LinkCmd parse(const char* text) {
    char line[96];
    strcpy(line,text);
    return studio::parse_link_cmd(line);
}
void codec_and_cv_command_ranges() {
    for (unsigned n=0;n<=15;++n) {
        char line[32]; snprintf(line,sizeof(line),"@GF IN %u",n);
        const auto cmd = parse(line);
        TEST_ASSERT_EQUAL(studio::LinkCmd::In,cmd.kind);
        TEST_ASSERT_EQUAL_UINT8(n,cmd.valor);
    }
    for (unsigned n=0;n<=127;++n) {
        char line[32]; snprintf(line,sizeof(line),"@GF HP %u",n);
        const auto cmd = parse(line);
        TEST_ASSERT_EQUAL(studio::LinkCmd::Hp,cmd.kind);
        TEST_ASSERT_EQUAL_UINT8(n,cmd.valor);
    }
    TEST_ASSERT_EQUAL(studio::LinkCmd::Cv,parse("@GF CV 0").kind);
    TEST_ASSERT_EQUAL_UINT16(4095,parse("@GF CV 4095").cv);
    TEST_ASSERT_EQUAL(studio::LinkCmd::Hp,parse("@GF HP 127\r").kind);
    const char* bad[] = {"@GF IN 16","@GF IN -1","@GF IN abc","@GF IN 1 extra", "@GF IN",
        "@GF HP 128","@GF HP -1","@GF HP 1.5","@GF HP 1 2 3 4", "@GF HP", "@GF IN 256",
        "@GF CV -1","@GF CV 4096","@GF CV 1 extra","@GF CV x","@GF CV 99999999999999999999"};
    for (const auto line : bad) TEST_ASSERT_EQUAL(studio::LinkCmd::None,parse(line).kind);
}
void pot_low_holds_from_first_sample_and_resumes_filter() {
    const uint32_t periods[] = {1000,10000};
    for (const uint32_t period : periods) {
        PotInput pot;
        AnalogChange reference;
        uint8_t midi=255, expected=255;
        TEST_ASSERT_TRUE(pot.sample(3000,0,midi));
        TEST_ASSERT_TRUE(reference.sample(3000,expected));
        TEST_ASSERT_EQUAL_UINT8(expected,midi);
        const uint8_t held=midi;
        for (uint32_t t=period;t<=400000;t+=period) {
            TEST_ASSERT_FALSE(pot.sample((t/period)%41,t,midi));
            TEST_ASSERT_EQUAL_UINT8(held,midi);
        }
        TEST_ASSERT_TRUE(pot.disconnected());
        // The reference never saw the unplugged samples: its whole recovery must match.
        for (uint32_t t=410000;t<600000;t+=period) {
            const uint16_t raw=t<500000 ? 1500 : 4095;
            TEST_ASSERT_EQUAL(reference.sample(raw,expected),pot.sample(raw,t,midi));
            TEST_ASSERT_EQUAL_UINT8(expected,midi);
            TEST_ASSERT_FALSE(pot.disconnected());
        }
    }
}
void pot_confirmation_boundary_noise_and_valid_cancel() {
    PotInput pot;
    uint8_t midi=255;
    pot.sample(2048,0,midi);
    for (uint32_t t=1000;t<=201000;t+=1000) {
        TEST_ASSERT_FALSE(pot.sample(t%41,t,midi));
        TEST_ASSERT_FALSE(pot.disconnected());
    }
    TEST_ASSERT_FALSE(pot.sample(40,201001,midi));
    TEST_ASSERT_TRUE(pot.disconnected());
    pot.sample(41,202000,midi);
    TEST_ASSERT_FALSE(pot.disconnected());
    for (uint32_t t=203000;t<=303000;t+=1000) pot.sample(0,t,midi);
    pot.sample(42,304000,midi);
    for (uint32_t t=305000;t<=505000;t+=1000) {
        pot.sample(0,t,midi);
        TEST_ASSERT_FALSE(pot.disconnected());
    }
    pot.sample(0,505001,midi);
    TEST_ASSERT_TRUE(pot.disconnected());
}
void pot_low_at_boot_never_publishes_zero() {
    PotInput pot;
    uint8_t midi=77;
    for (uint32_t t=0;t<=210000;t+=10000) {
        TEST_ASSERT_FALSE(pot.sample(0,t,midi));
        TEST_ASSERT_EQUAL_UINT8(77,midi);
    }
    TEST_ASSERT_TRUE(pot.disconnected());
    TEST_ASSERT_TRUE(pot.sample(4095,220000,midi));
    TEST_ASSERT_EQUAL_UINT8(127,midi);
    TEST_ASSERT_FALSE(pot.disconnected());
    pot.sample(0,230000,midi);
    TEST_ASSERT_FALSE(pot.sample(4095,240000,midi));
    TEST_ASSERT_FALSE(pot.disconnected());
}
void pot_missing_samples_cannot_confirm_and_wrap_is_safe() {
    PotInput pot;
    uint8_t midi=64;
    const uint32_t began=UINT32_MAX-100000;
    for (uint32_t t=0;t<=100000;t+=10000) pot.sample(0,began+t,midi);
    // No sample for 30 ms restarts the pending 200 ms, across micros() wrap.
    for (uint32_t t=130000;t<=330000;t+=10000) {
        pot.sample(0,began+t,midi);
        TEST_ASSERT_FALSE(pot.disconnected());
    }
    pot.sample(0,began+330001,midi);
    TEST_ASSERT_TRUE(pot.disconnected());
    pot.sample(0,began+900000,midi);
    TEST_ASSERT_TRUE(pot.disconnected());
    pot.sample(1000,began+910000,midi);
    TEST_ASSERT_FALSE(pot.disconnected());
}
void pot_channels_hold_independently() {
    PotInput pots[3];
    uint8_t midi[3]{};
    for (unsigned i=0;i<3;++i) pots[i].sample(1000+i*1000,0,midi[i]);
    const uint8_t master=midi[0], input=midi[2];
    for (uint32_t t=1000;t<=210000;t+=1000) {
        TEST_ASSERT_FALSE(pots[0].sample(0,t,midi[0]));
        pots[1].sample(4095,t,midi[1]);
        TEST_ASSERT_FALSE(pots[2].sample(40,t,midi[2]));
    }
    TEST_ASSERT_TRUE(pots[0].disconnected());
    TEST_ASSERT_FALSE(pots[1].disconnected());
    TEST_ASSERT_TRUE(pots[2].disconnected());
    TEST_ASSERT_EQUAL_UINT8(master,midi[0]);
    TEST_ASSERT_EQUAL_UINT8(127,midi[1]);
    TEST_ASSERT_EQUAL_UINT8(input,midi[2]);
    pots[0].sample(2000,220000,midi[0]);
    TEST_ASSERT_FALSE(pots[0].disconnected());
    TEST_ASSERT_TRUE(pots[2].disconnected());
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(mcp_full_map_and_reserved_pins);
    RUN_TEST(mcp_noise_wrap_unplug_and_minimal_port_a);
    RUN_TEST(selector_initial_thresholds_and_jumps);
    RUN_TEST(selector_hysteresis_both_directions);
    RUN_TEST(prototype_masks_midi_and_pins);
    RUN_TEST(usb_product_name_matches_descriptor);
    RUN_TEST(retry_backoff_reconnect_and_wrap);
    RUN_TEST(codec_and_cv_command_ranges);
    RUN_TEST(adc_frames_slow_mailboxes_and_missing_coco);
    RUN_TEST(adc_wrap_velocity_and_recovery);
    RUN_TEST(cc_flood_is_bounded_and_relative_steps_survive);
    RUN_TEST(retry_exponential_caps_and_success_resets);
    RUN_TEST(ads_disabled_has_no_bus_traffic_and_recovers_after_backoff);
    RUN_TEST(pot_low_holds_from_first_sample_and_resumes_filter);
    RUN_TEST(pot_confirmation_boundary_noise_and_valid_cancel);
    RUN_TEST(pot_low_at_boot_never_publishes_zero);
    RUN_TEST(pot_missing_samples_cannot_confirm_and_wrap_is_safe);
    RUN_TEST(pot_channels_hold_independently);
    return UNITY_END();
}
