#pragma once

#include <stddef.h>
#include <stdint.h>

#include "page_params.h"

/**
 * @file preset_bank.h
 * @brief Presets del Studio: formato binario, banco de 16 por engine y banco de fábrica (código custom).
 *
 * Lógica pura (native): el almacenamiento real (LittleFS_Program en el sketch49) entra por PresetStorage.
 * Teoría: apps/docs/variants/studio/theory/06-studio-presets.md.
 */

namespace studio {

constexpr uint8_t PRESET_MAX = 16;          ///< presets por engine (el tope de la cabeza)
constexpr uint8_t PRESET_NAME_LEN = 16;     ///< sin el terminador
constexpr uint8_t PRESET_FORMAT = 2;        ///< versión del formato (2: NOISE del Model D en su parada, 2026-10-19)

/// Un sonido: el patch editable + el filtro de ENC L / R.
struct Preset {
    char name[PRESET_NAME_LEN + 1] = "INIT";
    Patch patch;
    float cutoff_hz = 2000.0f;
    float reso_norm = 0.15f;                ///< resonancia normalizada 0–1 sobre el rango del engine
};

/// Bytes de un preset serializado (cabecera + campos en orden fijo, little-endian).
constexpr size_t PRESET_BYTES = 4 + 1 + PRESET_NAME_LEN + 4 * (12 + GF_DEST_COUNT + GF_OSC_STOPS * GF_OSC_SLOTS +
                                GF_MOD_STOPS_MAX * GF_OSC_SLOTS + GF_SFX_COUNT + GF_SFX_COUNT * GF_SFX_PARAMS + 2) +
                                2 + GF_SFX_COUNT;

/// Serializa `p` del engine `engine` en `buf`. @return bytes escritos, 0 si `cap` < PRESET_BYTES.
size_t preset_encode(const Preset& p, uint8_t engine, uint8_t* buf, size_t cap);

/// Lee un preset. @return false si la firma, la versión, el engine o el tamaño no coinciden.
bool preset_decode(const uint8_t* buf, size_t len, uint8_t engine, Preset* out);

/// Patches clásicos del engine (theory/06 §8). @return cuántos escribió en `out` (≤ cap).
uint8_t factory_presets(uint8_t engine, Preset* out, uint8_t cap);

/// Almacenamiento de un archivo por preset (`/presets/<engine>/<nn>.gfp` en el Teensy).
class PresetStorage {
public:
    virtual ~PresetStorage() = default;
    /// @return bytes leídos (0 si no existe).
    virtual size_t read(uint8_t engine, uint8_t index, uint8_t* buf, size_t cap) = 0;
    virtual bool write(uint8_t engine, uint8_t index, const uint8_t* buf, size_t len) = 0;
};

/// Banco de cada engine: nombres en RAM, patches en el almacenamiento.
class PresetBank {
public:
    explicit PresetBank(PresetStorage& s) : storage_(s) {}

    /// Lee los nombres del banco del engine; si está vacío, escribe el banco de fábrica.
    void scan(uint8_t engine);

    uint8_t count(uint8_t engine) const { return engine < GF_ENGINE_COUNT ? count_[engine] : 0; }
    const char* name(uint8_t engine, uint8_t index) const;

    /// Carga el preset `index`. @return false si no existe o está corrupto.
    bool load(uint8_t engine, uint8_t index, Preset* out);

    /// Guarda `p` en el próximo lugar libre como `USER nn`. @return el índice, o -1 si el banco está lleno.
    int save_new(uint8_t engine, Preset p);

private:
    PresetStorage& storage_;
    uint8_t count_[GF_ENGINE_COUNT] = {};
    char names_[GF_ENGINE_COUNT][PRESET_MAX][PRESET_NAME_LEN + 1] = {};
};

}  // namespace studio
