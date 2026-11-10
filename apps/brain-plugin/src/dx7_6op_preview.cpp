#include "dx7_6op_preview.h"
namespace gf {
bool decode_dx7_6op_preset(const uint8_t* bytes, size_t size, dx7six::Patch& out, uint8_t bank_index) {
    if (!bytes || !size) return false;
    return bytes[0] == 0xf0 ? dx7six::parse_sysex(bytes, size, out, size == dx7six::SINGLE_SYSEX_BYTES ? 0 : bank_index) :
        dx7six::decode_gfp(bytes, size, out);
}
}
