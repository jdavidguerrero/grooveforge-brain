#include "session.h"

#include <string.h>

namespace studio {

namespace {
const uint8_t MAGIC[3] = {'G', 'F', 'S'};
}

size_t session_encode(const Session& s, uint8_t* buf, size_t cap) {
    const size_t need = SESSION_HEADER + GF_ENGINE_COUNT * PRESET_BYTES + (s.has_layer ? LAYER_BYTES : 0);
    if (cap < need) return 0;
    memcpy(buf, MAGIC, 3);
    buf[3] = SESSION_FORMAT;
    uint8_t* p = buf + 4;
    p[0] = s.mode;
    p[1] = s.engine;
    p[2] = uint8_t(s.layer_idx);
    p[3] = s.layer_cursor;
    p[4] = s.layer_focus;
    p[5] = s.has_layer;
    p += 6;
    memcpy(p, s.preset_loaded, GF_ENGINE_COUNT);
    p += GF_ENGINE_COUNT;
    memcpy(p, &s.volume_db, 4);
    p += 4;
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e, p += PRESET_BYTES)
        if (preset_encode(s.synth[e], e, p, PRESET_BYTES) != PRESET_BYTES) return 0;
    if (s.has_layer && layer_encode(s.layer, p, LAYER_BYTES) != LAYER_BYTES) return 0;
    return need;
}

bool session_decode(const uint8_t* buf, size_t len, Session* out) {
    if (len < SESSION_HEADER || memcmp(buf, MAGIC, 3) != 0 || buf[3] != SESSION_FORMAT) return false;
    const uint8_t* p = buf + 4;
    Session s;
    s.mode = p[0] == 2 ? 2 : 0;
    s.engine = p[1] < GF_ENGINE_COUNT ? p[1] : 0;
    s.layer_idx = int8_t(p[2]);
    s.layer_cursor = p[3];
    s.layer_focus = p[4] ? 1 : 0;
    s.has_layer = p[5] != 0;
    p += 6;
    memcpy(s.preset_loaded, p, GF_ENGINE_COUNT);
    p += GF_ENGINE_COUNT;
    memcpy(&s.volume_db, p, 4);
    if (!(s.volume_db >= GF_VOL_DB_MIN && s.volume_db <= 0.0f)) s.volume_db = 0.0f;
    p += 4;
    const size_t need = SESSION_HEADER + GF_ENGINE_COUNT * PRESET_BYTES + (s.has_layer ? LAYER_BYTES : 0);
    if (len != need) return false;
    for (uint8_t e = 0; e < GF_ENGINE_COUNT; ++e, p += PRESET_BYTES)
        if (!preset_decode(p, PRESET_BYTES, e, &s.synth[e])) return false;
    if (s.has_layer && !layer_decode(p, LAYER_BYTES, &s.layer)) return false;
    if (!s.has_layer && s.mode == 2) s.mode = 0;
    *out = s;
    return true;
}

}  // namespace studio
