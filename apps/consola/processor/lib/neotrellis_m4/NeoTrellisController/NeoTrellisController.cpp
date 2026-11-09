#include <Arduino.h>
#include <math.h>
#include "NeoTrellisController.h"
#include "shared/Config.h"
#include "LiveControllerStates.h"
#include "MidiCommands.h"
#include "UartInterface.h"

// External reference to UART interface (initialized in main.cpp)
extern UartInterface uartInterface;

// Keypad pin configuration for NeoTrellis M4
static const byte ROWS = 4;
static const byte COLS = 8;
static byte rowPins[ROWS] = {14, 15, 16, 17};  // ROW0-ROW3
static byte colPins[COLS] = {2, 3, 4, 5, 6, 7, 8, 9};  // COL0-COL7

// Keymap required by Adafruit_Keypad
static char keys[ROWS][COLS] = {
    {0, 1, 2, 3, 4, 5, 6, 7},
    {8, 9, 10, 11, 12, 13, 14, 15},
    {16, 17, 18, 19, 20, 21, 22, 23},
    {24, 25, 26, 27, 28, 29, 30, 31}
};

NeoTrellisController::NeoTrellisController()
    : pixels(NUM_KEYS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800),
      keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS) {
    keyScanningEnabled = false;
    skipFirstScan = false;
    gridInitialized = false;
    for (int i = 0; i < TOTAL_KEYS; ++i) {
        lastPadUpdateMs[i] = 0;
        clipStates[i] = CLIP_STATE_EMPTY;
    }
}

NeoTrellisController::~NeoTrellisController() {}

void NeoTrellisController::begin() {
    Serial.println("NeoTrellis M4: Initializing LED and keypad controller...");

    pixels.begin();
    pixels.setBrightness(LED_BRIGHTNESS);
    pixels.clear();
    pixels.show();
    Serial.println("NeoTrellis M4: NeoPixels initialized (32 LEDs, no DMA)");

    keypad.begin();
    Serial.println("NeoTrellis M4: Keypad initialized (8x4 matrix)");

    Serial.println("NeoTrellis M4: Ready - 32-pad controller active, waiting for enable command");
}

void NeoTrellisController::setupKeyCallbacks() {
    Serial.println("Key scanning uses polling via Adafruit_Keypad");
}

void NeoTrellisController::enableKeyScanning() {
    skipFirstScan = true;
    keyScanningEnabled = true;
    Serial.println("NeoTrellis M4: Key scanning ENABLED");
}

void NeoTrellisController::disableKeyScanning() {
    keyScanningEnabled = false;
    Serial.println("NeoTrellis M4: Key scanning DISABLED");
}

void NeoTrellisController::read() {
    checkKeys();
}

void NeoTrellisController::setPixelColor(int key, uint32_t color) {
    if (key < 0 || key >= TOTAL_KEYS) return;
    pixels.setPixelColor(key, color);
    pixels.show();
}

void NeoTrellisController::applyGridColors7bit(const uint8_t* rgb7, int length) {
    if (!rgb7 || length != 96) return;

    int pad = 0;
    for (int i = 0; i < length && pad < TOTAL_KEYS; i += 3, ++pad) {
        unsigned long now = millis();
        if (now - lastPadUpdateMs[pad] < PAD_SUPPRESS_MS) continue;
        applyPadColor7bit(pad, rgb7[i + 0], rgb7[i + 1], rgb7[i + 2], false);
    }
    pixels.show();
}

void NeoTrellisController::applyGridColors14bit(const uint8_t* rgb14, int length) {
    if (!rgb14 || length != 192) return;
    int pad = 0;
    for (int i = 0; i < length && pad < TOTAL_KEYS; i += 6, ++pad) {
        unsigned long now = millis();
        if (now - lastPadUpdateMs[pad] < PAD_SUPPRESS_MS) continue;
        uint8_t r = (uint8_t)(((rgb14[i + 0] & 0x7F) << 7) | (rgb14[i + 1] & 0x7F));
        uint8_t g = (uint8_t)(((rgb14[i + 2] & 0x7F) << 7) | (rgb14[i + 3] & 0x7F));
        uint8_t b = (uint8_t)(((rgb14[i + 4] & 0x7F) << 7) | (rgb14[i + 5] & 0x7F));
        applyPadColor8bit(pad, r, g, b, false);
    }
    pixels.show();
}

uint8_t NeoTrellisController::gamma7To8(uint8_t v7) const {
    const float gamma = 2.2f;
    float x = (float)v7 / 127.0f;
    float y = powf(x, gamma);
    int v8 = (int)roundf(y * 255.0f);
    if (v8 < 0) v8 = 0;
    if (v8 > 255) v8 = 255;
    return (uint8_t)v8;
}

uint8_t NeoTrellisController::gamma8(uint8_t v8) const {
    const float gamma = 2.2f;
    float x = (float)v8 / 255.0f;
    float y = powf(x, gamma);
    int out = (int)roundf(y * 255.0f);
    if (out < 0) out = 0;
    if (out > 255) out = 255;
    return (uint8_t)out;
}

void NeoTrellisController::applyPadColor7bit(int pad, uint8_t r7, uint8_t g7, uint8_t b7, bool pushNow) {
    if (pad < 0 || pad >= TOTAL_KEYS) return;
    const float WB_R = 1.00f, WB_G = 0.92f, WB_B = 1.00f;
    uint8_t r = gamma7To8(r7);
    uint8_t g = gamma7To8(g7);
    uint8_t b = gamma7To8(b7);
    int rW = (int)roundf(r * WB_R);
    int gW = (int)roundf(g * WB_G);
    int bW = (int)roundf(b * WB_B);
    if (rW > 255) rW = 255;
    if (gW > 255) gW = 255;
    if (bW > 255) bW = 255;
    uint32_t color = pixels.Color(rW, gW, bW);
    pixels.setPixelColor(pad, color);
    lastPadUpdateMs[pad] = millis();
    if (pushNow) {
        pixels.show();
    }
}

void NeoTrellisController::applyPadColor8bit(int pad, uint8_t r8, uint8_t g8, uint8_t b8, bool pushNow) {
    if (pad < 0 || pad >= TOTAL_KEYS) return;
    const float WB_R = 1.00f, WB_G = 0.92f, WB_B = 1.00f;
    uint8_t r = gamma8(r8);
    uint8_t g = gamma8(g8);
    uint8_t b = gamma8(b8);
    int rW = (int)roundf(r * WB_R);
    int gW = (int)roundf(g * WB_G);
    int bW = (int)roundf(b * WB_B);
    if (rW > 255) rW = 255;
    if (gW > 255) gW = 255;
    if (bW > 255) bW = 255;
    uint32_t color = pixels.Color(rW, gW, bW);
    pixels.setPixelColor(pad, color);
    lastPadUpdateMs[pad] = millis();
    if (pushNow) {
        pixels.show();
    }
}

void NeoTrellisController::checkKeys() {
    static unsigned long lastDebugMs = 0;
    static int checkCount = 0;

    if (!keyScanningEnabled) {
        return;
    }

    checkCount++;
    if (millis() - lastDebugMs > 5000) {
        Serial.print("M4 DEBUG: checkKeys() called ");
        Serial.print(checkCount);
        Serial.println(" times in 5s");
        checkCount = 0;
        lastDebugMs = millis();
    }

    keypad.tick();

    if (skipFirstScan) {
        while (keypad.available()) {
            keypad.read();
        }
        skipFirstScan = false;
        Serial.println("M4: First scan skipped, key scanning now active");
        return;
    }

    while (keypad.available()) {
        keypadEvent e = keypad.read();
        int key = (int)e.bit.KEY;
        int track = key % GRID_TRACKS;
        int scene = key / GRID_TRACKS;

        Serial.print("M4: Event - Key=");
        Serial.print(key);
        Serial.print(" Track=");
        Serial.print(track);
        Serial.print(" Scene=");
        Serial.println(scene);

        if (!gridInitialized) {
            Serial.println("M4: Grid not initialized, ignoring key event.");
            continue;
        }

        if (e.bit.EVENT == KEY_JUST_PRESSED) {
            handleKeyPress(key);
        } else if (e.bit.EVENT == KEY_JUST_RELEASED) {
            handleKeyRelease(key);
        }
    }
}

void NeoTrellisController::handleKeyPress(int key) {
    int track = key % GRID_TRACKS;
    int scene = key / GRID_TRACKS;
    Serial.printf("M4: Key press -> track %d scene %d\n", track, scene);
    sendPadEvent(CMD_CLIP_TRIGGER, track, scene);
}

void NeoTrellisController::handleKeyRelease(int key) {
    int track = key % GRID_TRACKS;
    int scene = key / GRID_TRACKS;
    Serial.printf("M4: Key release -> track %d scene %d\n", track, scene);
}

void NeoTrellisController::sendPadEvent(uint8_t command, uint8_t track, uint8_t scene) {
    uint8_t payload[2] = {track & 0x7F, scene & 0x7F};
    uartInterface.sendToTeensy(command, payload, sizeof(payload));
}

void NeoTrellisController::runDiagnostics() {
    Serial.println("M4: Running diagnostics (LED sweep)...");
    for (int i = 0; i < TOTAL_KEYS; ++i) {
        pixels.setPixelColor(i, pixels.Color(0x40, 0x20, 0x00));
        pixels.show();
        delay(30);
    }
    allOff();
}

void NeoTrellisController::testAllPads() {
    Serial.println("M4: Testing pad LEDs...");
    for (int i = 0; i < TOTAL_KEYS; ++i) {
        uint32_t color = pixels.Color(0x00, 0x40, 0x20);
        pixels.setPixelColor(i, color);
        pixels.show();
        delay(10);
    }
    allOff();
}

void NeoTrellisController::connectionAnimation() {
    Serial.println("M4: Running connection animation");
    for (int i = 0; i < TOTAL_KEYS; ++i) {
        pixels.setPixelColor(i, pixels.Color(0x00, 0x20, 0x20));
        pixels.show();
        delay(12);
    }
    allOff();
}

void NeoTrellisController::allOff() {
    pixels.clear();
    pixels.show();
}

void NeoTrellisController::updateClipState(int padIndex, int state) {
    if (padIndex < 0 || padIndex >= TOTAL_KEYS) return;
    clipStates[padIndex] = static_cast<uint8_t>(state);
}

void NeoTrellisController::setClipStateCache(int padIndex, uint8_t state) {
    if (padIndex < 0 || padIndex >= TOTAL_KEYS) return;
    clipStates[padIndex] = state;
}
