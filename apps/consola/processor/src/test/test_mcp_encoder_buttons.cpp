/*
 * TEST: MCP23017 #1 - BOTONES DE ENCODERS
 * ========================================
 *
 * PROPÓSITO:
 * Probar el primer MCP23017 con los 4 botones integrados en los encoders
 *
 * HARDWARE:
 * - 1x MCP23017 I/O Expander @ dirección I2C 0x20
 * - 4 botones (push switches integrados en encoders)
 * - Conexión I2C:
 *   - SDA: Pin 18 (Teensy 4.1)
 *   - SCL: Pin 19 (Teensy 4.1)
 *
 * CABLEADO MCP23017:
 *   Pin 21 (GPA0) → Botón Encoder 1
 *   Pin 22 (GPA1) → Botón Encoder 2
 *   Pin 23 (GPA2) → Botón Encoder 3
 *   Pin 24 (GPA3) → Botón Encoder 4
 *   Pin 15 (A0)   → GND (dirección 0x20)
 *   Pin 16 (A1)   → GND
 *   Pin 17 (A2)   → GND
 *   Pin 18 (RESET)→ 3.3V (pull-up 10kΩ)
 *   Pin 9  (VDD)  → 3.3V
 *   Pin 10 (VSS)  → GND
 *   Pin 12 (SCL)  → Pin 19 (Teensy SCL)
 *   Pin 13 (SDA)  → Pin 18 (Teensy SDA)
 *
 * NOTA: Los botones deben conectarse entre el pin GPA y GND.
 * El MCP23017 tiene pull-ups internos habilitados.
 *
 * QUÉ ESPERAR:
 * - Al presionar cada botón: detecta PRESS
 * - Al soltar: detecta RELEASE
 * - Debouncing automático (50ms)
 * - Sin eventos fantasma
 *
 * CÓMO COMPILAR Y EJECUTAR:
 * pio run -e test_mcp_encoder_buttons_teensy -t upload && pio device monitor
 *
 * TROUBLESHOOTING:
 * - Si no detecta el MCP, verifica dirección I2C con: i2cdetect
 * - Asegúrate de tener resistencias pull-up (4.7kΩ) en SDA/SCL
 *
 * AUTOR: Push Clone Project
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>

// ========== CONFIGURACIÓN ==========
const uint8_t MCP_ADDRESS = 0x20;  // Dirección I2C del MCP23017
const int NUM_BUTTONS = 4;         // 4 botones de encoders

// Pines del MCP23017 (GPA0-GPA3)
const uint8_t BUTTON_PINS[NUM_BUTTONS] = {0, 1, 2, 3};

// ========== OBJETOS ==========
Adafruit_MCP23X17 mcp;

// Estado previo de cada botón
bool previousStates[NUM_BUTTONS] = {false, false, false, false};

// Debouncing
unsigned long lastChangeTime[NUM_BUTTONS] = {0, 0, 0, 0};
const unsigned long DEBOUNCE_DELAY = 50;  // 50ms

// Estadísticas
unsigned long pressCount[NUM_BUTTONS] = {0, 0, 0, 0};
unsigned long releaseCount[NUM_BUTTONS] = {0, 0, 0, 0};
unsigned long lastPressTime[NUM_BUTTONS] = {0, 0, 0, 0};

// ========== FUNCIONES AUXILIARES ==========

// Calcula duración de pulsación
unsigned long getPressDuration(int buttonIndex) {
    if (lastPressTime[buttonIndex] == 0) return 0;
    return millis() - lastPressTime[buttonIndex];
}

// ========== SETUP ==========
void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000);
    delay(1000);

    Serial.println();
    Serial.println("╔═══════════════════════════════════════════════╗");
    Serial.println("║  TEST: MCP23017 #1 - Botones Encoders 0x20   ║");
    Serial.println("╚═══════════════════════════════════════════════╝");
    Serial.println();

    // Inicializar I2C
    Wire.begin();
    Serial.println("I2C inicializado (SDA=18, SCL=19)");

    // Inicializar MCP23017
    Serial.print("Buscando MCP23017 @ 0x");
    Serial.print(MCP_ADDRESS, HEX);
    Serial.print("... ");

    if (!mcp.begin_I2C(MCP_ADDRESS)) {
        Serial.println("✗ FALLO");
        Serial.println();
        Serial.println("ERROR: No se detectó el MCP23017");
        Serial.println();
        Serial.println("Verifica:");
        Serial.println("  1. Conexiones I2C (SDA=18, SCL=19)");
        Serial.println("  2. Dirección I2C (pines A0-A2 a GND = 0x20)");
        Serial.println("  3. Alimentación (VDD=3.3V, VSS=GND)");
        Serial.println("  4. Pull-ups en SDA/SCL (4.7kΩ)");
        Serial.println();
        Serial.println("Usa: i2cdetect para escanear dispositivos I2C");

        while (1) {
            delay(1000);
        }
    }
    Serial.println("✓ OK");

    // Configurar pines como entrada con pull-up
    Serial.println("\nConfigurando pines de botones:");
    for (int i = 0; i < NUM_BUTTONS; i++) {
        mcp.pinMode(BUTTON_PINS[i], INPUT_PULLUP);
        Serial.print("  Botón Encoder ");
        Serial.print(i + 1);
        Serial.print(" → GPA");
        Serial.println(BUTTON_PINS[i]);

        // Leer estado inicial
        previousStates[i] = mcp.digitalRead(BUTTON_PINS[i]);
    }

    Serial.println("\n✓ Configuración completa");
    Serial.println("───────────────────────────────────────────────");
    Serial.println("Presiona los botones para ver eventos...");
    Serial.println("  PRESIONADO = GND (LOW)");
    Serial.println("  SUELTO     = 3.3V (HIGH, pull-up)");
    Serial.println("───────────────────────────────────────────────\n");

    delay(1000);
}

// ========== LOOP PRINCIPAL ==========
void loop() {
    unsigned long currentTime = millis();

    // Leer todos los botones
    for (int i = 0; i < NUM_BUTTONS; i++) {
        // Leer estado actual (LOW = presionado, HIGH = suelto con pull-up)
        bool currentState = mcp.digitalRead(BUTTON_PINS[i]);

        // Detectar cambio
        if (currentState != previousStates[i]) {
            // Debouncing: esperar 50ms desde último cambio
            if ((currentTime - lastChangeTime[i]) > DEBOUNCE_DELAY) {
                lastChangeTime[i] = currentTime;
                previousStates[i] = currentState;

                // LOW = Botón presionado (conectado a GND)
                // HIGH = Botón suelto (pull-up a 3.3V)
                bool isPressed = !currentState;  // Invertir lógica

                if (isPressed) {
                    // PRESS EVENT
                    pressCount[i]++;
                    lastPressTime[i] = currentTime;

                    Serial.println();
                    Serial.print("┌─ Botón Encoder ");
                    Serial.print(i + 1);
                    Serial.println(" ─────────────────┐");
                    Serial.print("│ ★ PRESIONADO (GPA");
                    Serial.print(BUTTON_PINS[i]);
                    Serial.println(")          │");
                    Serial.print("│ Press count: ");
                    Serial.print(pressCount[i]);
                    Serial.println("                  │");
                    Serial.println("└──────────────────────────────────┘");

                } else {
                    // RELEASE EVENT
                    releaseCount[i]++;
                    unsigned long duration = getPressDuration(i);

                    Serial.println();
                    Serial.print("┌─ Botón Encoder ");
                    Serial.print(i + 1);
                    Serial.println(" ─────────────────┐");
                    Serial.print("│ ○ SUELTO (GPA");
                    Serial.print(BUTTON_PINS[i]);
                    Serial.println(")               │");
                    Serial.print("│ Release count: ");
                    Serial.print(releaseCount[i]);
                    Serial.println("                │");
                    Serial.print("│ Duración: ");
                    Serial.print(duration);
                    Serial.println(" ms                 │");
                    Serial.println("└──────────────────────────────────┘");

                    lastPressTime[i] = 0;
                }
            }
        }
    }

    // Resumen cada 5 segundos
    static unsigned long lastSummaryTime = 0;
    if (currentTime - lastSummaryTime > 5000) {
        lastSummaryTime = currentTime;

        Serial.println("\n╔═════════════════════════════════════════════╗");
        Serial.println("║         RESUMEN BOTONES ENCODERS            ║");
        Serial.println("╠═════════════════════════════════════════════╣");

        for (int i = 0; i < NUM_BUTTONS; i++) {
            Serial.print("║ Enc ");
            Serial.print(i + 1);
            Serial.print(": ");

            // Estado actual
            bool currentState = mcp.digitalRead(BUTTON_PINS[i]);
            bool isPressed = !currentState;

            if (isPressed) {
                Serial.print("[★ PRESIONADO]");
            } else {
                Serial.print("[○ SUELTO    ]");
            }

            Serial.print(" │ ↓");
            if (pressCount[i] < 10) Serial.print(" ");
            Serial.print(pressCount[i]);
            Serial.print(" ↑");
            if (releaseCount[i] < 10) Serial.print(" ");
            Serial.print(releaseCount[i]);
            Serial.println(" ║");
        }

        Serial.println("╚═════════════════════════════════════════════╝\n");
    }

    delay(10);  // Small delay para no saturar
}
