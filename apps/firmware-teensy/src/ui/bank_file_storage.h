#pragma once

#include <stdint.h>
#include <string.h>

#include "preset_bank.h"

/**
 * @file bank_file_storage.h
 * @brief Presets de un engine en UN archivo con sus 64 lugares (variants/studio/theory/13 rev. 3), lógica pura.
 *
 * En la Teensy 4.1 LittleFS_Program usa bloques de 64 KB y cada archivo ocupa al menos uno: con un archivo por preset,
 * 527 bytes gastaban 64 KB y el área se llenaba con ~23 presets. Un archivo por engine (~34 KB) entra en un bloque.
 * El banco del engine se carga en RAM, las escrituras lo cambian ahí y `flush()` lo guarda en una sola escritura
 * (un borrado de bloque por banco en lugar de uno por preset).
 */
namespace studio {

/// Lo que necesita el almacenamiento del sistema de archivos: leer un archivo entero y escribirlo de forma atómica.
class FileBackend {
public:
    virtual ~FileBackend() = default;
    virtual size_t read_file(const char* path, uint8_t* buf, size_t cap) = 0;
    virtual bool write_file(const char* path, const uint8_t* buf, size_t len) = 0;
};

class BankFileStorage : public PresetStorage {
public:
    static constexpr size_t SLOT = 2 + PRESET_BYTES;                 ///< largo (2 bytes LE) + preset
    static constexpr uint8_t SLOTS = PRESET_MAX + 1;                 ///< + FACTORY_SLOT
    static constexpr size_t HEADER = 4;                              ///< "GFB" + versión
    static constexpr size_t FILE_BYTES = HEADER + SLOT * SLOTS;

    /// `buffer` necesita FILE_BYTES (en la Teensy va en RAM2).
    BankFileStorage(FileBackend& files, uint8_t* buffer) : files_(files), buf_(buffer) {}

    size_t read(uint8_t e, uint8_t i, uint8_t* out, size_t cap) override {
        if (i >= SLOTS || !load(e)) return 0;
        const uint8_t* s = slot(i);
        const size_t len = size_t(s[0] | (s[1] << 8));
        if (len == 0 || len > PRESET_BYTES || len > cap) return 0;
        memcpy(out, s + 2, len);
        return len;
    }

    bool write(uint8_t e, uint8_t i, const uint8_t* data, size_t len) override {
        if (i >= SLOTS || len > PRESET_BYTES || !load(e)) return false;
        uint8_t* s = slot(i);
        s[0] = uint8_t(len & 0xFF);
        s[1] = uint8_t(len >> 8);
        memcpy(s + 2, data, len);
        dirty_ = true;
        return true;
    }

    bool remove(uint8_t e, uint8_t i) override {
        if (i >= SLOTS || !load(e)) return false;
        slot(i)[0] = slot(i)[1] = 0;
        dirty_ = true;
        return true;
    }

    void flush() override {
        if (!dirty_ || loaded_ < 0) return;
        char path[16];
        path_of(uint8_t(loaded_), path);
        if (files_.write_file(path, buf_, FILE_BYTES)) dirty_ = false;
    }

    /// Después de formatear el área: olvida el banco en RAM (sin guardarlo).
    void reset() {
        loaded_ = -1;
        dirty_ = false;
    }

    /// Para tests y diagnóstico: el engine cuyo banco está en RAM y si tiene cambios sin guardar.
    int8_t loaded() const { return loaded_; }
    bool dirty() const { return dirty_; }

private:
    static void path_of(uint8_t e, char* out) {
        memcpy(out, "/bank0.gfb", 11);
        out[5] = char('0' + e);
    }
    uint8_t* slot(uint8_t i) { return buf_ + HEADER + size_t(i) * SLOT; }

    bool load(uint8_t e) {
        if (e >= GF_ENGINE_COUNT) return false;
        if (loaded_ == int8_t(e)) return true;
        flush();                                      // el banco anterior se guarda antes de cambiar
        if (dirty_) return false;                     // no se pudo guardar: no se pisa
        char path[16];
        path_of(e, path);
        const size_t n = files_.read_file(path, buf_, FILE_BYTES);
        if (n != FILE_BYTES || buf_[0] != 'G' || buf_[1] != 'F' || buf_[2] != 'B' || buf_[3] != 1) {
            memset(buf_, 0, FILE_BYTES);              // no existe o es de otra versión: banco vacío
            buf_[0] = 'G';
            buf_[1] = 'F';
            buf_[2] = 'B';
            buf_[3] = 1;
        }
        loaded_ = int8_t(e);
        return true;
    }

    FileBackend& files_;
    uint8_t* buf_;
    int8_t loaded_ = -1;
    bool dirty_ = false;
};

}  // namespace studio
