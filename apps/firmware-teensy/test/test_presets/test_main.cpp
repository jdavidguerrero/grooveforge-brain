// Presets del Studio (variants/studio/theory/06-studio-presets.md §6): formato, banco y banco de fábrica.
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "ui/preset_bank.h"
#include "ui/preset_link.h"

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
    bool remove(uint8_t e, uint8_t i) override {
        len[e][i] = 0;
        return true;
    }
    uint8_t data[GF_ENGINE_COUNT][PRESET_MAX + 1][PRESET_BYTES] = {};   // + FACTORY_SLOT
    size_t len[GF_ENGINE_COUNT][PRESET_MAX + 1] = {};
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
        Preset fac[PRESET_MAX];
        TEST_ASSERT_EQUAL_UINT8(factory_presets(e, fac, PRESET_MAX), bank.count(e));
        TEST_ASSERT_TRUE(bank.count(e) >= 6);
        Preset p;
        for (uint8_t i = 0; i < bank.count(e); ++i) TEST_ASSERT_TRUE(bank.load(e, i, &p));
    }
    TEST_ASSERT_EQUAL_STRING("E.PIANO", bank.name(4, 0));
    TEST_ASSERT_EQUAL_STRING("POLYMOD BELL", bank.name(2, 3));
    const int writes = s.writes;
    PresetBank again(s);                                                  // segundo arranque: no reescribe
    again.scan(2);
    TEST_ASSERT_EQUAL_INT(writes, s.writes);
    TEST_ASSERT_EQUAL_UINT8(bank.count(2), again.count(2));
}

void test_save_new_hasta_16() {
    FakeStorage s;
    PresetBank bank(s);
    bank.scan(0);
    Preset p;
    p.patch.drive = 0.8f;
    const uint8_t n0 = bank.count(0);                                   // 6 clásicos + los del track
    TEST_ASSERT_EQUAL_INT(n0, bank.save_new(0, p));
    char user[16];
    snprintf(user, sizeof(user), "USER %02u", unsigned(n0 + 1));
    TEST_ASSERT_EQUAL_STRING(user, bank.name(0, n0));
    Preset q;
    TEST_ASSERT_TRUE(bank.load(0, n0, &q));
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

/// Un preset de formato 2 = el actual sin GROOVE ni velocity al final; el 3, sin la velocity.
static size_t encode_v2(const Preset& p, uint8_t e, uint8_t* buf) {
    preset_encode(p, e, buf, PRESET_BYTES);
    buf[3] = 2;
    return PRESET_BYTES_V2;
}

void test_migra_formato_3_con_velocity_del_engine() {
    Preset p;
    patch_init(p.patch, 1);
    p.patch.vel_amp = 90.0f;                                            // no viaja en el formato 3
    p.patch.groove[GF_GROOVE_PUMP][GF_PUMP_DEPTH] = 77;                 // sí viaja
    uint8_t buf[PRESET_BYTES];
    preset_encode(p, 1, buf, PRESET_BYTES);
    buf[3] = 3;
    Preset q;
    TEST_ASSERT_TRUE(preset_decode(buf, PRESET_BYTES_V3, 1, &q));
    TEST_ASSERT_EQUAL_FLOAT(77.0f, q.patch.groove[GF_GROOVE_PUMP][GF_PUMP_DEPTH]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, q.patch.vel_amp);                     // Juno: sin velocity, como sonaba
    TEST_ASSERT_TRUE(preset_decode(buf, PRESET_BYTES_V3, 1, &q));
    buf[3] = 4;
    TEST_ASSERT_TRUE(preset_decode(buf, PRESET_BYTES, 1, &q));
    TEST_ASSERT_EQUAL_FLOAT(90.0f, q.patch.vel_amp);
}

void test_migra_formato_2() {
    Preset p;
    patch_init(p.patch, 1);
    strcpy(p.name, "MI PAD VIEJO");
    p.patch.attack_ms = 777.0f;
    p.patch.groove[GF_GROOVE_PUMP][GF_PUMP_DEPTH] = 99;   // no viaja en el formato 2
    p.patch.groove_on[GF_GROOVE_ARP] = true;
    uint8_t buf[PRESET_BYTES];
    const size_t len = encode_v2(p, 1, buf);
    Preset q;
    TEST_ASSERT_TRUE(preset_decode(buf, len, 1, &q));
    TEST_ASSERT_EQUAL_STRING("MI PAD VIEJO", q.name);
    TEST_ASSERT_EQUAL_FLOAT(777.0f, q.patch.attack_ms);
    const Patch neutral;
    TEST_ASSERT_EQUAL_FLOAT(neutral.groove[GF_GROOVE_PUMP][GF_PUMP_DEPTH], q.patch.groove[GF_GROOVE_PUMP][GF_PUMP_DEPTH]);
    TEST_ASSERT_FALSE(q.patch.groove_on[GF_GROOVE_ARP]);
    TEST_ASSERT_FALSE(preset_decode(buf, PRESET_BYTES, 1, &q));   // formato 2 con largo de 3: corrupto
    buf[3] = 1;
    TEST_ASSERT_FALSE(preset_decode(buf, len, 1, &q));            // formato 1: no se migra
}

void test_banco_existente_conserva_y_agrega_fabrica() {
    FakeStorage s;
    Preset mine;
    patch_init(mine.patch, 2);
    strcpy(mine.name, "USER 01");
    mine.patch.drive = 0.9f;
    uint8_t buf[PRESET_BYTES];
    s.write(2, 0, buf, encode_v2(mine, 2, buf));                // un banco viejo: un preset propio en formato 2
    PresetBank bank(s);
    bank.scan(2);
    Preset fac[PRESET_MAX];
    const uint8_t n = factory_presets(2, fac, PRESET_MAX);
    TEST_ASSERT_EQUAL_UINT8(1 + n, bank.count(2));                // el propio queda primero, la fábrica detrás
    TEST_ASSERT_EQUAL_STRING("USER 01", bank.name(2, 0));
    Preset q;
    TEST_ASSERT_TRUE(bank.load(2, 0, &q));
    TEST_ASSERT_EQUAL_FLOAT(0.9f, q.patch.drive);
    TEST_ASSERT_EQUAL_STRING(fac[0].name, bank.name(2, 1));
    const int writes = s.writes;
    PresetBank again(s);                                          // segundo arranque: ya están todos
    again.scan(2);
    TEST_ASSERT_EQUAL_INT(writes, s.writes);
}

/// Busca un preset de fábrica por nombre. @return su índice o -1.
static int find_factory(uint8_t e, const char* name, Preset* out) {
    Preset fac[PRESET_MAX];
    const uint8_t n = factory_presets(e, fac, PRESET_MAX);
    for (uint8_t i = 0; i < n; ++i)
        if (strcmp(fac[i].name, name) == 0) { *out = fac[i]; return i; }
    return -1;
}

void test_presets_del_track() {
    static const struct { uint8_t e; const char* name; } TRACK[] = {
        {0, "JD RUN FUNKBASS"},   {5, "JD RUN BASS BITE"}, {4, "JD RUN PLUCK"}, {2, "JD RUN LEAD BODY"},
        {3, "JD HORIZON NOTCH"},  {1, "JD HORIZON JUNO"}, {1, "JD RAP STAB JUNO"}, {2, "JD RAP STAB P5"},
        {1, "JD VELVET JUNO"},    {4, "JD VELVET RHODES"}, {5, "JD ZAP PERC ARP"},  {4, "JD ZAP PERC WOOD"}};
    Preset p;
    for (const auto& t : TRACK) TEST_ASSERT_TRUE_MESSAGE(find_factory(t.e, t.name, &p) >= 0, t.name);
    find_factory(4, "JD RUN PLUCK", &p);                 // par de 2 operadores: OP3 y OP4 callados
    TEST_ASSERT_EQUAL_FLOAT(1.0f, p.patch.osc[0][2]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, p.patch.osc[2][1]);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, p.patch.osc[1][0]);
    find_factory(1, "JD VELVET JUNO", &p);                    // DRIFT precargado al 60 %
    TEST_ASSERT_EQUAL_FLOAT(60.0f, p.patch.groove[GF_GROOVE_MACRO][GF_MACRO_DRIFT]);
    find_factory(0, "JD RUN FUNKBASS", &p);
    TEST_ASSERT_EQUAL_FLOAT(160.0f, p.cutoff_hz);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.35f, p.patch.drive);
    TEST_ASSERT_EQUAL_FLOAT(80.0f, p.patch.vel_cut);                 // velocity → filtro al 80 %, el pedido original
}

void test_pilotos_signature() {
    Preset p;
    TEST_ASSERT_TRUE(find_factory(0, "GF POCKET BASS", &p) >= 0);
    TEST_ASSERT_EQUAL_FLOAT(60.0f, p.patch.groove[GF_GROOVE_MACRO][GF_MACRO_POCKET]);
    TEST_ASSERT_TRUE(find_factory(1, "GF PUMP PAD", &p) >= 0);
    TEST_ASSERT_TRUE(p.patch.groove_on[GF_GROOVE_PUMP]);
    TEST_ASSERT_TRUE(find_factory(2, "GF ORGANIC ARP", &p) >= 0);
    TEST_ASSERT_TRUE(p.patch.groove_on[GF_GROOVE_ARP]);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, p.patch.groove[GF_GROOVE_ARP][GF_ARP_MODE]);   // SMART
    TEST_ASSERT_TRUE(find_factory(3, "GF MELODIC LEAD", &p) >= 0);
    TEST_ASSERT_TRUE(p.patch.groove_on[GF_GROOVE_UNISON]);
    TEST_ASSERT_TRUE(find_factory(4, "GF FM DISCO STAB", &p) >= 0);
    TEST_ASSERT_TRUE(p.patch.groove_on[GF_GROOVE_CHORD]);
    TEST_ASSERT_FALSE(p.patch.groove_on[GF_GROOVE_ARP]);
}

// ── Bibliotecario (theory/11) ─────────────────────────────────────────────────

void test_tamanos_iguales_al_script_de_la_mac() {
    // tools/preset-librarian/gf_presets.py SIZE = {2: 355, 3: 519, 4: 527}: si cambia el formato, cambiar los dos
    TEST_ASSERT_EQUAL_UINT32(355, PRESET_BYTES_V2);
    TEST_ASSERT_EQUAL_UINT32(519, PRESET_BYTES_V3);
    TEST_ASSERT_EQUAL_UINT32(527, PRESET_BYTES);
}

void test_hex_ida_y_vuelta() {
    const uint8_t in[4] = {0x00, 0x7F, 0xA5, 0xFF};
    char hex[9];
    hex_encode(in, 4, hex);
    TEST_ASSERT_EQUAL_STRING("007FA5FF", hex);
    uint8_t out[4];
    TEST_ASSERT_EQUAL_UINT32(4, hex_decode("007fa5ff", out, 4));
    TEST_ASSERT_EQUAL_MEMORY(in, out, 4);
    TEST_ASSERT_EQUAL_UINT32(0, hex_decode("ABC", out, 4));       // impar
    TEST_ASSERT_EQUAL_UINT32(0, hex_decode("ZZ", out, 4));        // inválido
    TEST_ASSERT_EQUAL_UINT32(0, hex_decode("0011223344", out, 4));   // no cabe
}

void test_parseo_de_comandos() {
    char l1[] = "@GF HELLO\r";
    TEST_ASSERT_EQUAL(int(LinkCmd::Hello), int(parse_link_cmd(l1).kind));
    char l2[] = "@GF PUT 3 7 47465004";
    LinkCmd c = parse_link_cmd(l2);
    TEST_ASSERT_EQUAL(int(LinkCmd::Put), int(c.kind));
    TEST_ASSERT_EQUAL_UINT8(3, c.engine);
    TEST_ASSERT_EQUAL_UINT8(7, c.index);
    TEST_ASSERT_EQUAL_STRING("47465004", c.hex);
    char l3[] = "@GF TRUNC 1 9";
    c = parse_link_cmd(l3);
    TEST_ASSERT_EQUAL(int(LinkCmd::Trunc), int(c.kind));
    TEST_ASSERT_EQUAL_UINT8(9, c.index);
    char l4[] = "[UI] modo SYNTH";
    TEST_ASSERT_EQUAL(int(LinkCmd::None), int(parse_link_cmd(l4).kind));
    char l5[] = "@GF PUT 3 x 00";
    TEST_ASSERT_EQUAL(int(LinkCmd::None), int(parse_link_cmd(l5).kind));
    char l6[] = "@GF EXPORT";
    TEST_ASSERT_EQUAL(int(LinkCmd::Export), int(parse_link_cmd(l6).kind));
}

void test_put_y_trunc_del_banco() {
    FakeStorage s;
    PresetBank bank(s);
    bank.scan(2);
    const uint8_t n = bank.count(2);
    Preset p;
    patch_init(p.patch, 2);
    strcpy(p.name, "MI STAB");
    uint8_t buf[PRESET_BYTES];
    preset_encode(p, 2, buf, sizeof(buf));
    TEST_ASSERT_EQUAL(int(PutResult::Ok), int(bank.put(2, 1, buf, PRESET_BYTES)));     // reemplaza el 1
    TEST_ASSERT_EQUAL_STRING("MI STAB", bank.name(2, 1));
    TEST_ASSERT_EQUAL_UINT8(n, bank.count(2));
    TEST_ASSERT_EQUAL(int(PutResult::Ok), int(bank.put(2, n, buf, PRESET_BYTES)));     // agrega al final
    TEST_ASSERT_EQUAL_UINT8(n + 1, bank.count(2));
    TEST_ASSERT_EQUAL(int(PutResult::Index), int(bank.put(2, n + 5, buf, PRESET_BYTES)));   // hueco
    TEST_ASSERT_EQUAL(int(PutResult::Format), int(bank.put(1, 0, buf, PRESET_BYTES)));      // es de otro engine
    TEST_ASSERT_EQUAL(int(PutResult::Engine), int(bank.put(9, 0, buf, PRESET_BYTES)));
    buf[3] = 2;                                                                          // formato 2: se migra
    TEST_ASSERT_EQUAL(int(PutResult::Ok), int(bank.put(2, 0, buf, PRESET_BYTES_V2)));
    Preset q;
    TEST_ASSERT_TRUE(bank.load(2, 0, &q));
    TEST_ASSERT_EQUAL_STRING("MI STAB", q.name);
    TEST_ASSERT_EQUAL_UINT32(PRESET_BYTES, s.len[2][0]);                                // guardado en el formato actual
    TEST_ASSERT_TRUE(bank.truncate(2, 2));
    TEST_ASSERT_EQUAL_UINT8(2, bank.count(2));
    TEST_ASSERT_EQUAL_UINT32(0, s.len[2][2]);
    PresetBank again(s);                                    // otro arranque: lo borrado de fábrica no vuelve
    again.scan(2);
    TEST_ASSERT_EQUAL_UINT8(2, again.count(2));
    TEST_ASSERT_EQUAL_STRING("MI STAB", again.name(2, 1));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip_completo);
    RUN_TEST(test_rechaza_otro_engine_version_y_tamano);
    RUN_TEST(test_banco_vacio_escribe_fabrica);
    RUN_TEST(test_save_new_hasta_16);
    RUN_TEST(test_fabrica_dentro_de_rango);
    RUN_TEST(test_presets_del_track);
    RUN_TEST(test_migra_formato_2);
    RUN_TEST(test_migra_formato_3_con_velocity_del_engine);
    RUN_TEST(test_banco_existente_conserva_y_agrega_fabrica);
    RUN_TEST(test_pilotos_signature);
    RUN_TEST(test_tamanos_iguales_al_script_de_la_mac);
    RUN_TEST(test_hex_ida_y_vuelta);
    RUN_TEST(test_parseo_de_comandos);
    RUN_TEST(test_put_y_trunc_del_banco);
    return UNITY_END();
}
