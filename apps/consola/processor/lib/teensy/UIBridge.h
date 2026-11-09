#pragma once
#include <Arduino.h>
#include "UIPanelCommands.h"
#include "MidiCommands.h"

// === UI BRIDGE FOR TEENSY 4.1 ===
// Bridges M4 UI commands to Live Remote Script via USB MIDI

struct SessionRingInfo {
    int trackOffset;
    int sceneOffset;
    int width;
    int height;
    int totalPads;
};

class UIBridge {
public:
    UIBridge();
    void begin();
    void update();  // Call in main loop
    
    // === UART COMMAND HANDLING (M4 → Teensy) ===
    void handleUARTCommand(uint8_t cmd, uint8_t* payload, uint8_t len);
    
    // === LIVE FEEDBACK HANDLING (Live → Teensy → M4) ===
    void handleGridUpdateFromLive(uint8_t* colorData, int dataLen);
    void handleUIStateFromLive(uint8_t panelId, bool isVisible);
    
    // === SESSION RING MANAGEMENT ===
    void shiftSessionRing(int trackDelta, int sceneDelta);
    void setSessionRingPosition(int track, int scene, int width, int height);
    void setSessionRingSize(int width, int height);
    SessionRingInfo getSessionRingInfo();
    
    // === LIVE COMMUNICATION ===
    void sendSysExToLive(uint8_t cmd, uint8_t* payload, uint8_t len);
    void requestGridUpdateFromLive();
    void updateSessionRingInLive();
    void establishSessionRing();
    
    // === LIVE SYSEX PROCESSING ===
    void processLiveSysEx(uint8_t cmd, const uint8_t* payload, uint8_t len);

private:
    // === SESSION RING STATE ===
    int sessionRingTrack;   // Current track offset (0-based)
    int sessionRingScene;   // Current scene offset (0-based)
    int sessionRingWidth;   // Ring width (4 for 8x4 grid)
    int sessionRingHeight;  // Ring height (8 for 8x4 grid)
    
    // === UPDATE MANAGEMENT ===
    bool gridUpdatePending;
    unsigned long lastGridUpdateTime;
    unsigned long lastSessionRingCheck;
    void sendSessionRingPosition();
    void sendSessionRingToGui();
    
    // === COMMAND HANDLERS (M4 → Live) ===
    void handleBrowserCommand(uint8_t* payload, uint8_t len);
    void handleDevicePanelCommand(uint8_t* payload, uint8_t len);
    void handleClipPanelCommand(uint8_t* payload, uint8_t len);
    void handleHotswapCommand(uint8_t* payload, uint8_t len);
    void handleCreateTrackCommand(uint8_t* payload, uint8_t len);
    void handleCreateSceneCommand(uint8_t* payload, uint8_t len);
    
    void handleClipLaunchCommand(uint8_t* payload, uint8_t len);
    void handleClipStopCommand(uint8_t* payload, uint8_t len);
    void handleSceneLaunchCommand(uint8_t* payload, uint8_t len);
    void handleTrackStopCommand(uint8_t* payload, uint8_t len);
    
    void handleRingPositionCommand(uint8_t* payload, uint8_t len);
    void handleRingGridRequestCommand(uint8_t* payload, uint8_t len);
    
    // === M4 COMMUNICATION ===
    void sendGridUpdateToM4(uint8_t* colorData, int dataLen);
    void sendUIStateToM4(uint8_t panelId, bool state);
};
