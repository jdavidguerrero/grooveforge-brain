#pragma once

#include <Arduino.h>
#include "MidiCommands.h"
#include "shared/BinaryProtocol.h"

// Lightweight bridge that mirrors Ableton/NeoTrellis state to a USB Serial GUI.
// The on-board firmware compiles even if no desktop GUI is connected; the class
// simply prints concise messages that higher level tools can parse.
class GUIInterface {
public:
    void begin(Stream& serialPort = Serial);
    void update();

    void sendGridColors7bit(const uint8_t* data, int length);
    void sendGridColors14bit(const uint8_t* data, int length);
    void sendPadColor14bit(int padIndex,
                           uint8_t rMsb, uint8_t rLsb,
                           uint8_t gMsb, uint8_t gLsb,
                           uint8_t bMsb, uint8_t bLsb);
    void sendClipState(uint8_t track, uint8_t scene, uint8_t state,
                       uint8_t rMsb, uint8_t rLsb,
                       uint8_t gMsb, uint8_t gLsb,
                       uint8_t bMsb, uint8_t bLsb);
    void sendClipName(uint8_t track, uint8_t scene, const char* name);
    void sendTrackName(uint8_t track, const char* name);
    void sendTrackColor(uint8_t track, uint8_t r, uint8_t g, uint8_t b);
    void sendSceneName(uint8_t scene, const char* name);
    void sendSceneColor(uint8_t scene, uint8_t r, uint8_t g, uint8_t b);
    void sendSceneState(uint8_t scene, uint8_t flags);
    void sendSceneTriggered(uint8_t scene, uint8_t flag);
    void sendBPM(float bpm);
    void sendTransportState(bool isPlaying, bool isRecording);
    void sendUiState(uint8_t panelId, bool state);
    void sendShiftState(bool pressed);
    void sendSelectedTrack(uint8_t track);
    void sendMixerMode(uint8_t mode);  // Notify GUI of mixer mode change
    void sendSessionRingPosition(int trackOffset, int sceneOffset, int width, int height);

    // Mixer parameter updates
    void sendMixerVolume(uint8_t track, uint8_t msb, uint8_t lsb);
    void sendMixerPan(uint8_t track, uint8_t msb, uint8_t lsb);
    void sendMixerSend(uint8_t track, uint8_t sendIndex, uint8_t msb, uint8_t lsb);
    void sendMixerMute(uint8_t track, uint8_t state);
    void sendMixerSolo(uint8_t track, uint8_t state);
    void sendMixerArm(uint8_t track, uint8_t state);

    bool isConnected() const { return guiConnected; }

private:
    void sendTag(const char* tag);
    void printHexPreview(const uint8_t* data, int length, int maxBytes = 16);
    void processIncoming();
    void handleIncomingCommand(uint8_t cmd, uint8_t* payload, uint8_t len);
    void sendBinary(uint8_t cmd, const uint8_t* payload, uint8_t len);
    void sendHandshake();
    void sendDisconnectEvent();

    Stream* io = nullptr;
    unsigned long lastHeartbeatMs = 0;
    unsigned long lastPingMs = 0;
    unsigned long lastPongMs = 0;
    bool handshakePending = false;
    bool guiConnected = false;
    bool disconnectNotified = false;
    bool everConnected = false;
    uint8_t rxBuffer[256];
    uint16_t rxIndex = 0;
    uint16_t expectedLength = 0;
};
