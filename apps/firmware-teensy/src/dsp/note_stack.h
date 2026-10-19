#pragma once

#include <stdint.h>

/**
 * @file note_stack.h
 * @brief Pila de notas para modo monofónico con prioridad a la última nota (código custom, lógica pura).
 *
 * Al soltar la nota de arriba vuelve a la anterior que siga apretada, como el ARP 2600 y el Minimoog
 * (theory/fidelidad-engines.md §11.4).
 */

namespace dsp {

class NoteStack {
public:
    static constexpr uint8_t MAX = 8;
    static constexpr uint8_t NONE = 255;

    /// Agrega (o sube al tope) la nota. @return true si ya había otras apretadas (legato).
    bool push(uint8_t note) {
        remove_only(note);
        const bool legato = n_ > 0;
        if (n_ == MAX) {
            for (uint8_t i = 0; i + 1 < MAX; ++i) s_[i] = s_[i + 1];
            --n_;
        }
        s_[n_++] = note;
        return legato;
    }

    /// Saca la nota. @return la nota que queda arriba (NONE si no queda ninguna).
    uint8_t remove(uint8_t note) {
        remove_only(note);
        return top();
    }

    uint8_t top() const { return n_ ? s_[n_ - 1] : NONE; }
    uint8_t size() const { return n_; }
    void clear() { n_ = 0; }

private:
    void remove_only(uint8_t note) {
        for (uint8_t i = 0; i < n_; ++i)
            if (s_[i] == note) {
                for (uint8_t j = i; j + 1 < n_; ++j) s_[j] = s_[j + 1];
                --n_;
                return;
            }
    }

    uint8_t s_[MAX] = {};
    uint8_t n_ = 0;
};

}  // namespace dsp
