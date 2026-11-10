#include <unity.h>
#include <cstring>
#include <cstdio>
#include <climits>
#include "consola_s/ConsolaMap.h"
#include "consola_s/McpButtons.h"
#include "consola_s/PadSelector.h"
#include "consola_s/RetryGate.h"
#include "ui/preset_link.h"
void setUp() {}
void tearDown() {}

void mcp_full_map_and_reserved_pins() {
    bool used[128] = {};
    McpButtons::Event events[14];
    for (uint8_t chip = 0; chip < 3; ++chip) {
        for (uint8_t input = 0; input < 14; ++input) {
            McpButtons buttons(chip);
            const uint8_t expected = chip < 2 ? 80+chip*14+input : input < 13 ? 110+input : 0xff;
            TEST_ASSERT_EQUAL_UINT8(expected,consola::mcp_note(chip,input));
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
    TEST_ASSERT_FALSE(used[108]);
    TEST_ASSERT_FALSE(used[109]);
    TEST_ASSERT_TRUE(used[88]); // jog click
    TEST_ASSERT_TRUE(used[122]); // pedal, chip 3 physical GPB5
    TEST_ASSERT_EQUAL_UINT8(0xff,consola::mcp_note(3,0));
    TEST_ASSERT_EQUAL_UINT8(0xff,consola::mcp_note(0,14));
}
void mcp_noise_wrap_unplug_and_minimal_port_a() {
    McpButtons buttons(2);
    McpButtons::Event events[14];
    constexpr uint16_t PEDAL = 0xffff ^ (1u << 13);
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
    for (uint8_t chip = 0; chip < 2; ++chip) {
        McpButtons minimal(chip,consola::MINIMAL.mcp_inputs[chip]);
        minimal.sample(0x00ff,0,events); // floating/unwired port B is ignored
        TEST_ASSERT_EQUAL_UINT8(0,minimal.sample(0x00ff,6000,events));
        minimal.sample(0xfffe,7000,events);
        TEST_ASSERT_EQUAL_UINT8(1,minimal.sample(0xfffe,12000,events));
        TEST_ASSERT_EQUAL_UINT8(chip == 0 ? 80 : 94,events[0].note);
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
    TEST_ASSERT_EQUAL_UINT8(1,consola::MINIMAL.pots);
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
    return UNITY_END();
}
