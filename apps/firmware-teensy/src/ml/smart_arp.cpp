/**
 * @file smart_arp.cpp
 * @brief Smart Arpeggiator — Markov chain + chord enrichment + beat sync.
 *
 * Theory: apps/docs/sprints/42-smart-arpeggiator.md
 * Sprint 6.3 — GrooveForge Brain Fase 6 Layer 1 AI.
 */

#include "smart_arp.h"
#include <string.h>

// ── Intervalos de cada tipo de acorde (semitones sobre la raíz) ──────────────
// Índices: 0=maj, 1=min, 2=7, 3=m7, 4=maj7, 5=N
// -1 = no hay más intervalos en este tipo
static const int8_t CHORD_INTERVALS[6][4] = {
    { 0,  4,  7, -1},   // maj
    { 0,  3,  7, -1},   // min
    { 0,  4,  7, 10},   // dom7
    { 0,  3,  7, 10},   // m7
    { 0,  4,  7, 11},   // maj7
    {-1, -1, -1, -1},   // N (no chord)
};

// ── Probabilidades Markov (acumulativas, sobre 256) ───────────────────────────
// 0-101: step +1 (40%)  | 102-152: step -1 (20%)  | 153-191: repeat (15%)
// 192-229: skip +2 (15%)| 230-255: random (10%)

SmartArp::SmartArp() {
    memset(_held,      0xFF, sizeof(_held));
    memset(_arp_notes, 0,    sizeof(_arp_notes));
    _held_count = 0;
    _arp_count  = 0;
}

// ── Configuración ──────────────────────────────────────────────────────────────

void SmartArp::set_mode(Mode m)        { _mode = m; _notes_dirty = true; }
void SmartArp::set_division(Division d){ _div  = d; }
void SmartArp::set_gate(float f)       { _gate = (f < 0.05f) ? 0.05f : (f > 1.0f ? 1.0f : f); }

void SmartArp::set_groove(GrooveHumanizer::Style style, float amount) {
    _humanizer.set_style(style);
    _humanizer.set_amount(amount);
}

void SmartArp::set_callbacks(NoteOnCb on_cb, NoteOffCb off_cb, void* ctx) {
    _on_cb  = on_cb;
    _off_cb = off_cb;
    _cb_ctx = ctx;
}

// ── Contexto AI ────────────────────────────────────────────────────────────────

void SmartArp::set_bpm(float bpm) {
    if (bpm < 20.0f)  bpm = 20.0f;
    if (bpm > 300.0f) bpm = 300.0f;
    _bpm = bpm;
}

void SmartArp::set_key(uint8_t root, bool minor) {
    _key_root  = root % 12;
    _key_minor = minor;
}

void SmartArp::set_chord(uint8_t root, uint8_t type) {
    _chord_root  = root;
    _chord_type  = type;
    _notes_dirty = true;
}

// ── Input MIDI del usuario ──────────────────────────────────────────────────────

void SmartArp::note_on(uint8_t midi_note) {
    // Evitar duplicados
    for (uint8_t i = 0; i < _held_count; i++) {
        if (_held[i] == midi_note) return;
    }
    if (_held_count < 16) {
        _held[_held_count++] = midi_note;
        _notes_dirty = true;
    }
}

void SmartArp::note_off(uint8_t midi_note) {
    for (uint8_t i = 0; i < _held_count; i++) {
        if (_held[i] == midi_note) {
            // Compactar array
            for (uint8_t j = i; j < _held_count - 1; j++) {
                _held[j] = _held[j + 1];
            }
            _held_count--;
            _notes_dirty = true;
            return;
        }
    }
}

// ── PRNG — LFSR Galois 16-bit ──────────────────────────────────────────────────

uint8_t SmartArp::_prng() {
    uint8_t lsb = _lfsr & 1;
    _lfsr >>= 1;
    if (lsb) _lfsr ^= 0xB400u;
    return (uint8_t)(_lfsr & 0xFF);
}

// ── Construcción del arp note set ─────────────────────────────────────────────

void SmartArp::_build_arp_notes() {
    _arp_count = 0;
    _notes_dirty = false;

    if (_held_count == 0) return;

    // Copiar notas sostenidas
    for (uint8_t i = 0; i < _held_count && _arp_count < 32; i++) {
        _arp_notes[_arp_count++] = _held[i];
    }

    // En modo SMART: enriquecer con chord tones del acorde detectado
    if (_mode == Mode::SMART && _chord_root < 12 && _chord_type < 5) {
        uint8_t base = _held[0];  // nota más baja sostenida como referencia

        for (uint8_t ci = 0; ci < 4 && CHORD_INTERVALS[_chord_type][ci] >= 0; ci++) {
            uint8_t target_pc = (uint8_t)((_chord_root + CHORD_INTERVALS[_chord_type][ci]) % 12);

            // Encontrar la instancia de este pitch class más cercana a base
            // Busca dentro de ±12 semitones del base
            for (int8_t oct_off = 0; oct_off <= 1; oct_off++) {
                uint8_t candidate = (uint8_t)(base + oct_off * 12);
                // Ajustar al pitch class correcto hacia arriba
                while ((candidate % 12) != target_pc && candidate < 127) candidate++;
                if (candidate > 127) continue;

                // ¿Ya está en el set?
                bool dup = false;
                for (uint8_t j = 0; j < _arp_count; j++) {
                    if (_arp_notes[j] == candidate) { dup = true; break; }
                }
                if (!dup && _arp_count < 32) {
                    _arp_notes[_arp_count++] = candidate;
                }
            }
        }
    }

    // Ordenar por pitch (insertion sort — array pequeño, <32 elementos)
    for (uint8_t i = 1; i < _arp_count; i++) {
        uint8_t key = _arp_notes[i];
        int8_t  j   = (int8_t)(i - 1);
        while (j >= 0 && _arp_notes[j] > key) {
            _arp_notes[j + 1] = _arp_notes[j];
            j--;
        }
        _arp_notes[j + 1] = key;
    }

    // Resetear cursor si queda fuera de rango
    if (_step >= _arp_count)  _step = 0;
    if (_markov_pos >= _arp_count) _markov_pos = 0;
}

// ── Timing ─────────────────────────────────────────────────────────────────────

uint32_t SmartArp::_step_period_ms() const {
    // step = 60000ms / (bpm * subdivision_factor)
    float bpm_safe = (_bpm < 20.0f) ? 120.0f : _bpm;
    uint8_t sub = 1;
    switch (_div) {
        case Division::QUARTER:   sub = 1; break;
        case Division::EIGHTH:    sub = 2; break;
        case Division::SIXTEENTH: sub = 4; break;
    }
    float period = 60000.0f / (bpm_safe * (float)sub);
    uint32_t p = (uint32_t)(period + 0.5f);
    if (p < 20) p = 20;    // floor 20ms para no spamear noteOn
    return p;
}

// ── Selección de nota ──────────────────────────────────────────────────────────

uint8_t SmartArp::_next_markov() {
    if (_arp_count == 0) return 60;

    uint8_t r = _prng();
    uint8_t next_pos;

    if (r < 102) {
        // 40%: paso adelante (wrap)
        next_pos = (_markov_pos + 1) % _arp_count;
    } else if (r < 153) {
        // 20%: paso atrás (wrap)
        next_pos = (_markov_pos + _arp_count - 1) % _arp_count;
    } else if (r < 192) {
        // 15%: repetir
        next_pos = _markov_pos;
    } else if (r < 230) {
        // 15%: saltar 2 adelante
        next_pos = (_markov_pos + 2) % _arp_count;
    } else {
        // 10%: posición aleatoria
        next_pos = _prng() % _arp_count;
    }

    _markov_pos = next_pos;
    return _arp_notes[next_pos];
}

uint8_t SmartArp::_next_note() {
    if (_arp_count == 0) return 60;

    uint8_t note;
    switch (_mode) {
        case Mode::UP:
            note = _arp_notes[_step % _arp_count];
            _step = (_step + 1) % _arp_count;
            break;

        case Mode::DOWN:
            note = _arp_notes[(_arp_count - 1 - _step % _arp_count)];
            _step = (_step + 1) % _arp_count;
            break;

        case Mode::UP_DOWN:
            if (_arp_count == 1) {
                note = _arp_notes[0];
            } else {
                note = _arp_notes[_step];
                if (_going_up) {
                    if (_step >= _arp_count - 1) { _going_up = false; _step--; }
                    else _step++;
                } else {
                    if (_step == 0) { _going_up = true; _step++; }
                    else _step--;
                }
            }
            break;

        case Mode::RANDOM:
            note = _arp_notes[_prng() % _arp_count];
            break;

        case Mode::SMART:
        default:
            note = _next_markov();
            break;
    }

    return note;
}

// ── Fire / Silence ─────────────────────────────────────────────────────────────

void SmartArp::_silence_current(uint32_t /*now_ms*/) {
    if (_note_active && _last_note != 0xFF && _off_cb) {
        _off_cb(_last_note, _cb_ctx);
    }
    _note_active = false;
    _last_note   = 0xFF;
}

void SmartArp::_fire_step(uint32_t now_ms) {
    if (_arp_count == 0) return;

    // Silenciar nota anterior si sigue activa
    _silence_current(now_ms);

    uint8_t note = _next_note();
    uint32_t period = _step_period_ms();

    if (_on_cb) {
        _on_cb(note, 100, _cb_ctx);  // velocity 100 — param expuesto en sprint posterior
    }

    _last_note    = note;
    _note_active  = true;
    _gate_off_ms  = now_ms + (uint32_t)((float)period * _gate + 0.5f);
    _last_step_ms = now_ms;
}

// ── update() — llamar desde loop() ─────────────────────────────────────────────

void SmartArp::update(uint32_t now_ms) {
    if (!enabled) {
        // Si se desactiva mientras hay nota activa, silenciar
        if (_note_active) _silence_current(now_ms);
        return;
    }

    // Sin notas sostenidas: silenciar y no hacer nada
    if (_held_count == 0) {
        if (_note_active) _silence_current(now_ms);
        _last_step_ms = now_ms;  // reset timer para que el próximo step sea inmediato
        return;
    }

    // Reconstruir arp note set si cambió
    if (_notes_dirty) _build_arp_notes();
    if (_arp_count == 0) return;

    uint32_t period = _step_period_ms();

    // Aplicar humanización al timing del paso (Sprint 6.4 Groove Humanizer)
    int32_t  groove_off = _humanizer.offset_ms(_step_parity, period);
    uint32_t adj_period;
    if (groove_off < 0 && (uint32_t)(-groove_off) >= period) {
        adj_period = 10u;  // floor mínimo — no dividir por 0 ni invertir el tiempo
    } else {
        adj_period = (uint32_t)((int32_t)period + groove_off);
    }

    // Gate off: silenciar cuando el gate expira (antes del próximo step)
    if (_note_active && now_ms >= _gate_off_ms) {
        _silence_current(now_ms);
    }

    // Step: disparar siguiente nota cuando el período humanizado expira
    if (now_ms - _last_step_ms >= adj_period) {
        _step_parity ^= 1;   // alternar paridad para swing
        _fire_step(now_ms);
    }
}
