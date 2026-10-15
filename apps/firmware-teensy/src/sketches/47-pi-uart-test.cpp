/**
 * 47-pi-uart-test.cpp — Prueba de la UART Teensy ↔ Raspberry Pi 3B de Studio (theory/02 §4.3).
 *
 * Valida el cableado y el baudrate del bridge antes de pedir la portadora: es independiente del
 * Bridge Protocol (no arma frames, solo líneas de texto).
 *
 * Qué hace:
 *   - Cada segundo manda "teensy <n>\n" por Serial1.
 *   - Cada línea que llega por Serial1 la imprime en el monitor USB y la devuelve como "eco <línea>\n",
 *     así el script de la Pi (apps/studio-head/tools/uart_test.py) mide el ida y vuelta.
 *   - Cada 5 s imprime bytes recibidos y errores de línea (líneas > 120 caracteres = ruido o baudrate mal).
 *
 * Cableado (protoboard; en la portadora es J6 → pines 1–10 del GPIO de la Pi):
 *   Teensy pin 1 (TX1) → Pi pin 10 (GPIO15, RXD)
 *   Teensy pin 0 (RX1) ← Pi pin 8  (GPIO14, TXD)
 *   GND               — Pi pin 6
 *   921600 8N1, 3.3 V en los dos lados (sin conversor). La Pi con su propia fuente.
 *
 * Cómo flashear:
 *   cd apps/firmware-teensy
 *   pio run -e sketch47 -t upload
 *   pio device monitor -b 115200
 */
#include <Arduino.h>

constexpr uint32_t UART_BAUD = 921600;
constexpr size_t MAX_LINE = 120;

char line[MAX_LINE + 1];
size_t line_len = 0;
uint32_t sent = 0, rx_bytes = 0, rx_lines = 0, line_errors = 0;
uint32_t last_send = 0, last_stats = 0;

void setup() {
    Serial.begin(115200);
    Serial1.begin(UART_BAUD);
    while (!Serial && millis() < 2000) {
    }
    Serial.println("\n=== sketch47 — UART Teensy ↔ Pi a 921600 ===");
    Serial.println("Esperando líneas de la Pi (uart_test.py)...");
}

void loop() {
    uint32_t now = millis();
    if (now - last_send >= 1000) {
        last_send = now;
        Serial1.printf("teensy %lu\n", (unsigned long)sent++);
    }

    while (Serial1.available()) {
        char c = Serial1.read();
        rx_bytes++;
        if (c == '\n') {
            line[line_len] = '\0';
            rx_lines++;
            Serial.printf("[pi] %s\n", line);
            Serial1.printf("eco %s\n", line);
            line_len = 0;
        } else if (c != '\r') {
            if (line_len < MAX_LINE) {
                line[line_len++] = c;
            } else {
                line_errors++;
                line_len = 0;
            }
        }
    }

    if (now - last_stats >= 5000) {
        last_stats = now;
        Serial.printf("-- enviados %lu · recibidos %lu bytes / %lu líneas · errores %lu\n",
                      (unsigned long)sent, (unsigned long)rx_bytes, (unsigned long)rx_lines,
                      (unsigned long)line_errors);
    }
}
