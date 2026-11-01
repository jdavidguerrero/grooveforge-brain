#pragma once
#include "ui/layers.h"
#include <array>
#include <cstdint>
namespace gf {
/** @brief Distribuye notas a dos capas y mezcla sus señales sin asignar memoria durante el audio. */
class HybridRouter {
public:
    using NoteOn = void (*)(void*, uint8_t, uint8_t, float);
    using NoteOff = void (*)(void*, uint8_t, uint8_t);
    /** @brief Prepara la frecuencia del DAW y reinicia los eventos pendientes. */
    void prepare(double sample_rate);
    /** @brief Instala los receptores de notas de las capas 0 y 1. */
    void set_callbacks(void* context, NoteOn on, NoteOff off);
    /** @brief Configura balance, ganancias base, retraso y transposición de B. */
    void configure(float morph, float gain_a_db, float gain_b_db, uint8_t delay_ms, int8_t transpose);
    /** @brief Cambia MORPH sin alterar las notas. */
    void set_morph(float morph);
    /** @brief Dispara A y agenda o dispara B. */
    void note_on(uint8_t note, float velocity);
    /** @brief Suelta A y cancela o suelta la nota correspondiente de B. */
    void note_off(uint8_t note);
    /** @brief Avanza una muestra y dispara las notas B cuyo retraso terminó. */
    void tick();
    /** @brief Mezcla las señales estéreo usando la curva compartida del Brain. */
    void mix(float al, float ar, float bl, float br, float& left, float& right) const;
    /** @brief Libera las notas activas y cancela los eventos pendientes. */
    void reset();
private:
    struct Key { bool active = false, started = false; uint8_t b_note = 0; float velocity = 0; uint64_t due = 0; };
    void start_b(Key& key);
    void update_next_due();
    std::array<Key, 128> keys_{};
    std::array<uint16_t, 128> b_refs_{};
    void* context_ = nullptr;
    NoteOn on_ = nullptr;
    NoteOff off_ = nullptr;
    double fs_ = 44100;
    uint64_t sample_ = 0, next_due_ = UINT64_MAX;
    uint8_t delay_ms_ = 0;
    int8_t transpose_ = 0;
    float gain_a_db_ = 0, gain_b_db_ = 0, gain_a_ = 1, gain_b_ = 1;
};
}
