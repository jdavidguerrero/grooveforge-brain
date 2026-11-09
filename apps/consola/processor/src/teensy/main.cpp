#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include "Hardware.h"
#include "LiveController/LiveController.h"
#include "GUIInterface/GUIInterface.h"
#include "MidiCommands.h"
#include "teensy/UIBridge.h"
#include "NeoTrellisLink/NeoTrellisLink.h"
#include "shared/Config.h"

// Declare external reference to global instances
extern LiveController liveController;
extern UIBridge uiBridge;
extern NeoTrellisLink neoTrellisLink;
extern GUIInterface guiInterface;

// Simple serial command parser for navigation and clip play
static char serialLine[64];
static uint8_t serialIdx = 0;
static bool guiLinkStarted = false;

static void handleSerialCommand(char* line) {
    // Trim leading spaces
    while (*line == ' ') ++line;
    if (*line == '\0') return;

    // Tokenize
    char* cmd = strtok(line, " ");
    if (!cmd) return;

    if (strcmp(cmd, "help") == 0) {
        Serial.println("Commands:");
        Serial.println("  nav [u|d|l|r] - Navigate session ring");
        Serial.println("  u|d|l|r - Quick navigation");
        Serial.println("  play <track> <scene> - Trigger clip");
        Serial.println("  grid - Force grid refresh");
        Serial.println("  enable - Enable pad scanning manually");
        Serial.println("  status - Show system status");
        return;
    }

    if (strcmp(cmd, "enable") == 0) {
        Serial.println("Manually enabling NeoTrellis key scanning...");
        neoTrellisLink.sendCommand(CMD_ENABLE_KEYS, nullptr, 0);
        Serial.println("Key scanning command sent to M4");
        return;
    }

    if (strcmp(cmd, "status") == 0) {
        Serial.println("=== System Status ===");
        Serial.print("Live connected: ");
        Serial.println(liveController.isLiveConnected() ? "YES" : "NO");
        Serial.print("Grid seen: ");
        Serial.println(liveController.hasSeenGrid() ? "YES" : "NO");
        Serial.print("M4 connected: ");
        Serial.println(neoTrellisLink.isConnected() ? "YES" : "NO");
        Serial.print("GUI connected: ");
        Serial.println((guiLinkStarted && guiInterface.isConnected()) ? "YES" : "NO");
        Serial.print("Hardware ready: ");
        Serial.println(liveController.isHardwareReady() ? "YES" : "NO");
        return;
    }

    if (strcmp(cmd, "nav") == 0) {
        char* dir = strtok(nullptr, " ");
        if (!dir) { Serial.println("Usage: nav [u|d|l|r]"); return; }
        int dx = 0, dy = 0;
        if (dir[0] == 'u') dy = -1;
        else if (dir[0] == 'd') dy = 1;
        else if (dir[0] == 'l') dx = -1;
        else if (dir[0] == 'r') dx = 1;
        else { Serial.println("Usage: nav [u|d|l|r]"); return; }
        uiBridge.shiftSessionRing(dx, dy);
        Serial.print("NAV: "); Serial.print(dir); Serial.println(" sent");
        return;
    }

    if (strcmp(cmd, "u") == 0 || strcmp(cmd, "d") == 0 || strcmp(cmd, "l") == 0 || strcmp(cmd, "r") == 0) {
        int dx = 0, dy = 0;
        if (cmd[0] == 'u') dy = -1;
        else if (cmd[0] == 'd') dy = 1;
        else if (cmd[0] == 'l') dx = -1;
        else if (cmd[0] == 'r') dx = 1;
        uiBridge.shiftSessionRing(dx, dy);
        Serial.print("NAV: "); Serial.print(cmd); Serial.println(" sent");
        return;
    }

    if (strcmp(cmd, "play") == 0) {
        char* tStr = strtok(nullptr, " ");
        char* sStr = strtok(nullptr, " ");
        if (!tStr || !sStr) { Serial.println("Usage: play <track 0-7> <scene 0-3>"); return; }
        int track = atoi(tStr);
        int scene = atoi(sStr);
        if (track < 0 || track >= GRID_TRACKS || scene < 0 || scene >= GRID_SCENES) {
            Serial.println("Range: track 0-7, scene 0-3");
            return;
        }
        liveController.sendClipTrigger((uint8_t)track, (uint8_t)scene);
        Serial.print("PLAY: track "); Serial.print(track); Serial.print(" scene "); Serial.println(scene);
        return;
    }

    if (strcmp(cmd, "grid") == 0) {
        // No direct grid request in spec; force a refresh by nav down+up
        uint8_t down = 0x03, up = 0x02;
        liveController.sendSysExToAbleton(CMD_RING_NAVIGATE, &down, 1);
        liveController.sendSysExToAbleton(CMD_RING_NAVIGATE, &up, 1);
        Serial.println("Requested grid refresh via NAV down/up (0x68)");
        return;
    }

    if (strcmp(cmd, "pos") == 0) {
        // Absolute ring position (send 0x6A) — track/scene 14-bit, width/height, overview
        char* tStr = strtok(nullptr, " ");
        char* sStr = strtok(nullptr, " ");
        char* wStr = strtok(nullptr, " ");
        char* hStr = strtok(nullptr, " ");
        char* oStr = strtok(nullptr, " ");
        if (!tStr || !sStr) { Serial.println("Usage: pos <track> <scene> [width] [height] [overview 0|1]"); return; }
        uint16_t track = (uint16_t)atoi(tStr);
        uint16_t scene = (uint16_t)atoi(sStr);
        uint8_t width = wStr ? (uint8_t)atoi(wStr) : GRID_TRACKS;  // default to 8
        uint8_t height = hStr ? (uint8_t)atoi(hStr) : GRID_SCENES; // default to 4
        uint8_t overview = oStr ? (uint8_t)atoi(oStr) : 0;
        uint8_t payload[7] = {
            (uint8_t)((track >> 7) & 0x7F), (uint8_t)(track & 0x7F),
            (uint8_t)((scene >> 7) & 0x7F), (uint8_t)(scene & 0x7F),
            (uint8_t)(width & 0x7F), (uint8_t)(height & 0x7F), (uint8_t)(overview & 0x7F)
        };
        liveController.sendSysExToAbleton(CMD_RING_POSITION, payload, sizeof(payload));
        Serial.print("POS (0x6A) -> track:"); Serial.print(track);
        Serial.print(" scene:"); Serial.print(scene);
        Serial.print(" w:"); Serial.print(width);
        Serial.print(" h:"); Serial.print(height);
        Serial.print(" ov:"); Serial.println(overview);
        return;
    }


    Serial.println("Unknown command. Type 'help'.");
}

static void processSerialCommands() {
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n') {
            serialLine[serialIdx] = '\0';
            handleSerialCommand(serialLine);
            serialIdx = 0;
        } else if (serialIdx < sizeof(serialLine) - 1) {
            serialLine[serialIdx++] = c;
        } else {
            // overflow, reset
            serialIdx = 0;
        }
    }
}


// This function is unused and has been removed.

// MIDI Test code removed.
void setup() {
    Serial.begin(115200);
    delay(2000); // Give time for serial monitor to connect
    
    Serial.println("TEENSY STARTUP - TEST 1");
    Serial.println("TEENSY STARTUP - TEST 2");
    Serial.println("TEENSY STARTUP - TEST 3");
    Serial.println("=== Push Clone - Teensy 4.1 ===");
    Serial.println("USB MIDI to Ableton Live Integration Ready");
    Serial.println("Serial debug logging enabled (does not interfere with USB MIDI)");
    
    // UART to the NeoTrellis M4 (Serial5: TX5=20, RX5=21, carrier socket J15)
    TEENSY_M4_SERIAL.begin(UART_BAUD_RATE);
    Serial.print("Serial5 (UART to M4) initialized @ ");
    Serial.print(UART_BAUD_RATE);
    Serial.print(" bps");
    Serial.println(" (TX5=Pin 20, RX5=Pin 21)");

    delay(1000);

    setupHardware();

    // Step 1: Initialize M4 UART communication
    Serial.println("\n=== Step 1: M4 Setup ===");
    bool m4Ready = neoTrellisLink.initializeCommunication();
    if (!m4Ready) {
        Serial.println("Teensy: WARNING - M4 communication not ready. Live handshake will wait.");
    }

    if (neoTrellisLink.isConnected()) {
        Serial.println("\n=== Step 2: GUI Interface Setup ===");
        TEENSY_GUI_SERIAL.begin(GUI_BAUD_RATE);
        Serial.println("Serial1 (GUI link, carrier J6) initialized @ 115200 bps");
        guiInterface.begin(TEENSY_GUI_SERIAL);
        guiLinkStarted = true;
        Serial.println("GUI Interface ready — awaiting GUI handshake");
    } else {
        Serial.println("Teensy: Deferring GUI setup until NeoTrellis link is up");
    }
    
    Serial.println("\n=== USB MIDI Ready ===");
    Serial.println("System ready - now open Live and watch the logs...");
    Serial.println("Note: Live integration will start automatically when Live sends handshake");
}

void loop() {
    if (!guiLinkStarted && neoTrellisLink.isConnected()) {
        Serial.println("Teensy: NeoTrellis ready — starting GUI link");
        TEENSY_GUI_SERIAL.begin(GUI_BAUD_RATE);
        Serial.println("Serial1 (GUI link, carrier J6) initialized @ 115200 bps");
        guiInterface.begin(TEENSY_GUI_SERIAL);
        guiLinkStarted = true;
    }

    // PRIORITY 1: Process hardware (UART from M4) FIRST - critical for pad responsiveness
    loopHardware();  // This calls uartHandler.read() to process pad events from M4

    // PRIORITY 2: Process GUI Interface (USB Serial) - receive commands from GUI
    guiInterface.update();

    // PRIORITY 3: Process MIDI messages based on system state
    bool m4Ready = neoTrellisLink.isConnected();
    bool guiReady = guiLinkStarted && guiInterface.isConnected();

    if (liveController.isLiveConnected()) {
        // Full operation mode - process limited MIDI per loop to avoid blocking UART
        liveController.processMIDI();
    } else {
        if (m4Ready && guiReady) {
            liveController.waitForLiveHandshake();
        } else {
            static unsigned long lastGateLog = 0;
            if (millis() - lastGateLog > 2000) {
                Serial.println("Waiting for GUI/NeoTrellis before attempting Live handshake...");
                lastGateLog = millis();
            }
        }
    }

    // PRIORITY 4: Run periodic tasks
    liveController.read();
    processSerialCommands();

    // Debug: Check status less frequently
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck > 5000) {
        Serial.print("Loop: Status - Live connected: ");
        Serial.println(liveController.isLiveConnected() ? "YES" : "NO");
        lastCheck = millis();
    }

    // NO DELAY - process MIDI and UART as fast as possible
}
