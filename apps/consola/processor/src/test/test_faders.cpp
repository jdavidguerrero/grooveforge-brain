/*
 * TEST: FADERS ALPS B50K
 * =====================
 *
 * PROPÓSITO:
 * Probar 4 faders analógicos ALPS B50K conectados a la Teensy 4.1
 *
 * HARDWARE:
 * - 4 faders ALPS B50K (potenciómetros lineales 50kΩ)
 * - Conexión:
 *   - Fader 1 → A0 (pin analógico 14)
 *   - Fader 2 → A1 (pin analógico 15)
 *   - Fader 3 → A2 (pin analógico 16)
 *   - Fader 4 → A3 (pin analógico 17)
 *
 * CABLEADO:
 * Cada fader tiene 3 pines:
 *   - Pin 1 (extremo): GND
 *   - Pin 2 (cursor):  A0/A1/A2/A3
 *   - Pin 3 (extremo): 3.3V
 *
 * CÓMO COMPILAR Y EJECUTAR:
 * pio run -e test_faders_teensy -t upload && pio device monitor
 *
 * AUTOR: Push Clone Project
 */

#include <Arduino.h>

// ========== CONFIGURACIÓN ==========
const int NUM_FADERS = 4;
const int FADER_PINS[NUM_FADERS] = {A0, A1, A2, A3};

// Resolución ADC: Teensy 4.1 soporta hasta 12-bit (0-4095)
const int ADC_RESOLUTION_BITS = 12;
const int ADC_MAX_VALUE = (1 << ADC_RESOLUTION_BITS) - 1;  // 4095

// Tolerancia para evitar ruido
const int FADER_TOLERANCE = 8;

// Estado anterior de cada fader
int previousValues[NUM_FADERS] = {-1, -1, -1, -1};

// Tiempo de última actualización
unsigned long lastUpdateTime = 0;
const unsigned long UPDATE_INTERVAL_MS = 50;  // 50ms = 20Hz

// ========== FUNCIONES AUXILIARES ==========

int valueToPercent(int adcValue) {
    return map(adcValue, 0, ADC_MAX_VALUE, 0, 100);
}

int valueToMidi(int adcValue) {
    return map(adcValue, 0, ADC_MAX_VALUE, 0, 127);
}

void drawBar(int adcValue, int barLength = 30) {
    int fillChars = map(adcValue, 0, ADC_MAX_VALUE, 0, barLength);
    Serial.print("[");
    for (int i = 0; i < barLength; i++) {
        if (i < fillChars) {
            Serial.print("=");
        } else {
            Serial.print(" ");
        }
    }
    Serial.print("]");
}

// ========== SETUP ==========
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    delay(1000);

    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║   TEST: FADERS ALPS B50K (Teensy 4.1)   ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();

    analogReadResolution(ADC_RESOLUTION_BITS);
    Serial.print("ADC Resolution: ");
    Serial.print(ADC_RESOLUTION_BITS);
    Serial.print(" bits (0-");
    Serial.print(ADC_MAX_VALUE);
    Serial.println(")");

    Serial.println("\nConfigurando pines de faders:");
    for (int i = 0; i < NUM_FADERS; i++) {
        pinMode(FADER_PINS[i], INPUT);
        Serial.print("  Fader ");
        Serial.print(i + 1);
        Serial.print(" → Pin A");
        Serial.print(i);
        Serial.print(" (físico ");
        Serial.print(FADER_PINS[i]);
        Serial.println(")");
    }

    Serial.println("\n✓ Configuración completa");
    Serial.println("─────────────────────────────────────────");
    Serial.println("Mueve los faders para ver los valores...");
    Serial.println("─────────────────────────────────────────\n");

    delay(1000);

    // Lectura inicial
    for (int i = 0; i < NUM_FADERS; i++) {
        previousValues[i] = analogRead(FADER_PINS[i]);
    }
}

// ========== LOOP PRINCIPAL ==========
void loop() {
    unsigned long currentTime = millis();

    if (currentTime - lastUpdateTime < UPDATE_INTERVAL_MS) {
        return;
    }
    lastUpdateTime = currentTime;

    // Leer todos los faders
    for (int i = 0; i < NUM_FADERS; i++) {
        int currentValue = analogRead(FADER_PINS[i]);

        if (abs(currentValue - previousValues[i]) > FADER_TOLERANCE) {
            previousValues[i] = currentValue;

            Serial.println();
            Serial.print("Fader ");
            Serial.print(i + 1);
            Serial.print(" (A");
            Serial.print(i);
            Serial.println("):");

            Serial.print("  RAW:  ");
            Serial.println(currentValue);

            Serial.print("  %:    ");
            Serial.print(valueToPercent(currentValue));
            Serial.println("%");

            Serial.print("  MIDI: ");
            Serial.println(valueToMidi(currentValue));

            Serial.print("  ");
            drawBar(currentValue);
            Serial.println();
        }
    }

    // Resumen cada 2 segundos
    static unsigned long lastSummaryTime = 0;
    if (currentTime - lastSummaryTime > 2000) {
        lastSummaryTime = currentTime;

        Serial.println("\n┌─────────────────────────────────────────┐");
        Serial.println("│         RESUMEN ACTUAL (MIDI)           │");
        Serial.println("├─────────────────────────────────────────┤");

        for (int i = 0; i < NUM_FADERS; i++) {
            Serial.print("│ Fader ");
            Serial.print(i + 1);
            Serial.print(": ");

            int midiVal = valueToMidi(previousValues[i]);
            if (midiVal < 10) Serial.print("  ");
            else if (midiVal < 100) Serial.print(" ");
            Serial.print(midiVal);
            Serial.print(" │ ");
            drawBar(previousValues[i], 15);
            Serial.println(" │");
        }

        Serial.println("└─────────────────────────────────────────┘\n");
    }
}
