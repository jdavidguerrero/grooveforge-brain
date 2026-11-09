/*
 * TEST: ENCODERS ROTATORIOS
 * =========================
 *
 * PROPÓSITO:
 * Probar 4 encoders incrementales (cuadratura) conectados a la Teensy 4.1
 *
 * HARDWARE:
 * - 4 encoders rotatorios con salida de cuadratura (señales A y B)
 * - Conexión:
 *   - Encoder 1 → Pines 2 (A), 3 (B)
 *   - Encoder 2 → Pines 4 (A), 5 (B)
 *   - Encoder 3 → Pines 6 (A), 7 (B)
 *   - Encoder 4 → Pines 10 (A), 11 (B)
 *
 * CABLEADO:
 * Cada encoder tiene típicamente 5 pines:
 *   - CLK (A): Pin A del encoder → 2/4/6/10
 *   - DT (B):  Pin B del encoder → 3/5/7/11
 *   - SW:      Botón (push) → MCP23017 (test separado)
 *   - +:       3.3V o 5V (depende del encoder)
 *   - GND:     GND
 *
 * NOTA: Los encoders típicamente tienen pull-ups internos, pero si
 * los valores son erráticos, añade resistencias pull-up de 10kΩ en A y B
 *
 * QUÉ ESPERAR:
 * - Al girar horario (CW): contador aumenta
 * - Al girar antihorario (CCW): contador disminuye
 * - Detección precisa de cada paso (detent)
 * - Sin saltos o valores erráticos
 *
 * CÓMO COMPILAR Y EJECUTAR:
 * pio run -e test_encoders_teensy -t upload && pio device monitor
 *
 * AUTOR: Push Clone Project
 */

#include <Arduino.h>
#include <Encoder.h>

// ========== CONFIGURACIÓN ==========
const int NUM_ENCODERS = 4;

// Pines de los encoders (A, B)
const int ENCODER_PINS[NUM_ENCODERS][2] = {
    {2, 3},    // Encoder 1 (carrier J5.5 / J5.7)
    {4, 5},    // Encoder 2 (J5.9 / J5.10)
    {9, 10},   // Encoder 3 (J5.13 / J5.15)
    {11, 12}   // Encoder 4 (J5.17 / J5.19)
};

// ========== OBJETOS ENCODER ==========
Encoder* encoders[NUM_ENCODERS];

// Estado previo de cada encoder
long previousPositions[NUM_ENCODERS] = {0, 0, 0, 0};
long totalIncrements[NUM_ENCODERS] = {0, 0, 0, 0};
long totalDecrements[NUM_ENCODERS] = {0, 0, 0, 0};

// Control de frecuencia de actualización
unsigned long lastUpdateTime = 0;
const unsigned long UPDATE_INTERVAL_MS = 10;  // Leer cada 10ms

// ========== FUNCIONES AUXILIARES ==========

// Convierte cambio de posición a valor MIDI relativo
int deltaToMidiRelative(long delta) {
    if (delta > 0) {
        // Clockwise: valores 1-63
        return constrain(delta, 1, 63);
    } else if (delta < 0) {
        // Counter-clockwise: valores 65-127 (64 = sin movimiento)
        return constrain(127 + delta, 65, 127);
    }
    return 64; // Sin movimiento
}

// Dibuja una barra visual para la posición
void drawPositionBar(long position, int range = 20) {
    // Normalizar posición a un rango visible
    int normalizedPos = constrain(position, -range, range);
    int barPos = map(normalizedPos, -range, range, 0, 40);

    Serial.print("[");
    for (int i = 0; i < 40; i++) {
        if (i == 20) {
            Serial.print("|");  // Centro
        } else if (i == barPos) {
            Serial.print("●");  // Posición actual
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
    Serial.println("║  TEST: ENCODERS ROTATORIOS (Teensy 4.1) ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();

    Serial.println("Configurando encoders:");
    for (int i = 0; i < NUM_ENCODERS; i++) {
        int pinA = ENCODER_PINS[i][0];
        int pinB = ENCODER_PINS[i][1];

        // Crear objeto encoder
        encoders[i] = new Encoder(pinA, pinB);

        Serial.print("  Encoder ");
        Serial.print(i + 1);
        Serial.print(" → Pines ");
        Serial.print(pinA);
        Serial.print(" (A), ");
        Serial.print(pinB);
        Serial.println(" (B)");

        // Resetear posición inicial
        encoders[i]->write(0);
        previousPositions[i] = 0;
    }

    Serial.println("\n✓ Configuración completa");
    Serial.println("─────────────────────────────────────────");
    Serial.println("Gira los encoders para ver los valores...");
    Serial.println("  CW  (horario) = incrementa");
    Serial.println("  CCW (antihorario) = decrementa");
    Serial.println("─────────────────────────────────────────\n");

    delay(1000);
}

// ========== LOOP PRINCIPAL ==========
void loop() {
    unsigned long currentTime = millis();

    if (currentTime - lastUpdateTime < UPDATE_INTERVAL_MS) {
        return;
    }
    lastUpdateTime = currentTime;

    // Leer todos los encoders
    bool anyChanged = false;

    for (int i = 0; i < NUM_ENCODERS; i++) {
        long currentPosition = encoders[i]->read();

        // Detectar cambio (con filtro anti-rebote para encoder 3)
        long delta = currentPosition - previousPositions[i];

        // Para encoder 3: ignorar cambios menores a 2 pasos (anti-rebote)
        if (i == 2 && abs(delta) < 2) {
            continue;  // Ignorar rebotes pequeños
        }

        if (currentPosition != previousPositions[i]) {
            anyChanged = true;
            previousPositions[i] = currentPosition;

            // Contar incrementos/decrementos
            if (delta > 0) {
                totalIncrements[i] += delta;
            } else {
                totalDecrements[i] += abs(delta);
            }

            // Dirección del giro
            const char* direction = (delta > 0) ? "CW " : "CCW";

            Serial.println();
            Serial.print("Encoder ");
            Serial.print(i + 1);
            Serial.print(" (");
            Serial.print(ENCODER_PINS[i][0]);
            Serial.print("-");
            Serial.print(ENCODER_PINS[i][1]);
            Serial.println("):");

            // Posición absoluta
            Serial.print("  Posición: ");
            Serial.println(currentPosition);

            // Dirección y delta
            Serial.print("  Dirección: ");
            Serial.print(direction);
            Serial.print(" (Δ: ");
            if (delta > 0) Serial.print("+");
            Serial.print(delta);
            Serial.println(")");

            // Valor MIDI relativo
            Serial.print("  MIDI REL: ");
            Serial.println(deltaToMidiRelative(delta));

            // Barra visual
            Serial.print("  ");
            drawPositionBar(currentPosition);
            Serial.println();

            // Estadísticas
            Serial.print("  Stats: ↑");
            Serial.print(totalIncrements[i]);
            Serial.print(" ↓");
            Serial.println(totalDecrements[i]);
        }
    }

    // Diagnóstico especial para Encoder 3 (sospechoso)
    static unsigned long lastDiagTime = 0;
    if (currentTime - lastDiagTime > 1000) {
        lastDiagTime = currentTime;

        // Leer estado crudo de pines del Encoder 3
        int enc3_pinA = digitalRead(ENCODER_PINS[2][0]);  // Pin 9
        int enc3_pinB = digitalRead(ENCODER_PINS[2][1]);  // Pin 10

        Serial.print("[DIAG] Encoder 3 raw: A=");
        Serial.print(enc3_pinA);
        Serial.print(" B=");
        Serial.print(enc3_pinB);
        Serial.print(" | Pos=");
        Serial.print(previousPositions[2]);
        Serial.print(" (↑");
        Serial.print(totalIncrements[2]);
        Serial.print(" ↓");
        Serial.print(totalDecrements[2]);
        Serial.println(")");
    }

    // Resumen compacto cada 3 segundos
    static unsigned long lastSummaryTime = 0;
    if (currentTime - lastSummaryTime > 3000) {
        lastSummaryTime = currentTime;

        Serial.println("\n┌────────────────────────────────────────────────┐");
        Serial.println("│           RESUMEN ENCODERS (Posición)          │");
        Serial.println("├────────────────────────────────────────────────┤");

        for (int i = 0; i < NUM_ENCODERS; i++) {
            Serial.print("│ Enc ");
            Serial.print(i + 1);
            Serial.print(": ");

            long pos = previousPositions[i];
            if (pos >= 0 && pos < 10) Serial.print("   ");
            else if (pos >= 10 && pos < 100) Serial.print("  ");
            else if (pos >= 100 && pos < 1000) Serial.print(" ");
            else if (pos < 0 && pos > -10) Serial.print("  ");
            else if (pos < 0 && pos > -100) Serial.print(" ");

            Serial.print(pos);
            Serial.print(" │ ↑");
            Serial.print(totalIncrements[i]);
            Serial.print(" ↓");
            Serial.print(totalDecrements[i]);

            Serial.print(" │ ");
            drawPositionBar(pos, 10);
            Serial.println(" │");
        }

        Serial.println("└────────────────────────────────────────────────┘\n");
    }
}
