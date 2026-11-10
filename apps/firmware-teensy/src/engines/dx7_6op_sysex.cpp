// VMEM field layout adapted from MSFA patch.cc, Copyright 2013 Google Inc.
// Apache-2.0; see msfa/LICENSE. Packing, framing, validation and GFP are new.
#include "dx7_6op_sysex.h"
#include <algorithm>
#include <cstring>
namespace gf::dx7six {
uint8_t checksum(const uint8_t* p, size_t n) {
    unsigned sum = 0; for (size_t i = 0; i < n; ++i) sum += p[i];
    return uint8_t((128 - (sum & 127)) & 127);
}
bool pack_voice(const Patch& p, uint8_t* out, size_t n) {
    if (!out || n != VMEM_BYTES || !valid_patch(p)) return false;
    const auto* v = p.vced.data();
    for (int op = 0; op < 6; ++op) {
        const auto* a = v + op * 21; auto* b = out + op * 17;
        std::copy_n(a, 11, b);
        b[11] = a[11] | (a[12] << 2); b[12] = a[13] | (a[20] << 3);
        b[13] = a[14] | (a[15] << 2); b[14] = a[16];
        b[15] = a[17] | (a[18] << 1); b[16] = a[19];
    }
    std::copy_n(v + 126, 9, out + 102);
    out[111] = v[135] | (v[136] << 3);
    std::copy_n(v + 137, 4, out + 112);
    out[116] = v[141] | (v[142] << 1) | (v[143] << 4);
    std::copy_n(v + 144, 11, out + 117); return true;
}
bool unpack_voice(const uint8_t* b, size_t n, Patch& out) {
    if (!b || n != VMEM_BYTES) return false;
    for (size_t i = 0; i < n; ++i) if (b[i] > 127) return false;
    Patch candidate; auto* v = candidate.vced.data();
    for (int op = 0; op < 6; ++op) {
        const auto* a = b + op * 17; auto* o = v + op * 21;
        if (a[11] > 15 || a[13] > 31 || a[15] > 63) return false;
        std::copy_n(a, 11, o);
        o[11] = a[11] & 3; o[12] = (a[11] >> 2) & 3;
        o[13] = a[12] & 7; o[20] = a[12] >> 3;
        o[14] = a[13] & 3; o[15] = a[13] >> 2;
        o[16] = a[14]; o[17] = a[15] & 1; o[18] = a[15] >> 1; o[19] = a[16];
    }
    if (b[111] > 15) return false;
    std::copy_n(b + 102, 9, v + 126);
    v[135] = b[111] & 7; v[136] = b[111] >> 3;
    std::copy_n(b + 112, 4, v + 137);
    v[141] = b[116] & 1; v[142] = (b[116] >> 1) & 7; v[143] = b[116] >> 4;
    std::copy_n(b + 117, 11, v + 144);
    if (!valid_patch(candidate)) return false;
    out = candidate; return true;
}
bool parse_sysex(const uint8_t* b, size_t n, Patch& out, uint8_t index) {
    if (!b || (n != SINGLE_SYSEX_BYTES && n != BANK_SYSEX_BYTES) || index > 31) return false;
    if (b[0] != 0xf0 || b[1] != 0x43 || b[2] > 15 || b[n - 1] != 0xf7) return false;
    const bool bank = n == BANK_SYSEX_BYTES;
    if (b[3] != (bank ? 9 : 0) || b[4] != (bank ? 0x20 : 1) || b[5] != (bank ? 0 : 0x1b)) return false;
    if (!bank && index != 0) return false;
    for (size_t i = 6; i < n - 1; ++i) if (b[i] > 127) return false;
    if (checksum(b + 6, n - 8) != b[n - 2]) return false;
    Patch candidate;
    if (bank) {
        for (int i = 0; i < 32; ++i) {
            Patch voice;
            if (!unpack_voice(b + 6 + 128 * i, 128, voice)) return false;
            if (i == index) candidate = voice;
        }
    } else std::copy_n(b + 6, VCED_BYTES, candidate.vced.begin());
    if (!valid_patch(candidate)) return false;
    out = candidate; return true;
}
size_t write_single_sysex(const Patch& p, uint8_t* b, size_t n, uint8_t device) {
    if (!b || n < SINGLE_SYSEX_BYTES || device > 15 || !valid_patch(p)) return 0;
    const uint8_t header[] = {0xf0,0x43,device,0,1,0x1b};
    std::copy_n(header, 6, b); std::copy(p.vced.begin(), p.vced.end(), b + 6);
    b[161] = checksum(b + 6, 155); b[162] = 0xf7; return SINGLE_SYSEX_BYTES;
}
size_t write_bank_sysex(const Patch* p, uint8_t* b, size_t n, uint8_t device) {
    if (!p || !b || n < BANK_SYSEX_BYTES || device > 15) return 0;
    for (int i = 0; i < 32; ++i) if (!valid_patch(p[i])) return 0;
    const uint8_t header[] = {0xf0,0x43,device,9,0x20,0}; std::copy_n(header, 6, b);
    for (int i = 0; i < 32; ++i) pack_voice(p[i], b + 6 + 128 * i, 128);
    b[4102] = checksum(b + 6, 4096); b[4103] = 0xf7; return BANK_SYSEX_BYTES;
}
size_t encode_gfp(const Patch& p, uint8_t* b, size_t n) {
    if (!b || n < GFP_BYTES || !valid_patch(p)) return 0;
    const uint8_t header[] = {'G','F','P',6,ENGINE_ID}; std::copy_n(header, 5, b);
    std::copy(p.vced.begin(), p.vced.end(), b + 5); b[160] = p.enabled;
    b[161] = checksum(b, 161); return GFP_BYTES;
}
bool decode_gfp(const uint8_t* b, size_t n, Patch& out) {
    if (!b || n != GFP_BYTES || std::memcmp(b, "GFP", 3) || b[3] != 6 || b[4] != ENGINE_ID || b[161] != checksum(b, 161)) return false;
    Patch p; std::copy_n(b + 5, 155, p.vced.begin()); p.enabled = b[160];
    if (!valid_patch(p)) return false;
    out = p; return true;
}
}
