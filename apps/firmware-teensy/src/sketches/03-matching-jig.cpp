// Sprint 1.3 — Matching Jig: mide Vbe de 2N3904 a ~100µA via ADC.
// Theory y wiring: apps/docs/sprints/03-matching-jig.md

#include <Arduino.h>

constexpr uint8_t VBE_PIN       = A0;   // Teensy pin 14 (A0 en Teensy 4.1)
constexpr uint8_t ADC_BITS      = 12;
constexpr uint8_t ADC_AVERAGING = 32;
constexpr float   VREF          = 3.3f;
constexpr int     ADC_MAX       = (1 << ADC_BITS) - 1;  // 4095

constexpr uint8_t MAX_READINGS  = 100;
float readings[MAX_READINGS];
uint8_t reading_count = 0;

float measure_vbe() {
    long sum = 0;
    for (int i = 0; i < 5; i++) {
        sum += analogRead(VBE_PIN);
        delayMicroseconds(100);
    }
    float raw = sum / 5.0f;
    return (raw / ADC_MAX) * VREF * 1000.0f;  // mV
}

void print_session_stats() {
    if (reading_count < 2) {
        Serial.println("Necesitas al menos 2 mediciones.");
        return;
    }

    float sorted[MAX_READINGS];
    memcpy(sorted, readings, reading_count * sizeof(float));
    for (int i = 0; i < reading_count - 1; i++)
        for (int j = 0; j < reading_count - i - 1; j++)
            if (sorted[j] > sorted[j+1]) {
                float tmp = sorted[j]; sorted[j] = sorted[j+1]; sorted[j+1] = tmp;
            }

    Serial.println("\n─────────────────────────────────");
    Serial.printf("Transistores medidos: %d\n", reading_count);
    Serial.printf("Vbe min:  %.2f mV\n", sorted[0]);
    Serial.printf("Vbe max:  %.2f mV\n", sorted[reading_count - 1]);
    Serial.printf("Spread:   %.2f mV\n", sorted[reading_count - 1] - sorted[0]);

    Serial.println("\nMejores pares (ΔVbe < 2mV):");
    int pair_count = 0;
    for (int i = 0; i < reading_count - 1; i++) {
        float delta = sorted[i + 1] - sorted[i];
        if (delta < 2.0f) {
            Serial.printf("  %.2f / %.2f mV → ΔVbe = %.2f mV ✓\n",
                sorted[i], sorted[i + 1], delta);
            pair_count++;
        }
    }
    if (pair_count == 0)
        Serial.println("  Ninguno con ΔVbe < 2mV — procesá más transistores.");

    Serial.printf("\nPares válidos: %d / 8 necesarios para el ladder\n", pair_count);
    Serial.println("─────────────────────────────────");
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);

    analogReadResolution(ADC_BITS);
    analogReadAveraging(ADC_AVERAGING);

    Serial.println("═══════════════════════════════════");
    Serial.println("  Sprint 1.3 — Matching Jig 2N3904");
    Serial.println("  Target: ΔVbe < 2mV por par");
    Serial.println("═══════════════════════════════════");
    Serial.println("r = medir  |  s = resumen  |  c = limpiar");
    Serial.println("Insertar transistor y presionar 'r'.");
}

void loop() {
    if (!Serial.available()) return;

    char cmd = Serial.read();

    switch (cmd) {
        case 'r': {
            float vbe = measure_vbe();
            Serial.printf("#%02d  Vbe = %.2f mV", reading_count + 1, vbe);
            if (reading_count > 0) {
                float delta = fabsf(vbe - readings[reading_count - 1]);
                Serial.printf("  (Δ vs anterior: %.2f mV%s)",
                    delta, delta < 2.0f ? " ✓" : "");
            }
            Serial.println();
            if (reading_count < MAX_READINGS)
                readings[reading_count++] = vbe;
            break;
        }
        case 's':
            print_session_stats();
            break;
        case 'c':
            reading_count = 0;
            Serial.println("Sesión limpiada.");
            break;
    }
}
