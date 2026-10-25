#pragma once

#include <stdint.h>

/**
 * @file chord_memory.h
 * @brief Modo CHORD: una tecla dispara un acorde (theory/09 de Studio §4), lógica pura.
 *
 * MEMORIA: intervalos grabados desde la nota más grave del acorde tocado. EN TONALIDAD: tríada diatónica de la
 * tonalidad elegida (key 0–23: 0–11 = C … B mayor, 12–23 = C … B menor natural). INVERSIÓN sube a la octava las
 * notas de abajo.
 */
namespace dsp {

class ChordMemory {
public:
    static constexpr uint8_t MAX = 6;
    enum class Type : uint8_t { Memory = 0, Key };

    ChordMemory() {
        const int8_t def[4] = {0, 3, 7, 12};   // menor + octava: el stab típico de house
        for (uint8_t i = 0; i < 4; ++i) iv_[i] = def[i];
        n_ = 4;
    }

    /// Graba el acorde a partir de las notas apretadas (cualquier orden). Con menos de 2 notas no cambia nada.
    bool learn(const uint8_t* notes, uint8_t count) {
        if (count < 2) return false;
        uint8_t lo = 127;
        for (uint8_t i = 0; i < count; ++i) lo = notes[i] < lo ? notes[i] : lo;
        n_ = 0;
        for (uint8_t i = 0; i < count && n_ < MAX; ++i) {
            const int8_t v = int8_t(notes[i] - lo);
            bool dup = false;
            for (uint8_t j = 0; j < n_; ++j) dup |= iv_[j] == v;
            if (!dup) iv_[n_++] = v;
        }
        sort();
        return true;
    }

    void set_type(Type t) { type_ = t; }
    void set_inversion(uint8_t inv) { inv_ = inv > 3 ? 3 : inv; }
    void set_key(uint8_t key) { key_ = key % 24; }
    uint8_t size() const { return type_ == Type::Key ? 3 : n_; }

    /// Notas del acorde para la tecla `root`, en orden de grave a agudo. Devuelve cuántas escribió.
    uint8_t notes(uint8_t root, uint8_t* out) const {
        int tmp[MAX];
        uint8_t n;
        if (type_ == Type::Key) {
            n = triad(root, tmp);
        } else {
            n = n_;
            for (uint8_t i = 0; i < n; ++i) tmp[i] = root + iv_[i];
        }
        // Inversión: las `inv_` notas de abajo suben una octava (en cada vuelta se reordena)
        for (uint8_t k = 0; k < inv_ && n > 1; ++k) {
            const int low = tmp[0] + 12;
            for (uint8_t i = 0; i + 1 < n; ++i) tmp[i] = tmp[i + 1];
            tmp[n - 1] = low;
        }
        for (uint8_t i = 0; i < n; ++i) out[i] = uint8_t(tmp[i] < 0 ? 0 : tmp[i] > 127 ? 127 : tmp[i]);
        return n;
    }

private:
    void sort() {
        for (uint8_t i = 1; i < n_; ++i)
            for (uint8_t j = i; j > 0 && iv_[j - 1] > iv_[j]; --j) {
                const int8_t t = iv_[j];
                iv_[j] = iv_[j - 1];
                iv_[j - 1] = t;
            }
    }
    /// Tríada diatónica: la tecla se lleva a la nota de la escala más cercana (empate: hacia abajo).
    uint8_t triad(uint8_t root, int* out) const {
        static const uint8_t MAJ[7] = {0, 2, 4, 5, 7, 9, 11};
        static const uint8_t MIN[7] = {0, 2, 3, 5, 7, 8, 10};
        const uint8_t* sc = key_ < 12 ? MAJ : MIN;
        const int tonic = key_ % 12;
        const int rel = ((int(root) - tonic) % 12 + 12) % 12;
        int deg = 0, best = 99;
        for (int d = 0; d < 7; ++d) {
            int dist = rel - sc[d];
            if (dist < 0) dist = -dist;
            if (dist < best) { best = dist; deg = d; }
        }
        const int base = int(root) - rel + sc[deg];   // la tecla, llevada a la escala
        for (int k = 0; k < 3; ++k) {
            const int dd = deg + 2 * k;
            out[k] = base - sc[deg] + sc[dd % 7] + 12 * (dd / 7);
        }
        return 3;
    }

    int8_t iv_[MAX] = {};
    uint8_t n_ = 0;
    Type type_ = Type::Memory;
    uint8_t inv_ = 0, key_ = 9 + 12;   // A menor por defecto
};

}  // namespace dsp
