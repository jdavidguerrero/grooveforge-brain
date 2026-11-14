#pragma once

// Separate NeoPixel and Keypad control to avoid SERCOM conflicts with UART
#include <Adafruit_NeoPixel.h>
#include <Adafruit_Keypad.h>
#include "shared/Config.h"

// NeoTrellis M4 hardware pins
#define NEOPIXEL_PIN 10  // NeoPixels are on pin 10
#define NUM_KEYS 32      // 8x4 grid = 32 keys

class NeoTrellisController {
public:
    NeoTrellisController();
    ~NeoTrellisController();

    void begin();
    void read();

    void setPixelColor(int key, uint32_t color);
    void updateClipState(int padIndex, int state);
    void setClipStateCache(int padIndex, uint8_t state);

    void runDiagnostics();
    void testAllPads();
    void connectionAnimation();
    void allOff();

    void enableKeyScanning();
    void disableKeyScanning();

    void applyGridColors7bit(const uint8_t* rgb7, int length);
    void applyPadColor7bit(int pad, uint8_t r7, uint8_t g7, uint8_t b7, bool pushNow = true);
    void applyGridColors14bit(const uint8_t* rgb14, int length);
    void applyPadColor8bit(int pad, uint8_t r8, uint8_t g8, uint8_t b8, bool pushNow = true);

    void setGridInitialized(bool v) { gridInitialized = v; }

private:
    Adafruit_NeoPixel pixels;
    Adafruit_Keypad keypad;

    bool keyScanningEnabled = false;
    bool skipFirstScan = false;
    bool gridInitialized = false;
    uint8_t clipStates[TOTAL_KEYS];

    static const unsigned long PAD_SUPPRESS_MS = 100; // Prevent flicker between updates
    unsigned long lastPadUpdateMs[TOTAL_KEYS];

    void handleKeyPress(int key);
    void handleKeyRelease(int key);
    void setupKeyCallbacks();
    void checkKeys();
    void sendPadEvent(uint8_t command, uint8_t track, uint8_t scene);

    uint8_t gamma7To8(uint8_t v7) const;
    uint8_t gamma8(uint8_t v8) const;
};

