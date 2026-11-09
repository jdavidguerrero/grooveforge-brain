#pragma once
#include <Arduino.h>
#include "UIPanelCommands.h"

// === UI PANEL HANDLER FOR NEOTRELLIS M4 ===
// Handles 8x4 grid UI integration with Live via Teensy

class UIPanelHandler {
public:
    // Operating modes
    enum UIMode {
        MODE_SESSION,    // Session View (clip launching)
        MODE_NOTE,       // Note mode (scale playing)  
        MODE_DEVICE,     // Device control mode
        MODE_BROWSER     // Browser navigation mode
    };
    
    UIPanelHandler();
    void begin();
    
    // === PAD HANDLING ===
    void handlePadPress(int padIndex, bool pressed);
    void handlePadRelease(int padIndex) { /* Handle releases if needed */ }
    
    // === UI PANEL CONTROLS ===
    void toggleBrowserPanel();
    void toggleDevicePanel(); 
    void toggleClipPanel();
    void toggleHotswapMode();
    
    // === TRACK/SCENE CREATION ===
    void sendCreateTrackCommand(uint8_t trackType);  // 0=MIDI, 1=Audio, 2=Return
    void sendCreateSceneCommand();
    void sendLoadDeviceCommand();
    
    // === GRID NAVIGATION ===
    void shiftGridLeft();
    void shiftGridRight();
    void shiftGridUp();
    void shiftGridDown();
    void bankUp();    // Jump 8 scenes up
    void bankDown();  // Jump 8 scenes down
    
    // === MODE SWITCHING ===
    void setUIMode(UIMode mode);
    UIMode getCurrentMode() { return currentMode; }
    
    // === LED FEEDBACK ===
    void updateGridFromLive(uint8_t* colorData, int dataLen);  // Receive from Teensy
    void updateStatusLEDs();
    void setCornerPadColor(int padIndex, uint32_t color);
    void flashPadColor(int padIndex, uint32_t color, int durationMs);
    
    // === STATE GETTERS ===
    bool isBrowserActive() { return browserActive; }
    bool isDevicePanelActive() { return devicePanelActive; } 
    bool isHotswapMode() { return hotswapMode; }
    int getGridTrackOffset() { return gridTrackOffset; }
    int getGridSceneOffset() { return gridSceneOffset; }

private:
    // === STATE VARIABLES ===
    UIMode currentMode;
    bool browserActive;
    bool devicePanelActive;
    bool clipPanelActive;
    bool hotswapMode;
    
    // Grid windowing (for navigation beyond 8x4)
    int gridTrackOffset;   // Current track offset (0-based)
    int gridSceneOffset;   // Current scene offset (0-based)
    
    // === INTERNAL METHODS ===
    bool checkSpecialCombinations(int padIndex);
    bool handlePadCombo(int pad1, int pad2);
    void sendClipLaunchCommand(int track, int scene);
    void sendUARTCommand(uint8_t cmd, uint8_t* payload, uint8_t len);
    void requestGridUpdate();
    
    // === COORDINATE CONVERSION (8×4 grid) ===
    inline int getTrackFromPad(int padIndex) { return padIndex % 8; }  // 0-7 tracks
    inline int getSceneFromPad(int padIndex) { return padIndex / 8; }  // 0-3 scenes  
    inline int getPadFromCoords(int track, int scene) { return (scene * 8) + track; }
    
    // === SCENE LAUNCHING ===
    void launchScene(int sceneIndex);
    void flashRowColor(int rowIndex, uint32_t color, int durationMs);
    
    // === HARDWARE ABSTRACTION ===
    // These would interface with actual NeoTrellis hardware
    void setPixelColor(int padIndex, uint32_t color);
    void showPixels();  // Update physical LEDs
};