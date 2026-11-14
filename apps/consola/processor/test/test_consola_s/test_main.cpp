#include <unity.h>
#include <climits>
#include "consola_s/FsrVelocity.h"
#include "consola_s/AnalogChange.h"
#include "consola_s/RelativeEncoder.h"
#include "consola_s/HandSensor.h"
void setUp() {}
void tearDown() {}
void peak_window() {
    FsrVelocity pad({100,1100,60});
    TEST_ASSERT_FALSE(pad.sample(99,0).changed);
    TEST_ASSERT_FALSE(pad.sample(100,1000).changed);
    TEST_ASSERT_FALSE(pad.sample(600,2000).changed);
    TEST_ASSERT_FALSE(pad.sample(200,3999).changed);
    const auto event = pad.sample(100,4000);
    TEST_ASSERT_TRUE(event.changed);
    TEST_ASSERT_TRUE(event.on);
    TEST_ASSERT_EQUAL_UINT8(64,event.velocity);
    TEST_ASSERT_FALSE(pad.sample(1100,5000).changed);
}
void window_closes_at_3ms() {
    FsrVelocity pad({100,1100,60});
    pad.sample(600,0);
    TEST_ASSERT_FALSE(pad.sample(600,2999).changed);
    const auto event = pad.sample(600,3000);
    TEST_ASSERT_TRUE(event.changed);
    TEST_ASSERT_TRUE(event.on);
}
void velocity_endpoints() {
    FsrVelocity soft({100,1100,60}), hard({100,1100,60});
    soft.sample(100,0);
    TEST_ASSERT_EQUAL_UINT8(1,soft.sample(100,4000).velocity);
    hard.sample(4095,0);
    TEST_ASSERT_EQUAL_UINT8(127,hard.sample(60,4000).velocity);
    const auto off = hard.sample(0,4400);
    TEST_ASSERT_TRUE(off.changed);
    TEST_ASSERT_FALSE(off.on);
}
void hysteresis_and_retrigger() {
    FsrVelocity pad({100,1100,60});
    pad.sample(300,0);
    pad.sample(800,4000);
    TEST_ASSERT_FALSE(pad.sample(80,5000).changed);
    TEST_ASSERT_FALSE(pad.sample(99,5500).changed);
    auto event = pad.sample(60,6000);
    TEST_ASSERT_TRUE(event.changed);
    TEST_ASSERT_FALSE(event.on);
    TEST_ASSERT_FALSE(pad.sample(400,7000).changed);
    TEST_ASSERT_FALSE(pad.sample(400,16000).changed);
    pad.sample(0,17000);
    pad.sample(400,17400);
    TEST_ASSERT_TRUE(pad.sample(800,21400).on);
}
void micros_wrap_and_bad_calibration() {
    FsrVelocity pad({100,1100,60});
    pad.sample(1100,UINT32_MAX-2000);
    TEST_ASSERT_FALSE(pad.sample(400,500).changed);
    TEST_ASSERT_EQUAL_UINT8(127,pad.sample(400,999).velocity);
    FsrVelocity bad({100,100,60}), inverted({100,1100,100});
    TEST_ASSERT_FALSE(bad.sample(500,0).changed);
    TEST_ASSERT_FALSE(bad.sample(500,4000).changed);
    TEST_ASSERT_FALSE(inverted.sample(500,0).changed);
}
void analog_noise_and_step() {
    AnalogChange analog;
    uint8_t midi;
    TEST_ASSERT_TRUE(analog.sample(0,midi));
    TEST_ASSERT_EQUAL_UINT8(0,midi);
    for (int i=0; i<100; ++i) TEST_ASSERT_FALSE(analog.sample(i%2 ? 10 : 0,midi));
    TEST_ASSERT_TRUE(analog.sample(4095,midi));
    TEST_ASSERT_INT_WITHIN(1,32,midi);
    bool top = false;
    for (int i=0; i<100; ++i) if (analog.sample(4095,midi) && midi==127) top=true;
    TEST_ASSERT_TRUE(top);
    TEST_ASSERT_FALSE(analog.sample(50000,midi));
    bool bottom = false;
    for (int i=0; i<100; ++i) if (analog.sample(-200,midi) && midi==0) bottom=true;
    TEST_ASSERT_TRUE(bottom);
}
void analog_deadband_and_ads_range() {
    AnalogChange analog(26400);
    uint8_t midi;
    TEST_ASSERT_TRUE(analog.sample(13200,midi));
    TEST_ASSERT_EQUAL_UINT8(64,midi);
    for (int i=0; i<30; ++i) TEST_ASSERT_FALSE(analog.sample(13300,midi));
    TEST_ASSERT_TRUE(analog.sample(26400,midi));
    TEST_ASSERT_GREATER_THAN(64,midi);
}
void encoder_mapping() {
    TEST_ASSERT_EQUAL_UINT8(0,RelativeEncoder::encode(0));
    TEST_ASSERT_EQUAL_UINT8(1,RelativeEncoder::encode(1));
    TEST_ASSERT_EQUAL_UINT8(127,RelativeEncoder::encode(-1));
    TEST_ASSERT_EQUAL_UINT8(63,RelativeEncoder::encode(INT_MAX));
    TEST_ASSERT_EQUAL_UINT8(65,RelativeEncoder::encode(INT_MIN));
    RelativeEncoder encoder;
    TEST_ASSERT_EQUAL_UINT8(0,encoder.update(3));
    TEST_ASSERT_EQUAL_UINT8(1,encoder.update(1));
    TEST_ASSERT_EQUAL_UINT8(127,encoder.update(-4));
    TEST_ASSERT_EQUAL_UINT8(0,encoder.update(2));
    TEST_ASSERT_EQUAL_UINT8(0,encoder.update(-2));
    TEST_ASSERT_EQUAL_UINT8(63,encoder.update(256));
    TEST_ASSERT_EQUAL_UINT8(1,encoder.update(0));
}
void hand_endpoints_and_cadence() {
    HandSensor far, near;
    TEST_ASSERT_FALSE(far.sample(3200,0).presence_changed);
    TEST_ASSERT_FALSE(far.sample(3200,39).presence_changed);
    auto event = far.sample(3200,40);
    TEST_ASSERT_TRUE(event.presence_changed);
    TEST_ASSERT_TRUE(event.present);
    TEST_ASSERT_TRUE(event.cc_changed);
    TEST_ASSERT_EQUAL_UINT8(0,event.value);
    near.sample(21200,0);
    event = near.sample(21200,40);
    TEST_ASSERT_TRUE(event.present);
    TEST_ASSERT_EQUAL_UINT8(127,event.value);
    TEST_ASSERT_FALSE(near.sample(21200,80).cc_changed);
}
void hand_noise_smoothing_and_hysteresis() {
    HandSensor hand;
    hand.sample(12200,0);
    hand.sample(12200,40);
    for (uint32_t t=80;t<480;t+=40) {
        const auto event = hand.sample(t%80 ? 12250 : 12150,t);
        TEST_ASSERT_FALSE(event.cc_changed);
        TEST_ASSERT_FALSE(event.presence_changed);
    }
    HandSensor moving;
    moving.sample(0,0);
    moving.sample(21200,40);
    auto event = moving.sample(21200,80);
    TEST_ASSERT_TRUE(event.present);
    TEST_ASSERT_GREATER_THAN(0,event.value);
    TEST_ASSERT_LESS_THAN(127,event.value);
    for (uint32_t t=120;t<2000;t+=40) event=moving.sample(21200,t);
    TEST_ASSERT_EQUAL_UINT8(127,event.value);
    HandSensor edge;
    edge.sample(3200,0);
    edge.sample(3200,40);
    for (uint32_t t=80;t<1200;t+=40) {
        event=edge.sample(3000,t);
        TEST_ASSERT_TRUE(event.present);
        TEST_ASSERT_FALSE(event.presence_changed);
    }
    unsigned releases=0;
    for (uint32_t t=1200;t<3000;t+=40) {
        event=edge.sample(2000,t);
        if (event.presence_changed) {
            ++releases;
            TEST_ASSERT_FALSE(event.present);
            TEST_ASSERT_TRUE(event.cc_changed);
            TEST_ASSERT_EQUAL_UINT8(0,event.value);
        }
    }
    TEST_ASSERT_EQUAL_UINT(1,releases);
    TEST_ASSERT_FALSE(event.present);
    TEST_ASSERT_FALSE(edge.sample(0,3040).cc_changed);
}
void hand_timeout_wrap_and_invalid() {
    HandSensor hand;
    hand.sample(10000,UINT32_MAX-20);
    TEST_ASSERT_TRUE(hand.sample(10000,19).present);
    TEST_ASSERT_FALSE(hand.expire(218).presence_changed);
    auto event=hand.expire(219);
    TEST_ASSERT_TRUE(event.presence_changed);
    TEST_ASSERT_FALSE(event.present);
    TEST_ASSERT_TRUE(event.cc_changed);
    TEST_ASSERT_FALSE(hand.expire(220).presence_changed);
    TEST_ASSERT_FALSE(hand.sample(10000,221).presence_changed);
    TEST_ASSERT_TRUE(hand.sample(10000,261).presence_changed);
    TEST_ASSERT_FALSE(hand.sample(-1,301).presence_changed);
    event=hand.sample(32767,341);
    TEST_ASSERT_TRUE(event.presence_changed);
    TEST_ASSERT_FALSE(event.present);
    HandSensor invalid({3200,3200,400});
    TEST_ASSERT_FALSE(invalid.sample(5000,0).present);
    TEST_ASSERT_FALSE(invalid.sample(5000,40).present);
    HandSensor out_of_range;
    out_of_range.sample(23000,0);
    TEST_ASSERT_FALSE(out_of_range.sample(23000,40).present);
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(peak_window);
    RUN_TEST(window_closes_at_3ms);
    RUN_TEST(velocity_endpoints);
    RUN_TEST(hysteresis_and_retrigger);
    RUN_TEST(micros_wrap_and_bad_calibration);
    RUN_TEST(analog_noise_and_step);
    RUN_TEST(analog_deadband_and_ads_range);
    RUN_TEST(encoder_mapping);
    RUN_TEST(hand_endpoints_and_cadence);
    RUN_TEST(hand_noise_smoothing_and_hysteresis);
    RUN_TEST(hand_timeout_wrap_and_invalid);
    return UNITY_END();
}
