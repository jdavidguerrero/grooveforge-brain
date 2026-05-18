/**
 * @file test_serialization.cpp
 * @brief Tests de serialización/deserialización de frames GF_Frame
 *
 * Verifica que gf_frame_build + gf_frame_serialize producen el wire format
 * correcto, y que gf_frame_valid_crc detecta frames correctos e incorrectos.
 */

#include <unity.h>
#include <protocol.h>
#include <stdint.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* ── gf_frame_build ──────────────────────────────────────────────────────── */

void test_build_heartbeat_no_payload(void) {
    GF_Frame f;
    bool ok = gf_frame_build(&f, GF_CMD_HEARTBEAT, 0x01, nullptr, 0);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_HEX8(GF_CMD_HEARTBEAT, f.cmd);
    TEST_ASSERT_EQUAL_UINT8(0, f.len);
    TEST_ASSERT_EQUAL_UINT8(1, f.seq);
}

void test_build_engine_changed_with_payload(void) {
    const uint8_t payload[] = { 0x00, 'M','O','O','G','\0' };
    GF_Frame f;
    bool ok = gf_frame_build(&f, GF_CMD_ENGINE_CHANGED, 42, payload, sizeof(payload));
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_HEX8(GF_CMD_ENGINE_CHANGED, f.cmd);
    TEST_ASSERT_EQUAL_UINT8(sizeof(payload), f.len);
    TEST_ASSERT_EQUAL_UINT8(42, f.seq);
    TEST_ASSERT_EQUAL_MEMORY(payload, f.payload, sizeof(payload));
}

void test_build_max_payload(void) {
    uint8_t payload[255];
    memset(payload, 0xBB, sizeof(payload));
    GF_Frame f;
    bool ok = gf_frame_build(&f, GF_CMD_PARAM_CHANGED, 0xFF, payload, 255);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_UINT8(255, f.len);
    TEST_ASSERT_EQUAL_MEMORY(payload, f.payload, 255);
}

/* ── gf_frame_valid_crc ──────────────────────────────────────────────────── */

void test_valid_crc_heartbeat(void) {
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_HEARTBEAT, 0x05, nullptr, 0);
    uint8_t crc = gf_frame_crc(&f);
    TEST_ASSERT_TRUE(gf_frame_valid_crc(&f, crc));
}

void test_invalid_crc_detected(void) {
    GF_Frame f;
    const uint8_t payload[] = { 0x01 };
    gf_frame_build(&f, GF_CMD_SET_ENGINE, 0x10, payload, 1);
    uint8_t correct_crc = gf_frame_crc(&f);
    TEST_ASSERT_FALSE(gf_frame_valid_crc(&f, correct_crc ^ 0xFF));
}

void test_payload_corruption_detected(void) {
    GF_Frame f;
    const uint8_t payload[] = { 0x02, 0x00, 0x00, 0x80, 0x3F };  /* param_id=2, float=1.0 */
    gf_frame_build(&f, GF_CMD_PARAM_CHANGED, 0x20, payload, sizeof(payload));
    uint8_t crc = gf_frame_crc(&f);

    /* Corromper 1 byte del payload */
    f.payload[2] ^= 0x01;
    TEST_ASSERT_FALSE(gf_frame_valid_crc(&f, crc));
}

/* ── gf_frame_serialize (wire format) ───────────────────────────────────── */

void test_serialize_heartbeat(void) {
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_HEARTBEAT, 0x01, nullptr, 0);

    uint8_t buf[16];
    size_t written = gf_frame_serialize(&f, buf, sizeof(buf));

    /* Frame: CMD=0x00, LEN=0x00, SEQ=0x01, CRC=? → 4 bytes total */
    TEST_ASSERT_EQUAL_size_t(4, written);
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[0]);   /* CMD HEARTBEAT */
    TEST_ASSERT_EQUAL_HEX8(0x00, buf[1]);   /* LEN */
    TEST_ASSERT_EQUAL_HEX8(0x01, buf[2]);   /* SEQ */
    /* buf[3] es el CRC — verificamos que coincide con gf_frame_crc */
    TEST_ASSERT_EQUAL_HEX8(gf_frame_crc(&f), buf[3]);
}

void test_serialize_engine_changed(void) {
    const uint8_t payload[] = { 0x00, 'M','O','O','G','\0' };
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_ENGINE_CHANGED, 0x2A, payload, sizeof(payload));

    uint8_t buf[64];
    size_t written = gf_frame_serialize(&f, buf, sizeof(buf));

    /* 3 header + 6 payload + 1 CRC = 10 bytes */
    TEST_ASSERT_EQUAL_size_t(10, written);
    TEST_ASSERT_EQUAL_HEX8(GF_CMD_ENGINE_CHANGED, buf[0]);
    TEST_ASSERT_EQUAL_HEX8(6, buf[1]);
    TEST_ASSERT_EQUAL_HEX8(0x2A, buf[2]);
    TEST_ASSERT_EQUAL_MEMORY(payload, buf + 3, sizeof(payload));
    TEST_ASSERT_EQUAL_HEX8(gf_frame_crc(&f), buf[9]);
}

void test_serialize_buffer_too_small_returns_zero(void) {
    GF_Frame f;
    const uint8_t payload[10] = {};
    gf_frame_build(&f, GF_CMD_PARAM_CHANGED, 1, payload, 10);

    uint8_t buf[5];  /* necesita 14 bytes, buffer tiene 5 */
    size_t written = gf_frame_serialize(&f, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(0, written);
}

/* ── Roundtrip: build → serialize → validar wire bytes ──────────────────── */

void test_roundtrip_version_frame(void) {
    /* VERSION: major=1, minor=0, patch=0, build=0xCAFEBABE */
    uint8_t payload[] = { 1, 0, 0, 0xBE, 0xBA, 0xFE, 0xCA };
    GF_Frame f;
    gf_frame_build(&f, GF_CMD_VERSION, 0x03, payload, sizeof(payload));

    uint8_t wire[64];
    size_t len = gf_frame_serialize(&f, wire, sizeof(wire));
    TEST_ASSERT_EQUAL_size_t(11, len);   /* 3 + 7 + 1 */

    /* Re-parsear manualmente: reconstruir GF_Frame desde wire */
    GF_Frame parsed;
    parsed.cmd = wire[0];
    parsed.len = wire[1];
    parsed.seq = wire[2];
    memcpy(parsed.payload, wire + 3, parsed.len);
    uint8_t rcv_crc = wire[3 + parsed.len];

    TEST_ASSERT_EQUAL_HEX8(f.cmd, parsed.cmd);
    TEST_ASSERT_EQUAL_UINT8(f.len, parsed.len);
    TEST_ASSERT_EQUAL_UINT8(f.seq, parsed.seq);
    TEST_ASSERT_EQUAL_MEMORY(f.payload, parsed.payload, f.len);
    TEST_ASSERT_TRUE(gf_frame_valid_crc(&parsed, rcv_crc));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_build_heartbeat_no_payload);
    RUN_TEST(test_build_engine_changed_with_payload);
    RUN_TEST(test_build_max_payload);
    RUN_TEST(test_valid_crc_heartbeat);
    RUN_TEST(test_invalid_crc_detected);
    RUN_TEST(test_payload_corruption_detected);
    RUN_TEST(test_serialize_heartbeat);
    RUN_TEST(test_serialize_engine_changed);
    RUN_TEST(test_serialize_buffer_too_small_returns_zero);
    RUN_TEST(test_roundtrip_version_frame);
    return UNITY_END();
}
