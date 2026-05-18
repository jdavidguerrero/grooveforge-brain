// apps/firmware-teensy/src/sketches/21-usb-midi-host.cpp
// Sprint 4.1 — USB-A Host MIDI Input
//
// Demo: teclado MIDI USB-A conectado al Teensy 4.1.
// NoteOn/Off → engine activo. CC 74 → cutoff. CC 71 → resonance. CC 0 → engine.
//
// ┌─────────────────────────────────────────────────────────────────────────┐
// │  SELECCIONAR ENGINE — CAMBIAR ESTE NÚMERO Y REFLASHEAR:                 │
// │                                                                         │
// │    0 = Moog Model D  (monofónico, 3 VCO + ladder)                       │
// │    1 = Juno-106      (monofónico, DCO + chorus BBD)                     │
// │    2 = Prophet-5     (5 voces polifónicas, 2 VCO/voz + crossmod)        │
// └─────────────────────────────────────────────────────────────────────────┘

#define ACTIVE_ENGINE 0

// ── Por qué selección en compilación, no en runtime ──────────────────────────
// Cada engine tiene su propio AudioOutputI2S como miembro privado.
// Teensy Audio Library solo admite UNA instancia de AudioOutputI2S en el programa.
// Instanciar más de uno genera un segundo callback DMA → conflicto de hardware
// → silencio o crash en el primer AudioStream::update_all().
// El cambio de engine "en runtime" es responsabilidad del firmware de producción
// (main.cpp) con un diseño de grafo unificado. Este sketch prueba un engine a la vez.

#include <Arduino.h>
#include <Audio.h>
#include "usb/midi_host.h"

// ─── Engine seleccionado en compilación ──────────────────────────────────────

#if ACTIVE_ENGINE == 0
    #include "engines/moog_model_d.h"
    MoogModelD engine;
    static constexpr const char* ENGINE_NAME = "Moog Model D";

#elif ACTIVE_ENGINE == 1
    #include "engines/juno_106.h"
    Juno106 engine;
    static constexpr const char* ENGINE_NAME = "Juno-106";

#elif ACTIVE_ENGINE == 2
    #include "engines/prophet_5.h"
    Prophet5 engine;
    static constexpr const char* ENGINE_NAME = "Prophet-5";

#else
    #error "ACTIVE_ENGINE debe ser 0 (Moog), 1 (Juno) o 2 (Prophet)"
#endif

// ─── USB MIDI host ────────────────────────────────────────────────────────────

MidiHost midi_host;

// ─── Callbacks MIDI ──────────────────────────────────────────────────────────

static void on_note_on_cb(uint8_t channel, uint8_t note, uint8_t velocity) {
    // MIDI running status: NoteOn con velocity 0 = NoteOff (Standard MIDI 1.0 §2.1).
    // Muchos controladores lo usan para ahorrar bytes en el stream.
    if (velocity == 0) {
#if ACTIVE_ENGINE == 2
        engine.noteOff(note);
#else
        engine.noteOff();
#endif
        Serial.print(F("[MIDI] NoteOff (via v=0) ch:")); Serial.print(channel);
        Serial.print(F(" note:")); Serial.println(note);
        return;
    }

    // Velocity MIDI (0–127) → normalizada (0.0–1.0).
    float vel_norm = velocity / 127.0f;

#if ACTIVE_ENGINE == 2
    engine.noteOn(note, vel_norm);
#else
    engine.noteOn(note, vel_norm);
#endif

    Serial.print(F("[MIDI] NoteOn ch:")); Serial.print(channel);
    Serial.print(F(" note:")); Serial.print(note);
    Serial.print(F(" vel:")); Serial.println(velocity);
}

static void on_note_off_cb(uint8_t channel, uint8_t note, uint8_t velocity) {
    (void)velocity;  // velocity del NoteOff raramente se usa en síntesis convencional

#if ACTIVE_ENGINE == 2
    engine.noteOff(note);
#else
    (void)note;
    engine.noteOff();
#endif

    Serial.print(F("[MIDI] NoteOff ch:")); Serial.print(channel);
    Serial.print(F(" note:")); Serial.println(note);
}

static void on_cc_cb(uint8_t channel, uint8_t cc, uint8_t value) {
    (void)channel;

    // Mapeo lineal MIDI (0–127) → parámetro normalizado (0.0–1.0)
    float norm = value / 127.0f;

    switch (cc) {
        case 74: {
            // CC 74 — Brightness / Cutoff (GM2 §7.7 "Brightness")
            // 0→127 mapea a 20Hz–20kHz en escala logarítmica para matchear
            // la percepción auditiva del cutoff (frecuencias son log, no lin).
            // Aproximación log: f = 20 * (20000/20)^norm = 20 * 1000^norm Hz
            float hz = 20.0f * powf(1000.0f, norm);
            engine.setFilterCutoff(hz);
            Serial.print(F("[CC] Cutoff: ")); Serial.print(hz, 0); Serial.println(F(" Hz"));
            break;
        }

        case 71: {
            // CC 71 — Resonance (GM2 §7.7 "Timbre/Harmonic Intensity")
            // 0→127 mapea a Q 0.7 (Butterworth flat) → 8.0 (cerca de auto-oscilación).
            // El límite superior depende del engine — no se llega a self-oscillation
            // del ladder analógico desde aquí (ese control es el ENC L físico).
            float q = 0.7f + norm * 7.3f;
            engine.setFilterResonance(q);
            Serial.print(F("[CC] Resonance Q: ")); Serial.println(q, 2);
            break;
        }

        case 0: {
            // CC 0 — Bank Select (General MIDI §5.4)
            // Reinterpretado como selector de engine en el sketch de prueba.
            // En el firmware de producción, el cambio de engine se coordina con
            // el Bridge Protocol y requiere re-ruteo del grafo de audio.
            Serial.print(F("[CC] Bank Select (engine): "));
            switch (value) {
                case 0:  Serial.println(F("0 = Moog Model D (reflashear ACTIVE_ENGINE 0)")); break;
                case 1:  Serial.println(F("1 = Juno-106 (reflashear ACTIVE_ENGINE 1)")); break;
                case 2:  Serial.println(F("2 = Prophet-5 (reflashear ACTIVE_ENGINE 2)")); break;
                default: Serial.println(F("(valor fuera de rango, ignorado)")); break;
            }
            // No hay switch real de engine en este sketch — ver comentario en cabecera.
            break;
        }

        default:
            // CCs no mapeados: loguear sin crashear.
            Serial.print(F("[CC] ch:")); Serial.print(channel);
            Serial.print(F(" cc:")); Serial.print(cc);
            Serial.print(F(" val:")); Serial.println(value);
            break;
    }
}

static void on_pitch_bend_cb(uint8_t channel, int16_t bend) {
    // Pitch bend no se aplica al engine en este sketch de prueba —
    // los engines actuales no tienen API de bend por Sprint 4.1.
    // La implementación completa viene en Sprint 4.2 (MIDI Poly + pitch bend).
    (void)channel;
    Serial.print(F("[MIDI] PitchBend: ")); Serial.println(bend);
}

// ─── setup / loop ─────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    delay(1200);

    Serial.print(F("=== Sprint 4.1 — USB-A Host MIDI | Engine: "));
    Serial.print(ENGINE_NAME);
    Serial.println(F(" ==="));
    Serial.println(F("Enchufar teclado MIDI USB al conector USB-A del Teensy 4.1."));
    Serial.println(F("  CC 74 (0-127)  → cutoff     (20Hz-20kHz log)"));
    Serial.println(F("  CC 71 (0-127)  → resonance  (Q 0.7-8.0)"));
    Serial.println(F("  CC  0 (0/1/2)  → engine     (info only — reflashear para cambiar)"));
    Serial.println(F("  Pitch Bend     → logueado (no implementado en engines Sprint 4.1)"));

    // El engine llama AudioMemory() internamente — no llamar aquí.
    engine.begin(0.6f);

    // Registrar callbacks antes de init() para no perder eventos tempranos.
    midi_host.on_note_on(on_note_on_cb);
    midi_host.on_note_off(on_note_off_cb);
    midi_host.on_cc(on_cc_cb);
    midi_host.on_pitch_bend(on_pitch_bend_cb);

    midi_host.init();

    Serial.println(F("Engine + USB host listos. Esperando dispositivo MIDI..."));
}

void loop() {
    // poll() es non-blocking: consume todos los mensajes USB MIDI pendientes
    // y retorna. No usa delay() ni while(!available()).
    midi_host.poll();

    // update() es requerido por MoogModelD y Juno106 para los filter envelopes
    // software y el glide. Prophet5 también lo necesita (filter env + crossmod).
    engine.update();

    // Status periódico: CPU + memoria de audio cada 2 segundos.
    static uint32_t t0 = 0;
    uint32_t now = millis();
    if (now - t0 >= 2000) {
        t0 = now;
        Serial.print(F("CPU: ")); Serial.print(AudioProcessorUsage(), 1);
        Serial.print(F("% | Mem: ")); Serial.print(AudioMemoryUsage());
        Serial.print(F(" | MIDI connected: "));
        Serial.println(midi_host.is_connected() ? F("YES") : F("NO"));
    }
}
