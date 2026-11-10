#pragma once
#include "dx7_6op_core.h"
namespace gf::dx7six {
constexpr size_t VCED_BYTES = 155, VMEM_BYTES = 128;
constexpr size_t SINGLE_SYSEX_BYTES = 163, BANK_SYSEX_BYTES = 4104;
constexpr size_t GFP_BYTES = 162;
/** @brief Pack a validated VCED; operator mask is not represented in VMEM. */
bool pack_voice(const Patch& p, uint8_t* out, size_t size);
/** @brief Strict VMEM unpack, preserving output on error. */
bool unpack_voice(const uint8_t* packed, size_t size, Patch& out);
/** @brief Yamaha checksum over 7-bit data bytes. */
uint8_t checksum(const uint8_t* data, size_t size);
/** @brief Parse one complete VCED or VMEM SysEx; bank index 0–31, device 0–15. */
bool parse_sysex(const uint8_t* data, size_t size, Patch& out, uint8_t bank_index = 0);
/** @brief Serialize a VCED SysEx; returns bytes written or zero. */
size_t write_single_sysex(const Patch& p, uint8_t* out, size_t size, uint8_t device = 0);
/** @brief Serialize a complete bank of 32 voices; returns bytes written or zero. */
size_t write_bank_sysex(const Patch* voices, uint8_t* out, size_t size, uint8_t device = 0);
/** @brief GFP v6/engine 7 encoding; separate from unchanged Studio GFP 2–5. */
size_t encode_gfp(const Patch& p, uint8_t* out, size_t size);
/** @brief GFP v6 decoding, preserving output on malformed data. */
bool decode_gfp(const uint8_t* data, size_t size, Patch& out);
}
