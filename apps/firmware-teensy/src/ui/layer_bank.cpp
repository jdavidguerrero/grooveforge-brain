#include "layer_bank.h"

#include <stdio.h>
#include <string.h>

#include "layers.h"

namespace studio {

namespace {

const uint8_t MAGIC[3] = {'G', 'F', 'L'};
const uint8_t FILE_MAGIC[3] = {'G', 'F', 'K'};

void set_name(char* dst, const char* src) {
    memset(dst, 0, LAYER_NAME_LEN + 1);
    memcpy(dst, src, strnlen(src, LAYER_NAME_LEN));
}

bool find_factory(uint8_t e, const char* name, Preset* out) {
    for (uint8_t f = 0; f < factory_count(e); ++f)
        if (factory_preset(e, f, out) && !strcmp(out->name, name)) return true;
    return false;
}

}  // namespace

size_t layer_encode(const LayerPreset& l, uint8_t* buf, size_t cap) {
    if (cap < LAYER_BYTES) return 0;
    memcpy(buf, MAGIC, 3);
    buf[3] = LAYER_FORMAT;
    memset(buf + 4, 0, LAYER_NAME_LEN);
    memcpy(buf + 4, l.name, strnlen(l.name, LAYER_NAME_LEN));
    uint8_t* p = buf + 4 + LAYER_NAME_LEN;
    p[0] = l.eng_a;
    p[1] = l.eng_b;
    p[2] = l.morph;
    p[3] = l.delay_b_ms;
    p[4] = uint8_t(l.transpose_b);
    p[5] = uint8_t(l.gain_a_db);
    p[6] = uint8_t(l.gain_b_db);
    p += 7;
    if (preset_encode(l.a, l.eng_a, p, PRESET_BYTES) == 0) return 0;
    if (preset_encode(l.b, l.eng_b, p + PRESET_BYTES, PRESET_BYTES) == 0) return 0;
    return LAYER_BYTES;
}

bool layer_decode(const uint8_t* buf, size_t len, LayerPreset* out) {
    if (len < 4 || memcmp(buf, MAGIC, 3) != 0) return false;
    size_t half;                                   // tamaño de cada mitad según la versión
    if (buf[3] == LAYER_FORMAT && len == LAYER_BYTES) half = PRESET_BYTES;
    else if (buf[3] == 1 && len == LAYER_BYTES_V1) half = PRESET_BYTES_V4;
    else return false;
    LayerPreset l;
    memcpy(l.name, buf + 4, LAYER_NAME_LEN);
    l.name[LAYER_NAME_LEN] = '\0';
    const uint8_t* p = buf + 4 + LAYER_NAME_LEN;
    l.eng_a = p[0];
    l.eng_b = p[1];
    if (l.eng_a >= GF_ENGINE_COUNT || l.eng_b >= GF_ENGINE_COUNT || l.eng_a == l.eng_b) return false;
    l.morph = p[2] > 100 ? 100 : p[2];
    l.delay_b_ms = p[3];
    l.transpose_b = int8_t(p[4]);
    l.gain_a_db = int8_t(p[5]);
    l.gain_b_db = int8_t(p[6]);
    p += 7;
    if (!preset_decode(p, half, l.eng_a, &l.a) || !preset_decode(p + half, half, l.eng_b, &l.b)) return false;
    *out = l;
    return true;
}

uint8_t factory_layer_count() { return LAYER_COUNT; }

bool factory_layer(uint8_t i, LayerPreset* out) {
    if (i >= LAYER_COUNT) return false;
    const LayerDef& d = layer_def(i);
    LayerPreset l;
    set_name(l.name, d.name);
    l.eng_a = d.eng_a;
    l.eng_b = d.eng_b;
    if (!find_factory(d.eng_a, d.preset_a, &l.a) || !find_factory(d.eng_b, d.preset_b, &l.b)) return false;
    l.morph = 50;
    l.delay_b_ms = d.delay_b_ms;
    l.transpose_b = d.transpose_b;
    l.gain_a_db = int8_t(d.gain_a_db);
    l.gain_b_db = int8_t(d.gain_b_db);
    *out = l;
    return true;
}

void LayerBank::rescan() {
    count_ = 0;
    while (count_ < LAYER_MAX) {
        const uint8_t* s = slot(count_);
        const size_t len = size_t(s[0] | (s[1] << 8));
        if (len != LAYER_BYTES && len != LAYER_BYTES_V1) break;
        set_name(names_[count_], reinterpret_cast<const char*>(s + 2 + 4));
        ++count_;
    }
}

void LayerBank::load() {
    if (loaded_) return;
    loaded_ = true;
    const size_t n = files_.read_file("/layers.gfl", buf_, FILE_BYTES);
    const bool gfk = n >= HEADER && !memcmp(buf_, FILE_MAGIC, 3);
    if (gfk && buf_[3] == 2 && n == FILE_BYTES) {
        rescan();
        return;
    }
    if (gfk && buf_[3] == 1 && n == FILE_BYTES_V1) {
        // Archivo de la versión 1 (capas con mitades en formato 4): los lugares se reubican al paso nuevo y las capas
        // se leen migradas; al guardar se escribe la versión 2 (theory/fase3-fx.md §3)
        for (int i = LAYER_MAX - 1; i >= 0; --i) {
            uint8_t* dst = buf_ + HEADER + size_t(i) * SLOT;
            memmove(dst, buf_ + HEADER + size_t(i) * SLOT_V1, SLOT_V1);
            memset(dst + SLOT_V1, 0, SLOT - SLOT_V1);
        }
        buf_[3] = 2;
        rescan();
        return;
    }
    memset(buf_, 0, FILE_BYTES);                     // no existe o es de otra versión: las de fábrica
    memcpy(buf_, FILE_MAGIC, 3);
    buf_[3] = 2;
    count_ = 0;
    LayerPreset l;
    for (uint8_t i = 0; i < factory_layer_count(); ++i)
        if (factory_layer(i, &l) && write_slot(count_, l)) ++count_;
    rescan();
    dirty_ = true;
    flush();
}

bool LayerBank::write_slot(uint8_t i, const LayerPreset& l) {
    if (i >= LAYER_MAX) return false;
    uint8_t* s = slot(i);
    if (layer_encode(l, s + 2, LAYER_BYTES) != LAYER_BYTES) return false;
    s[0] = uint8_t(LAYER_BYTES & 0xFF);
    s[1] = uint8_t(LAYER_BYTES >> 8);
    dirty_ = true;
    return true;
}

bool LayerBank::get(uint8_t i, LayerPreset* out) const {
    if (i >= count_) return false;
    const uint8_t* s = slot(i);
    return layer_decode(s + 2, size_t(s[0] | (s[1] << 8)), out);
}

int LayerBank::save_new(LayerPreset l) {
    if (count_ >= LAYER_MAX) return -1;
    const uint8_t i = count_;
    snprintf(l.name, sizeof(l.name), "LAYER %02u", unsigned(i + 1));
    if (!write_slot(i, l)) return -1;
    rescan();
    flush();
    return i;
}

PutResult LayerBank::put(uint8_t i, const uint8_t* buf, size_t len) {
    LayerPreset l;
    if (!layer_decode(buf, len, &l)) return PutResult::Format;
    if (i > count_ || i >= LAYER_MAX) return PutResult::Index;
    if (!write_slot(i, l)) return PutResult::Write;
    rescan();
    return PutResult::Ok;
}

size_t LayerBank::raw(uint8_t i, uint8_t* out, size_t cap) const {
    LayerPreset l;                                   // sale siempre en el formato actual (migrada si era vieja)
    if (i >= count_ || cap < LAYER_BYTES || !get(i, &l)) return 0;
    return layer_encode(l, out, cap);
}

bool LayerBank::truncate(uint8_t n) {
    if (n > count_) return false;
    for (uint8_t i = n; i < LAYER_MAX; ++i) slot(i)[0] = slot(i)[1] = 0;
    dirty_ = true;
    rescan();
    return true;
}

void LayerBank::flush() {
    if (!dirty_) return;
    if (files_.write_file("/layers.gfl", buf_, FILE_BYTES)) dirty_ = false;
}

}  // namespace studio
