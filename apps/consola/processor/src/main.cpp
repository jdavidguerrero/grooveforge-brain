#include <Arduino.h>
#include "Hardware.h"
#include "LiveController/LiveController.h"

// Declare external reference to global instances
extern LiveController liveController;


void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 5000);
    
    Serial.println("=== Push Clone - NeoTrellis 8x4 ===");
    Serial.println("Ableton API Integration Ready");

    setupHardware();
    
    // Run pad test on startup
    delay(1000);
    liveController.testAllPads();
    
    Serial.println("\n=== Ready for Ableton Live ===");
    Serial.println("Pads send SysEx commands to Ableton API");
    Serial.println("Connect to Python script for full integration");
}

void loop() {
    loopHardware();
    delay(1);
}
