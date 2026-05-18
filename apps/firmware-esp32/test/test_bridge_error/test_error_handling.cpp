/**
 * @file test_error_handling.cpp
 * @brief Tests de manejo de errores del protocolo Bridge
 *
 * Verifica: payload overflow, SEQ rollover, NACK reasons, CRC corruption.
 */

#include <unity.h>
#include <protocol.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── Payload overflow ────────────────────────────────────────────────────── */

void test_build_rejects_payload_over_255(void) {
    /* gf_frame_build debe retornar false si payload_len > GF_PAYLOAD_MAX (255) */
    uint8_t big_payload[256];
    memset(big_payload, 0xAA, sizeof(big_payload));
    GF_Frame f;
    /* Pasamos 256 bytes (size_t, sin truncación) — debe rechazarse */
    bool ok = gf_frame_build(&f, GF_CMD_PARAM_CHANGED, 0, big_payload, (size_t)256);
    TEST_ASSERT_FALSE(ok);
}

void test_build_accepts_exactly_255(void) {
    uint8_t payload[255];
    memset(payload, 0xBB, sizeof(payload));
    GF_Frame f;
    bool ok = gf_frame_build(&f, GF_CMD_PARAM_CHANGED, 0, payload, 255);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(255, f.len);
}

/* ── SEQ rollover ────────────────────────────────────────────────────────── */

void test_seq_rollover_255_to_0(void) {
    /* El SEQ es un uint8_t — (uint8_t)(255 + 1) == 0 */
    uint8_t seq = 255;
    seq = (uint8_t)(seq + 1u);
    TEST_ASSERT_EQUAL_UINT8(0, seq);
}

void test_seq_field_is_1_byte(void) {
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_HEARTBEAT, 0xFF, nullptr, 0);
    TEST_ASSERT_EQUAL_UINT8(0xFF, f.seq);

    gf_frame_build(&f, GF_CMD_HEARTBEAT, 0x00, nullptr, 0);
    TEST_ASSERT_EQUAL_UINT8(0x00, f.seq);
}

/* ── NACK reasons ────────────────────────────────────────────────────────── */

void test_nack_reasons_have_correct_values(void) {
    TEST_ASSERT_EQUAL_HEX8(0x01, GF_NACK_CRC_ERROR);
    TEST_ASSERT_EQUAL_HEX8(0x02, GF_NACK_INVALID_CMD);
    TEST_ASSERT_EQUAL_HEX8(0x03, GF_NACK_INVALID_LEN);
    TEST_ASSERT_EQUAL_HEX8(0x04, GF_NACK_INVALID_PARAM);
    TEST_ASSERT_EQUAL_HEX8(0x05, GF_NACK_OOM);
    TEST_ASSERT_EQUAL_HEX8(0x06, GF_NACK_TIMEOUT);
    TEST_ASSERT_EQUAL_HEX8(0x07, GF_NACK_NOT_CONNECTED);
    TEST_ASSERT_EQUAL_HEX8(0x08, GF_NACK_AUTH_FAILED);
}

void test_nack_frame_payload_structure(void) {
    /* NACK payload: [SEQ_nacked: 1B, REASON: 1B] */
    uint8_t nacked_seq = 42;
    GF_NackReason reason = GF_NACK_CRC_ERROR;
    uint8_t payload[2] = { nacked_seq, (uint8_t)reason };

    GF_Frame f;
    gf_frame_build(&f, GF_CMD_NACK, 0x01, payload, 2);

    TEST_ASSERT_EQUAL_UINT8(2, f.len);
    TEST_ASSERT_EQUAL_UINT8(42, f.payload[0]);
    TEST_ASSERT_EQUAL_UINT8(GF_NACK_CRC_ERROR, f.payload[1]);
}

/* ── ACK frame structure ─────────────────────────────────────────────────── */

void test_ack_frame_payload_structure(void) {
    /* ACK payload: [SEQ_acked: 1B] */
    uint8_t acked_seq = 77;
    uint8_t payload[1] = { acked_seq };

    GF_Frame f;
    gf_frame_build(&f, GF_CMD_ACK, 0x01, payload, 1);

    TEST_ASSERT_EQUAL_HEX8(GF_CMD_ACK, f.cmd);
    TEST_ASSERT_EQUAL_UINT8(1, f.len);
    TEST_ASSERT_EQUAL_UINT8(77, f.payload[0]);
}

/* ── CRC corruption ──────────────────────────────────────────────────────── */

void test_single_bit_flip_in_cmd_detected(void) {
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_ENGINE_CHANGED, 5, nullptr, 0);
    uint8_t crc = gf_frame_crc(&f);

    /* Flip 1 bit en CMD */
    f.cmd ^= 0x01;
    TEST_ASSERT_FALSE(gf_frame_valid_crc(&f, crc));
}

void test_single_bit_flip_in_seq_detected(void) {
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_HEARTBEAT, 100, nullptr, 0);
    uint8_t crc = gf_frame_crc(&f);

    f.seq ^= 0x80;
    TEST_ASSERT_FALSE(gf_frame_valid_crc(&f, crc));
}

void test_crc_correct_after_build(void) {
    GF_Frame f;
    uint8_t payload[] = { 0xDE, 0xAD };
    gf_frame_build(&f, GF_CMD_CC, 99, payload, 2);
    uint8_t crc = gf_frame_crc(&f);
    /* Frame sin modificar → CRC válido */
    TEST_ASSERT_TRUE(gf_frame_valid_crc(&f, crc));
}

/* ── CMD IDs — rangos correctos ──────────────────────────────────────────── */

void test_cmd_ids_in_spec_ranges(void) {
    TEST_ASSERT_LESS_OR_EQUAL(0x0F, (uint8_t)GF_CMD_STATUS);
    TEST_ASSERT_GREATER_OR_EQUAL(0x10, (uint8_t)GF_CMD_SET_ENGINE);
    TEST_ASSERT_LESS_OR_EQUAL(0x1F, (uint8_t)GF_CMD_PRESET_LOADED);
    TEST_ASSERT_GREATER_OR_EQUAL(0x20, (uint8_t)GF_CMD_FX_ENABLE);
    TEST_ASSERT_LESS_OR_EQUAL(0x2F, (uint8_t)GF_CMD_FX_CHAIN_SAVE);
    TEST_ASSERT_GREATER_OR_EQUAL(0x30, (uint8_t)GF_CMD_NOTE_ON);
    TEST_ASSERT_LESS_OR_EQUAL(0x3F, (uint8_t)GF_CMD_TRANSPORT);
    TEST_ASSERT_GREATER_OR_EQUAL(0x40, (uint8_t)GF_CMD_DISPLAY_UPDATE);
    TEST_ASSERT_LESS_OR_EQUAL(0x4F, (uint8_t)GF_CMD_LED_PATTERN);
    TEST_ASSERT_GREATER_OR_EQUAL(0x50, (uint8_t)GF_CMD_CLOUD_STATUS);
    TEST_ASSERT_GREATER_OR_EQUAL(0x80, (uint8_t)GF_CMD_KEY_DETECTED);
    TEST_ASSERT_LESS_OR_EQUAL(0x8F, (uint8_t)GF_CMD_GENRE_DETECTED);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_build_rejects_payload_over_255);
    RUN_TEST(test_build_accepts_exactly_255);
    RUN_TEST(test_seq_rollover_255_to_0);
    RUN_TEST(test_seq_field_is_1_byte);
    RUN_TEST(test_nack_reasons_have_correct_values);
    RUN_TEST(test_nack_frame_payload_structure);
    RUN_TEST(test_ack_frame_payload_structure);
    RUN_TEST(test_single_bit_flip_in_cmd_detected);
    RUN_TEST(test_single_bit_flip_in_seq_detected);
    RUN_TEST(test_crc_correct_after_build);
    RUN_TEST(test_cmd_ids_in_spec_ranges);
    return UNITY_END();
}
