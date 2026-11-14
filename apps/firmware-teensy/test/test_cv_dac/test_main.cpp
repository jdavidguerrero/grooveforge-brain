#include <unity.h>
#include <climits>
#include <cstring>
#include "consola_s/CvDac.h"
#include "../../../consola/processor/include/shared/Config.h"
using consola::CvDac;
using Request = CvDac::Request;
void setUp() {}
void tearDown() {}

// Transaction-level fake; no audio/codec/Arduino driver is replaced.
struct Bus : consola::CvDacBus {
    struct Write { uint8_t address, size, data[3]; } writes[32]{};
    unsigned count = 0, reads = 0;
    bool ack[2] = {true,true};
    uint8_t bytes = 5, data[5] = {0x80,0,0,0,0}, read_address = 0;
    bool write(uint8_t address, const uint8_t* payload, uint8_t size) override {
        TEST_ASSERT_LESS_THAN_UINT(32,count);
        writes[count].address = address;
        writes[count].size = size;
        memcpy(writes[count++].data,payload,size);
        return ack[address-0x60];
    }
    uint8_t read(uint8_t address, uint8_t* out, uint8_t size) override {
        ++reads; read_address = address;
        TEST_ASSERT_EQUAL_UINT8(5,size);
        memcpy(out,data,bytes);
        return bytes;
    }
};
void assert_write(const Bus& bus, unsigned i, uint8_t address, uint16_t value, bool eeprom=false) {
    TEST_ASSERT_EQUAL_UINT8(address,bus.writes[i].address);
    TEST_ASSERT_EQUAL_UINT8(eeprom ? 3 : 2,bus.writes[i].size);
    if (eeprom) {
        TEST_ASSERT_EQUAL_UINT8(0x60,bus.writes[i].data[0]);
        TEST_ASSERT_EQUAL_UINT8(0,bus.writes[i].data[1]);
        TEST_ASSERT_EQUAL_UINT8(0,bus.writes[i].data[2]);
    } else {
        TEST_ASSERT_EQUAL_UINT8(value >> 8,bus.writes[i].data[0]);
        TEST_ASSERT_EQUAL_UINT8(value & 255,bus.writes[i].data[1]);
    }
}
void boot_zeros_both_and_cv2_stays_zero_on_refresh() {
    CvDac dac; Bus bus;
    char out[100]="untouched";
    TEST_ASSERT_FALSE(dac.reply(out,sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("untouched",out);
    dac.poll(bus,0); dac.poll(bus,100);
    TEST_ASSERT_EQUAL_UINT(2,bus.count);
    assert_write(bus,0,0x60,0); assert_write(bus,1,0x61,0);
    TEST_ASSERT_FALSE(dac.busy());
    TEST_ASSERT_EQUAL_UINT(0,bus.reads);
    TEST_ASSERT_TRUE(dac.status(0).online());
    TEST_ASSERT_TRUE(dac.status(1).online());
    TEST_ASSERT_FALSE(dac.set(4096));
    TEST_ASSERT_TRUE(dac.set(4095));
    dac.poll(bus,200); assert_write(bus,2,0x60,4095);
    dac.poll(bus,999999); TEST_ASSERT_EQUAL_UINT(3,bus.count);
    dac.poll(bus,1000100); assert_write(bus,3,0x61,0);
    dac.poll(bus,1000200); assert_write(bus,4,0x60,4095);
    for (unsigned i=0;i<bus.count;++i) TEST_ASSERT_EQUAL_UINT8(2,bus.writes[i].size);
}
void absent_dac_does_not_block_other_and_retries_with_wrap() {
    CvDac dac; Bus bus; bus.ack[0]=false;
    const uint32_t t=UINT32_MAX-500;
    dac.poll(bus,t); dac.poll(bus,t+100);
    assert_write(bus,0,0x60,0); assert_write(bus,1,0x61,0);
    TEST_ASSERT_FALSE(dac.status(0).online());
    TEST_ASSERT_TRUE(dac.status(1).online());
    dac.poll(bus,t+999999); TEST_ASSERT_EQUAL_UINT(2,bus.count);
    dac.poll(bus,t+1000000); TEST_ASSERT_EQUAL_UINT(3,bus.count);
    TEST_ASSERT_EQUAL_UINT32(2000000,dac.status(0).retry_us());
    bus.ack[0]=true;
    dac.poll(bus,t+1000100); assert_write(bus,3,0x61,0);
    dac.poll(bus,t+2999999); assert_write(bus,4,0x61,0);
    dac.poll(bus,t+3000000); assert_write(bus,5,0x60,0);
    TEST_ASSERT_EQUAL_UINT32(0,dac.status(0).retry_us());
}
void provisioning_is_explicit_single_write_then_verified_reply() {
    CvDac dac; Bus bus;
    TEST_ASSERT_TRUE(CvDac::is_provision_command("@GF CV EEPROM0"));
    const char* bad[]={"@GF CV EEPROM0 extra","@GF CV EEPROM0 ","@GF CV eeprom0","@GF CV 0","CV EEPROM0"};
    for (auto line:bad) TEST_ASSERT_FALSE(CvDac::is_provision_command(line));
    dac.poll(bus,0); dac.poll(bus,1);
    TEST_ASSERT_TRUE(dac.set(1234));
    TEST_ASSERT_EQUAL(Request::Started,dac.provision());
    TEST_ASSERT_EQUAL(Request::Busy,dac.provision());
    TEST_ASSERT_FALSE(dac.set(1));
    const uint32_t t=UINT32_MAX-500;
    dac.poll(bus,t); assert_write(bus,2,0x60,0,true);
    dac.poll(bus,t+999); TEST_ASSERT_EQUAL_UINT(0,bus.reads);
    bus.data[0]=0;
    dac.poll(bus,t+1000); TEST_ASSERT_EQUAL_UINT(1,bus.reads);
    TEST_ASSERT_EQUAL_UINT(3,bus.count);
    bus.data[0]=0x80;
    dac.poll(bus,t+2000); TEST_ASSERT_EQUAL_UINT8(0x60,bus.read_address);
    dac.poll(bus,t+2001); assert_write(bus,3,0x61,0,true);
    dac.poll(bus,t+3001); TEST_ASSERT_EQUAL_UINT8(0x61,bus.read_address);
    TEST_ASSERT_EQUAL(Request::Busy,dac.provision()); // Result must survive until emitted.
    char out[100]; char small[2];
    TEST_ASSERT_FALSE(dac.reply(small,sizeof(small)));
    TEST_ASSERT_TRUE(dac.reply(out,sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("@GF OK CV EEPROM0 VERIFIED=60,61 WRITTEN=3\n",out);
    TEST_ASSERT_FALSE(dac.reply(out,sizeof(out)));
    TEST_ASSERT_EQUAL(Request::Already,dac.provision());
    dac.poll(bus,t+1004000); assert_write(bus,4,0x60,0);
    dac.poll(bus,t+1004001); assert_write(bus,5,0x61,0);
    TEST_ASSERT_EQUAL_UINT(6,bus.count);
}
void nack_short_read_busy_timeout_are_not_success_or_write_retry() {
    CvDac dac; Bus bus; bus.ack[0]=false;
    TEST_ASSERT_EQUAL(Request::Started,dac.provision());
    dac.poll(bus,0); assert_write(bus,0,0x60,0,true);
    dac.poll(bus,1); assert_write(bus,1,0x61,0,true);
    bus.bytes=4; dac.poll(bus,1001);
    bus.bytes=5; bus.data[0]=0; dac.poll(bus,2001);
    dac.poll(bus,100000); TEST_ASSERT_TRUE(dac.busy());
    dac.poll(bus,100001);
    char out[100]; TEST_ASSERT_TRUE(dac.reply(out,sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("@GF ERR CV EEPROM0 WRITTEN=2 VERIFIED=0\n",out);
    TEST_ASSERT_EQUAL_UINT(2,bus.count);
    TEST_ASSERT_EQUAL_UINT32(1,dac.status(0).errors());
    TEST_ASSERT_EQUAL_UINT32(1,dac.status(1).errors());
    dac.poll(bus,100002); TEST_ASSERT_EQUAL_UINT(2,bus.count);
    bus.ack[0]=true; bus.data[0]=0x80;
    TEST_ASSERT_EQUAL(Request::Started,dac.provision());
    dac.poll(bus,101000); dac.poll(bus,102000);
    dac.poll(bus,102001); dac.poll(bus,103001);
    TEST_ASSERT_TRUE(dac.reply(out,sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("@GF OK CV EEPROM0 VERIFIED=60,61 WRITTEN=3\n",out);
}
void verification_rejects_nonzero_register_eeprom_and_powerdown() {
    const uint8_t bad[][5] = {
        {0x82,0,0,0,0}, {0x80,1,0,0,0}, {0x80,0,0x10,0,0},
        {0x80,0,0,0x20,0}, {0x80,0,0,1,0}, {0x80,0,0,0,1}};
    for (auto data:bad) {
        CvDac dac; Bus bus; memcpy(bus.data,data,5);
        dac.provision(); dac.poll(bus,0); dac.poll(bus,1000);
        // First DAC fails but second is still provisioned and verified independently.
        memset(bus.data,0,5); bus.data[0]=0x80;
        dac.poll(bus,1001); dac.poll(bus,2001);
        char out[100]; TEST_ASSERT_TRUE(dac.reply(out,sizeof(out)));
        TEST_ASSERT_EQUAL_STRING("@GF ERR CV EEPROM0 WRITTEN=3 VERIFIED=2\n",out);
    }
}
void uart_constants_match_trellis_core_contract() {
    TEST_ASSERT_EQUAL_UINT8(22,UART_RX_PIN);
    TEST_ASSERT_EQUAL_UINT8(21,UART_TX_PIN);
    TEST_ASSERT_EQUAL_UINT32(1000000,UART_BAUD_RATE);
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(boot_zeros_both_and_cv2_stays_zero_on_refresh);
    RUN_TEST(absent_dac_does_not_block_other_and_retries_with_wrap);
    RUN_TEST(provisioning_is_explicit_single_write_then_verified_reply);
    RUN_TEST(nack_short_read_busy_timeout_are_not_success_or_write_retry);
    RUN_TEST(verification_rejects_nonzero_register_eeprom_and_powerdown);
    RUN_TEST(uart_constants_match_trellis_core_contract);
    return UNITY_END();
}
