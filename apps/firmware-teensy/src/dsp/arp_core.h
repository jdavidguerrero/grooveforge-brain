#pragma once

#include <math.h>
#include <stdint.h>

/**
 * @file arp_core.h
 * @brief Arpegiador que avanza con la posición del reloj común (theory/09 de Studio §3), lógica pura.
 *
 * No cuenta milisegundos propios: el paso actual sale de la posición en beats, así que el arpegio queda en fase con
 * el PUMP y el DELAY y sigue los cambios de tempo. `update()` se llama a tasa de control (~1 kHz) y emite NOTE ON /
 * NOTE OFF por callbacks.
 */
namespace dsp {

class ArpCore {
public:
    enum class Mode : uint8_t { Up = 0, Down, UpDown, Random, Smart };
    static constexpr uint8_t MAX_NOTES = 16;
    static constexpr uint8_t NONE = 255;
    static constexpr uint16_t FRESH = 0xFFFF;

    using NoteOn = void (*)(uint8_t note, uint8_t vel, void* ctx);
    using NoteOff = void (*)(uint8_t note, void* ctx);

    void set_callbacks(NoteOn on, NoteOff off, void* ctx) { on_ = on; off_ = off; ctx_ = ctx; }
    void set_mode(Mode m) { mode_ = m; }
    /// Largo del paso en beats: 1/4 = 1, 1/8 = 0.5, 1/16 = 0.25, 1/8T = 1/3, 1/16T = 1/6, 1/8. = 0.75.
    void set_step_beats(float b) { step_ = b > 0.02f ? b : 0.02f; }
    void set_gate(float g) { gate_ = g < 0.05f ? 0.05f : g > 1.0f ? 1.0f : g; }
    void set_octaves(uint8_t o) { oct_ = o < 1 ? 1 : o > 3 ? 3 : o; }
    /// 0.5 = recto; 0.75 = los pasos impares caen al 75 % del par de pasos.
    void set_swing(float s) { swing_ = s < 0.5f ? 0.5f : s > 0.75f ? 0.75f : s; }
    void set_latch(bool on) {
        if (latch_ && !on) {                    // al soltar el latch, se quedan solo las teclas apretadas
            n_ = 0;
            for (uint8_t i = 0; i < pressed_n_; ++i) n_ = insert(notes_, n_, pressed_[i]);
        }
        latch_ = on;
    }

    // ── Notas del usuario ─────────────────────────────────────────────────────
    void note_on(uint8_t note, uint8_t vel) {
        vel_ = vel;
        if (latch_ && pressed_n_ == 0) n_ = 0;  // con latch, tocar con todo suelto reemplaza el acorde
        pressed_n_ = insert(pressed_, pressed_n_, note);
        n_ = insert(notes_, n_, note);
    }
    void note_off(uint8_t note) {
        pressed_n_ = remove(pressed_, pressed_n_, note);
        if (!latch_) n_ = remove(notes_, n_, note);
    }
    void clear() {
        n_ = pressed_n_ = 0;
        silence();
    }
    uint8_t held() const { return n_; }
    /// El transporte se detuvo: apaga la nota que suena sin olvidar las sostenidas.
    void pause() {
        silence();
        last_step_ = -1;
    }

    /// Avanza a la posición `pos` (beats) del reloj.
    void update(double pos) {
        if (n_ == 0) {
            silence();
            last_step_ = -1;
            idx_ = FRESH;
            return;
        }
        // Pasos agrupados de a dos para el swing: el impar arranca en swing·(2·paso) dentro del par
        const double pair = 2.0 * double(step_);
        const long pair_i = long(floor(pos / pair));
        const double in_pair = pos - double(pair_i) * pair;
        const double odd_start = double(swing_) * pair;
        const bool odd = in_pair >= odd_start;
        const long step_i = pair_i * 2 + (odd ? 1 : 0);
        const double step_start = odd ? odd_start : 0.0;
        const double step_len = odd ? pair - odd_start : odd_start;
        if (step_i != last_step_) {
            silence();
            last_step_ = step_i;
            const uint8_t note = next_note();
            if (note != NONE && on_) {
                on_(note, vel_, ctx_);
                sounding_ = note;
            }
        }
        if (sounding_ != NONE && in_pair - step_start >= double(gate_) * step_len) silence();
    }

    uint8_t sounding() const { return sounding_; }

private:
    static uint8_t insert(uint8_t* a, uint8_t n, uint8_t v) {   // orden ascendente, sin repetidos
        for (uint8_t i = 0; i < n; ++i)
            if (a[i] == v) return n;
        if (n == MAX_NOTES) return n;
        uint8_t i = n;
        while (i > 0 && a[i - 1] > v) {
            a[i] = a[i - 1];
            --i;
        }
        a[i] = v;
        return uint8_t(n + 1);
    }
    static uint8_t remove(uint8_t* a, uint8_t n, uint8_t v) {
        for (uint8_t i = 0; i < n; ++i)
            if (a[i] == v) {
                for (uint8_t j = i; j + 1 < n; ++j) a[j] = a[j + 1];
                return uint8_t(n - 1);
            }
        return n;
    }
    void silence() {
        if (sounding_ != NONE && off_) off_(sounding_, ctx_);
        sounding_ = NONE;
    }
    uint8_t rnd() {
        rng_ ^= uint16_t(rng_ << 7);
        rng_ ^= uint16_t(rng_ >> 9);
        rng_ ^= uint16_t(rng_ << 8);
        return uint8_t(rng_);
    }
    /// La nota `i` de la secuencia extendida por octavas (n_ · oct_ notas).
    uint8_t at(uint16_t i) const {
        const uint16_t o = uint16_t(i / n_);
        const int v = notes_[i % n_] + 12 * o;
        return uint8_t(v > 127 ? 127 : v);
    }
    uint8_t next_note() {
        const uint16_t total = uint16_t(n_ * oct_);
        if (idx_ >= total) {   // arranque (o se soltaron notas): empieza por un extremo
            idx_ = mode_ == Mode::Down ? uint16_t(total - 1) : 0;
            dir_up_ = true;
            return at(idx_);
        }
        switch (mode_) {
            case Mode::Up: idx_ = uint16_t((idx_ + 1) % total); break;
            case Mode::Down: idx_ = uint16_t((idx_ + total - 1) % total); break;
            case Mode::UpDown:
                if (total == 1) { idx_ = 0; break; }
                if (dir_up_ && idx_ + 1 >= total) dir_up_ = false;
                else if (!dir_up_ && idx_ == 0) dir_up_ = true;
                idx_ = uint16_t(dir_up_ ? idx_ + 1 : idx_ - 1);
                break;
            case Mode::Random: idx_ = uint16_t(rnd() % total); break;
            case Mode::Smart: {   // paso corto (±1 o ±2) sin repetir; rebota en los bordes
                if (total == 1) { idx_ = 0; break; }
                const uint8_t r = rnd();
                int d = (r & 3) == 0 ? 2 : 1;
                if (r & 4) d = -d;
                int ni = int(idx_) + d;
                if (ni < 0) ni = -ni;
                if (ni >= int(total)) ni = 2 * int(total) - 2 - ni;
                if (ni < 0 || ni == int(idx_)) ni = idx_ == 0 ? 1 : int(idx_) - 1;   // total = 2 rebota a sí misma
                idx_ = uint16_t(ni);
                break;
            }
        }
        return at(idx_);
    }

    NoteOn on_ = nullptr;
    NoteOff off_ = nullptr;
    void* ctx_ = nullptr;
    Mode mode_ = Mode::Up;
    float step_ = 0.25f, gate_ = 0.6f, swing_ = 0.5f;
    uint8_t oct_ = 1;
    bool latch_ = false, dir_up_ = true;
    uint8_t notes_[MAX_NOTES] = {}, n_ = 0;
    uint8_t pressed_[MAX_NOTES] = {}, pressed_n_ = 0;
    uint8_t vel_ = 100, sounding_ = NONE;
    uint16_t idx_ = FRESH, rng_ = 0xACE1;
    long last_step_ = -1;
};

}  // namespace dsp
