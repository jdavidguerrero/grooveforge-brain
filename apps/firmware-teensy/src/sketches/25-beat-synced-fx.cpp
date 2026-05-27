// Sprint 5.4 — Beat-synced FX (Filter Pump)
// USB-A MIDI → Beat Follower → BeatSync → filter cutoff sincronizado al tempo
//
// MoogModelD encapsula su propio AudioOutputI2S — no se puede agregar un segundo
// sin violar el constraint de la Teensy Audio Library (una sola instancia global).
// La solución de este sprint es un "filter pump": el cutoff del VCF oscila en
// fase con el quarter note detectado. Musicalmente equivale a un side-chain
// compressor o un auto-wah sincronizado al ritmo.
//
// CC MIDI  93 (knob 5 MiniLab): intensidad del pump 0=off, 127=maximo
//   Mapeo: norm 0.0 → sin modulacion, norm 1.0 → ±1 octava de swing
// CC MIDI  74 (knob 1 MiniLab): cutoff base (800–8000 Hz, log scale)
// CC MIDI  71 (knob 2 MiniLab): resonancia del filtro (0.7–4.0)
//
// Cross-ref: apps/docs/sprints/28-beat-synced-fx.md
//            apps/docs/05-fx-architecture.md §2 — CPU budget

#include <Arduino.h>
#include <Audio.h>
#include <math.h>
#include "usb/midi_host.h"
#include "ml/beat_follower.h"
#include "fx/beat_sync.h"
#include "engines/moog_model_d.h"

static MidiHost     midi_host;
static BeatFollower beat_fol;
static BeatSync     beat_sync;
static MoogModelD   engine;

// Parámetros del filter pump — accedidos solo desde loop() (no desde ISR).
// cutoff_base arranca en 2kHz para que el pump sea audible desde el inicio.
static float    cutoff_base_hz  = 2000.0f;
static float    pump_depth      = 0.0f;   // 0.0 = off, 1.0 = maximo

// El beat_period_ms se recalcula cada vez que llega un BPM valido.
// Valor inicial 500ms = 120 BPM para que el LFO ya tenga una frecuencia si
// el BeatFollower no ha inferido aún.
static uint32_t beat_period_ms  = 500;

// Timestamp del último onset recibido — para detectar silencio.
// Si no llega ningún NoteOn en SILENCE_TIMEOUT_MS Y no hay nota activa,
// se resetea el BeatFollower. Una nota sostenida no cuenta como silencio.
static uint32_t last_onset_ms   = 0;
static bool     note_held       = false;   // true mientras haya una nota presionada
static constexpr uint32_t SILENCE_TIMEOUT_MS = 3000;

static constexpr uint32_t INFERENCE_PERIOD_MS = 500;

// ---------------------------------------------------------------------------
// Callbacks MIDI
// ---------------------------------------------------------------------------

static void on_note_on(uint8_t ch, uint8_t note, uint8_t vel) {
    // vel == 0 en NoteOn equivale a NoteOff por spec MIDI 1.0 §2.1
    if (vel == 0) {
        engine.noteOff();
        return;
    }
    // Registrar onset para el Beat Follower — el histograma IOI se acumula aquí.
    last_onset_ms = millis();
    note_held     = true;
    beat_fol.on_onset(last_onset_ms);
    engine.noteOn(note, vel / 127.0f);
}

static void on_note_off(uint8_t ch, uint8_t note, uint8_t vel) {
    note_held = false;
    engine.noteOff();
}

static void on_cc(uint8_t ch, uint8_t cc, uint8_t val) {
    float norm = val / 127.0f;
    switch (cc) {
        case 74:
            // CC 74 (knob 1 MiniLab) → cutoff base, mapeo log (200–8000 Hz).
            // Rango reducido vs 20-20kHz: el pump necesita espacio para barrer
            // arriba y abajo — si el base está en 20Hz el pump baja por debajo
            // del rango audible y no se escucha.
            cutoff_base_hz = 200.0f * powf(40.0f, norm);   // 200Hz a 8kHz
            engine.setFilterCutoff(cutoff_base_hz);
            break;
        case 71:
            // CC 71 (Timbre) → resonancia. Q 0.7 = Butterworth, Q 8.0 = cerca de auto-oscilacion.
            engine.setFilterResonance(0.7f + norm * 7.3f);
            break;
        case 93:
            // CC 93 (knob 5 MiniLab mkII por defecto) → intensidad del pump.
            // CC 91 no está mapeado en el MiniLab por defecto — CC 93 sí lo está.
            pump_depth = norm;
            Serial.print(F("[BeatFX] Pump depth: "));
            Serial.println(pump_depth, 2);
            break;
    }
}

// ---------------------------------------------------------------------------
// Beat Follower inference
// ---------------------------------------------------------------------------

static void run_beat_inference() {
    uint32_t now = millis();

    // Timeout de silencio: resetear solo si no hay nota activa Y pasaron
    // SILENCE_TIMEOUT_MS desde el último NoteOn. Una nota sostenida no es silencio.
    if (!note_held && last_onset_ms > 0 && (now - last_onset_ms) > SILENCE_TIMEOUT_MS) {
        beat_fol.reset();
        last_onset_ms = 0;
        beat_period_ms = 500;   // volver a default 120 BPM
        Serial.println(F("[Beat] silencio — BPM reseteado"));
        return;
    }

    BeatResult br = beat_fol.infer();
    if (!br.valid) return;

    // Tempo folding: el modelo detecta IOIs entre notas consecutivas, no beats.
    // Si tocas corcheas a 90 BPM, los IOIs son 333ms → modelo ve ~180 BPM.
    // Fix heurístico: si BPM > 180, dividir a la mitad (asume notas = 2x el beat).
    // Si sigue > 180 tras dividir, dividir de nuevo (notas = 4x el beat).
    // Rango musical razonable: 60–180 BPM.
    float folded_bpm = br.bpm;
    while (folded_bpm > 180.0f) {
        folded_bpm *= 0.5f;
    }

    // Crear un BeatResult corregido para BeatSync
    BeatResult br_folded   = br;
    br_folded.bpm          = folded_bpm;
    br_folded.beat_period_ms = (uint32_t)(60000.0f / folded_bpm);

    // BeatSync usa EMA alpha=0.1 (diseñado para delays — converge en ~15s).
    // Para el filter pump queremos respuesta rápida: usamos media de 3 muestras.
    static float bpm_window[3] = { 0.0f, 0.0f, 0.0f };
    static uint8_t bpm_idx = 0;
    static uint8_t bpm_filled = 0;
    bpm_window[bpm_idx] = folded_bpm;
    bpm_idx = (bpm_idx + 1) % 3;
    if (bpm_filled < 3) bpm_filled++;

    float bpm_avg = 0.0f;
    for (uint8_t i = 0; i < bpm_filled; i++) bpm_avg += bpm_window[i];
    bpm_avg /= bpm_filled;

    beat_period_ms = (uint32_t)(60000.0f / bpm_avg);

    Serial.print(F("[Beat] "));
    Serial.print(bpm_avg, 1);
    Serial.print(F(" BPM  (raw: "));
    Serial.print(br.bpm, 0);
    Serial.print(F(")  period: "));
    Serial.print(beat_period_ms);
    Serial.println(F("ms"));
}

// ---------------------------------------------------------------------------
// Filter pump — sincronizado al quarter note
// ---------------------------------------------------------------------------
//
// La fase se calcula a partir de millis() modulo beat_period_ms.
// Este approach no usa un PLL — la fase puede driftar si hay jitter en millis()
// o si el tempo cambia lentamente. Para Sprint 5.4 es aceptable: el objetivo es
// demostrar sincronía perceptual, no sincronía de muestras.
//
// Curva: sin(π × phase) → 0 en downbeat, 1 en mitad del beat, 0 en upbeat.
// El oído percibe el "pump" como un acento en el downbeat (cutoff sube y baja
// en un beat completo), similar al efecto de un compresor side-chained al kick.
//
// Rango del sweep con pump_depth=1.0:
//   multiplier = 4^(lfo * 1.0 - 0.5) → rango [4^-0.5, 4^0.5] = [0.5, 2.0]
//   → cutoff varía entre cutoff_base/2 y cutoff_base*2 (±1 octava)
//   pump_depth=0.5 → ±0.5 octavas, pump_depth=0.0 → sin modulación.

static void update_filter_pump() {
    if (pump_depth < 0.01f || !beat_sync.has_bpm()) return;

    uint32_t now   = millis();
    // Phase [0.0, 1.0) dentro del beat actual.
    float phase    = (float)(now % beat_period_ms) / (float)beat_period_ms;

    // Seno media onda: sube en primera mitad del beat, baja en la segunda.
    float lfo       = sinf(3.14159265f * phase);

    // Mapeo exponencial: multiplier = 4^(lfo * pump_depth - pump_depth/2).
    // Centrado en 1.0 cuando lfo=0.5 (mitad del beat).
    float exponent   = lfo * pump_depth - pump_depth * 0.5f;
    float multiplier = powf(4.0f, exponent);
    float cutoff_hz  = cutoff_base_hz * multiplier;

    // Clamp al rango audible — setFilterCutoff lo acepta entre 20 y 20000 Hz.
    if (cutoff_hz < 20.0f)    cutoff_hz = 20.0f;
    if (cutoff_hz > 20000.0f) cutoff_hz = 20000.0f;

    engine.setFilterCutoff(cutoff_hz);
}

// ---------------------------------------------------------------------------
// setup / loop
// ---------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println(F("=== Sprint 5.4 - Beat-synced FX (Filter Pump) ==="));
    Serial.println(F("Toca con ritmo -> Brain detecta BPM -> filtro bombea al tempo"));
    Serial.println(F("  Knob 5 / CC 93 -> pump depth (0=off, 127=maximo efecto)"));
    Serial.println(F("  Knob 1 / CC 74 -> cutoff base (200-8000 Hz)  [arranca en 2kHz]"));
    Serial.println(F("  Knob 2 / CC 71 -> resonancia (0.7-4.0)"));
    Serial.println();

    Serial.print(F("Init BeatFollower... "));
    bool ok = beat_fol.init();
    Serial.println(ok ? F("OK") : F("ERROR"));
    if (!ok) {
        Serial.println(F("FATAL: BeatFollower no inicializo. Verificar lib/tflite-micro."));
        // El sketch sigue funcionando: engine + MIDI operan sin sync de BPM.
        // El filter pump queda inactivo (has_bpm() retorna false).
    } else {
        Serial.print(F("  Arena usada: "));
        Serial.print(beat_fol.arena_used_bytes());
        Serial.println(F(" bytes"));
    }

    engine.begin(0.6f);

    midi_host.on_note_on(on_note_on);
    midi_host.on_note_off(on_note_off);
    midi_host.on_cc(on_cc);
    midi_host.init();

    Serial.println(F("Listo. Conecta teclado MIDI y toca con ritmo constante."));
}

void loop() {
    midi_host.poll();
    engine.update();
    update_filter_pump();

    uint32_t now = millis();

    // Inferencia del Beat Follower cada 500ms.
    static uint32_t t_infer = 0;
    if (now - t_infer >= INFERENCE_PERIOD_MS) {
        t_infer = now;
        run_beat_inference();
    }

    // Status cada 5s
    static uint32_t t_status = 0;
    if (now - t_status >= 5000) {
        t_status = now;
        Serial.print(F("CPU: "));
        Serial.print(AudioProcessorUsage(), 1);
        Serial.print(F("% | BPM: "));
        if (beat_sync.has_bpm()) {
            Serial.print(beat_sync.bpm(), 1);
        } else {
            Serial.print(F("--"));
        }
        Serial.print(F(" | Cutoff base: "));
        Serial.print(cutoff_base_hz, 0);
        Serial.print(F("Hz | Pump: "));
        Serial.println(pump_depth, 2);
    }
}
