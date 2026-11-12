#include <unity.h>
#include <cstring>
#include <climits>
#include <initializer_list>
#include "consola_s/KeyLedReturn.h"
#include "consola_s/PadLedReturn.h"
void setUp(){}
void tearDown(){}
using K=KeyLedReturn;
static bool cc(K& k,unsigned n,unsigned v,uint32_t t=0){return k.receive(16,0xb0,n,v,t);}
static void start(K& k,uint32_t t=0){cc(k,102,27,t);cc(k,25,31,t);}
static void key(K& k,unsigned i,unsigned state,unsigned palette=K::VIOLET){
    cc(k,103,i);cc(k,104,state);cc(k,105,palette);TEST_ASSERT_TRUE(cc(k,106,1));
}
void isolation_and_atomic_rejection(){
    K k;PadLedReturn pads;
    TEST_ASSERT_FALSE(k.receive(1,0xb0,102,27,0));
    TEST_ASSERT_FALSE(k.receive(16,0x90,48,127,0));
    TEST_ASSERT_FALSE(cc(k,104,2));TEST_ASSERT_FALSE(cc(k,102,26));
    start(k);TEST_ASSERT_FALSE(pads.receive(16,0xb0,102,27));
    key(k,0,K::ON);k.render(0);TEST_ASSERT_EQUAL(62,k.colors()[2]);
    cc(k,103,0);cc(k,104,K::OFF);TEST_ASSERT_FALSE(cc(k,106,1));
    cc(k,103,26);cc(k,104,K::OFF);cc(k,105,K::TEAL);TEST_ASSERT_FALSE(cc(k,106,1));
    cc(k,103,0);cc(k,104,5);cc(k,105,K::TEAL);TEST_ASSERT_FALSE(cc(k,106,1));
    cc(k,103,0);cc(k,104,K::OFF);cc(k,105,5);TEST_ASSERT_FALSE(cc(k,106,1));
    k.render(34);TEST_ASSERT_EQUAL(62,k.colors()[2]);
    cc(k,23,23);k.render(68);TEST_ASSERT_EQUAL(62,k.colors()[2]);
    cc(k,102,27);k.render(102);TEST_ASSERT_EQUAL(0,k.colors()[2]);
}
void colors_states_cap_and_settings(){
    K k;start(k);
    key(k,0,K::ON,K::VIOLET);key(k,1,K::ON,K::TEAL);key(k,2,K::DIM,K::WHITE);
    key(k,3,K::OFF);key(k,4,K::ON,K::WHITE);
    k.render(0);
    TEST_ASSERT_EQUAL(36,k.colors()[0]);TEST_ASSERT_EQUAL(9,k.colors()[1]);TEST_ASSERT_EQUAL(62,k.colors()[2]);
    TEST_ASSERT_EQUAL(9,k.colors()[3]);TEST_ASSERT_EQUAL(19,k.colors()[4]);TEST_ASSERT_EQUAL(16,k.colors()[5]);
    TEST_ASSERT_EQUAL(7,k.colors()[6]);TEST_ASSERT_EQUAL(0,k.colors()[9]);
    TEST_ASSERT_EQUAL(62,k.colors()[12]);
    cc(k,25,127);k.render(34);TEST_ASSERT_EQUAL(62,k.colors()[12]);
    cc(k,25,0);k.render(68);for(unsigned i=0;i<K::BYTES;++i)TEST_ASSERT_EQUAL(0,k.colors()[i]);
}
void blink_pulse_and_animation_off(){
    K k;start(k);key(k,0,K::BLINK);key(k,1,K::PULSE);
    k.render(0);const auto dim=k.colors()[5];TEST_ASSERT_EQUAL(62,k.colors()[2]);
    k.render(300);TEST_ASSERT_EQUAL(7,k.colors()[2]);TEST_ASSERT_GREATER_THAN(dim,k.colors()[5]);
    k.render(500);TEST_ASSERT_EQUAL(62,k.colors()[2]);TEST_ASSERT_EQUAL(62,k.colors()[5]);
    cc(k,26,0);k.render(800);TEST_ASSERT_EQUAL(62,k.colors()[2]);TEST_ASSERT_EQUAL(62,k.colors()[5]);
}
void tap_tempo_beat_dedup_and_stop(){
    K k;start(k);key(k,key_leds::index_of("TAP"),K::DIM,K::WHITE);
    key(k,key_leds::index_of("STOP"),K::DIM,K::WHITE);
    constexpr unsigned tap=key_leds::index_of("TAP")*3;
    cc(k,107,120);cc(k,108,0);cc(k,109,1,0);k.render(0);TEST_ASSERT_EQUAL(62,k.colors()[tap]);
    TEST_ASSERT_EQUAL(7,k.colors()[key_leds::index_of("STOP")*3]);
    k.render(100);TEST_ASSERT_EQUAL(7,k.colors()[tap]);
    k.render(500);TEST_ASSERT_EQUAL(62,k.colors()[tap]);
    cc(k,111,1,600);k.render(600);TEST_ASSERT_EQUAL(7,k.colors()[tap]);
    cc(k,28,10,634);k.render(634);TEST_ASSERT_EQUAL(62,k.colors()[tap]);
    cc(k,28,10,734);k.render(734);TEST_ASSERT_EQUAL(7,k.colors()[tap]);
    cc(k,28,11,768);k.render(768);TEST_ASSERT_EQUAL(62,k.colors()[tap]);
    cc(k,111,0);cc(k,109,2,900);k.render(900);TEST_ASSERT_EQUAL(62,k.colors()[tap]);
    cc(k,109,2,1000);k.render(1000);TEST_ASSERT_EQUAL(7,k.colors()[tap]);
    TEST_ASSERT_FALSE(cc(k,108,127));
    cc(k,26,0);k.render(1034);TEST_ASSERT_EQUAL(62,k.colors()[tap]);
}
void frame_budget_timeout_recovery_and_wrap(){
    K k;const uint32_t at=UINT32_MAX-50;start(k,at);key(k,0,K::ON);
    TEST_ASSERT_TRUE(k.render(at));TEST_ASSERT_FALSE(k.render(at+33));
    TEST_ASSERT_TRUE(k.render(at+34));TEST_ASSERT_TRUE(k.render(at+1000));
    TEST_ASSERT_GREATER_THAN(0,k.colors()[2]);
    k.render(at+1500);TEST_ASSERT_EQUAL(0,k.colors()[2]);
    cc(k,110,7,at+1534);k.render(at+1534);TEST_ASSERT_GREATER_THAN(0,k.colors()[2]);
    TEST_ASSERT_FALSE(k.render(at+1535));
}
void current_model_and_hard_limiter(){
    uint8_t rgb[K::BYTES];memset(rgb,255,sizeof(rgb));
    TEST_ASSERT_EQUAL(1586000,K::estimate(rgb));
    auto limited=K::limit(rgb);TEST_ASSERT_LESS_OR_EQUAL(K::LIMIT_UA,limited);
    TEST_ASSERT_EQUAL(limited,K::estimate(rgb));
    for(auto c:rgb)TEST_ASSERT_EQUAL(rgb[0],c);
    memset(rgb,0,sizeof(rgb));rgb[0]=200;rgb[1]=100;rgb[2]=50;
    TEST_ASSERT_LESS_THAN(K::LIMIT_UA,K::limit(rgb));TEST_ASSERT_EQUAL(200,rgb[0]);
    K k;start(k);for(unsigned i=0;i<K::COUNT;++i)key(k,i,K::ON,K::WHITE);
    k.render(0);TEST_ASSERT_LESS_OR_EQUAL(406000,k.current_ua());
    for(unsigned t=0;t<1500;t+=34){k.render(t);TEST_ASSERT_LESS_OR_EQUAL(K::LIMIT_UA,k.current_ua());}
}
void bench_count_expiry_and_zero_brightness(){
    K k;TEST_ASSERT_FALSE(k.bench(27,0));TEST_ASSERT_TRUE(k.bench(3,0));
    k.render(0);TEST_ASSERT_GREATER_THAN(0,k.colors()[0]);
    for(unsigned i=9;i<K::BYTES;++i)TEST_ASSERT_EQUAL(0,k.colors()[i]);
    k.render(1000);TEST_ASSERT_GREATER_THAN(k.colors()[1],k.colors()[2]);
    cc(k,25,0);k.render(1034);for(unsigned i=0;i<K::BYTES;++i)TEST_ASSERT_EQUAL(0,k.colors()[i]);
    cc(k,25,31);k.render(60000);for(unsigned i=0;i<K::BYTES;++i)TEST_ASSERT_EQUAL(0,k.colors()[i]);
    k.bench(26,61000);k.render(61000);TEST_ASSERT_GREATER_THAN(0,k.colors()[75]);
    k.bench(0,62000);k.render(62000);TEST_ASSERT_EQUAL(0,k.colors()[75]);
}
void bench_parser(){
    uint8_t count=99;
    for(const char* text:{"@GF KEYLEDS 0","@GF KEYLEDS 26","@GF KEYLEDS  3 "})TEST_ASSERT_TRUE(K::parse_bench(text,count));
    for(const char* text:{"@GF KEYLEDS","@GF KEYLEDS1","@GF KEYLEDS -1","@GF KEYLEDS 27","@GF KEYLEDS 1 extra","@GF KEYLEDS 99999999999999","@GF KEYLEDS +2"})TEST_ASSERT_FALSE(K::parse_bench(text,count));
}
int main(){UNITY_BEGIN();RUN_TEST(isolation_and_atomic_rejection);RUN_TEST(colors_states_cap_and_settings);
RUN_TEST(blink_pulse_and_animation_off);RUN_TEST(tap_tempo_beat_dedup_and_stop);
RUN_TEST(frame_budget_timeout_recovery_and_wrap);RUN_TEST(current_model_and_hard_limiter);
RUN_TEST(bench_count_expiry_and_zero_brightness);RUN_TEST(bench_parser);return UNITY_END();}
