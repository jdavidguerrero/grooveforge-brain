#pragma once

#include <stddef.h>
#include <stdint.h>

#include "bank_file_storage.h"
#include "preset_bank.h"

/**
 * @file layer_bank.h
 * @brief Presets de capa del modo HYBRID SYNTH (variants/studio/theory/12 §3d), lógica pura.
 *
 * Cada preset de capa guarda las dos mitades completas (no depende de los bancos de cada engine), MORPH, el retraso y la
 * transposición de B y la mezcla base. Todos van en un archivo (`/layers.gfl`, ~35 KB: un bloque de 64 KB de
 * LittleFS_Program, theory/13 rev. 3), cargado en RAM; `flush()` lo guarda en una sola escritura.
 */
namespace studio {

constexpr uint8_t LAYER_MAX = 32;               ///< presets de capa: bancos A y B de 16
constexpr uint8_t LAYER_NAME_LEN = 12;          ///< = GF_LAYER_NAME_LEN
constexpr uint8_t LAYER_FORMAT = 2;            ///< 2: mitades en formato de preset 5 (theory/fase3-fx §3)
/// Serializado: "GFL" + versión, nombre, engines, morph, retraso, transposición, ganancias y las dos mitades.
constexpr size_t LAYER_BYTES = 4 + LAYER_NAME_LEN + 7 + 2 * PRESET_BYTES;
constexpr size_t LAYER_BYTES_V1 = 4 + LAYER_NAME_LEN + 7 + 2 * PRESET_BYTES_V4;   ///< formato 1: mitades en formato 4

struct LayerPreset {
    char name[LAYER_NAME_LEN + 1] = "LAYER";
    uint8_t eng_a = 0, eng_b = 1;
    Preset a, b;                                ///< las mitades completas (nombre de la mitad incluido)
    uint8_t morph = 50;                         ///< 0–100 %
    uint8_t delay_b_ms = 0;
    int8_t transpose_b = 0;
    int8_t gain_a_db = 0, gain_b_db = 0;        ///< mezcla base con MORPH al 50 %
};

/// @return LAYER_BYTES, o 0 si `cap` no alcanza.
size_t layer_encode(const LayerPreset& l, uint8_t* buf, size_t cap);
/// Lee los formatos 1 y 2. @return false si la firma, la versión, los engines o alguna mitad no se pueden leer.
bool layer_decode(const uint8_t* buf, size_t len, LayerPreset* out);

/// Las capas de fábrica: las 8 parejas de layers.h armadas con sus mitades HY de fábrica.
uint8_t factory_layer_count();
bool factory_layer(uint8_t i, LayerPreset* out);

class LayerBank {
public:
    static constexpr size_t SLOT = 2 + LAYER_BYTES;
    static constexpr size_t HEADER = 4;                              ///< "GFK" + versión
    static constexpr size_t FILE_BYTES = HEADER + SLOT * LAYER_MAX;
    static constexpr size_t SLOT_V1 = 2 + LAYER_BYTES_V1;
    static constexpr size_t FILE_BYTES_V1 = HEADER + SLOT_V1 * LAYER_MAX;

    /// `buffer` necesita FILE_BYTES (en la Teensy va en RAM2).
    LayerBank(FileBackend& files, uint8_t* buffer) : files_(files), buf_(buffer) {}

    /// Lee el archivo; si no existe (o es de otra versión) lo arma con las capas de fábrica y lo guarda.
    void load();
    /// Después de formatear el área: vuelve a las de fábrica.
    void reset() { loaded_ = false; load(); }

    uint8_t count() const { return count_; }
    const char* name(uint8_t i) const { return i < count_ ? names_[i] : ""; }
    bool get(uint8_t i, LayerPreset* out) const;
    /// Guarda `l` al final como `LAYER nn`. @return el índice, o -1 si el banco está lleno.
    int save_new(LayerPreset l);
    /// Bibliotecario: el preset de capa binario `buf` en el lugar `i` (reemplaza o agrega al final, sin huecos).
    PutResult put(uint8_t i, const uint8_t* buf, size_t len);
    /// Bibliotecario: el binario del lugar `i`. @return bytes (0 si no existe).
    size_t raw(uint8_t i, uint8_t* out, size_t cap) const;
    bool truncate(uint8_t n);
    void flush();

private:
    uint8_t* slot(uint8_t i) const { return buf_ + HEADER + size_t(i) * SLOT; }
    bool write_slot(uint8_t i, const LayerPreset& l);
    void rescan();

    FileBackend& files_;
    uint8_t* buf_;
    bool loaded_ = false, dirty_ = false;
    uint8_t count_ = 0;
    char names_[LAYER_MAX][LAYER_NAME_LEN + 1] = {};
};

}  // namespace studio
