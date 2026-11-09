#include <Arduino.h>
#include "NeoTrellisController/NeoTrellisController.h"
#include "UartInterface/UartInterface.h"

NeoTrellisController controller;
UartInterface uartInterface;

void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println("=== Push Clone - NeoTrellis M4 ===");
    Serial.println("Initializing controller and UART interface...");

    controller.begin();
    uartInterface.begin();

    // Allow standalone keypad testing until Teensy enables scanning
    controller.enableKeyScanning();

    Serial.println("NeoTrellis M4 ready. Waiting for Teensy handshake...");
}

void loop() {
    controller.read();
    uartInterface.read();
    delay(1);
}

