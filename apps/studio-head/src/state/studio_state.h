#pragma once

#include <cstdint>

/**
 * @file studio_state.h
 * @brief Espejo del estado del Brain en la cabeza de Studio (theory doc §3).
 *
 * La UI solo dibuja este struct. Hoy lo llena el modo demo; después lo llenará el hilo de I/O
 * con los frames del bridge (02-bridge-protocol.md).
 */

enum class Mode : uint8_t { Synth, Fx };
enum class LinkState : uint8_t { Demo, Connecting, Up, Lost };

struct StudioState {
    Mode mode = Mode::Synth;
    LinkState link = LinkState::Demo;

    uint8_t engine = 0;              ///< engine_id 0x00–0x05 (02 §2.2)
    char preset[24] = "A-07 BASS FOUNDRY";

    float cutoff_hz = 1240.0f;       ///< 20–20000
    float resonance = 0.62f;         ///< 0–1
    bool filter_bypass = false;

    // AI (02 §2.9)
    uint8_t key_camelot = 8;         ///< 1–12
    bool key_minor = true;           ///< A = menor, B = mayor
    char key_name[12] = "A MINOR";
    uint8_t key_conf = 87;
    char chord[8] = "Am7";
    uint8_t chord_conf = 82;
    char genre[10] = "HOUSE";
    uint8_t genre_conf = 71;
    float bpm = 124.0f;
    float bar_phase = 0.0f;          ///< 0–1 dentro del compás de 4 tiempos
    float chroma[12] = {};           ///< actividad por clase de nota C..B, 0–1 (GROOVE_STATE 0x83)
    bool scale_lock = true;
    char snap_from[6] = "G#3";       ///< último snap de Scale Lock (nota tocada → nota corregida)
    char snap_to[6] = "A3";

    // Envolvente del filtro de la página ENV
    float env_attack_ms = 12.0f;
    float env_decay_ms = 340.0f;
    float env_sustain = 0.62f;       ///< 0–1
    float env_release_ms = 480.0f;

    /// Espectro de 32 bandas log 20 Hz–20 kHz; 0 = −60 dB, 255 = 0 dB (frame propuesto, ADR-002)
    uint8_t spectrum[32] = {};

    // Controles de página (ADR-004): P1/P2 y SHIFT
    float p1 = 0.48f;                ///< PERFORM: FLT ENV AMT
    float p2 = 0.20f;                ///< PERFORM: LFO→CUTOFF
    bool shift = false;

    float volume_db = -6.0f;
};
