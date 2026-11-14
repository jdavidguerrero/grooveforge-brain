#include <unity.h>
#include <cstring>
#include <climits>
#include "consola_s/PadLedReturn.h"
#include "../../../consola/processor/include/shared/ConsolaLed.h"
void setUp(){}
void tearDown(){}
static void cc(PadLedReturn& p,uint8_t n,uint8_t v,uint32_t t=0){p.receive(16,0xb0,n,v,t);}
static void setup(PadLedReturn& p,uint8_t mode=2,bool animation=true,uint32_t t=0){cc(p,23,23,t);cc(p,24,mode,t);cc(p,25,31,t);cc(p,26,animation,t);}
static void pad(PadLedReturn& p,uint8_t flags,uint8_t level,uint8_t serial=1,uint32_t t=600,unsigned index=9){
    cc(p,20,127,t);cc(p,21,64,t);cc(p,22,0,t);cc(p,29,flags,t);cc(p,30,level,t);cc(p,31,serial,t);p.receive(16,0x90,48+index,1,t);
}
void legacy_and_isolation(){
    PadLedReturn p;p.sent();TEST_ASSERT_FALSE(p.receive(1,0x90,48,127));
    cc(p,20,127);cc(p,21,12);cc(p,22,4);TEST_ASSERT_FALSE(p.dirty());
    p.receive(16,0x90,48,1);TEST_ASSERT_EQUAL(31,p.colors()[0]);TEST_ASSERT_EQUAL(12,p.colors()[1]);
    p.sent();p.receive(16,0x90,48,1);TEST_ASSERT_FALSE(p.dirty());
    p.receive(16,0x80,48,0);TEST_ASSERT_EQUAL(0,p.colors()[0]);TEST_ASSERT_FALSE(p.receive(16,0x90,80,1));
}
void velocity_flash_decay_and_dedup(){
    PadLedReturn p;setup(p);pad(p,PadLedReturn::HIT,127);p.render(600);TEST_ASSERT_EQUAL(31,p.colors()[27]);
    pad(p,PadLedReturn::HIT,127,1,668);p.render(668);TEST_ASSERT_LESS_THAN(31,p.colors()[27]);TEST_ASSERT_GREATER_THAN(3,p.colors()[27]);
    p.render(804);TEST_ASSERT_EQUAL(3,p.colors()[27]);
    pad(p,PadLedReturn::HIT,64,2,838);p.render(838);TEST_ASSERT_EQUAL(15,p.colors()[27]);
    pad(p,PadLedReturn::HIT|PadLedReturn::DIM,127,3,872);p.render(872);TEST_ASSERT_EQUAL(0,p.colors()[27]);
}
void held_release_and_roots(){
    PadLedReturn p;setup(p,1);pad(p,PadLedReturn::HELD,127);p.render(600);TEST_ASSERT_EQUAL(31,p.colors()[27]);
    pad(p,0,16,1,634);p.render(634);TEST_ASSERT_EQUAL(31,p.colors()[27]);
    p.render(702);TEST_ASSERT_LESS_THAN(31,p.colors()[27]);TEST_ASSERT_GREATER_THAN(3,p.colors()[27]);
    p.render(838);TEST_ASSERT_EQUAL(3,p.colors()[27]);
    pad(p,PadLedReturn::ROOT,16,1,872);p.render(872);TEST_ASSERT_EQUAL(10,p.colors()[27]);
}
void frame_budget_diffs_and_backpressure(){
    PadLedReturn p;setup(p,2,false);pad(p,0,16);TEST_ASSERT_TRUE(p.render(600));p.sent();
    for(unsigned i=601;i<634;++i)TEST_ASSERT_FALSE(p.render(i));
    TEST_ASSERT_TRUE(p.render(634));TEST_ASSERT_FALSE(p.dirty());
    pad(p,PadLedReturn::HELD,127,1,668);p.render(668);TEST_ASSERT_TRUE(p.dirty());
    pad(p,PadLedReturn::HELD,64,1,680);TEST_ASSERT_FALSE(p.render(680));
    p.render(1000);TEST_ASSERT_TRUE(p.dirty());TEST_ASSERT_EQUAL(15,p.colors()[27]);
    p.sent();p.reconnect();TEST_ASSERT_TRUE(p.dirty());
}
void cap_preserves_hue_and_zero(){
    PadLedReturn p;setup(p,1,false);pad(p,PadLedReturn::HELD,127);cc(p,25,127);p.render(600);
    TEST_ASSERT_EQUAL(31,p.colors()[27]);TEST_ASSERT_EQUAL(15,p.colors()[28]);
    cc(p,25,0);p.render(634);for(unsigned i=0;i<96;++i)TEST_ASSERT_EQUAL(0,p.colors()[i]);
    cc(p,25,28);p.render(668);TEST_ASSERT_EQUAL(28,p.colors()[27]);
}
void heartbeat_loss_recovery_and_rollover(){
    PadLedReturn p;const uint32_t start=UINT32_MAX-700;setup(p,1,false,start);pad(p,PadLedReturn::HELD,127,1,start);
    p.render(start+1499);TEST_ASSERT_EQUAL(15,p.colors()[28]);
    p.render(start+1533);TEST_ASSERT_EQUAL(15,p.colors()[1]);TEST_ASSERT_EQUAL(31,p.colors()[0]);
    cc(p,119,1,start+1567);p.render(start+1567);TEST_ASSERT_EQUAL(0,p.colors()[0]);TEST_ASSERT_EQUAL(31,p.colors()[27]);
    TEST_ASSERT_LESS_THAN(2048,sizeof(PadLedReturn));
}
void sweep_and_animation_off(){
    PadLedReturn p;setup(p);p.render(0);TEST_ASSERT_GREATER_THAN(0,p.colors()[0]);
    p.render(510);for(unsigned i=0;i<96;++i)TEST_ASSERT_EQUAL(0,p.colors()[i]);
    pad(p,PadLedReturn::HIT,127,1,544);cc(p,26,0);p.render(544);TEST_ASSERT_EQUAL(3,p.colors()[27]);
    cc(p,27,6);pad(p,PadLedReturn::SELECTED,16,1,578);p.render(578);TEST_ASSERT_GREATER_THAN(0,p.colors()[0]);
    TEST_ASSERT_GREATER_THAN(p.colors()[28],p.colors()[27]);
}
void playhead_and_stop(){
    PadLedReturn p;setup(p,2,false);pad(p,PadLedReturn::PLAYHEAD,0);cc(p,27,1);p.render(600);TEST_ASSERT_EQUAL(25,p.colors()[27]);
    cc(p,27,0);p.render(634);TEST_ASSERT_EQUAL(0,p.colors()[27]);
    TEST_ASSERT_FALSE(p.receive(16,0xb0,24,3));TEST_ASSERT_FALSE(p.receive(16,0xb0,29,64));
}
void m4_linear_cap_and_monotonicity(){
    unsigned last=0;
    for(unsigned i=0;i<128;++i){const unsigned pwm=unsigned(consola_led_raw(i))*64/256;
        TEST_ASSERT_LESS_OR_EQUAL(62,pwm);TEST_ASSERT_GREATER_OR_EQUAL(last,pwm);last=pwm;
        if(i>0)TEST_ASSERT_GREATER_THAN(0,pwm);
    }
    TEST_ASSERT_EQUAL(56,unsigned(consola_led_raw(28))*64/256);
    TEST_ASSERT_EQUAL(0,consola_led_raw(0));
}
int main(){UNITY_BEGIN();RUN_TEST(m4_linear_cap_and_monotonicity);RUN_TEST(legacy_and_isolation);RUN_TEST(velocity_flash_decay_and_dedup);RUN_TEST(held_release_and_roots);RUN_TEST(frame_budget_diffs_and_backpressure);RUN_TEST(cap_preserves_hue_and_zero);RUN_TEST(heartbeat_loss_recovery_and_rollover);RUN_TEST(sweep_and_animation_off);RUN_TEST(playhead_and_stop);return UNITY_END();}
