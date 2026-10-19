// Presets del Studio (variants/studio/theory/06-studio-presets.md §6): formato, banco y banco de fábrica.
#include <string.h>
#include <unity.h>

#include "ui/preset_bank.h"

using namespace studio;

void setUp() {}
void tearDown() {}

namespace {
/// Almacenamiento en RAM: un archivo por (engine, índice).
class FakeStorage : public PresetStorage {
public:
    size_t read(uint8_t e, uint8_t i, uint8_t* buf, size_t cap) override {
        if (len[e][i] == 0 || cap < len[e][i]) return 0;
        memcpy(buf, data[e][i], len[e][i]);
        return len[e][i];
    }
    bool write(uint8_t e, uint8_t i, const uint8_t* buf, size_t n) override {
        if (fail || n > sizeof(data[e][i])) return false;
        memcpy(data[e][i], buf, n);
        len[e][i] = n;
        ++writes;
        return true;
    }
    uint8_t data[GF_ENGINE_COUNT][PRESET_MAX][PRESET_BYTES] = {};
    size_t len[GF_ENGINE_COUNT][PRESET_MAX] = {};
    int writes = 0;
    bool fail = false;
};
}  // namespace

void test_roundtrip_completo() {
    Preset p;
    patch_init(p.patch, 2);
    strcpy(p.name, "TEST 16 CARACTER");
    p.patch.attack_ms = 123.0f;
    p.patch.osc[1][7] = 1.0f;
    p.patch.mod[1][5] = 1.0f;
    p.patch.lfo_shape = 4;
    p.patch.sfx_on[2] = true;
    p.patch.sfx_p[3][2] = 0.75f;
    p.cutoff_hz = 1234.0f;
    p.reso_norm = 0.9f;
    uint8_t buf[PRESET_BYTES];
    TEST_ASSERT_EQUAL_UINT32(PRESET_BYTES, preset_encode(p, 2, buf, sizeof(buf)));
    Preset q;
    TEST_ASSERT_TRUE(preset_decode(buf, PRESET_BYTES, 2, &q));
    TEST_ASSERT_EQUAL_STRING("TEST 16 CARACTER", q.name);
    TEST_ASSERT_EQUAL_MEMORY(&p.patch.osc, &q.patch.osc, sizeof(p.patch.osc));
    TEST_ASSERT_EQUAL_MEMORY(&p.patch.mod, &q.patch.mod, sizeof(p.patch.mod));
    TEST_ASSERT_EQUAL_FLOAT(123.0f, q.patch.attack_ms);
    TEST_ASSERT_EQUAL_UINT8(4, q.patch.lfo_shape);
    TEST_ASSERT_TRUE(q.patch.sfx_on[2]);
    TEST_ASSERT_FALSE(q.patch.sfx_on[1]);
    TEST_ASSERT_EQUAL_FLOAT(0.75f, q.patch.sfx_p[3][2]);
    TEST_ASSERT_EQUAL_FLOAT(1234.0f, q.cutoff_hz);
    TEST_ASSERT_EQUAL_FLOAT(0.9f, q.reso_norm);
}

void test_rechaza_otro_engine_version_y_tamano() {
    Preset p, q;
    uint8_t buf[PRESET_BYTES];
    preset_encode(p, 1, buf, sizeof(buf));
    TEST_ASSERT_FALSE(preset_decode(buf, PRESET_BYTES, 3, &q));          // otro engine
    TEST_ASSERT_FALSE(preset_decode(buf, PRESET_BYTES - 1, 1, &q));      // truncado
    buf[3] = PRESET_FORMAT + 1;
    TEST_ASSERT_FALSE(preset_decode(buf, PRESET_BYTES, 1, &q));          // versión futura
    TEST_ASSERT_EQUAL_UINT32(0, preset_encode(p, 1, buf, PRESET_BYTES - 1));
}

void test_banco_vacio_escribe_fabrica() {
    FakeStorage s;
    PresetBank bank(s);
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e) {
        bank.scan(e);
        TEST_ASSERT_EQUAL_UINT8(6, bank.count(e));
        Preset p;
        for (uint8_t i = 0; i < bank.count(e); ++i) TEST_ASSERT_TRUE(bank.load(e, i, &p));
    }
    TEST_ASSERT_EQUAL_STRING("E.PIANO", bank.name(4, 0));
    TEST_ASSERT_EQUAL_STRING("POLYMOD BELL", bank.name(2, 3));
    const int writes = s.writes;
    PresetBank again(s);                                                  // segundo arranque: no reescribe
    again.scan(2);
    TEST_ASSERT_EQUAL_INT(writes, s.writes);
    TEST_ASSERT_EQUAL_UINT8(6, again.count(2));
}

void test_save_new_hasta_16() {
    FakeStorage s;
    PresetBank bank(s);
    bank.scan(0);
    Preset p;
    p.patch.drive = 0.8f;
    TEST_ASSERT_EQUAL_INT(6, bank.save_new(0, p));
    TEST_ASSERT_EQUAL_STRING("USER 07", bank.name(0, 6));
    Preset q;
    TEST_ASSERT_TRUE(bank.load(0, 6, &q));
    TEST_ASSERT_EQUAL_FLOAT(0.8f, q.patch.drive);
    while (bank.count(0) < PRESET_MAX) bank.save_new(0, p);
    TEST_ASSERT_EQUAL_INT(-1, bank.save_new(0, p));                       // lleno
    TEST_ASSERT_FALSE(bank.load(0, PRESET_MAX, &q));
    s.fail = true;
    bank.scan(1);                                                         // si no se puede escribir, banco vacío
    TEST_ASSERT_EQUAL_UINT8(0, bank.count(1));
}

// Los patches de fábrica respetan la plantilla de cada engine (rangos y discretos de GF_OSC_SLOT / GF_MOD_SLOT).
void test_fabrica_dentro_de_rango() {
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e) {
        Preset fac[PRESET_MAX];
        const uint8_t n = factory_presets(e, fac, PRESET_MAX);
        TEST_ASSERT_TRUE(n >= 6);
        for (uint8_t i = 0; i < n; ++i) {
            const Patch& t = fac[i].patch;
            TEST_ASSERT_TRUE(strlen(fac[i].name) > 0);
            for (uint8_t st = 0; st < GF_OSC_STOPS; ++st)
                for (uint8_t sl = 0; sl < GF_OSC_SLOTS; ++sl) {
                    const GF_OscSlot* g = &GF_OSC_SLOT[e][st][sl];
                    if (!gf_osc_used(g) || (g->global && st > 0)) continue;
                    const float v = t.osc[st][sl];
                    TEST_ASSERT_TRUE_MESSAGE(v >= g->lo && v <= g->hi, fac[i].name);
                }
            for (uint8_t st = 0; st < GF_MOD_STOPS[e]; ++st)
                for (uint8_t sl = 0; sl < GF_OSC_SLOTS; ++sl) {
                    const GF_OscSlot* g = &GF_MOD_SLOT[e][st][sl];
                    if (gf_osc_used(g)) TEST_ASSERT_TRUE_MESSAGE(t.mod[st][sl] >= g->lo && t.mod[st][sl] <= g->hi, fac[i].name);
                }
            TEST_ASSERT_TRUE(fac[i].cutoff_hz >= GF_CUTOFF_RANGE[e].lo && fac[i].cutoff_hz <= GF_CUTOFF_RANGE[e].hi);
            TEST_ASSERT_TRUE(fac[i].reso_norm >= 0.0f && fac[i].reso_norm <= 1.0f);
            TEST_ASSERT_TRUE(t.sustain >= 0.0f && t.sustain <= 1.0f && t.f_sustain >= 0.0f && t.f_sustain <= 1.0f);
            TEST_ASSERT_TRUE(t.attack_ms <= GF_ATTACK_RANGE[e].hi && t.decay_ms >= 1.0f && t.release_ms >= 1.0f);
            TEST_ASSERT_TRUE(t.lfo_rate_hz >= GF_LFO_RATE_MIN && t.lfo_rate_hz <= GF_LFO_RATE_MAX);
            TEST_ASSERT_TRUE(t.lfo_shape < GF_LFO_SHAPES);
        }
    }
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip_completo);
    RUN_TEST(test_rechaza_otro_engine_version_y_tamano);
    RUN_TEST(test_banco_vacio_escribe_fabrica);
    RUN_TEST(test_save_new_hasta_16);
    RUN_TEST(test_fabrica_dentro_de_rango);
    return UNITY_END();
}
