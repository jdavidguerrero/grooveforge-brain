#include "sim/demo_source.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace {

// Respuesta aproximada de un low-pass de 4 polos con pico de resonancia (studio-data.js: filterDb).
float filter_db(float f, float fc, float res) {
    const float r = f / fc;
    const float lp = -10.0f * std::log10(1.0f + std::pow(r, 8.0f));
    const float l2 = std::log2(r);
    return lp + res * 14.0f * std::exp(-(l2 * l2) / 0.05f);
}

struct KeyDemo {
    uint8_t camelot; bool minor; const char* name; const char* chord; uint8_t conf;
    uint16_t chord_pcs;  ///< bit n = clase de nota n (0 = C) presente en el acorde
};
constexpr uint16_t pcs(std::initializer_list<int> l) { uint16_t m = 0; for (int n : l) m |= uint16_t(1u << n); return m; }
const KeyDemo KEYS[] = {
    {8, true, "A MINOR", "Am7", 87, pcs({9, 0, 4, 7})},
    {7, true, "D MINOR", "Dm9", 81, pcs({2, 5, 9, 0, 4})},
    {9, true, "E MINOR", "Em7", 84, pcs({4, 7, 11, 2})},
    {8, false, "C MAJOR", "Cmaj7", 78, pcs({0, 4, 7, 11})},
};

}  // namespace

void demo_tick(StudioState& s, uint32_t now_ms) {
    const float t = now_ms / 1000.0f;

    // Cutoff: barrido logarítmico lento 300 Hz ↔ 6 kHz (periodo 12 s) para ver moverse la línea.
    const float sweep = 0.5f - 0.5f * std::cos(t * 2.0f * float(M_PI) / 12.0f);
    s.cutoff_hz = 300.0f * std::pow(20.0f, sweep);
    s.resonance = 0.62f;

    // Compás de 4 tiempos a s.bpm.
    const float beats = t * s.bpm / 60.0f;
    s.bar_phase = std::fmod(beats, 4.0f) / 4.0f;

    // Espectro: ruido musical de base + bajo pulsante + forma del filtro (studio-data.js: spectrum).
    for (int i = 0; i < 32; ++i) {
        const float x = i / 31.0f;
        const float f = 20.0f * std::pow(1000.0f, (i + 0.5f) / 32.0f);
        float db = -12.0f - x * 20.0f
                 + std::exp(-std::pow((i - 4) / 2.2f, 2.0f)) * 9.0f * (0.55f + 0.45f * std::sin(t * 13.0f))
                 + std::sin(t * 2.3f + i * 0.9f) * 4.0f
                 + std::sin(t * 5.7f + i * 2.7f) * 3.0f;
        if (!s.filter_bypass) db += filter_db(f, s.cutoff_hz, s.resonance) * 0.85f;
        db = std::clamp(db, -60.0f, 0.0f);
        s.spectrum[i] = uint8_t((db + 60.0f) / 60.0f * 255.0f);
    }

    // Tonalidad y acorde: cambian cada 8 s.
    const KeyDemo& k = KEYS[(now_ms / 8000) % (sizeof(KEYS) / sizeof(KEYS[0]))];
    s.key_camelot = k.camelot;
    s.key_minor = k.minor;
    std::snprintf(s.key_name, sizeof(s.key_name), "%s", k.name);
    std::snprintf(s.chord, sizeof(s.chord), "%s", k.chord);
    s.key_conf = k.conf;

    // Cromagrama: las notas del acorde dominan; el resto queda bajo (studio-data.js: chroma).
    for (int n = 0; n < 12; ++n) {
        const float base = (k.chord_pcs >> n) & 1u ? 0.85f : 0.12f;
        s.chroma[n] = std::clamp(base * (0.8f + 0.3f * std::sin(t * 2.1f + n * 1.7f)), 0.05f, 1.0f);
    }

    // P1/P2 respiran despacio para ver las barras de la fila 2.
    s.p1 = 0.48f + 0.10f * std::sin(t * 0.7f);
    s.p2 = 0.20f + 0.08f * std::sin(t * 0.43f + 1.0f);
}
