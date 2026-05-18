// Sprint 5.1 — TFLite Micro Vendoring — Sketch de verificación
// Inicializa los 3 modelos ML y ejecuta una inferencia dummy para verificar
// que el setup de TFLite Micro funciona en Teensy 4.1.
//
// Si compilas y flasheas este sketch:
//   - setup() inicializa KeyDetector, ChordRecognizer, BeatFollower
//   - Ejecuta inferencia con histograma de C mayor (input dummy)
//   - Imprime por Serial los resultados + arena_used_bytes()
//   - No genera audio — es solo test de ML pipeline

#ifndef NATIVE_TEST
#include <Arduino.h>
#include "ml/key_detector.h"
#include "ml/chord_recognizer.h"
#include "ml/beat_follower.h"

static KeyDetector      key_det;
static ChordRecognizer  chord_rec;
static BeatFollower     beat_fol;

// C major pitch class histogram (C=1.0, E=0.6, G=0.8, resto≈0)
static const float c_major_histogram[12] = {
    1.0f, 0.0f, 0.2f, 0.0f, 0.6f, 0.1f,
    0.0f, 0.8f, 0.0f, 0.2f, 0.0f, 0.1f
};

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println(F("=== Sprint 5.1 — TFLite Micro Vendoring Test ==="));

    // --- Key Detector ---
    Serial.print(F("Inicializando KeyDetector... "));
    if (!key_det.init()) {
        Serial.println(F("ERROR"));
    } else {
        Serial.println(F("OK"));
        KeyResult kr;
        if (key_det.inference(c_major_histogram, kr)) {
            Serial.print(F("  Key: ")); Serial.print(kr.name);
            Serial.print(F("  confidence: ")); Serial.println(kr.confidence, 3);
        }
        Serial.print(F("  Arena used: ")); Serial.print(key_det.arena_used_bytes());
        Serial.println(F(" bytes"));
    }

    // --- Chord Recognizer ---
    Serial.print(F("Inicializando ChordRecognizer... "));
    if (!chord_rec.init()) {
        Serial.println(F("ERROR"));
    } else {
        Serial.println(F("OK"));
        ChordResult cr;
        if (chord_rec.inference(c_major_histogram, cr)) {
            Serial.print(F("  Chord: ")); Serial.print(cr.name);
            Serial.print(F("  confidence: ")); Serial.println(cr.confidence, 3);
        }
        Serial.print(F("  Arena used: ")); Serial.print(chord_rec.arena_used_bytes());
        Serial.println(F(" bytes"));
    }

    // --- Beat Follower ---
    Serial.print(F("Inicializando BeatFollower... "));
    if (!beat_fol.init()) {
        Serial.println(F("ERROR"));
    } else {
        Serial.println(F("OK"));
        // Simular 8 onsets a 120 BPM (500ms entre corcheas)
        uint32_t t = 0;
        for (int i = 0; i < 8; i++) {
            beat_fol.on_onset(t);
            t += 500;
        }
        BeatResult br = beat_fol.infer();
        if (br.valid) {
            Serial.print(F("  BPM: ")); Serial.print(br.bpm, 1);
            Serial.print(F("  confidence: ")); Serial.println(br.confidence, 3);
        }
        Serial.print(F("  Arena used: ")); Serial.print(beat_fol.arena_used_bytes());
        Serial.println(F(" bytes"));
    }

    Serial.println(F("\n=== Setup completo ==="));
}

void loop() {
    // Nada — el test es one-shot en setup()
    delay(5000);
}
#endif // NATIVE_TEST
