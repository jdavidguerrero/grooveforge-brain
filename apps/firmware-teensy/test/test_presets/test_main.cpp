// Presets del Studio (variants/studio/theory/06-studio-presets.md §6): formato, banco y banco de fábrica.
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "ui/preset_bank.h"
#include "ui/preset_link.h"
#include "ui/layers.h"
#include "ui/bank_file_storage.h"
#include "ui/layer_bank.h"
#include "ui/groove_macros.h"
#include "ui/session.h"

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
    TEST_ASSERT_TRUE(preset_decode(buf, PRESET_BYTES_V4, 1, &q));
    TEST_ASSERT_EQUAL_FLOAT(90.0f, q.patch.vel_amp);
}

void test_migra_formato_4_a_5_fx() {
    Preset p;
    patch_init(p.patch, 2);
    p.patch.sfx_p[GF_SFX_REVERB][1] = 2.0f / 3.0f;                     // CRISTAL en 4 pasos
    p.patch.sfx_p[GF_SFX_CHORUS][2] = 0.9f;                             // era DRIFT
    p.patch.sfx_p[GF_SFX_DRIVE][0] = 0.6f;                              // era BITS
    p.patch.sfx_on[GF_SFX_REVERB] = true;
    p.patch.sfx_on[GF_SFX_DELAY] = true;                                // no existe en el formato 4
    uint8_t buf[PRESET_BYTES];
    TEST_ASSERT_EQUAL_UINT32(PRESET_BYTES, preset_encode(p, 2, buf, PRESET_BYTES));
    Preset q;
    TEST_ASSERT_TRUE(preset_decode(buf, PRESET_BYTES, 2, &q));          // formato 5: ida y vuelta
    TEST_ASSERT_TRUE(q.patch.sfx_on[GF_SFX_DELAY]);
    buf[3] = 4;                                                         // el mismo, leído como formato 4
    TEST_ASSERT_TRUE(preset_decode(buf, PRESET_BYTES_V4, 2, &q));
    TEST_ASSERT_FALSE(q.patch.sfx_on[GF_SFX_DELAY]);                    // DELAY apagado
    TEST_ASSERT_TRUE(q.patch.sfx_on[GF_SFX_REVERB]);
    TEST_ASSERT_EQUAL_UINT8(2, gf_index_from_norm(q.patch.sfx_p[GF_SFX_REVERB][1], 5));   // sigue en CRISTAL
    TEST_ASSERT_EQUAL_FLOAT(0.0f, q.patch.sfx_p[GF_SFX_CHORUS][2]);    // TYPE = CHORUS
    TEST_ASSERT_EQUAL_UINT8(2, gf_index_from_norm(q.patch.sfx_p[GF_SFX_DRIVE][2], 3));    // DRIVE en CRUSH
    TEST_ASSERT_EQUAL_FLOAT(0.6f, q.patch.sfx_p[GF_SFX_DRIVE][0]);
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

void test_fabrica_de_a_uno_y_se_retoma() {
    FakeStorage s;
    PresetBank bank(s);
    bank.read_bank(3);                                          // el arranque solo lee: no escribe nada
    TEST_ASSERT_EQUAL_INT(0, s.writes);
    TEST_ASSERT_EQUAL_UINT8(0, bank.count(3));
    TEST_ASSERT_TRUE(bank.seeding(3));
    TEST_ASSERT_TRUE(bank.seed_one(3));                         // de a un preset por llamada
    TEST_ASSERT_EQUAL_INT(1, s.writes);
    TEST_ASSERT_TRUE(bank.seed_one(3));
    TEST_ASSERT_EQUAL_UINT8(2, bank.count(3));
    PresetBank after_cut(s);                                    // se cortó la corriente: el próximo arranque sigue
    after_cut.read_bank(3);
    TEST_ASSERT_EQUAL_UINT8(2, after_cut.count(3));
    while (after_cut.seed_one(3)) {
    }
    TEST_ASSERT_FALSE(after_cut.seeding(3));
    TEST_ASSERT_EQUAL_UINT8(factory_count(3), after_cut.count(3));
    TEST_ASSERT_TRUE(s.len[3][FACTORY_SLOT] > 0);              // anotados al terminar
    const int writes = s.writes;
    PresetBank again(s);
    again.read_bank(3);
    TEST_ASSERT_FALSE(again.seeding(3));                        // nada pendiente: no vuelve a escribir
    TEST_ASSERT_EQUAL_INT(writes, s.writes);
}

void test_trunc_durante_la_escritura_de_fabrica() {
    FakeStorage s;
    PresetBank bank(s);
    bank.read_bank(1);
    bank.seed_one(1);
    bank.seed_one(1);
    bank.seed_one(1);
    TEST_ASSERT_TRUE(bank.truncate(1, 1));                      // desde la Mac, a mitad del primer arranque
    while (bank.seed_one(1)) {
    }
    TEST_ASSERT_EQUAL_UINT8(1, bank.count(1));                  // no vuelve a crecer
    PresetBank again(s);
    again.read_bank(1);
    TEST_ASSERT_FALSE(again.seeding(1));
    TEST_ASSERT_EQUAL_UINT8(1, again.count(1));
}

void test_hibridos_en_pares() {
    static const struct { uint8_t e; const char* name; } HY[] = {
        {4, "HY WOODBASS A"},    {0, "HY WOODBASS B"},    {4, "HY KALIMBA A"},     {3, "HY KALIMBA B"},
        {1, "HY FRENCH STAB A"}, {2, "HY FRENCH STAB B"}, {5, "HY ACID ARP A"},    {0, "HY ACID ARP B"},
        {3, "HY DESERT LEAD A"}, {2, "HY DESERT LEAD B"}, {5, "HY SHAKER ZAP A"},  {4, "HY SHAKER ZAP B"},
        {0, "HY ROLLER SUB A"},  {1, "HY ROLLER SUB B"},  {2, "HY RETRO BRASS A"}, {1, "HY RETRO BRASS B"}};
    Preset p;
    for (const auto& h : HY) TEST_ASSERT_TRUE_MESSAGE(find_factory(h.e, h.name, &p) >= 0, h.name);
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e)                       // los de fábrica caben en A y B: C y D son tuyos
        TEST_ASSERT_TRUE(factory_count(e) <= 2 * PRESET_BANK_SIZE);
}

void test_capas_apuntan_a_sus_mitades() {
    Preset p;
    for (uint8_t i = 0; i < LAYER_COUNT; ++i) {
        const LayerDef& d = layer_def(i);
        TEST_ASSERT_TRUE_MESSAGE(find_factory(d.eng_a, d.preset_a, &p) >= 0, d.preset_a);
        TEST_ASSERT_TRUE_MESSAGE(find_factory(d.eng_b, d.preset_b, &p) >= 0, d.preset_b);
        TEST_ASSERT_NOT_EQUAL(d.eng_a, d.eng_b);
    }
}

void test_morph_igual_potencia_y_cc18() {
    const LayerDef& d = layer_def(1);                          // KALIMBA: A 0 dB, B −6 dB
    float ga, gb;
    layer_gains(d, 0.0f, &ga, &gb);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, ga);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, gb);
    layer_gains(d, 0.5f, &ga, &gb);                            // las dos a su ganancia base
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, ga);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.501f, gb);
    layer_gains(d, 1.0f, &ga, &gb);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, ga);
    TEST_ASSERT_EQUAL_UINT8(0, layer_from_cc(0));
    TEST_ASSERT_EQUAL_UINT8(1, layer_from_cc(1));
    TEST_ASSERT_EQUAL_UINT8(LAYER_COUNT, layer_from_cc(127));
    TEST_ASSERT_EQUAL_UINT8(48, layer_b_note(60, -12));
    TEST_ASSERT_EQUAL_UINT8(127, layer_b_note(120, 12));
}

void test_banco_de_64_y_anotacion_en_64() {
    TEST_ASSERT_EQUAL_UINT8(64, PRESET_MAX);
    TEST_ASSERT_EQUAL_UINT8(64, FACTORY_SLOT);
    FakeStorage s;
    PresetBank bank(s);
    bank.scan(1);
    Preset p;
    patch_init(p.patch, 1);
    while (bank.count(1) < PRESET_MAX) TEST_ASSERT_TRUE(bank.save_new(1, p) >= 0);   // hasta D-16
    TEST_ASSERT_EQUAL_INT(-1, bank.save_new(1, p));
    TEST_ASSERT_TRUE(s.len[1][FACTORY_SLOT] > 0);
    PresetBank again(s);
    again.read_bank(1);
    TEST_ASSERT_EQUAL_UINT8(64, again.count(1));
}

void test_tamanos_iguales_al_script_de_la_mac() {
    // tools/preset-librarian/gf_presets.py SIZE = {2: 355, 3: 519, 4: 527, 5: 544}: si cambia el formato, cambiar los dos
    TEST_ASSERT_EQUAL_UINT32(355, PRESET_BYTES_V2);
    TEST_ASSERT_EQUAL_UINT32(519, PRESET_BYTES_V3);
    TEST_ASSERT_EQUAL_UINT32(527, PRESET_BYTES_V4);
    TEST_ASSERT_EQUAL_UINT32(544, PRESET_BYTES);
    TEST_ASSERT_EQUAL_UINT32(1077, LAYER_BYTES_V1);
    TEST_ASSERT_EQUAL_UINT32(1111, LAYER_BYTES);
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
    char l7[] = "@GF FS";
    TEST_ASSERT_EQUAL(int(LinkCmd::Fs), int(parse_link_cmd(l7).kind));
    char l8[] = "@GF FORMAT YES";
    TEST_ASSERT_EQUAL(int(LinkCmd::Format), int(parse_link_cmd(l8).kind));
    char l9[] = "@GF FORMAT";                                  // sin YES no borra
    TEST_ASSERT_EQUAL(int(LinkCmd::None), int(parse_link_cmd(l9).kind));
}

void test_comandos_de_la_consola() {
    char usb[] = "@GF USB\r";
    TEST_ASSERT_EQUAL(studio::LinkCmd::Usb, studio::parse_link_cmd(usb).kind);
    char usb_arg[] = "@GF USB RESET";
    TEST_ASSERT_EQUAL(studio::LinkCmd::None, studio::parse_link_cmd(usb_arg).kind);
    char a[] = "@GF RUTA LINEA";
    studio::LinkCmd c = studio::parse_link_cmd(a);
    TEST_ASSERT_EQUAL(studio::LinkCmd::Ruta, c.kind);
    TEST_ASSERT_EQUAL_UINT8(1, c.valor);
    char b[] = "@GF RUTA SINTE\r";
    c = studio::parse_link_cmd(b);
    TEST_ASSERT_EQUAL(studio::LinkCmd::Ruta, c.kind);
    TEST_ASSERT_EQUAL_UINT8(0, c.valor);
    char d[] = "@GF CUE 1";
    c = studio::parse_link_cmd(d);
    TEST_ASSERT_EQUAL(studio::LinkCmd::Cue, c.kind);
    TEST_ASSERT_EQUAL_UINT8(1, c.valor);
    char e[] = "@GF RUTA OTRA";
    TEST_ASSERT_EQUAL(studio::LinkCmd::None, studio::parse_link_cmd(e).kind);
    char g[] = "@GF CUE 2";
    TEST_ASSERT_EQUAL(studio::LinkCmd::None, studio::parse_link_cmd(g).kind);
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

// ── Un archivo por engine (theory/13 rev. 3) ──────────────────────────────────

namespace {
class FakeFiles : public FileBackend {
public:
    size_t read_file(const char* path, uint8_t* buf, size_t cap) override {
        for (int k = 0; k < n; ++k)
            if (!strcmp(name[k], path)) {
                const size_t l = len[k] < cap ? len[k] : cap;
                memcpy(buf, data[k], l);
                return l;
            }
        return 0;
    }
    bool write_file(const char* path, const uint8_t* buf, size_t l) override {
        ++writes;
        int k = 0;
        while (k < n && strcmp(name[k], path)) ++k;
        if (k == n) { strcpy(name[n], path); ++n; }
        memcpy(data[k], buf, l);
        len[k] = l;
        return true;
    }
    char name[8][16] = {};
    uint8_t data[8][LayerBank::FILE_BYTES > BankFileStorage::FILE_BYTES ? LayerBank::FILE_BYTES
                                                                         : BankFileStorage::FILE_BYTES] = {};
    size_t len[8] = {};
    int n = 0, writes = 0;
};
uint8_t g_bank_buf[BankFileStorage::FILE_BYTES];
uint8_t g_layer_buf[LayerBank::FILE_BYTES];
}  // namespace

void test_un_archivo_por_engine() {
    TEST_ASSERT_TRUE(BankFileStorage::FILE_BYTES < 65536);     // entra en un bloque de 64 KB de la Teensy 4.1
    FakeFiles files;
    BankFileStorage st(files, g_bank_buf);
    PresetBank bank(st);
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e) bank.scan(e);
    TEST_ASSERT_EQUAL_INT(GF_ENGINE_COUNT, files.n);            // 6 archivos para todos los presets de fábrica
    TEST_ASSERT_EQUAL_INT(GF_ENGINE_COUNT, files.writes);       // una escritura por banco, no una por preset
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e) TEST_ASSERT_EQUAL_UINT8(factory_count(e), bank.count(e));
    TEST_ASSERT_EQUAL_STRING("/bank3.gfb", files.name[3]);

    BankFileStorage st2(files, g_bank_buf);                     // otro arranque: lee lo guardado
    PresetBank again(st2);
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e) {
        again.read_bank(e);
        TEST_ASSERT_EQUAL_UINT8(factory_count(e), again.count(e));
        TEST_ASSERT_FALSE(again.seeding(e));
    }
    Preset p;
    TEST_ASSERT_TRUE(again.load(4, 0, &p));
    TEST_ASSERT_EQUAL_STRING("E.PIANO", p.name);
    patch_init(p.patch, 2);
    const int w = files.writes;
    TEST_ASSERT_TRUE(again.save_new(2, p) >= 0);                // guardar = una escritura del banco de ese engine
    TEST_ASSERT_EQUAL_INT(w + 1, files.writes);
    TEST_ASSERT_TRUE(again.truncate(2, 3));
    BankFileStorage st3(files, g_bank_buf);
    PresetBank third(st3);
    third.read_bank(2);
    TEST_ASSERT_EQUAL_UINT8(3, third.count(2));
}

// ── Presets de capa de HYBRID SYNTH (theory/12 §3d) ──────────────────────────

void test_presets_de_capa() {
    TEST_ASSERT_TRUE(LayerBank::FILE_BYTES < 65536);           // un bloque de 64 KB
    static FakeFiles files;
    LayerBank bank(files, g_layer_buf);
    bank.load();                                                // sin archivo: las 8 de fábrica, una escritura
    TEST_ASSERT_EQUAL_UINT8(LAYER_COUNT, bank.count());
    TEST_ASSERT_EQUAL_INT(1, files.writes);
    TEST_ASSERT_EQUAL_STRING("/layers.gfl", files.name[0]);
    TEST_ASSERT_EQUAL_STRING("WOODBASS", bank.name(0));
    LayerPreset l;
    TEST_ASSERT_TRUE(bank.get(0, &l));                          // las dos mitades completas, de su engine
    TEST_ASSERT_EQUAL_UINT8(4, l.eng_a);
    TEST_ASSERT_EQUAL_UINT8(0, l.eng_b);
    TEST_ASSERT_EQUAL_STRING("HY WOODBASS A", l.a.name);
    TEST_ASSERT_EQUAL_STRING("HY WOODBASS B", l.b.name);
    TEST_ASSERT_EQUAL_UINT8(10, l.delay_b_ms);
    TEST_ASSERT_EQUAL_INT8(-6, l.gain_a_db);

    l.morph = 80;
    l.transpose_b = -12;
    l.b.cutoff_hz = 777.0f;
    TEST_ASSERT_EQUAL_INT(8, bank.save_new(l));
    TEST_ASSERT_EQUAL_STRING("LAYER 09", bank.name(8));

    LayerBank again(files, g_layer_buf);                        // otro arranque: lee el archivo
    again.load();
    TEST_ASSERT_EQUAL_UINT8(9, again.count());
    LayerPreset r;
    TEST_ASSERT_TRUE(again.get(8, &r));
    TEST_ASSERT_EQUAL_UINT8(80, r.morph);
    TEST_ASSERT_EQUAL_INT8(-12, r.transpose_b);
    TEST_ASSERT_EQUAL_FLOAT(777.0f, r.b.cutoff_hz);

    uint8_t raw[LAYER_BYTES];                                   // bibliotecario: exportar, subir y recortar
    TEST_ASSERT_EQUAL_UINT32(LAYER_BYTES, again.raw(8, raw, sizeof(raw)));
    TEST_ASSERT_EQUAL(int(PutResult::Ok), int(again.put(0, raw, LAYER_BYTES)));
    TEST_ASSERT_EQUAL_STRING("LAYER 09", again.name(0));
    TEST_ASSERT_EQUAL(int(PutResult::Index), int(again.put(11, raw, LAYER_BYTES)));
    raw[4 + LAYER_NAME_LEN] = raw[5 + LAYER_NAME_LEN];          // A y B del mismo engine: no se acepta
    TEST_ASSERT_EQUAL(int(PutResult::Format), int(again.put(9, raw, LAYER_BYTES)));
    TEST_ASSERT_TRUE(again.truncate(2));
    again.flush();
    LayerBank third(files, g_layer_buf);
    third.load();
    TEST_ASSERT_EQUAL_UINT8(2, third.count());
    TEST_ASSERT_EQUAL_UINT8(5, layer_from_cc(127, 5));          // CC 18 sigue al tamaño del banco
    char l1[] = "@GF LPUT 3 0A0B", l2[] = "@GF LTRUNC 8", l3[] = "@GF LEXPORT";
    LinkCmd c = parse_link_cmd(l1);
    TEST_ASSERT_EQUAL(int(LinkCmd::LayerPut), int(c.kind));
    TEST_ASSERT_EQUAL_UINT8(3, c.index);
    TEST_ASSERT_EQUAL_STRING("0A0B", c.hex);
    c = parse_link_cmd(l2);
    TEST_ASSERT_EQUAL(int(LinkCmd::LayerTrunc), int(c.kind));
    TEST_ASSERT_EQUAL_UINT8(8, c.index);
    TEST_ASSERT_EQUAL(int(LinkCmd::LayerExport), int(parse_link_cmd(l3).kind));
}

// Bancos y capas del tamaño viejo (formato 4 adentro) se leen enteros (theory/fase3-fx.md §3)
void test_archivos_viejos_se_leen_enteros() {
    static FakeFiles files;
    files = FakeFiles{};
    // Banco v1: "GFB"+1 y lugares de 2 + 527 con un preset de formato 4
    static uint8_t old[BankFileStorage::FILE_BYTES_V1];
    memset(old, 0, sizeof(old));
    old[0] = 'G'; old[1] = 'F'; old[2] = 'B'; old[3] = 1;
    Preset p;
    patch_init(p.patch, 3);
    strcpy(p.name, "VIEJO");
    uint8_t enc[PRESET_BYTES];
    preset_encode(p, 3, enc, PRESET_BYTES);
    enc[3] = 4;                                                         // el prefijo del formato 5 es el formato 4
    for (int i = 0; i < 3; ++i) {
        uint8_t* s = old + BankFileStorage::HEADER + i * BankFileStorage::SLOT_V1;
        s[0] = uint8_t(PRESET_BYTES_V4 & 0xFF);
        s[1] = uint8_t(PRESET_BYTES_V4 >> 8);
        memcpy(s + 2, enc, PRESET_BYTES_V4);
    }
    files.write_file("/bank3.gfb", old, sizeof(old));
    BankFileStorage st(files, g_bank_buf);
    PresetBank bank(st);
    bank.read_bank(3);
    TEST_ASSERT_EQUAL_UINT8(3, bank.count(3));
    Preset q;
    TEST_ASSERT_TRUE(bank.load(3, 2, &q));
    TEST_ASSERT_EQUAL_STRING("VIEJO", q.name);

    // Capas v1: "GFK"+1 y una capa de formato 1 (mitades en formato 4)
    static uint8_t oldl[LayerBank::FILE_BYTES_V1];
    memset(oldl, 0, sizeof(oldl));
    oldl[0] = 'G'; oldl[1] = 'F'; oldl[2] = 'K'; oldl[3] = 1;
    LayerPreset l;
    factory_layer(0, &l);
    uint8_t le[LAYER_BYTES];
    layer_encode(l, le, LAYER_BYTES);
    uint8_t v1[LAYER_BYTES_V1];                                         // re-armada con mitades de formato 4
    memcpy(v1, le, 4 + LAYER_NAME_LEN + 7);
    v1[3] = 1;
    uint8_t* h = v1 + 4 + LAYER_NAME_LEN + 7;
    memcpy(h, le + 4 + LAYER_NAME_LEN + 7, PRESET_BYTES_V4);
    h[3] = 4;
    memcpy(h + PRESET_BYTES_V4, le + 4 + LAYER_NAME_LEN + 7 + PRESET_BYTES, PRESET_BYTES_V4);
    h[PRESET_BYTES_V4 + 3] = 4;
    uint8_t* s = oldl + LayerBank::HEADER;
    s[0] = uint8_t(LAYER_BYTES_V1 & 0xFF);
    s[1] = uint8_t(LAYER_BYTES_V1 >> 8);
    memcpy(s + 2, v1, LAYER_BYTES_V1);
    files.write_file("/layers.gfl", oldl, sizeof(oldl));
    LayerBank lb(files, g_layer_buf);
    lb.load();
    TEST_ASSERT_EQUAL_UINT8(1, lb.count());
    LayerPreset r;
    TEST_ASSERT_TRUE(lb.get(0, &r));
    TEST_ASSERT_EQUAL_STRING("WOODBASS", r.name);
    TEST_ASSERT_EQUAL_STRING("HY WOODBASS B", r.b.name);
    uint8_t out[LAYER_BYTES];
    TEST_ASSERT_EQUAL_UINT32(LAYER_BYTES, lb.raw(0, out, sizeof(out)));   // exporta en el formato nuevo
}

// ── Fase 4: banco Signature y MOVE (theory/fase4-signature.md) ──

void test_signature_en_el_banco_de_fabrica() {
    static const struct { uint8_t e; const char* name; } SG[] = {
        {4, "GF LOG DRUM"}, {4, "GF CHONTA"}, {4, "GF KALIMBA TINE"}, {0, "GF AFRO SUB"}, {1, "GF BAMBOO PAD"},
        {2, "GF TALKING PLUCK"}, {2, "GF SUNRISE PLUCK"}, {3, "GF NOCTURNE PAD"}, {2, "GF ANTHEM SAW"},
        {0, "GF ROLLING BASS"}, {4, "GF GLASS ARP"}, {1, "GF STRING SWELL"}, {1, "GF DISCO STRINGS"},
        {1, "GF FILTER STAB"}, {0, "GF OCTAVE BASS"}, {5, "GF CLAV FUNK"}, {3, "GF BRASS HIT"}, {4, "GF HOUSE KEYS"},
        {5, "GF ACID LINE"}, {3, "GF DARK DRONE"}, {0, "GF GRIT BASS"}, {4, "GF METAL PERC"}, {2, "GF TAPE LEAD"},
        {5, "GF NOISE RISER"}};
    Preset p;
    for (const auto& g : SG) {
        TEST_ASSERT_TRUE_MESSAGE(find_factory(g.e, g.name, &p) >= 0, g.name);
        TEST_ASSERT_TRUE_MESSAGE(strlen(g.name) <= PRESET_NAME_LEN, g.name);
        uint8_t buf[PRESET_BYTES];                                       // ida y vuelta en el formato actual
        TEST_ASSERT_EQUAL_UINT32(PRESET_BYTES, preset_encode(p, g.e, buf, PRESET_BYTES));
        Preset q;
        TEST_ASSERT_TRUE(preset_decode(buf, PRESET_BYTES, g.e, &q));
    }
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e)                       // sin nombres repetidos en un banco
        for (uint8_t i = 0; i < factory_count(e); ++i)
            for (uint8_t j = uint8_t(i + 1); j < factory_count(e); ++j) {
                Preset a, b;
                factory_preset(e, i, &a);
                factory_preset(e, j, &b);
                TEST_ASSERT_TRUE_MESSAGE(strcmp(a.name, b.name) != 0, a.name);
            }
}

void test_move_solo_en_los_fx_encendidos() {
    Patch p;
    patch_init(p, 2);
    p.sfx_on[GF_SFX_DELAY] = true;
    p.sfx_on[GF_SFX_REVERB] = false;
    const Patch same = move_sfx(p, 0.0f);
    TEST_ASSERT_EQUAL_FLOAT(p.sfx_mix[GF_SFX_DELAY], same.sfx_mix[GF_SFX_DELAY]);   // MOVE 0: nada
    const Patch q = move_sfx(p, 1.0f);
    TEST_ASSERT_TRUE(q.sfx_mix[GF_SFX_DELAY] > p.sfx_mix[GF_SFX_DELAY]);             // DELAY encendido: sube
    TEST_ASSERT_TRUE(q.sfx_p[GF_SFX_DELAY][1] > p.sfx_p[GF_SFX_DELAY][1]);
    TEST_ASSERT_EQUAL_FLOAT(p.sfx_mix[GF_SFX_REVERB], q.sfx_mix[GF_SFX_REVERB]);     // REVERB apagada: igual
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.828427f, move_cutoff_mul(1.0f));               // una octava y media
    TEST_ASSERT_EQUAL_FLOAT(1.0f, move_cutoff_mul(0.0f));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, move_resonance(0.95f, 1.0f));                       // tope 1
}

// ── Sesión para el reinicio (variants/studio/theory/14 §3) ──

void test_sesion_ida_y_vuelta() {
    static Session s, r;
    s = Session{};
    s.mode = 2;
    s.engine = 3;
    s.volume_db = -12.0f;
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e) {
        patch_init(s.synth[e].patch, e);
        s.synth[e].cutoff_hz = 500.0f + 100.0f * e;
        s.preset_loaded[e] = uint8_t(e + 1);
    }
    s.synth[2].patch.attack_ms = 777.0f;                                // lo editado sin guardar
    s.has_layer = true;
    factory_layer(1, &s.layer);
    s.layer.morph = 80;
    s.layer_idx = 1;
    s.layer_focus = 1;
    static uint8_t buf[SESSION_BYTES_MAX];
    const size_t n = session_encode(s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(SESSION_BYTES_MAX, n);
    TEST_ASSERT_TRUE(session_decode(buf, n, &r));
    TEST_ASSERT_EQUAL_UINT8(2, r.mode);
    TEST_ASSERT_EQUAL_UINT8(3, r.engine);
    TEST_ASSERT_EQUAL_FLOAT(-12.0f, r.volume_db);
    TEST_ASSERT_EQUAL_FLOAT(777.0f, r.synth[2].patch.attack_ms);
    TEST_ASSERT_EQUAL_FLOAT(800.0f, r.synth[3].cutoff_hz);
    TEST_ASSERT_EQUAL_UINT8(6, r.preset_loaded[5]);
    TEST_ASSERT_TRUE(r.has_layer);
    TEST_ASSERT_EQUAL_UINT8(80, r.layer.morph);
    TEST_ASSERT_EQUAL_STRING("KALIMBA", r.layer.name);
    TEST_ASSERT_EQUAL_UINT8(1, r.layer_focus);
    s.has_layer = false;                                                // sin capa: más corta y en SYNTH
    const size_t m = session_encode(s, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_UINT32(SESSION_BYTES_MAX - LAYER_BYTES, m);
    TEST_ASSERT_TRUE(session_decode(buf, m, &r));
    TEST_ASSERT_EQUAL_UINT8(0, r.mode);
    buf[3] = 9;                                                         // otra versión: no se usa
    TEST_ASSERT_FALSE(session_decode(buf, m, &r));
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
    RUN_TEST(test_fabrica_de_a_uno_y_se_retoma);
    RUN_TEST(test_trunc_durante_la_escritura_de_fabrica);
    RUN_TEST(test_hibridos_en_pares);
    RUN_TEST(test_capas_apuntan_a_sus_mitades);
    RUN_TEST(test_morph_igual_potencia_y_cc18);
    RUN_TEST(test_banco_de_64_y_anotacion_en_64);
    RUN_TEST(test_un_archivo_por_engine);
    RUN_TEST(test_tamanos_iguales_al_script_de_la_mac);
    RUN_TEST(test_sesion_ida_y_vuelta);
    RUN_TEST(test_signature_en_el_banco_de_fabrica);
    RUN_TEST(test_move_solo_en_los_fx_encendidos);
    RUN_TEST(test_migra_formato_4_a_5_fx);
    RUN_TEST(test_archivos_viejos_se_leen_enteros);
    RUN_TEST(test_hex_ida_y_vuelta);
    RUN_TEST(test_parseo_de_comandos);
    RUN_TEST(test_comandos_de_la_consola);
    RUN_TEST(test_put_y_trunc_del_banco);
    RUN_TEST(test_presets_de_capa);
    return UNITY_END();
}
