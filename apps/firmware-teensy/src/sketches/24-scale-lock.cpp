// Sprint 5.3 — Scale Lock
// USB-A MIDI → Key Detector (cada 500ms) → Scale Lock → MoogModelD
//
// Demostración del feature "never play a wrong note":
//   - El Key Detector detecta la tonalidad de lo que tocas
//   - Scale Lock cuantiza cada nota entrante a la escala detectada
//   - MoogModelD sintetiza la nota cuantizada
//
// CC MIDI 64 (Sustain): toggle bypass del Scale Lock
//   valor  0-63  → Scale Lock ACTIVO (cuantización encendida)
//   valor 64-127 → Scale Lock BYPASS (notas sin cuantizar)
//
// CC MIDI 74 (Brightness): ajusta cutoff del filtro (20–8000 Hz, log scale)
// CC MIDI 71 (Resonance):  ajusta resonancia del filtro (0.7–4.0)
//
// Cross-ref: apps/docs/sprints/27-scale-lock.md
//            apps/docs/04-ai-architecture.md §1

#include <Arduino.h>
#include <Audio.h>
#include "usb/midi_host.h"
#include "ml/key_detector.h"
#include "ml/scale_lock.h"
#include "engines/moog_model_d.h"

static MidiHost    midi_host;
static KeyDetector key_det;
static ScaleLock   scale_lock;
static MoogModelD  engine;

// Acumulador de pitch class para Key Detector.
// Cada nota recibida incrementa su pitch class — se normaliza en inference.
static float pitch_counts[12] = {};

static uint32_t last_inference_ms = 0;
// Período de inferencia: 500ms es un balance entre reactividad y estabilidad.
// Muy corto → cambios de tonalidad ruidosos (1-2 notas no son suficiente evidencia).
// Muy largo → el Lock tarda en adaptarse cuando el usuario cambia de tonalidad.
static constexpr uint32_t INFERENCE_PERIOD_MS = 500;

// ---------------------------------------------------------------------------
// Callbacks MIDI
// ---------------------------------------------------------------------------

static void on_note_on(uint8_t ch, uint8_t note, uint8_t vel) {
    // vel == 0 en un NoteOn equivale a NoteOff por spec MIDI 1.0 §2.1
    if (vel == 0) {
        engine.noteOff();
        return;
    }

    // Acumular pitch class para el Key Detector.
    // El histograma crece con cada nota — se resetea tras cada inferencia.
    pitch_counts[note % 12] += 1.0f;

    // Cuantizar con Scale Lock antes de pasar al engine.
    uint8_t snapped = scale_lock.snap(note);
    float vel_norm  = vel / 127.0f;
    engine.noteOn(snapped, vel_norm);

    if (snapped != note) {
        Serial.print(F("[Lock] "));
        Serial.print(note);
        Serial.print(F(" -> "));
        Serial.print(snapped);
        Serial.print(F(" (en "));
        Serial.print(scale_lock.scale_name());
        Serial.println(F(")"));
    }
}

static void on_note_off(uint8_t ch, uint8_t note, uint8_t vel) {
    engine.noteOff();
}

static void on_cc(uint8_t ch, uint8_t cc, uint8_t val) {
    if (cc == 64) {
        // CC 64 (Sustain pedal) → toggle bypass del Scale Lock.
        // Convención: <64 = off/inactive (Lock activo), >=64 = on/active (bypass).
        bool bypass = (val >= 64);
        scale_lock.set_bypass(bypass);
        Serial.print(F("[Lock] Bypass: "));
        Serial.println(bypass ? F("ON (pass-through)") : F("OFF (activo)"));
    }
    else if (cc == 74) {
        // CC 74 (Brightness) → cutoff del filtro.
        // Mapeo logarítmico: val 0 → 20Hz, val 127 → 8000Hz.
        // Escala log porque el oído percibe la frecuencia en escala logarítmica.
        float hz = 20.0f * powf(400.0f, val / 127.0f);  // 20 * 400^(val/127) → 20..8000
        engine.setFilterCutoff(hz);
    }
    else if (cc == 71) {
        // CC 71 (Timbre/Harmonic Intensity) → resonancia del filtro.
        // Mapeo lineal: val 0 → Q=0.7 (Butterworth), val 127 → Q=4.0 (cerca de autooscilación).
        float q = 0.7f + (val / 127.0f) * 3.3f;
        engine.setFilterResonance(q);
    }
}

// ---------------------------------------------------------------------------
// Inferencia periódica de tonalidad
// ---------------------------------------------------------------------------

static void run_key_inference() {
    float total = 0.0f;
    for (int i = 0; i < 12; i++) total += pitch_counts[i];

    // Sin notas acumuladas → no hay información suficiente para inferir.
    if (total < 1e-6f) return;

    float histogram[12];
    for (int i = 0; i < 12; i++) histogram[i] = pitch_counts[i] / total;
    memset(pitch_counts, 0, sizeof(pitch_counts));

    KeyResult kr;
    if (key_det.inference(histogram, kr)) {
        scale_lock.update(kr, 0.75f);
        Serial.print(F("[Key] "));
        Serial.print(kr.name);
        Serial.print(F(" conf="));
        Serial.print(kr.confidence, 2);
        Serial.print(F(" -> escala: "));
        Serial.println(scale_lock.scale_name());
    }
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println(F("=== Sprint 5.3 - Scale Lock ==="));
    Serial.println(F("Toca notas -> Key Detector detecta tonalidad -> Scale Lock cuantiza"));
    Serial.println(F("CC 64 toggle bypass | CC 74 cutoff | CC 71 resonance"));
    Serial.println();

    Serial.print(F("Init KeyDetector... "));
    bool ok = key_det.init();
    Serial.println(ok ? F("OK") : F("ERROR"));
    if (!ok) {
        Serial.println(F("FATAL: KeyDetector no inicializo. Verificar lib/tflite-micro."));
        // No bloqueamos en loop infinito — el engine y midi_host siguen funcionando
        // sin Scale Lock ML (el bypass estara activo porque _has_scale = false).
    }

    engine.begin(0.6f);

    midi_host.on_note_on(on_note_on);
    midi_host.on_note_off(on_note_off);
    midi_host.on_cc(on_cc);
    midi_host.init();

    Serial.println(F("Listo. Scale Lock activo (sin escala hasta detectar tonalidad)."));
    last_inference_ms = millis();
}

void loop() {
    midi_host.poll();
    engine.update();

    uint32_t now = millis();
    if (now - last_inference_ms >= INFERENCE_PERIOD_MS) {
        last_inference_ms = now;
        run_key_inference();
    }

    // Status cada 5s — CPU + estado del Scale Lock
    static uint32_t t_status = 0;
    if (now - t_status >= 5000) {
        t_status = now;
        Serial.print(F("CPU: "));
        Serial.print(AudioProcessorUsage(), 1);
        Serial.print(F("% | Escala: "));
        Serial.print(scale_lock.scale_name());
        Serial.print(F(" | Bypass: "));
        Serial.println(scale_lock.is_bypass() ? F("ON") : F("OFF"));
    }
}
