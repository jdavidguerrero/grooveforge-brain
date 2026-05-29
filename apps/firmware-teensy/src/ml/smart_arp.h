#pragma once
#include <stdint.h>
#include "groove.h"

/**
 * @brief Smart Arpeggiator — Sprint 6.3 (Fase 6, Layer 1 AI completo).
 *
 * Genera arpegios musicalmente inteligentes usando:
 *   - Cadena de Markov para transiciones melódicas naturales
 *   - Chord tones del ChordRecognizer para enriquecer el arp note set
 *   - Sincronización de tempo al Beat Follower (BPM)
 *
 * Activación: automática cuando enabled=true y el usuario sostiene notas.
 * En AI mode el sketch setea enabled=true; en modo normal, false.
 *
 * Theory: apps/docs/sprints/42-smart-arpeggiator.md
 */
class SmartArp {
public:
    // ── Modos de arpegio ─────────────────────────────────────────────────────
    enum class Mode : uint8_t {
        UP     = 0,  ///< ascendente — clásico
        DOWN   = 1,  ///< descendente
        UP_DOWN = 2, ///< ping-pong
        RANDOM = 3,  ///< aleatorio entre notas sostenidas
        SMART  = 4,  ///< Markov chain + chord enrichment
    };

    // ── Subdivisión rítmica ───────────────────────────────────────────────────
    enum class Division : uint8_t {
        QUARTER    = 0,  ///< negra      — 1 paso por beat
        EIGHTH     = 1,  ///< corchea    — 2 pasos por beat (default)
        SIXTEENTH  = 2,  ///< semicorchea — 4 pasos por beat
    };

    SmartArp();

    // ── Configuración ────────────────────────────────────────────────────────
    void set_mode(Mode m);
    void set_division(Division d);
    /** @brief Gate como fracción del step period (0.0–1.0). Default 0.7. */
    void set_gate(float fraction);
    /** @brief Configura el Groove Humanizer (estilo + intensidad). */
    void set_groove(GrooveHumanizer::Style style, float amount);
    /** @brief Acceso directo al humanizador para inspección o ajuste fino. */
    GrooveHumanizer& groove() { return _humanizer; }

    // ── Input de notas MIDI del usuario ──────────────────────────────────────
    void note_on(uint8_t midi_note);
    void note_off(uint8_t midi_note);
    bool has_held_notes() const { return _held_count > 0; }

    // ── Contexto AI (actualizar cuando MLEngine detecta cambios) ─────────────
    void set_bpm(float bpm);
    /** @brief root 0-11 (C=0…B=11), minor=true → escala menor. */
    void set_key(uint8_t root, bool minor);
    /** @brief root 0-11; type 0=maj,1=min,2=7,3=m7,4=maj7,5=N. 0xFF = no chord. */
    void set_chord(uint8_t root, uint8_t type);

    // ── Tick principal (llamar desde loop() ) ─────────────────────────────────
    /** @brief Procesa timing y dispara noteOn/noteOff via callbacks cuando corresponde. */
    void update(uint32_t now_ms);

    // ── Callbacks para emitir notas al engine activo ──────────────────────────
    using NoteOnCb  = void(*)(uint8_t note, uint8_t vel, void* ctx);
    using NoteOffCb = void(*)(uint8_t note, void* ctx);
    void set_callbacks(NoteOnCb on_cb, NoteOffCb off_cb, void* ctx);

    // ── Estado público ────────────────────────────────────────────────────────
    bool enabled = false;  ///< true cuando AI mode activo (sketch lo controla)

private:
    // ── Notas sostenidas por el usuario ───────────────────────────────────────
    uint8_t _held[16];
    uint8_t _held_count = 0;

    // ── Arp note set (held + chord tones en SMART mode) ───────────────────────
    uint8_t _arp_notes[32];
    uint8_t _arp_count = 0;
    bool    _notes_dirty = true;  ///< reconstruir _arp_notes en próximo step

    // ── Estado del arpegio ────────────────────────────────────────────────────
    Mode     _mode      = Mode::SMART;
    Division _div       = Division::EIGHTH;
    float    _gate      = 0.7f;

    uint8_t  _step       = 0;    ///< cursor en _arp_notes (modo UP/DOWN/UD)
    bool     _going_up   = true; ///< dirección en UP_DOWN
    uint8_t  _markov_pos = 0;    ///< posición en Markov chain

    uint8_t  _last_note   = 0xFF;  ///< nota arp actualmente sonando (0xFF = silencio)
    uint32_t _last_step_ms = 0;    ///< timestamp del último step
    uint32_t _gate_off_ms  = 0;    ///< timestamp para el noteOff del gate
    bool     _note_active  = false;

    // ── Timing ────────────────────────────────────────────────────────────────
    float _bpm = 120.0f;

    // ── Contexto AI ──────────────────────────────────────────────────────────
    uint8_t _key_root   = 0;
    bool    _key_minor  = false;
    uint8_t _chord_root = 0xFF;
    uint8_t _chord_type = 0xFF;

    // ── Callbacks ────────────────────────────────────────────────────────────
    NoteOnCb  _on_cb  = nullptr;
    NoteOffCb _off_cb = nullptr;
    void*     _cb_ctx = nullptr;

    // ── PRNG (LFSR Galois 16-bit, period 65535) ───────────────────────────────
    uint16_t _lfsr = 0xACE1;
    uint8_t  _prng();

    // ── Groove Humanizer (Sprint 6.4) ─────────────────────────────────────────
    GrooveHumanizer _humanizer;
    uint8_t         _step_parity = 0;  ///< 0=par, 1=impar — alterna en cada step para swing

    // ── Helpers ───────────────────────────────────────────────────────────────
    uint32_t _step_period_ms() const;
    void     _build_arp_notes();
    uint8_t  _next_note();
    uint8_t  _next_markov();
    void     _fire_step(uint32_t now_ms);
    void     _silence_current(uint32_t now_ms);
};
