// apps/firmware-teensy/src/sketches/11-sub-genesis.cpp
// Sprint 2.6 — FX Sub Genesis — Sketch de demo
//
// Chord C mayor (C4+E4+G4, sawtooth) como señal fuente.
// SubGenesis suma un sub-oscilador una o dos octavas abajo con saturación tanh,
// LP post-saturación, y blend aditivo: el chord original nunca se atenúa.
//
// Theory: apps/docs/sprints/11-sub-genesis.md
// Refs:   apps/docs/05-fx-architecture.md §1.12
//         apps/docs/01-architecture.md §3.3 (pin mapping — sin cambios)

#include <Audio.h>
#include "fx/sub_genesis.h"

// ── Audio graph fuente: acorde C mayor ────────────────────────────────────────────
// Mismo patrón establecido en sprints 2.3–2.5 (TapeSaturate, PhaseChorus, BitSculpt).
AudioSynthWaveform osc1, osc2, osc3;   // C4 = 261.63 Hz, E4 = 329.63 Hz, G4 = 392.00 Hz
AudioMixer4        srcMix;

AudioConnection c1(osc1, 0, srcMix, 0);
AudioConnection c2(osc2, 0, srcMix, 1);
AudioConnection c3(osc3, 0, srcMix, 2);

// FX — la conexión dinámica (inputStream → dryWetMix) se crea en begin()
SubGenesis fx;

// ── Estado de parámetros con defaults de demo ─────────────────────────────────────
// LECCIÓN APRENDIDA: clampear SIEMPRE en el sketch antes de asignar g_XXX.
// Los setters de SubGenesis también clampean, pero la doble protección garantiza
// que g_XXX nunca almacene un valor fuera de rango (e.g., si se reporta g_octaves).
static float   g_rootFreq = 130.81f;   // C3 — una octava debajo de C4
static uint8_t g_octaves  = 1;         // 1 o 2 octavas abajo
static float   g_subLevel = 0.8f;      // presencia del sub antes del waveshaper
static float   g_drive    = 2.0f;      // saturación tanh — genera armónicos audibles
static float   g_cutoff   = 100.0f;    // cutoff LP post-saturación
static float   g_mix      = 0.5f;      // nivel del sub sumado sobre el dry
static bool    g_bypass   = false;
static uint8_t g_waveform = 1;         // 1 = WAVEFORM_SINE, 2 = WAVEFORM_SQUARE

// ── Helpers de reporte ────────────────────────────────────────────────────────────
static void print_status() {
    float subFreq = g_rootFreq / (float)(1 << g_octaves);
    Serial.print("CPU: ");
    Serial.print(AudioProcessorUsage(), 1);
    Serial.print("%  |  Mem: ");
    Serial.print(AudioMemoryUsageMax());
    Serial.print(" bloques  |  SubFreq: ");
    Serial.print(subFreq, 1);
    Serial.print("Hz  RootFreq: ");
    Serial.print(g_rootFreq, 1);
    Serial.print("Hz  Octaves: ");
    Serial.print(g_octaves);
    Serial.print("  Level: ");
    Serial.print(g_subLevel, 2);
    Serial.print("  Drive: ");
    Serial.print(g_drive, 2);
    Serial.print("  Cutoff: ");
    Serial.print((int)g_cutoff);
    Serial.print("Hz  Mix: ");
    Serial.print(g_mix, 2);
    Serial.print("  Wave: ");
    Serial.print(g_waveform == 2 ? "SQUARE" : "SINE");
    Serial.print("  Bypass: ");
    Serial.println(g_bypass ? "ON" : "OFF");
}

static void print_help() {
    Serial.println("─────────────────────────────────────────────────────────────────");
    Serial.println(" Sub Genesis — Sprint 2.6 — comandos Serial (115200 baud)");
    Serial.println("─────────────────────────────────────────────────────────────────");
    Serial.println(" l<val>  SubLevel  0.0–1.0   ej: l0.8  (presencia del sub)");
    Serial.println(" o<n>    Octaves   1–2        ej: o1    (octavas abajo)");
    Serial.println(" d<val>  Drive     1.0–8.0    ej: d4.0  (saturación tanh)");
    Serial.println(" k<val>  Cutoff    40–200 Hz  ej: k100  (LP post-saturación)");
    Serial.println(" m<val>  Mix       0.0–1.0    ej: m0.5  (nivel sub sobre dry)");
    Serial.println(" w<n>    Waveform  1=sine 2=square     ej: w2");
    Serial.println(" p       Bypass toggle");
    Serial.println("─────────────────────────────────────────────────────────────────");
    Serial.println(" Demo sugerido:");
    Serial.println("   p → comparar sin/con sub  |  m1.0 → sub aislado");
    Serial.println("   o2 d6.0 k150 → sub profundo agresivo con armónicos altos");
    Serial.println("   w2 d3.0 → square sub — más agresivo y presente en medios");
    Serial.println("   l0.8 m0.4 → sweet spot live (dub/techno)");
    Serial.println("─────────────────────────────────────────────────────────────────");
}

// ── setup() ──────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(800);   // esperar al Serial Monitor en re-enum USB

    // Chord C mayor: C4 + E4 + G4, sawtooth, amplitude 0.4 por oscilador
    // (mismo nivel que sprints 2.3–2.5 para comparación directa)
    osc1.begin(WAVEFORM_SAWTOOTH); osc1.frequency(261.63f); osc1.amplitude(0.4f);
    osc2.begin(WAVEFORM_SAWTOOTH); osc2.frequency(329.63f); osc2.amplitude(0.4f);
    osc3.begin(WAVEFORM_SAWTOOTH); osc3.frequency(392.00f); osc3.amplitude(0.4f);

    // Mezcla del chord: 0.33 por oscilador → nivel total ~1.0 con tres fuentes
    srcMix.gain(0, 0.33f);
    srcMix.gain(1, 0.33f);
    srcMix.gain(2, 0.33f);
    srcMix.gain(3, 0.0f);

    // Inicializar SubGenesis — begin() llama AudioMemory(20)
    fx.begin(srcMix, 0.5f);

    // Aplicar defaults con CLAMP EN SKETCH antes de asignar g_XXX
    // Lección aprendida: protección doble (sketch + setter) garantiza g_XXX coherente.

    // rootFreq: C3 = 130.81 Hz (una octava debajo del acorde C4)
    {
        float v = 130.81f;
        if (v < 20.0f)   v = 20.0f;
        if (v > 1000.0f) v = 1000.0f;
        g_rootFreq = v;
        fx.setRootFreq(g_rootFreq);
    }

    // octaves = 1 (sub en C3, f_sub = 130.81 / 2^1 = 65.41 Hz para octaves=1)
    // Nota: con rootFreq=130.81 y octaves=1 → subFreq = 65.41 Hz (C2)
    {
        uint8_t v = 1;
        if (v < 1) v = 1;
        if (v > 2) v = 2;
        g_octaves = v;
        fx.setOctaves(g_octaves);
    }

    {
        float v = 0.8f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        g_subLevel = v;
        fx.setSubLevel(g_subLevel);
    }

    {
        float v = 2.0f;
        if (v < 1.0f) v = 1.0f;
        if (v > 8.0f) v = 8.0f;
        g_drive = v;
        fx.setDrive(g_drive);
    }

    {
        float v = 100.0f;
        if (v < 40.0f)  v = 40.0f;
        if (v > 200.0f) v = 200.0f;
        g_cutoff = v;
        fx.setCutoff(g_cutoff);
    }

    {
        float v = 0.5f;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        g_mix = v;
        fx.setMix(g_mix);
    }

    g_bypass = false;
    fx.setBypass(g_bypass);

    {
        uint8_t v = 1;
        if (v < 1) v = 1;
        if (v > 2) v = 2;
        g_waveform = v;
        fx.setWaveform(g_waveform == 2 ? WAVEFORM_SQUARE : WAVEFORM_SINE);
    }

    print_help();
    print_status();
}

// ── loop() ───────────────────────────────────────────────────────────────────────
// El sub-oscilador es autónomo — no hay update() que llamar.
// Solo lectura de comandos Serial y reporte periódico.
void loop() {
    // ── Lectura de comandos Serial ────────────────────────────────────────────────
    if (Serial.available() > 0) {
        char cmd = Serial.read();

        // SubLevel: l<float>  [0.0, 1.0]
        if (cmd == 'l') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f;   // clamp en sketch
            if (v > 1.0f) v = 1.0f;
            g_subLevel = v;
            fx.setSubLevel(g_subLevel);
            Serial.print("SubLevel → "); Serial.println(g_subLevel, 2);
        }

        // Octaves: o<int>  [1, 2]
        else if (cmd == 'o') {
            int v = (int)Serial.parseInt();
            if (v < 1) v = 1;   // clamp en sketch
            if (v > 2) v = 2;
            g_octaves = (uint8_t)v;
            fx.setOctaves(g_octaves);
            float subFreq = g_rootFreq / (float)(1 << g_octaves);
            Serial.print("Octaves → "); Serial.print(g_octaves);
            Serial.print("  (subFreq = "); Serial.print(subFreq, 1); Serial.println(" Hz)");
        }

        // Drive: d<float>  [1.0, 8.0]
        else if (cmd == 'd') {
            float v = Serial.parseFloat();
            if (v < 1.0f) v = 1.0f;   // clamp en sketch
            if (v > 8.0f) v = 8.0f;
            g_drive = v;
            fx.setDrive(g_drive);
            Serial.print("Drive → "); Serial.println(g_drive, 2);
        }

        // Cutoff: k<float>  [40.0, 200.0]
        else if (cmd == 'k') {
            float v = Serial.parseFloat();
            if (v < 40.0f)  v = 40.0f;    // clamp en sketch
            if (v > 200.0f) v = 200.0f;
            g_cutoff = v;
            fx.setCutoff(g_cutoff);
            Serial.print("Cutoff → "); Serial.print(g_cutoff, 1); Serial.println(" Hz");
        }

        // Mix: m<float>  [0.0, 1.0]
        else if (cmd == 'm') {
            float v = Serial.parseFloat();
            if (v < 0.0f) v = 0.0f;   // clamp en sketch
            if (v > 1.0f) v = 1.0f;
            g_mix = v;
            fx.setMix(g_mix);
            Serial.print("Mix → "); Serial.println(g_mix, 2);
        }

        // Waveform: w<int>  1=sine, 2=square
        else if (cmd == 'w') {
            int v = (int)Serial.parseInt();
            if (v < 1) v = 1;   // clamp en sketch
            if (v > 2) v = 2;
            g_waveform = (uint8_t)v;
            fx.setWaveform(g_waveform == 2 ? WAVEFORM_SQUARE : WAVEFORM_SINE);
            Serial.print("Waveform → "); Serial.println(g_waveform == 2 ? "SQUARE" : "SINE");
        }

        // Bypass: p  (toggle)
        else if (cmd == 'p') {
            g_bypass = !g_bypass;
            fx.setBypass(g_bypass);
            Serial.print("Bypass → "); Serial.println(g_bypass ? "ON" : "OFF");
        }
    }

    // ── Reporte periódico ─────────────────────────────────────────────────────────
    static uint32_t lastReport = 0;
    if (millis() - lastReport >= 1000) {
        lastReport = millis();
        print_status();
    }
}
