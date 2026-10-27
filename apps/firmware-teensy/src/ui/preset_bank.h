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

constexpr uint8_t PRESET_BANK_SIZE = 16;    ///< un banco (A, B, C, D) son 16 lugares seguidos (theory/13)
constexpr uint8_t PRESET_MAX = 64;          ///< presets por engine: bancos A–D
/// Lugar del almacenamiento donde cada banco anota qué presets de fábrica ya se ofrecieron (no es un preset).
constexpr uint8_t FACTORY_SLOT = PRESET_MAX;
constexpr uint8_t PRESET_NAME_LEN = 16;     ///< sin el terminador
constexpr uint8_t PRESET_FORMAT = 4;        ///< versión del formato (4: velocity por preset, theory/10, 2026-10-26)
constexpr uint8_t PRESET_FORMAT_MIN = 2;    ///< el más viejo que se migra al leer (2: sin GROOVE)

/// Un sonido: el patch editable + el filtro de ENC L / R.
struct Preset {
    char name[PRESET_NAME_LEN + 1] = "INIT";
    Patch patch;
    float cutoff_hz = 2000.0f;
    float reso_norm = 0.15f;                ///< resonancia normalizada 0–1 sobre el rango del engine
};

/// Bytes de un preset serializado (cabecera + campos en orden fijo, little-endian).
constexpr size_t PRESET_BYTES_V2 = 4 + 1 + PRESET_NAME_LEN + 4 * (12 + GF_DEST_COUNT + GF_OSC_STOPS * GF_OSC_SLOTS +
                                   GF_MOD_STOPS_MAX * GF_OSC_SLOTS + GF_SFX_COUNT + GF_SFX_COUNT * GF_SFX_PARAMS + 2) +
                                   2 + GF_SFX_COUNT;
/// Formato 3 = el 2 + GROOVE (valores de las 5 paradas y los 4 encendidos) al final.
constexpr size_t PRESET_BYTES_V3 = PRESET_BYTES_V2 + 4 * GF_GROOVE_STOPS * GF_OSC_SLOTS + GF_GROOVE_TOGGLES;
/// Formato 4 = el 3 + VEL → AMP y VEL → FILTER al final.
constexpr size_t PRESET_BYTES = PRESET_BYTES_V3 + 4 * 2;

/// Serializa `p` del engine `engine` en `buf`. @return bytes escritos, 0 si `cap` < PRESET_BYTES.
size_t preset_encode(const Preset& p, uint8_t engine, uint8_t* buf, size_t cap);

/// Lee un preset; los de formato 2 y 3 se migran (GROOVE neutro, velocity por defecto del engine).
/// @return false si la firma, la versión, el engine o el tamaño no coinciden.
bool preset_decode(const uint8_t* buf, size_t len, uint8_t engine, Preset* out);

/// Patches clásicos del engine (theory/06 §8). @return cuántos escribió en `out` (≤ cap).
uint8_t factory_presets(uint8_t engine, Preset* out, uint8_t cap);

/// Cantidad de presets de fábrica del engine y el preset `index` de esa lista.
uint8_t factory_count(uint8_t engine);
bool factory_preset(uint8_t engine, uint8_t index, Preset* out);

/// Almacenamiento de un archivo por preset (`/presets/<engine>/<nn>.gfp` en el Teensy).
class PresetStorage {
public:
    virtual ~PresetStorage() = default;
    /// @return bytes leídos (0 si no existe).
    virtual size_t read(uint8_t engine, uint8_t index, uint8_t* buf, size_t cap) = 0;
    virtual bool write(uint8_t engine, uint8_t index, const uint8_t* buf, size_t len) = 0;
    /// Borra el archivo (bibliotecario, theory/11). @return false si no se pudo.
    virtual bool remove(uint8_t engine, uint8_t index) = 0;
};

/// Resultado de subir un preset por el bibliotecario (theory/11 §2).
enum class PutResult : uint8_t { Ok, Format, Engine, Index, Write };

/// Banco de cada engine: nombres en RAM, patches en el almacenamiento.
class PresetBank {
public:
    explicit PresetBank(PresetStorage& s) : storage_(s) {}

    /// Lee los nombres del banco del engine y anota qué presets de fábrica faltan, sin escribir nada (rápido: va en el
    /// arranque). Faltan los que no están por nombre y nunca se ofrecieron en este banco (FACTORY_SLOT): un preset de
    /// fábrica que borraste con el bibliotecario no vuelve.
    void read_bank(uint8_t engine);

    /// Escribe UNO de los presets de fábrica pendientes al final del banco (se llama desde el loop, de a uno, para no
    /// bloquear el arranque; si se corta, sigue en el próximo arranque). Al terminar anota los ofrecidos.
    /// @return true si escribió un preset.
    bool seed_one(uint8_t engine);

    /// true mientras falte escribir presets de fábrica o la anotación de ofrecidos.
    bool seeding(uint8_t engine) const;

    /// read_bank + todos los seed_one de una vez (tests y herramientas).
    void scan(uint8_t engine) {
        read_bank(engine);
        while (seed_one(engine)) {
        }
    }

    uint8_t count(uint8_t engine) const { return engine < GF_ENGINE_COUNT ? count_[engine] : 0; }
    const char* name(uint8_t engine, uint8_t index) const;

    /// Carga el preset `index`. @return false si no existe o está corrupto.
    bool load(uint8_t engine, uint8_t index, Preset* out);

    /// Guarda `p` en el próximo lugar libre como `USER nn`. @return el índice, o -1 si el banco está lleno.
    int save_new(uint8_t engine, Preset p);

    /// Bibliotecario: escribe el preset binario `buf` (cualquier formato que se pueda leer) en el lugar `index`,
    /// reemplazando o agregando al final (sin huecos). Se guarda en el formato actual.
    PutResult put(uint8_t engine, uint8_t index, const uint8_t* buf, size_t len);

    /// Bibliotecario: deja el banco con `n` presets (borra del `n` en adelante).
    bool truncate(uint8_t engine, uint8_t n);

private:
    PresetStorage& storage_;
    void write_offered(uint8_t engine);

    uint8_t count_[GF_ENGINE_COUNT] = {};
    char names_[GF_ENGINE_COUNT][PRESET_MAX][PRESET_NAME_LEN + 1] = {};
    uint8_t pending_[GF_ENGINE_COUNT][PRESET_MAX] = {};   ///< índices de fábrica que faltan escribir
    uint8_t pending_n_[GF_ENGINE_COUNT] = {}, pending_i_[GF_ENGINE_COUNT] = {};
    bool offered_dirty_[GF_ENGINE_COUNT] = {};            ///< hay presets de fábrica nuevos sin anotar
};

}  // namespace studio
