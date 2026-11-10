#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include "engines/dx7_6op_core.h"
#include "engines/dx7_6op_params.h"
#include "engines/dx7_6op_sysex.h"
#ifdef GF_SIX_CTEST
#include <cstdlib>
#define TEST_ASSERT_TRUE(x) do { if (!(x)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); std::abort(); } } while (0)
#define TEST_ASSERT_EQUAL(a,b) TEST_ASSERT_TRUE((a)==(b))
#define TEST_ASSERT_FLOAT_WITHIN(e,a,b) TEST_ASSERT_TRUE(std::abs((a)-(b))<=(e))
#define RUN_TEST(f) f()
#define UNITY_BEGIN() 0
#define UNITY_END() 0
#else
#include <unity.h>
#endif
namespace six = gf::dx7six;
void setUp() {}
void tearDown() {}

void test_algorithms_all_routes() {
    // Carrier counts and feedback endpoints independently transcribed from the MSFA table.
    const int carriers[] = {2,2,2,2,3,3,2,2,2,2,2,2,2,2,2,1,1,1,3,3,4,4,4,5,5,3,3,3,4,4,5,6};
    for (int a = 0; a < 32; ++a) {
        int count=0,starts=0,ends=0;
        bool buses[3] = {true,false,false};
        for (auto flags : six::algorithms[a].ops) {
            const int from = (flags >> 4) & 3, to = flags & 3;
            TEST_ASSERT_TRUE(from < 3 && to < 3);
            TEST_ASSERT_TRUE(from == 0 || buses[from]);
            buses[to] = true;
            count += to == 0; starts += bool(flags & 64); ends += bool(flags & 128);
        }
        TEST_ASSERT_EQUAL(carriers[a],count); TEST_ASSERT_EQUAL(1,starts); TEST_ASSERT_EQUAL(1,ends);
    }
}
void test_eg_original_msfa_timings() {
    const int rates[] = {30,60,90,99};
    const int times[][4] = {{1087,2404,3575,156254},{39,87,130,150228},{1,3,5,150009},{1,2,3,150004}};
    for (int j=0;j<4;++j) {
        six::msfa::Env e{}; e.prepare(44100);
        int r[4] = {rates[j],rates[j],rates[j],rates[j]}, l[] = {99,80,65,0};
        e.init(r,l,4064,0); int previous=0;
        for (int tick=1;tick<=times[j][3];++tick) {
            if (tick==150001) e.keydown(false);
            e.getsample();
            if(e.stage()!=previous) {
                if (tick!=150001) TEST_ASSERT_EQUAL(times[j][e.stage()-1],tick);
                previous=e.stage();
            }
        }
        TEST_ASSERT_EQUAL(4,e.stage());
    }
    for (double fs : {44100.0,48000.0,96000.0}) {
        six::msfa::Env e{}; e.prepare(fs); int r[]={30,30,30,30},l[]={99,80,65,0}; e.init(r,l,4064,0);
        int tick=0; while(e.stage()<3 && tick<10000) {e.getsample();++tick;}
        TEST_ASSERT_FLOAT_WITHIN(.012,3575.0*64/44100,tick*64/fs);
    }
}
void test_keyboard_curves() {
    TEST_ASSERT_EQUAL(0,six::scale_level(56,39,99,99,0,3));
    TEST_ASSERT_EQUAL(-95,six::scale_level(20,39,99,0,0,0));
    TEST_ASSERT_EQUAL(95,six::scale_level(92,39,0,99,0,3));
    TEST_ASSERT_EQUAL(-15,six::scale_level(20,39,99,0,1,0));
    TEST_ASSERT_EQUAL(15,six::scale_level(92,39,0,99,0,2));
    for(int n=0;n<128;++n) {
        const int neg=six::scale_level(n,39,70,80,1,1),pos=six::scale_level(n,39,70,80,2,2);
        TEST_ASSERT_EQUAL(-neg,pos);
    }
}
void test_sysex_gfp_roundtrip_and_reject() {
    std::array<six::Patch,32> bank;
    std::array<uint8_t,six::BANK_SYSEX_BYTES> bytes{};
    for(int i=0;i<32;++i) {
        bank[i]=six::init_patch(); bank[i].vced[134]=i; bank[i].vced[135]=i%8;
        for(int op=0;op<6;++op) {
            auto* o=bank[i].vced.data()+op*21; o[11]=i%4; o[12]=(i+1)%4; o[13]=i%8;
            o[14]=i%4; o[15]=i%8; o[17]=i%2; o[18]=i; o[19]=i*3; o[20]=i%15;
        }
        uint8_t packed[128]; six::Patch decoded;
        TEST_ASSERT_TRUE(six::pack_voice(bank[i],packed,128)); TEST_ASSERT_TRUE(six::unpack_voice(packed,128,decoded));
        TEST_ASSERT_TRUE(bank[i].vced==decoded.vced);
    }
    TEST_ASSERT_EQUAL(bytes.size(),six::write_bank_sysex(bank.data(),bytes.data(),bytes.size(),15));
    for(int i=0;i<32;++i) { six::Patch p; TEST_ASSERT_TRUE(six::parse_sysex(bytes.data(),bytes.size(),p,i)); TEST_ASSERT_TRUE(p.vced==bank[i].vced); }
    six::Patch p=bank[0]; const auto saved=p;
    bytes[42]^=1; TEST_ASSERT_TRUE(!six::parse_sysex(bytes.data(),bytes.size(),p)); TEST_ASSERT_TRUE(p.vced==saved.vced);
    bytes[42]^=1; bytes[0]=0; TEST_ASSERT_TRUE(!six::parse_sysex(bytes.data(),bytes.size(),p));
    bytes[0]=0xf0; TEST_ASSERT_TRUE(!six::parse_sysex(bytes.data(),bytes.size()-1,p));
    TEST_ASSERT_EQUAL(six::SINGLE_SYSEX_BYTES,six::write_single_sysex(bank[20],bytes.data(),bytes.size(),3));
    TEST_ASSERT_TRUE(six::parse_sysex(bytes.data(),six::SINGLE_SYSEX_BYTES,p)); TEST_ASSERT_TRUE(p.vced==bank[20].vced);
    p.enabled=21; const auto enabled=p;
    TEST_ASSERT_EQUAL(six::GFP_BYTES,six::encode_gfp(p,bytes.data(),bytes.size()));
    TEST_ASSERT_TRUE(six::decode_gfp(bytes.data(),six::GFP_BYTES,p)); TEST_ASSERT_EQUAL(21,p.enabled);
    bytes[3]=5; TEST_ASSERT_TRUE(!six::decode_gfp(bytes.data(),six::GFP_BYTES,p)); TEST_ASSERT_TRUE(p.vced==enabled.vced);
    bytes[3]=6; bytes[4]=4; TEST_ASSERT_TRUE(!six::decode_gfp(bytes.data(),six::GFP_BYTES,p));
}
void test_metadata_complete_and_banked() {
    auto p=six::init_patch(); int hits[155]={},bits[6]={};
    for(size_t i=0;i<six::param_count;++i) {
        const auto& q=six::params[i]; TEST_ASSERT_TRUE(q.slot<8);
        int count=0;
        for(size_t j=0;j<six::param_count;++j) {
            const auto& r=six::params[j];
            if(q.bank==r.bank && !std::strcmp(q.page,r.page))++count;
            if(i!=j) TEST_ASSERT_TRUE(std::strcmp(q.id,r.id)!=0);
        }
        TEST_ASSERT_TRUE(count<=8);
        float value=0; TEST_ASSERT_TRUE(six::get_param(p,q.id,value)); TEST_ASSERT_EQUAL(q.initial,value);
        if(q.enable_bit<0)++hits[q.offset];else ++bits[q.enable_bit];
        TEST_ASSERT_TRUE(six::set_param(p,q.id,999)); TEST_ASSERT_TRUE(six::get_param(p,q.id,value)); TEST_ASSERT_EQUAL(q.maximum,value);
        TEST_ASSERT_TRUE(six::set_param(p,q.id,q.initial));
    }
    for(int i=0;i<145;++i)TEST_ASSERT_EQUAL(1,hits[i]);
    for(int b:bits)TEST_ASSERT_EQUAL(1,b);
    TEST_ASSERT_TRUE(!six::set_param(p,"algorithm",NAN)); TEST_ASSERT_TRUE(six::valid_patch(p));
}
void test_finite_all_algorithms_and_sample_rates() {
    six::Synth s;
    for(double fs:{8000.0,44100.0,48000.0,192000.0}) {
        s.prepare(fs);
        for(int a=0;a<32;++a)for(int mask:{0,1,21,63}) {
            auto p=six::init_patch();p.vced[134]=a;p.vced[135]=7;p.enabled=mask;
            p.vced[139]=99;p.vced[140]=99;p.vced[143]=7;p.vced[142]=a%6;
            for(int op=0;op<6;++op) {auto* o=p.vced.data()+21*op;o[16]=99;o[14]=3;o[15]=7;o[17]=op%2;o[18]=31;o[19]=99;o[20]=14;}
            TEST_ASSERT_TRUE(s.set_patch(p));
            for(int n:{0,24,60,90,110,127,50,65})s.note_on(n,127);
            for(int i=0;i<4096;++i)TEST_ASSERT_TRUE(std::isfinite(s.process_sample()));
            for(int n=0;n<128;++n)s.note_off(n);
            for(int i=0;i<4096;++i)TEST_ASSERT_TRUE(std::isfinite(s.process_sample()));
        }
    }
}
void test_block_independence_and_instance_rates() {
    six::Synth a,b,c;a.prepare(44100);b.prepare(44100);c.prepare(48000);
    a.note_on(60,100);b.note_on(60,100);c.note_on(64,100);
    float block[113];
    for(int k=0;k<80;++k) {
        a.render(block,113);
        for(float v:block) {c.process_sample();TEST_ASSERT_EQUAL(v,b.process_sample());}
    }
}
void test_pitch_velocity_and_live_edit() {
    auto p=six::init_patch();
    for(int i=0;i<3;++i) {p.vced[105+i]=99; p.vced[109+i]=99;}
    p.vced[108]=99;
    six::Synth a,b;a.prepare(48000);b.prepare(48000);
    p.vced[122]=1;p.vced[123]=2;
    a.set_patch(p);b.set_patch(p);a.note_on(30,100);b.note_on(90,100);
    for(int i=0;i<4096;++i)TEST_ASSERT_EQUAL(a.process_sample(),b.process_sample());
    p.vced[122]=0;p.vced[144]=36;a.set_patch(p);p.vced[144]=24;b.set_patch(p);
    a.note_on(60,100);b.note_on(72,100);
    for(int i=0;i<4096;++i)TEST_ASSERT_EQUAL(a.process_sample(),b.process_sample());
    p.vced[120]=7;a.set_patch(p);b.set_patch(p);a.note_on(60,30);b.note_on(60,127);
    double soft=0,loud=0;
    for(int i=0;i<4096;++i){const auto x=a.process_sample(),y=b.process_sample();soft+=x*x;loud+=y*y;}
    TEST_ASSERT_TRUE(loud>10*soft);
    p.vced[121]=60;TEST_ASSERT_TRUE(b.edit_patch(p));double lowered=0;
    for(int i=0;i<4096;++i){const auto x=b.process_sample();lowered+=x*x;}
    TEST_ASSERT_TRUE(lowered>0 && lowered<loud*.1);
    p.vced[121]=0;TEST_ASSERT_TRUE(b.edit_patch(p));
    for(int i=0;i<256;++i)b.process_sample();
    for(int i=0;i<256;++i)TEST_ASSERT_EQUAL(0,b.process_sample());
    p.vced[121]=99;TEST_ASSERT_TRUE(b.edit_patch(p));

    p.enabled=0;TEST_ASSERT_TRUE(b.edit_patch(p));
    for(int i=0;i<256;++i)b.process_sample();
    for(int i=0;i<256;++i)TEST_ASSERT_EQUAL(0,b.process_sample());
    p.enabled=1;TEST_ASSERT_TRUE(b.edit_patch(p));double resumed=0;
    for(int i=0;i<4096;++i){const auto x=b.process_sample();resumed+=x*x;}
    TEST_ASSERT_TRUE(resumed>.01);
    b.set_sustain(true);b.note_off(60);double held=0;
    for(int i=0;i<4096;++i){const auto x=b.process_sample();held+=x*x;}
    TEST_ASSERT_TRUE(held>.01);
    b.set_sustain(false);
    for(int i=0;i<4096;++i)b.process_sample();
    for(int i=0;i<256;++i)TEST_ASSERT_EQUAL(0,b.process_sample());
}
void test_cpu_estimate() {
    six::Synth s;s.prepare(48000);auto p=six::init_patch();p.vced[134]=4;p.vced[135]=7;
    for(int op=0;op<6;++op)p.vced[op*21+16]=85;
    s.set_patch(p);for(int n:{48,52,55,60,64,67})s.note_on(n,100);
    const auto start=std::chrono::steady_clock::now();double energy=0;
    for(int i=0;i<48000;++i){const float y=s.process_sample();energy+=y*y;}
    const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::printf("DX7_6OP CPU six voices %.3f%% one core; %.1f ns/sample/voice; energy %.3f\n",seconds*100,seconds*1e9/(48000*6),energy);
    TEST_ASSERT_TRUE(energy>0 && std::isfinite(energy));
}
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_algorithms_all_routes); RUN_TEST(test_eg_original_msfa_timings);RUN_TEST(test_keyboard_curves);
    RUN_TEST(test_sysex_gfp_roundtrip_and_reject);RUN_TEST(test_metadata_complete_and_banked);
    RUN_TEST(test_finite_all_algorithms_and_sample_rates);RUN_TEST(test_block_independence_and_instance_rates);RUN_TEST(test_pitch_velocity_and_live_edit);RUN_TEST(test_cpu_estimate);
    return UNITY_END();
}
