#include <unity.h>
#include <cstring>
#include <climits>
#include <initializer_list>
#include "consola_s/Bench.h"
using consola::Bench;
void setUp() {}
void tearDown() {}
void grammar() {
    using C=Bench::Command;
    TEST_ASSERT_TRUE(Bench::parse("@GF SCAN")==C::Scan);
    TEST_ASSERT_TRUE(Bench::parse("@GF INPUTS")==C::Inputs);
    TEST_ASSERT_TRUE(Bench::parse("@GF WATCH 0")==C::WatchOff);
    TEST_ASSERT_TRUE(Bench::parse("@GF WATCH 1")==C::WatchOn);
    for(const char* line:{"@GF WATCH","@GF WATCH 2","@GF WATCH 01","@GF WATCH 1 x","@GF WATCH 1 ","@GF WATCH -1","@GF WATCH 20","@GF WATCH 1 0"})
        TEST_ASSERT_TRUE(Bench::parse(line)==C::InvalidWatch);
    TEST_ASSERT_TRUE(Bench::parse("@GF SCAN x")==C::InvalidScan);
    TEST_ASSERT_TRUE(Bench::parse("@GF INPUTS ")==C::InvalidInputs);
    for(const char* line:{"@S STATS","@GF KEYLEDS 26","@GF CUE 1","@GF SCANNER","@GF WATCHDOG"})
        TEST_ASSERT_TRUE(Bench::parse(line)==C::Other);
}
void snapshot_format() {
    Bench::Snapshot s; s.mcp[0]=0xfffe; s.mcp_valid=1;
    s.enc[8]=INT32_MIN; s.enc_mask=511; s.ads[0]=-32768; s.ads_valid=1;
    s.fsr[0]=4095; s.fsr_valid=1; s.pot[2]=4095; s.pot_valid=4;
    s.sel=2048; s.sel_valid=true; s.m4=true; s.m4_bits=0x80000001;
    char out[1024];
    TEST_ASSERT_TRUE(Bench::inputs(out,sizeof(out),s,12));
    TEST_ASSERT_EQUAL_STRING("@GF OK INPUTS MCP=FFFE,0000,0000,0000 MCP_VALID=1 ENC=0,0,0,0,0,0,0,0,-2147483648 ENC_MASK=511 ADS=-32768,0,0,0,0,0,0,0 ADS_VALID=1 FSR=4095,0,0,0 FSR_VALID=1 POT=0,0,4095 POT_VALID=4 SEL=2048 SEL_VALID=1 M4=1 M4_BITS=80000001 DROPS=12\n",out);
    char tiny[9]="canary";
    TEST_ASSERT_FALSE(Bench::inputs(tiny,8,s,0)); TEST_ASSERT_EQUAL_CHAR(0,tiny[0]);
    TEST_ASSERT_FALSE(Bench::inputs(nullptr,0,s,0));
}
void incremental_scan() {
    Bench b; char out[512]; b.start_scan(UINT32_MAX-500);
    for(int a=8;a<0x78;++a) {
        const uint32_t now=UINT32_MAX-500+uint32_t(a-8)*1000;
        TEST_ASSERT_EQUAL(a,b.scan_address(now));
        TEST_ASSERT_EQUAL(a==0x77,b.scan_result(a==0x0a || a==0x60));
        TEST_ASSERT_EQUAL(-1,b.scan_address(now+999));
    }
    TEST_ASSERT_FALSE(b.scanning()); TEST_ASSERT_EQUAL(-1,b.scan_address(200000));
    TEST_ASSERT_TRUE(b.scan_output(out,sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("@GF OK SCAN 0A,60\n",out);
    b.start_scan(0); for(int a=8;a<0x78;++a)b.scan_result(false);
    b.scan_output(out,sizeof(out)); TEST_ASSERT_EQUAL_STRING("@GF OK SCAN -\n",out);
    b.start_scan(0); for(int a=8;a<0x78;++a)b.scan_result(true);
    TEST_ASSERT_TRUE(b.scan_output(out,sizeof(out))); TEST_ASSERT_NOT_NULL(strstr(out,",77\n"));
    TEST_ASSERT_FALSE(b.scan_output(out,10)); TEST_ASSERT_EQUAL_CHAR(0,out[0]);
}
void watch_deadbands() {
    Bench b; Bench::Snapshot s; char out[128]; b.watch(true,0,s);
    s.ads_valid=1; s.ads[0]=1000; b.observe(s,0); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    s.ads[0]=1127; b.observe(s,50); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    s.ads[0]=1128; b.observe(s,50); TEST_ASSERT_TRUE(b.next(out,sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("@GF OK EVENT ADS 0 1128\n",out);
    s.ads[0]=873; b.observe(s,100); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    s.ads[0]=872; b.observe(s,100); TEST_ASSERT_TRUE(b.next(out,sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("@GF OK EVENT ADS 0 872\n",out);
    s.ads_valid=0; b.observe(s,110); s.ads_valid=1; s.ads[0]=2000; b.observe(s,120);
    TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    s.fsr_valid=1; s.pot_valid=1; s.sel_valid=true; b.observe(s,120);
    s.fsr[0]=31; s.pot[0]=31; s.sel=31; b.observe(s,170);
    TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    s.fsr[0]=32; s.pot[0]=32; s.sel=32; b.observe(s,170);
    for(const char* kind:{"FSR","POT","SEL"}) {
        TEST_ASSERT_TRUE(b.next(out,sizeof(out))); TEST_ASSERT_NOT_NULL(strstr(out,kind));
    }
    s.fsr[0]=-31; s.pot[0]=-31; s.sel=-31; b.observe(s,220);
    TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    s.fsr[0]=-32; s.pot[0]=-32; s.sel=-32; b.observe(s,220);
    for(const char* kind:{"FSR","POT","SEL"}) {
        TEST_ASSERT_TRUE(b.next(out,sizeof(out))); TEST_ASSERT_NOT_NULL(strstr(out,kind));
    }
}
void watch_rate_latest_independent_and_wrap() {
    Bench b; Bench::Snapshot s; char out[128];
    s.ads_valid=3; s.fsr_valid=1; s.pot_valid=1;
    const uint32_t start=UINT32_MAX-20;
    b.watch(true,start,s);
    s.ads[0]=1000; b.observe(s,start+49); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    s.ads[0]=2000; b.observe(s,start+50); TEST_ASSERT_TRUE(b.next(out,sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("@GF OK EVENT ADS 0 2000\n",out);
    s.ads[1]=1000; s.fsr[0]=64; s.pot[0]=64; b.observe(s,start+51);
    for (const char* expected : {"ADS 1 1000", "FSR 0 64", "POT 0 64"}) {
        TEST_ASSERT_TRUE(b.next(out,sizeof(out))); TEST_ASSERT_NOT_NULL(strstr(out,expected));
    }
    // Intermediate excursions do not become delayed historical events.
    s.ads[0]=10000; b.observe(s,start+99); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    s.ads[0]=2000; b.observe(s,start+100); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    s.ads[0]=2128; b.observe(s,start+101); TEST_ASSERT_TRUE(b.next(out,sizeof(out)));
    TEST_ASSERT_EQUAL_STRING("@GF OK EVENT ADS 0 2128\n",out);
    b.watch(true,start+102,s); s.ads[0]=3000;
    b.observe(s,start+151); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    b.observe(s,start+152); TEST_ASSERT_TRUE(b.next(out,sizeof(out)));
    s.ads_valid=2; b.observe(s,start+153);
    s.ads_valid=3; s.ads[0]=0; b.observe(s,start+154);
    s.ads[0]=1000; b.observe(s,start+203); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    b.observe(s,start+204); TEST_ASSERT_TRUE(b.next(out,sizeof(out)));
}
void watch_sixty_seconds_noise_bounded_all_channels_and_digital_unfiltered() {
    Bench b; Bench::Snapshot s; char out[128];
    s.ads_valid=255; s.fsr_valid=15; s.pot_valid=7; s.sel_valid=true;
    b.watch(true,0,s);
    unsigned counts[16]{};
    for (uint32_t now=1;now<=60000;++now) {
        const int value=now%2 ? 1500 : 3000;
        for(auto& v:s.ads)v=value*8;
        for(auto& v:s.fsr)v=value;
        for(auto& v:s.pot)v=value;
        s.sel=value;
        b.observe(s,now);
        while(b.next(out,sizeof(out))) {
            char kind[8]; unsigned i; long v;
            TEST_ASSERT_EQUAL(3,sscanf(out,"@GF OK EVENT %7s %u %ld",kind,&i,&v));
            const unsigned offset=!strcmp(kind,"ADS")?0:!strcmp(kind,"FSR")?8:!strcmp(kind,"POT")?12:15;
            ++counts[offset+i];
        }
    }
    for(unsigned count:counts) { TEST_ASSERT_GREATER_THAN(0,count); TEST_ASSERT_LESS_OR_EQUAL(1200,count); }
    TEST_ASSERT_EQUAL_UINT32(0,b.drops());
    for(unsigned i=0;i<2000;++i) {
        b.event("KEY",84,i%2); b.event("ENC",0,i);
        TEST_ASSERT_TRUE(b.next(out,sizeof(out))); TEST_ASSERT_NOT_NULL(strstr(out,"KEY"));
        TEST_ASSERT_TRUE(b.next(out,sizeof(out))); TEST_ASSERT_NOT_NULL(strstr(out,"ENC"));
        TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    }
    TEST_ASSERT_EQUAL_UINT32(0,b.drops());
}
void watch_queue_expiration_and_wrap() {
    Bench b; Bench::Snapshot s; char out[128];
    b.event("KEY",94,1); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    b.watch(true,UINT32_MAX-100,s);
    for(int i=0;i<65;++i)b.event("ENC",8,i);
    TEST_ASSERT_EQUAL_UINT32(1,b.drops());
    TEST_ASSERT_FALSE(b.next(out,2));
    b.next(out,sizeof(out)); TEST_ASSERT_EQUAL_STRING("@GF OK EVENT ENC 8 0\n",out);
    for(int i=1;i<64;++i) { TEST_ASSERT_TRUE(b.next(out,sizeof(out))); }
    TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    b.event("KEY",94,1);
    TEST_ASSERT_FALSE(b.expire(119898)); TEST_ASSERT_TRUE(b.expire(119899));
    TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    TEST_ASSERT_FALSE(b.expire(120000));
    b.event("KEY",94,1); TEST_ASSERT_FALSE(b.next(out,sizeof(out)));
    b.watch(true,130000,s); b.event("KEY",94,1); b.watch(false,130001,s);
    TEST_ASSERT_FALSE(b.next(out,sizeof(out))); TEST_ASSERT_EQUAL_UINT32(1,b.drops());
}
int main() {
    UNITY_BEGIN(); RUN_TEST(grammar); RUN_TEST(snapshot_format); RUN_TEST(incremental_scan);
    RUN_TEST(watch_deadbands); RUN_TEST(watch_rate_latest_independent_and_wrap);
    RUN_TEST(watch_sixty_seconds_noise_bounded_all_channels_and_digital_unfiltered); RUN_TEST(watch_queue_expiration_and_wrap); return UNITY_END();
}
