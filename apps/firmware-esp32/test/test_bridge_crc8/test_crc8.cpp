/**
 * @file test_crc8.cpp
 * @brief Tests unitarios de CRC-8 poly 0x07 (Dallas/Maxim SMBUS)
 *
 * Vector canónico: gf_crc8("123456789", 9) == 0xF4
 * Fuente: Williams, R. "A Painless Guide to CRC", §15 — check value CRC-8/SMBUS.
 */

#include <unity.h>
#include <protocol.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Vector canónico ─────────────────────────────────────────────────────── */

void test_crc8_canonical_vector(void) {
    const uint8_t input[] = { '1','2','3','4','5','6','7','8','9' };
    TEST_ASSERT_EQUAL_HEX8(0xF4, gf_crc8(input, sizeof(input)));
}

/* ── Casos borde ─────────────────────────────────────────────────────────── */

void test_crc8_empty_is_zero(void) {
    TEST_ASSERT_EQUAL_HEX8(0x00, gf_crc8(nullptr, 0));
}

void test_crc8_single_zero_byte(void) {
    const uint8_t input[] = { 0x00 };
    /* CRC de 0x00: XOR con crc=0 → crc=0, todos los bits son 0, resultado = 0x00 */
    TEST_ASSERT_EQUAL_HEX8(0x00, gf_crc8(input, 1));
}

void test_crc8_single_ff_byte(void) {
    const uint8_t input[] = { 0xFF };
    /* Valor pre-calculado manualmente con poly 0x07 */
    TEST_ASSERT_EQUAL_HEX8(0xF3, gf_crc8(input, 1));
}

/* ── Propiedades del CRC ─────────────────────────────────────────────────── */

void test_crc8_same_input_same_output(void) {
    const uint8_t input[] = { 0xAA, 0xBB, 0xCC };
    uint8_t a = gf_crc8(input, sizeof(input));
    uint8_t b = gf_crc8(input, sizeof(input));
    TEST_ASSERT_EQUAL_HEX8(a, b);
}

void test_crc8_different_inputs_different_outputs(void) {
    const uint8_t a[] = { 0x10, 0x00, 0x01 };   /* ENGINE_CHANGED, LEN=0, SEQ=1 */
    const uint8_t b[] = { 0x00, 0x00, 0x01 };   /* HEARTBEAT, LEN=0, SEQ=1 */
    TEST_ASSERT_NOT_EQUAL(gf_crc8(a, sizeof(a)), gf_crc8(b, sizeof(b)));
}

void test_crc8_bit_flip_changes_result(void) {
    uint8_t buf[] = { 0x12, 0x05, 0x42, 0xDE, 0xAD };
    uint8_t original = gf_crc8(buf, sizeof(buf));
    buf[2] ^= 0x01;   /* flip 1 bit */
    uint8_t flipped = gf_crc8(buf, sizeof(buf));
    TEST_ASSERT_NOT_EQUAL(original, flipped);
}

void test_crc8_max_length_no_overflow(void) {
    /* CRC sobre 255 bytes debe completar sin crash */
    uint8_t buf[255];
    memset(buf, 0xA5, sizeof(buf));
    uint8_t result = gf_crc8(buf, sizeof(buf));
    /* Solo verificamos que devuelve algo (no crashea) */
    (void)result;
    TEST_PASS();
}

/* ── Frames del protocolo ────────────────────────────────────────────────── */

void test_crc8_heartbeat_frame(void) {
    /* HEARTBEAT: CMD=0x00, LEN=0x00, SEQ=0x01 → CRC pre-calculado */
    const uint8_t frame_header[] = { 0x00, 0x00, 0x01 };
    uint8_t crc = gf_crc8(frame_header, 3);
    /* Verificamos que el CRC es el mismo llamando dos veces */
    TEST_ASSERT_EQUAL_HEX8(crc, gf_crc8(frame_header, 3));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_crc8_canonical_vector);
    RUN_TEST(test_crc8_empty_is_zero);
    RUN_TEST(test_crc8_single_zero_byte);
    RUN_TEST(test_crc8_single_ff_byte);
    RUN_TEST(test_crc8_same_input_same_output);
    RUN_TEST(test_crc8_different_inputs_different_outputs);
    RUN_TEST(test_crc8_bit_flip_changes_result);
    RUN_TEST(test_crc8_max_length_no_overflow);
    RUN_TEST(test_crc8_heartbeat_frame);
    return UNITY_END();
}
