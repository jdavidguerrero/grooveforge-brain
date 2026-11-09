#pragma once

#include "shared/Config.h"
#include <Arduino.h> // For byte type

class LiveController {
public:
    LiveController(); // Constructor
    ~LiveController(); // Destructor
    void begin();
    void read();
    
    void processMIDI(); // Process incoming MIDI for LED feedback
    void updateClipState(int padIndex, int state); // Update pad LED based on Ableton state
    
    // MIDI utility functions
    void sendClipTrigger(byte track, byte scene);
    void sendTransportCommand(byte command);
    void sendSysExToAbleton(uint8_t command, const uint8_t* data, int dataLength, bool requireLiveConnection = true);
    void sendHandshakeResponse();
    void waitForLiveHandshake(); // Wait for Live to initiate handshake
    void resendCachedNamesToGUI();
    
    // System state management
    bool isLiveConnected() { return liveConnected; }

private:
    // System state variables
    bool liveConnected = false;
    bool hardwareReady = false; // All required hardware validated
    bool gridSeen = false;      // First full grid received from Live
    unsigned long liveConnectedAt = 0;       // Timestamp when Live connected
    unsigned long gridRequestLastAttempt = 0; // Last time we requested grid
    uint8_t gridRequestRetries = 0;           // Number of grid requests sent

    // SysEx buffer for handling fragmented messages
    static const int SYSEX_BUFFER_SIZE = 64;
    uint8_t sysexBuffer[SYSEX_BUFFER_SIZE];
    int sysexBufferIndex = 0;
    bool sysexInProgress = false;
    
    // All I2C-related member variables and functions have been removed
    // to match the UART-based architecture.
    void handleKeyPress(int board, int key);
    void handleKeyRelease(int board, int key);
    int getBoardFromKey(int globalKey);
    int getLocalKeyFromGlobal(int globalKey);
    void processSysEx(byte* data, int length);
    void processHandshakeMessage(uint8_t* data, int length);
    void broadcastCachedNamesToGUI();

public:
    bool isHardwareReady() const { return hardwareReady; }
    void setHardwareReady(bool v) { hardwareReady = v; }
    void setLiveConnected(bool v) { liveConnected = v; }
    bool hasSeenGrid() const { return gridSeen; }

private:
    static constexpr uint8_t MAX_TRACK_NAME_LEN = 64;
    static constexpr uint8_t MAX_CLIP_NAME_LEN = 64;
    char trackNameCache[GRID_TRACKS][MAX_TRACK_NAME_LEN];
    bool trackNameValid[GRID_TRACKS] = {false};
    char clipNameCache[TOTAL_KEYS][MAX_CLIP_NAME_LEN];
    bool clipNameValid[TOTAL_KEYS] = {false};
};
