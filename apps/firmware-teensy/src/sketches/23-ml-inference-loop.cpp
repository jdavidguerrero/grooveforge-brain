// Sprint 5.2 — ML Inference Loop
// Combina USB-A Host MIDI con inference de Key Detector + Chord Recognizer + Beat Follower.
//
// Flujo:
//   NoteOn MIDI → acumular en pitch_counts[] + on_onset(millis())
//   NoteOff MIDI → no afecta el histograma (la nota ya se contó en NoteOn)
//   Cada 500ms → normalizar histograma → key_det.inference() + chord_rec.inference()
//                                      → beat_fol.infer() → imprimir por Serial
//
// No genera audio — es un sketch de ML puro para verificar el pipeline.
// La integración con el engine viene en Sprint 5.3 (Scale Lock).
//
// Ref: apps/docs/sprints/26-inference-loop.md
//      apps/docs/04-ai-architecture.md §1 — Layer 1 TinyML, budget <20ms p99

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "usb/midi_host.h"
#include "ml/key_detector.h"
#include "ml/chord_recognizer.h"
#include "ml/beat_follower.h"

static MidiHost       midi_host;
static KeyDetector    key_det;
static ChordRecognizer chord_rec;
static BeatFollower   beat_fol;

// Acumulador de pitch class — raw counts, se normalizan antes de inference.
// Snapshot: se resetea cada INFERENCE_PERIOD_MS. Ver theory §2 en sprint doc.
static float    pitch_counts[12] = {};
static uint32_t note_count       = 0;

// Timer para inference periódica de key + chord + beat.
static uint32_t last_inference_ms = 0;
static constexpr uint32_t INFERENCE_PERIOD_MS = 500;

// ---------------------------------------------------------------------------
// Callbacks MIDI
// ---------------------------------------------------------------------------

// Callback NoteOn: acumular pitch class + registrar onset para BPM.
// El guard vel==0 maneja NoteOn-with-velocity-zero (MIDI running status = NoteOff
// disfrazado). Teclados baratos usan esta convención — sin el guard contaríamos
// NoteOffs como onsets, distorsionando el histograma y el BPM.
static void on_note_on(uint8_t /*ch*/, uint8_t note, uint8_t vel) {
    if (vel == 0) return;

    uint8_t pc = note % 12;  // pitch class: octave-invariant [0, 11]
    pitch_counts[pc] += 1.0f;
    note_count++;

    beat_fol.on_onset(millis());  // timestamp exacto — ver theory §1

    Serial.print(F("[MIDI] note="));
    Serial.print(note);
    Serial.print(F(" pc="));
    Serial.println(pc);
}

// NoteOff no modifica el histograma — la nota ya fue contada en NoteOn.
static void on_note_off(uint8_t /*ch*/, uint8_t /*note*/, uint8_t /*vel*/) {}

// ---------------------------------------------------------------------------
// Inference periódica
// ---------------------------------------------------------------------------

static void run_inference() {
    // Normalizar histograma (suma → 1.0) antes de pasarlo a los modelos.
    // El modelo fue entrenado con distribuciones normalizadas; sin normalizar
    // el modelo vería la densidad de notas como señal, no solo la distribución.
    float histogram[12] = {};
    float total = 0.0f;
    for (int i = 0; i < 12; i++) total += pitch_counts[i];

    if (total < 1e-6f) {
        // Sin notas en este período — nada que inferir.
        Serial.println(F("[ML] Sin notas — saltando inference"));
        // Resetear de todas formas para el siguiente período.
        memset(pitch_counts, 0, sizeof(pitch_counts));
        note_count = 0;
        return;
    }

    for (int i = 0; i < 12; i++) histogram[i] = pitch_counts[i] / total;

    // Resetear acumulador para el período siguiente (ventana snapshot).
    memset(pitch_counts, 0, sizeof(pitch_counts));
    note_count = 0;

    // --- Key Detector ---
    KeyResult kr;
    uint32_t t0    = micros();
    bool key_ok    = key_det.inference(histogram, kr);
    uint32_t key_us = micros() - t0;

    // --- Chord Recognizer ---
    ChordResult cr;
    t0               = micros();
    bool chord_ok    = chord_rec.inference(histogram, cr);
    uint32_t chord_us = micros() - t0;

    // --- Beat Follower ---
    // infer() calcula el IOI histogram internamente desde el buffer de onsets.
    // Se llama aquí (desde loop, no desde ISR) conforme a la nota de thread safety
    // del header: on_onset() es ISR-safe, infer() no lo es.
    BeatResult br = beat_fol.infer();

    // --- Imprimir resultados ---
    Serial.print(F("[ML] Key: "));
    if (key_ok) {
        Serial.print(kr.name);
        Serial.print(F(" ("));
        Serial.print(kr.confidence, 2);
        Serial.print(F(")"));
    } else {
        Serial.print(F("ERR"));
    }

    Serial.print(F("  Chord: "));
    if (chord_ok) {
        Serial.print(cr.name);
        Serial.print(F(" ("));
        Serial.print(cr.confidence, 2);
        Serial.print(F(")"));
    } else {
        Serial.print(F("ERR"));
    }

    Serial.print(F("  BPM: "));
    if (br.valid) {
        Serial.print(br.bpm, 1);
        Serial.print(F(" ("));
        Serial.print(br.confidence, 2);
        Serial.print(F(")"));
    } else {
        Serial.print(F("--"));
    }

    // Latencias de inference — validan que estamos dentro del budget 20ms p99.
    // Ref: apps/docs/04-ai-architecture.md §8
    Serial.print(F("  [key="));
    Serial.print(key_us);
    Serial.print(F("us chord="));
    Serial.print(chord_us);
    Serial.print(F("us]"));
    Serial.println();
}

// ---------------------------------------------------------------------------
// Setup & loop
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println(F("=== Sprint 5.2 — ML Inference Loop ==="));
    Serial.println(F("Conectar teclado MIDI USB al conector USB-A del Teensy."));
    Serial.println(F("Tocar notas → cada 500ms imprime Key + Chord + BPM detectados."));

    // Inicializar modelos — reportar arena usada para validar budget RAM.
    // Budget total Layer 1: ~200KB. KeyDetector ~16KB + ChordRecognizer ~24KB +
    // BeatFollower ~16KB = ~56KB. Ref: apps/docs/04-ai-architecture.md §1.2
    Serial.print(F("Init KeyDetector... "));
    if (key_det.init()) {
        Serial.print(F("OK (arena="));
        Serial.print(key_det.arena_used_bytes());
        Serial.println(F("B)"));
    } else {
        Serial.println(F("ERROR"));
    }

    Serial.print(F("Init ChordRecognizer... "));
    if (chord_rec.init()) {
        Serial.print(F("OK (arena="));
        Serial.print(chord_rec.arena_used_bytes());
        Serial.println(F("B)"));
    } else {
        Serial.println(F("ERROR"));
    }

    Serial.print(F("Init BeatFollower... "));
    if (beat_fol.init()) {
        Serial.print(F("OK (arena="));
        Serial.print(beat_fol.arena_used_bytes());
        Serial.println(F("B)"));
    } else {
        Serial.println(F("ERROR"));
    }

    // Registrar callbacks antes de init() — el MidiHost los guarda internamente
    // y los pasa a MIDIDevice al hacer init().
    midi_host.on_note_on(on_note_on);
    midi_host.on_note_off(on_note_off);
    midi_host.init();

    Serial.println(F("Listo."));
    last_inference_ms = millis();
}

void loop() {
    // Avanza la state machine USB host y despacha callbacks MIDI pendientes.
    // Non-blocking — ver comentario en midi_host.h::poll().
    midi_host.poll();

    uint32_t now = millis();
    if (now - last_inference_ms >= INFERENCE_PERIOD_MS) {
        last_inference_ms = now;
        run_inference();
    }
}

#endif // NATIVE_TEST
