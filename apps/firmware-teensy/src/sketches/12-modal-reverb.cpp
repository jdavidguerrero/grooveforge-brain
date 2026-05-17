// apps/firmware-teensy/src/sketches/12-modal-reverb.cpp
// Sprint 2.7 — FX Modal Reverb demo
// Theory: apps/docs/sprints/12-modal-reverb.md
//
// Chord C mayor (C4+E4+G4) como fuente de audio continua.
// ModalReverb como capa FX sobre el chord — 6 filtros bandpass en paralelo
// modelando resonancias físicas de campana, guadua, cristal y madera.
//
// Comandos Serial (115200 baud):
//   t<n>   Material    1=campana 2=guadua 3=cristal 4=madera
//   z<n>   Size        1=small 2=medium 3=large
//   e<val> Decay mult  0.2–4.0
//   f<n>   Diffusion   1–6 modos activos
//   m<val> Mix         0.0–1.0 (dry/wet)
//   p      bypass toggle

#include <Arduino.h>
#include <Audio.h>
#include "fx/modal_reverb.h"

// ── Audio graph — fuente: chord C mayor sostenido ────────────────────────────────
// Teensy Audio Library (oficial)
AudioSynthWaveform osc1;     // C4 = 261.63 Hz
AudioSynthWaveform osc2;     // E4 = 329.63 Hz
AudioSynthWaveform osc3;     // G4 = 392.00 Hz
AudioMixer4        srcMix;   // mezcla los 3 oscs → una señal mono para el FX

// Conexiones estáticas de la fuente — osc → srcMix
AudioConnection c1(osc1, 0, srcMix, 0);
AudioConnection c2(osc2, 0, srcMix, 1);
AudioConnection c3(osc3, 0, srcMix, 2);

// ── FX ───────────────────────────────────────────────────────────────────────────
ModalReverb fx;   // Custom — apps/firmware-teensy/src/fx/modal_reverb.h

// ── Estado de parámetros (fuente de verdad para los setters con clamp) ───────────
// Clampear SIEMPRE aquí antes de llamar fx.set*() — los setters de la clase
// también clampean, pero la doble capa asegura que g_* refleje el valor real aplicado.
static uint8_t g_material  = 1;     // 1=campana, 2=guadua, 3=cristal, 4=madera
static uint8_t g_size      = 2;     // 1=small, 2=medium, 3=large
static float   g_decay     = 1.0f;  // 0.2–4.0
static uint8_t g_diffusion = 6;     // 1–6
static float   g_mix       = 0.6f;  // 0.0–1.0
static bool    g_bypass    = false;

// ── Helpers de nombre ────────────────────────────────────────────────────────────
static const char* material_name(uint8_t m) {
    switch (m) {
        case 1: return "CAMPANA";
        case 2: return "GUADUA";
        case 3: return "CRISTAL";
        case 4: return "MADERA";
        default: return "?";
    }
}

static const char* size_name(uint8_t s) {
    switch (s) {
        case 1: return "SMALL";
        case 2: return "MEDIUM";
        case 3: return "LARGE";
        default: return "?";
    }
}

// ── Mapeo de uint8_t → enum ───────────────────────────────────────────────────────
static ModalReverb::Material to_material(uint8_t m) {
    switch (m) {
        case 1: return ModalReverb::Material::CAMPANA;
        case 2: return ModalReverb::Material::GUADUA;
        case 3: return ModalReverb::Material::CRISTAL;
        default: return ModalReverb::Material::MADERA;
    }
}

static ModalReverb::Size to_size(uint8_t s) {
    switch (s) {
        case 1: return ModalReverb::Size::SMALL;
        case 3: return ModalReverb::Size::LARGE;
        default: return ModalReverb::Size::MEDIUM;
    }
}

// ── setup() ──────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1200);   // tiempo para que el monitor serial conecte

    // Fuente: chord C mayor, sawtooth, amplitud 0.25 por oscilador
    // (3 oscs en paralelo → suma máxima de 0.75 antes del srcMix)
    osc1.begin(WAVEFORM_SAWTOOTH); osc1.frequency(261.63f); osc1.amplitude(0.25f);
    osc2.begin(WAVEFORM_SAWTOOTH); osc2.frequency(329.63f); osc2.amplitude(0.25f);
    osc3.begin(WAVEFORM_SAWTOOTH); osc3.frequency(392.00f); osc3.amplitude(0.25f);

    // srcMix: 3 entradas con ganancia 1/3 cada una para evitar clipping en la suma
    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);
    srcMix.gain(3, 0.0f);

    // begin() crea las AudioConnections dinámicas desde srcMix hacia los 6 modos + dry,
    // inicializa el codec SGTL5000 y llama AudioMemory(30).
    fx.begin(srcMix, 0.7f);

    // Estado inicial: campana, medium, todos los modos, mix=0.6
    fx.setMaterial(to_material(g_material));
    fx.setSize(to_size(g_size));
    fx.setDecay(g_decay);
    fx.setDiffusion(g_diffusion);
    fx.setMix(g_mix);
    fx.setBypass(g_bypass);

    Serial.println(F("=== Sprint 2.7 --- FX Modal Reverb ==="));
    Serial.println(F("Modos resonantes fisicos: campana, guadua, cristal, madera"));
    Serial.println(F("Marketing hook: \"Reverb de guadua colombiana --- unico en el mundo\""));
    Serial.println();
    Serial.println(F("  t<n>   Material   1=campana 2=guadua 3=cristal 4=madera"));
    Serial.println(F("  z<n>   Size       1=small 2=medium 3=large"));
    Serial.println(F("  e<val> Decay mult (0.2-4.0)"));
    Serial.println(F("  f<n>   Diffusion modos activos (1-6)"));
    Serial.println(F("  m<val> Mix (0.0-1.0)"));
    Serial.println(F("  p      bypass toggle"));
    Serial.println();
    Serial.println(F("Demo:"));
    Serial.println(F("  t1  campana (largo, metalico)"));
    Serial.println(F("  t2  guadua (organico, colombiano) <- el diferenciador"));
    Serial.println(F("  t3  cristal (muy largo, brillante)"));
    Serial.println(F("  t4  madera (corto, percusivo)"));
    Serial.println(F("  f1  solo modo 1 (una resonancia)"));
    Serial.println(F("  z3  large (decays 2.5x)"));
    Serial.println(F("  e0.2 decay muy corto"));
    Serial.println();
    Serial.println(F("Defaults: t1 z2 e1.0 f6 m0.6 bypass=OFF"));
    Serial.println();
}

// ── loop() ───────────────────────────────────────────────────────────────────────
void loop() {
    // Actualizar decay envelopes de los modos modales
    fx.update();

    // ── Parser de comandos Serial ─────────────────────────────────────────────────
    if (Serial.available()) {
        char cmd = Serial.read();

        if (cmd == 't') {
            // Material: clamp [1, 4]
            uint8_t val = (uint8_t)Serial.parseInt();
            if (val < 1) val = 1;
            if (val > 4) val = 4;
            g_material = val;
            fx.setMaterial(to_material(g_material));
            Serial.print(F("Material: "));
            Serial.println(material_name(g_material));
        }
        else if (cmd == 'z') {
            // Size: clamp [1, 3]
            uint8_t val = (uint8_t)Serial.parseInt();
            if (val < 1) val = 1;
            if (val > 3) val = 3;
            g_size = val;
            fx.setSize(to_size(g_size));
            Serial.print(F("Size: "));
            Serial.println(size_name(g_size));
        }
        else if (cmd == 'e') {
            // Decay mult: clamp [0.2, 4.0]
            float val = Serial.parseFloat();
            if (val < 0.2f) val = 0.2f;
            if (val > 4.0f) val = 4.0f;
            g_decay = val;
            fx.setDecay(g_decay);
            Serial.print(F("Decay mult: "));
            Serial.println(g_decay, 2);
        }
        else if (cmd == 'f') {
            // Diffusion: clamp [1, 6]
            uint8_t val = (uint8_t)Serial.parseInt();
            if (val < 1) val = 1;
            if (val > 6) val = 6;
            g_diffusion = val;
            fx.setDiffusion(g_diffusion);
            Serial.print(F("Diffusion: "));
            Serial.print(g_diffusion);
            Serial.println(F(" modos"));
        }
        else if (cmd == 'm') {
            // Mix: clamp [0.0, 1.0]
            float val = Serial.parseFloat();
            if (val < 0.0f) val = 0.0f;
            if (val > 1.0f) val = 1.0f;
            g_mix = val;
            fx.setMix(g_mix);
            Serial.print(F("Mix: "));
            Serial.println(g_mix, 2);
        }
        else if (cmd == 'p') {
            // Bypass toggle
            g_bypass = !g_bypass;
            fx.setBypass(g_bypass);
            Serial.print(F("Bypass: "));
            Serial.println(g_bypass ? F("ON") : F("OFF"));
        }
    }

    // ── Reporte de estado cada 1 segundo ─────────────────────────────────────────
    static uint32_t last_report = 0;
    uint32_t now = millis();
    if (now - last_report >= 1000) {
        last_report = now;
        Serial.print(F("CPU: "));
        Serial.print(AudioProcessorUsage(), 1);
        Serial.print(F("% | Mem: "));
        Serial.print(AudioMemoryUsage());
        Serial.print(F("/"));
        Serial.print(AudioMemoryUsageMax());
        Serial.print(F(" | Mat: "));
        Serial.print(material_name(g_material));
        Serial.print(F(" | Size: "));
        Serial.print(size_name(g_size));
        Serial.print(F(" | Decay: "));
        Serial.print(g_decay, 2);
        Serial.print(F("x | Diff: "));
        Serial.print(g_diffusion);
        Serial.print(F(" | Mix: "));
        Serial.print(g_mix, 2);
        Serial.print(F(" | Bypass: "));
        Serial.println(g_bypass ? F("ON") : F("OFF"));
    }
}
