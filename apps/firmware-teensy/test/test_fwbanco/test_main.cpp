#include <unity.h>
#include <climits>
#include <initializer_list>
#include "consola_s/ConsolaMap.h"
#include "consola_s/FsrVelocity.h"
#include "consola_s/I2cBranchRecovery.h"
using consola::I2cBranchRecovery;
using Action = I2cBranchRecovery::Action;
void setUp() {}
void tearDown() {}

void floating_fsr_requires_continuous_low_rest() {
    TEST_ASSERT_EQUAL_UINT8(3,consola::MINIMAL.fsrs);
    for (unsigned i=0;i<4;++i) {
        FsrVelocity pad({600,3000,400},3000,12000,true);
        for (uint32_t now=0;now<60000000;now+=400)
            TEST_ASSERT_FALSE(pad.sample(1500 + (now%7)*80,now).changed);
        // Brief dips cannot qualify; the next high sample restarts the timer.
        pad.sample(400,60000000);
        pad.sample(0,60002800);
        TEST_ASSERT_FALSE(pad.sample(1500,60003200).changed);
        TEST_ASSERT_FALSE(pad.sample(2000,60006400).changed);
        pad.sample(0,60008000);
        pad.sample(0,60010000);
        TEST_ASSERT_FALSE(pad.sample(401,60010800).changed);
        TEST_ASSERT_FALSE(pad.sample(3000,60014000).changed);
        pad.sample(400,60016000);
        pad.sample(0,60019200);
        TEST_ASSERT_FALSE(pad.sample(600,60019600).changed);
        const auto hit=pad.sample(3000,60022800);
        TEST_ASSERT_TRUE(hit.changed); TEST_ASSERT_TRUE(hit.on);
        TEST_ASSERT_EQUAL_UINT8(127,hit.velocity);
    }
}
void connected_fsr_preserves_velocity_release_and_retrigger_with_wrap() {
    for (uint16_t peak : {uint16_t(600),uint16_t(1800),uint16_t(3000),uint16_t(4095)}) {
        FsrVelocity protected_pad({600,3000,400},3000,12000,true);
        FsrVelocity previous_pad({600,3000,400});
        const uint32_t began=UINT32_MAX-2000;
        protected_pad.sample(0,began-3200);
        protected_pad.sample(400,began-200);
        for (uint32_t t=0;t<40000;t+=400) {
            const uint16_t value=t<5000 || (t>=16000 && t<22000) ? peak : 0;
            const auto a=protected_pad.sample(value,began+t), b=previous_pad.sample(value,began+t);
            TEST_ASSERT_EQUAL(b.changed,a.changed);
            TEST_ASSERT_EQUAL(b.on,a.on);
            TEST_ASSERT_EQUAL_UINT8(b.velocity,a.velocity);
        }
    }
}
void previous_fsr_at_air_did_emit_a_note() {
    FsrVelocity old({600,3000,400});
    TEST_ASSERT_FALSE(old.sample(1500,0).changed);
    const auto hit=old.sample(1500,3200);
    TEST_ASSERT_TRUE(hit.changed); TEST_ASSERT_TRUE(hit.on);
}
void branch_streaks_are_per_chip_and_success_means_complete_read() {
    I2cBranchRecovery r(3);
    TEST_ASSERT_EQUAL_UINT8(0,consola::I2C_BRANCH_EN_PIN);
    r.failed(0); r.failed(1); r.failed(0); r.failed(1);
    TEST_ASSERT_FALSE(r.paused());
    r.succeeded(0); r.failed(0); r.failed(0);
    TEST_ASSERT_FALSE(r.paused());
    for (unsigned i=0;i<10;++i) { r.failed(2); r.failed(3); r.failed(255); }
    TEST_ASSERT_FALSE(r.paused());
    r.failed(1); TEST_ASSERT_TRUE(r.paused());
    TEST_ASSERT_TRUE(r.tick(100,true)==Action::None);
    TEST_ASSERT_TRUE(r.tick(110,true)==Action::Disable);
    TEST_ASSERT_EQUAL_UINT32(1,r.resets());
    TEST_ASSERT_TRUE(r.tick(111,true)==Action::None);
    TEST_ASSERT_TRUE(r.tick(1110,true)==Action::Enable);
    TEST_ASSERT_FALSE(r.paused());
    r.failed(0); TEST_ASSERT_FALSE(r.paused()); // Common EN cleared every streak.
    r.failed(0); TEST_ASSERT_FALSE(r.paused());
    r.failed(0); TEST_ASSERT_TRUE(r.paused());
}
void branch_pulse_waits_for_idle_is_nonblocking_and_wrap_safe() {
    I2cBranchRecovery r(15);
    r.failed(3); r.failed(3); r.failed(3);
    for (unsigned i=0;i<1000;++i) TEST_ASSERT_TRUE(r.tick(i,false)==Action::None);
    TEST_ASSERT_EQUAL_UINT32(0,r.resets());
    const uint32_t began=UINT32_MAX-500;
    TEST_ASSERT_TRUE(r.tick(began-10,true)==Action::None);
    TEST_ASSERT_TRUE(r.tick(began-1,false)==Action::None);
    TEST_ASSERT_TRUE(r.tick(began,true)==Action::None);
    TEST_ASSERT_TRUE(r.tick(began+9,true)==Action::None);
    TEST_ASSERT_TRUE(r.tick(began+10,true)==Action::Disable);
    TEST_ASSERT_EQUAL_UINT32(1,r.resets());
    for (uint32_t t=11;t<1010;++t) {
        // Failures while isolated cannot schedule another pulse.
        r.failed(0);
        TEST_ASSERT_TRUE(r.tick(began+t,true)==Action::None);
    }
    TEST_ASSERT_TRUE(r.paused());
    TEST_ASSERT_TRUE(r.tick(began+1010,true)==Action::Enable);
    TEST_ASSERT_FALSE(r.paused());
    for (uint32_t t=1011;t<5000;++t) TEST_ASSERT_TRUE(r.tick(began+t,true)==Action::None);
    TEST_ASSERT_EQUAL_UINT32(1,r.resets());
    r.clear_count(); TEST_ASSERT_EQUAL_UINT32(0,r.resets());
}
void branch_low_bus_and_reset_counter_do_not_cancel_pulse() {
    I2cBranchRecovery r(1);
    r.failed(0); r.failed(0); r.failed(0);
    r.tick(0,true); TEST_ASSERT_TRUE(r.tick(10,true)==Action::Disable);
    r.clear_count();
    TEST_ASSERT_TRUE(r.tick(5000,false)==Action::None);
    TEST_ASSERT_TRUE(r.paused());
    TEST_ASSERT_TRUE(r.tick(6000,true)==Action::None);
    TEST_ASSERT_TRUE(r.tick(6009,true)==Action::None);
    TEST_ASSERT_TRUE(r.tick(6010,true)==Action::Enable);
    TEST_ASSERT_EQUAL_UINT32(0,r.resets());
    I2cBranchRecovery disabled(0);
    for (unsigned i=0;i<10;++i) disabled.failed(0);
    TEST_ASSERT_FALSE(disabled.paused());
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(floating_fsr_requires_continuous_low_rest);
    RUN_TEST(connected_fsr_preserves_velocity_release_and_retrigger_with_wrap);
    RUN_TEST(previous_fsr_at_air_did_emit_a_note);
    RUN_TEST(branch_streaks_are_per_chip_and_success_means_complete_read);
    RUN_TEST(branch_pulse_waits_for_idle_is_nonblocking_and_wrap_safe);
    RUN_TEST(branch_low_bus_and_reset_counter_do_not_cancel_pulse);
    return UNITY_END();
}
