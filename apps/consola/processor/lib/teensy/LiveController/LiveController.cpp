#include "LiveController/LiveController.h"
#include "MidiCommands.h"
#include "shared/Config.h"
#include "LiveControllerStates.h"
#include "UIPanelCommands.h"
#include "../UIBridge.h"
#include "../NeoTrellisLink/NeoTrellisLink.h"
#include "../GUIInterface/GUIInterface.h"
#include <cstring>
#include <usb_midi.h>

namespace {
bool parseLiveSysExFrame(const uint8_t* data,
                         uint16_t length,
                         uint8_t& command,
                         uint8_t& sequence,
                         uint16_t& payloadLen,
                         const uint8_t*& payload) {
    if (!data || length < 10) {
        return false;
    }
    if (data[0] != SYSEX_START || data[1] != MANUFACTURER_ID || data[2] != DEVICE_ID || data[3] != 0x7F) {
        return false;
    }
    if (data[length - 1] != SYSEX_END) {
        return false;
    }
    command = data[4] & 0x7F;
    sequence = data[5] & 0x7F;
    payloadLen = (uint16_t)(((data[6] & 0x7F) << 7) | (data[7] & 0x7F));
    const uint16_t payloadStart = 8;
    const uint16_t expectedLength = payloadStart + payloadLen + 2; // checksum + F7
    if (length != expectedLength) {
        return false;
    }
    payload = &data[payloadStart];
    const uint8_t checksum = data[payloadStart + payloadLen] & 0x7F;
    uint8_t computed = command ^ sequence;
    for (uint16_t i = 0; i < payloadLen; ++i) {
        computed ^= payload[i];
    }
    return ((computed & 0x7F) == checksum);
}
}

// Constructor
LiveController::LiveController() {
    memset(trackNameCache, 0, sizeof(trackNameCache));
    memset(trackNameValid, 0, sizeof(trackNameValid));
    memset(clipNameCache, 0, sizeof(clipNameCache));
    memset(clipNameValid, 0, sizeof(clipNameValid));
}

// Destructor - nothing to free (global lifetime on MCU)
LiveController::~LiveController() {}

// Global reference to the main LiveController instance (defined in Teensy Hardware.cpp)
extern LiveController liveController;
extern UIBridge uiBridge;
extern NeoTrellisLink neoTrellisLink;
extern class GUIInterface guiInterface;

// The I2C-based key callback has been removed.
// Key events are now handled by the M4 board and sent via UART.

void LiveController::begin() {
    // This function is now a placeholder.
    // The actual hardware (M4) is initialized via UART.
    // The I2C-based NeoTrellis initialization has been removed
    // to match the UART-based architecture.
    Serial.println("LiveController class initialized (UART mode).");
}

void LiveController::read() {
    // Watchdog: if Live connected but no grid yet, request it after a timeout
    if (liveConnected && !gridSeen) {
        unsigned long now = millis();
        if (liveConnectedAt == 0) liveConnectedAt = now;
        if ((now - liveConnectedAt) > 2000 && (now - gridRequestLastAttempt) > 2000 && gridRequestRetries < 3) {
            Serial.println("Teensy: No grid received yet — send a manual 'grid' command via Serial");
            gridRequestLastAttempt = now;
            gridRequestRetries++;
        }
    }
}

void LiveController::processMIDI() {
    // Limit MIDI processing to avoid blocking UART
    const int MAX_MIDI_PER_LOOP = 5;
    int processedCount = 0;

    while (usbMIDI.read() && processedCount < MAX_MIDI_PER_LOOP) {
        if (usbMIDI.getType() != usbMIDI.SystemExclusive) {
            continue;
        }
        processedCount++;

        uint8_t* sysexData = usbMIDI.getSysExArray();
        uint16_t length = usbMIDI.getSysExArrayLength();
        if (!sysexData || length == 0) {
            continue;
        }

        #ifdef DEBUG_LIVE_LOG
        Serial.print("Live command (");
        Serial.print(length);
        Serial.print(" bytes): ");
        for (int i = 0; i < min(length, 10); i++) {
            Serial.print(sysexData[i], HEX);
            Serial.print(" ");
        }
        if (length > 10) Serial.print("...");
        Serial.println();
        #endif

        bool isLiveFrame = (length >= 10 &&
                            sysexData[0] == SYSEX_START &&
                            sysexData[1] == MANUFACTURER_ID &&
                            sysexData[2] == DEVICE_ID &&
                            sysexData[3] == 0x7F);
        if (!isLiveFrame) {
            processSysEx(sysexData, length);
            if (neoTrellisLink.isConnected()) {
                neoTrellisLink.sendRaw(sysexData, length);
            }
            continue;
        }

        uint8_t command = 0;
        uint8_t sequence = 0;
        uint16_t payloadLen = 0;
        const uint8_t* payload = nullptr;
        if (!parseLiveSysExFrame(sysexData, length, command, sequence, payloadLen, payload)) {
            Serial.printf("Teensy: Ignoring malformed Live SysEx frame (length=%u bytes, expected format F0 7F 00 7F CMD SEQ LEN... F7)\n", length);
            Serial.print("  First 16 bytes: ");
            for (int i = 0; i < min(length, 16); i++) {
                Serial.printf("%02X ", sysexData[i]);
            }
            Serial.println();
            continue;
        }

        #ifdef DEBUG_LIVE_LOG
        Serial.print("Live SysEx CMD:0x");
        Serial.print(command, HEX);
        Serial.print(" PAYLOAD:");
        Serial.println(payloadLen);
        #endif

        // Always log bulk commands for debugging
        if (command == CMD_SESSION_RING_METADATA || command == CMD_SESSION_RING_CLIPS) {
            Serial.printf("Teensy: Received Live CMD 0x%02X (payload %u bytes)\n", command, payloadLen);
        }

        switch (command) {
            case CMD_GRID_UPDATE: {
                if (payloadLen == 96) {
                    neoTrellisLink.sendCommand(CMD_LED_GRID_UPDATE, payload, static_cast<int>(payloadLen));
                    guiInterface.sendGridColors7bit(payload, static_cast<int>(payloadLen));
                    Serial.println("Teensy: Forwarded grid bulk (96 bytes, 7-bit RGB) to M4 + GUI");
                } else if (payloadLen == 192) {
                    neoTrellisLink.sendCommand(CMD_LED_GRID_UPDATE_14, payload, static_cast<int>(payloadLen));
                    guiInterface.sendGridColors14bit(payload, static_cast<int>(payloadLen));
                    Serial.println("Teensy: Forwarded grid bulk (192 bytes, 14-bit RGB) to M4 + GUI");
                } else {
                    Serial.print("Live Grid: payload length invalid (expected 96 or 192, got ");
                    Serial.print(payloadLen);
                    Serial.println(")");
                    break;
                }

                if (!gridSeen) {
                    gridSeen = true;
                    Serial.println("Teensy: First grid seen — enabling key scanning on M4");
                    neoTrellisLink.sendCommand(CMD_ENABLE_KEYS, nullptr, 0);
                }
                
                // After a bulk update, resend names to ensure GUI is in sync
                broadcastCachedNamesToGUI();
                break;
            }



            case CMD_RING_POSITION: {
                if (payloadLen >= 7) {
                    uint16_t track = static_cast<uint16_t>(((payload[0] & 0x7F) << 7) | (payload[1] & 0x7F));
                    uint16_t scene = static_cast<uint16_t>(((payload[2] & 0x7F) << 7) | (payload[3] & 0x7F));
                    uint8_t width = payload[4] & 0x7F;
                    uint8_t height = payload[5] & 0x7F;
                    uint8_t overview = payload[6] & 0x7F;
                    Serial.printf("Ring position -> track %u scene %u w=%u h=%u ov=%u\n",
                                  track, scene, width, height, overview);
                } else {
                    Serial.println("Ring position payload too short");
                }
                uiBridge.processLiveSysEx(CMD_RING_POSITION, payload, static_cast<uint8_t>(payloadLen));
                break;
            }

            case CMD_TRACK_SELECT: {
                if (payloadLen >= 1) {
                    uint8_t idx = payload[0] & 0x7F;
                    Serial.printf("Live: Track select echo -> %u\n", idx);
                } else {
                    Serial.println("Live: Track select payload too short");
                }
                break;
            }

            case CMD_SCENE_SELECT: {
                if (payloadLen >= 1) {
                    uint8_t idx = payload[0] & 0x7F;
                    Serial.printf("Live: Scene select echo -> %u\n", idx);
                } else {
                    Serial.println("Live: Scene select payload too short");
                }
                break;
            }

            case CMD_DETAIL_CLIP: {
                if (payloadLen >= 2) {
                    uint8_t track = payload[0] & 0x7F;
                    uint8_t scene = payload[1] & 0x7F;
                    Serial.printf("Live: Detail clip focus -> track %u scene %u\n", track, scene);
                } else {
                    Serial.println("Live: Detail clip payload too short");
                }
                break;
            }

            case CMD_HANDSHAKE_REPLY: {
                Serial.println("Live: Handshake final (0x01) received.");
                break;
            }

            case CMD_GRID_SINGLE_PAD: {
                if (payloadLen < 7) {
                    Serial.println("Live: CMD_GRID_SINGLE_PAD payload too short");
                    break;
                }
                uint8_t padIndex = payload[0] & 0x7F;
                
                // Decode 14-bit color
                // auto decodeColor = [&](uint16_t offset) -> uint8_t {
                //     return (uint8_t)(((payload[offset] & 0x7F) << 7) | (payload[offset + 1] & 0x7F));
                // };
                // uint8_t r = decodeColor(1);
                // uint8_t g = decodeColor(3);
                // uint8_t b = decodeColor(5);

                if (padIndex < TOTAL_KEYS) {
                    // Serial.printf("GRID_SINGLE_PAD pad %02d RGB=%u,%u,%u\n", padIndex, r, g, b);
                    
                    // Forward to NeoTrellis
                    uint8_t m4Data[] = {
                        padIndex,
                        payload[1], payload[2],
                        payload[3], payload[4],
                        payload[5], payload[6]
                    };
                    neoTrellisLink.sendCommand(CMD_LED_PAD_UPDATE_14, m4Data, sizeof(m4Data));
                    
                    // Forward to GUI
                    guiInterface.sendPadColor14bit(padIndex, payload[1], payload[2], payload[3], payload[4], payload[5], payload[6]);
                }
                break;
            }

            case CMD_CLIP_STATE: {
                if (payloadLen < 9) {
                    Serial.println("Live: CMD_CLIP_STATE payload too short");
                    break;
                }
                uint8_t track = payload[0] & 0x7F;
                uint8_t scene = payload[1] & 0x7F;
                uint8_t state = payload[2] & 0x7F;
                auto decodeColor = [&](uint16_t offset) -> uint8_t {
                    return (uint8_t)(((payload[offset] & 0x7F) << 7) | (payload[offset + 1] & 0x7F));
                };
                uint8_t r = decodeColor(3);
                uint8_t g = decodeColor(5);
                uint8_t b = decodeColor(7);
                int padIndex = scene * GRID_TRACKS + track;
                if (padIndex < TOTAL_KEYS) {
                    Serial.printf("CLIP_STATE pad %02d (T%d,S%d) state=%u RGB=%u,%u,%u\n",
                                  padIndex, track, scene, state, r, g, b);
                    uint8_t m4Data[] = {
                        static_cast<uint8_t>(padIndex),
                        payload[3], payload[4],
                        payload[5], payload[6],
                        payload[7], payload[8]
                    };
                    neoTrellisLink.sendCommand(CMD_LED_PAD_UPDATE_14, m4Data, sizeof(m4Data));
                    // Forward full clip state to GUI (includes state + color)
                    guiInterface.sendClipState(track, scene, state, payload[3], payload[4], payload[5], payload[6], payload[7], payload[8]);
                    uint8_t statePayload[] = { static_cast<uint8_t>(padIndex), state };
                    neoTrellisLink.sendCommand(CMD_LED_CLIP_STATE, statePayload, sizeof(statePayload));
                }
                break;
            }

            case CMD_CLIP_NAME: {
                if (payloadLen < 3) {
                    Serial.println("Live: Clip name payload too short");
                    break;
                }
                uint8_t track = payload[0] & 0x7F;
                uint8_t scene = payload[1] & 0x7F;
                // Payload format: [track] [scene] [name_chars...]
                // Extract null-terminated name string
                char clipName[64] = {0};
                int nameLen = payloadLen - 2; // Subtract track and scene bytes
                if (nameLen > 0 && nameLen < 63) {
                    for (int i = 0; i < nameLen; i++) {
                        clipName[i] = static_cast<char>(payload[2 + i] & 0x7F);
                    }
                    clipName[nameLen] = '\0';
                    guiInterface.sendClipName(track, scene, clipName);
                    int padIndex = scene * GRID_TRACKS + track;
                    if (padIndex >= 0 && padIndex < TOTAL_KEYS) {
                        size_t copyLen = static_cast<size_t>(nameLen);
                        if (copyLen > MAX_CLIP_NAME_LEN - 1) {
                            copyLen = MAX_CLIP_NAME_LEN - 1;
                        }
                        memcpy(clipNameCache[padIndex], clipName, copyLen);
                        clipNameCache[padIndex][copyLen] = '\0';
                        clipNameValid[padIndex] = true;
                    }
                    Serial.printf("Clip name -> track %u scene %u: %s\n", track, scene, clipName);
                } else {
                    Serial.printf("Clip name -> track %u scene %u (len=%u)\n", track, scene, payloadLen);
                }
                break;
            }



            case CMD_CLIP_TRIGGER:
            case CMD_CLIP_STOP:
            case CMD_SCENE_FIRE:
            case CMD_TRACK_PLAYING_SLOT:
            case CMD_TRACK_FIRED_SLOT: {
                // These normally originate from hardware or are informational from Live
                // Silently ignore to prevent spam
                break;
            }

            case CMD_SCENE_NAME: {
                if (payloadLen < 2) {
                    Serial.println("Live: Scene name payload too short");
                    break;
                }
                uint8_t scene = payload[0] & 0x7F;
                char sceneName[64] = {0};
                int nameLen = payloadLen - 1;
                if (nameLen > 0 && nameLen < 63) {
                    for (int i = 0; i < nameLen; ++i) {
                        sceneName[i] = static_cast<char>(payload[1 + i] & 0x7F);
                    }
                    sceneName[nameLen] = '\0';
                    guiInterface.sendSceneName(scene, sceneName);
                    Serial.printf("Scene name -> scene %u: %s\n", scene, sceneName);
                } else {
                    Serial.printf("Scene name -> scene %u (len=%u)\n", scene, payloadLen);
                }
                break;
            }

            case CMD_SCENE_COLOR: {
                if (payloadLen < 4) {
                    Serial.println("Live: Scene color payload too short");
                    break;
                }
                uint8_t scene = payload[0] & 0x7F;
                uint8_t r = payload[1] & 0x7F;
                uint8_t g = payload[2] & 0x7F;
                uint8_t b = payload[3] & 0x7F;
                guiInterface.sendSceneColor(scene, r, g, b);
                Serial.printf("Scene color -> scene %u (%u,%u,%u)\n", scene, r, g, b);
                break;
            }

            case CMD_SCENE_STATE: {
                if (payloadLen < 2) {
                    Serial.println("Live: Scene state payload too short");
                    break;
                }
                uint8_t scene = payload[0] & 0x7F;
                uint8_t flags = payload[1] & 0x7F;
                guiInterface.sendSceneState(scene, flags);
                Serial.printf("Scene state -> scene %u flags 0x%02X\n", scene, flags);
                break;
            }

            case CMD_SCENE_IS_TRIGGERED: {
                if (payloadLen < 2) {
                    Serial.println("Live: Scene triggered payload too short");
                    break;
                }
                uint8_t scene = payload[0] & 0x7F;
                uint8_t flag = payload[1] & 0x7F;
                guiInterface.sendSceneTriggered(scene, flag);
                Serial.printf("Scene triggered -> scene %u flag %u\n", scene, flag);
                break;
            }

            case CMD_TRACK_NAME: {
                if (payloadLen < 2) {
                    Serial.println("Live: Track name payload too short");
                    break;
                }
                uint8_t track = payload[0] & 0x7F;
                // Payload format: [track] [name_chars...]
                char trackName[64] = {0};
                int nameLen = payloadLen - 1; // Subtract track byte
                if (nameLen > 0 && nameLen < 63) {
                    for (int i = 0; i < nameLen; i++) {
                        trackName[i] = static_cast<char>(payload[1 + i] & 0x7F);
                    }
                    trackName[nameLen] = '\0';
                    guiInterface.sendTrackName(track, trackName);
                    if (track < GRID_TRACKS) {
                        size_t copyLen = static_cast<size_t>(nameLen);
                        if (copyLen > MAX_TRACK_NAME_LEN - 1) {
                            copyLen = MAX_TRACK_NAME_LEN - 1;
                        }
                        memcpy(trackNameCache[track], trackName, copyLen);
                        trackNameCache[track][copyLen] = '\0';
                        trackNameValid[track] = true;
                    }
                    Serial.printf("Track name -> track %u: %s\n", track, trackName);
                } else {
                    Serial.printf("Track name -> track %u (len=%u)\n", track, payloadLen);
                }
                break;
            }

            case CMD_TRACK_VOLUME:
            case CMD_TRACK_PAN:
            {
                // Volume and Pan use 14-bit resolution (3 bytes: track, MSB, LSB)
                Serial.printf("Received CMD 0x%02X: payloadLen=%u\n", command, payloadLen);

                if (payloadLen < 3) {
                    Serial.printf("❌ Live: Volume/Pan payload too short! Got %u bytes, need 3. Payload: ",
                                 payloadLen);
                    for (uint16_t i = 0; i < payloadLen; i++) {
                        Serial.printf("0x%02X ", payload[i]);
                    }
                    Serial.println();
                    break;
                }
                uint8_t track = payload[0] & 0x7F;
                uint8_t msb = payload[1] & 0x7F;
                uint8_t lsb = payload[2] & 0x7F;

                if (command == CMD_TRACK_VOLUME) {
                    guiInterface.sendMixerVolume(track, msb, lsb);
                    uint16_t value14bit = (msb << 7) | lsb;
                    Serial.printf("✅ Mixer VOLUME -> track %u: %u (14bit) → forwarded to GUI\n", track, value14bit);
                } else {
                    guiInterface.sendMixerPan(track, msb, lsb);
                    uint16_t value14bit = (msb << 7) | lsb;
                    Serial.printf("✅ Mixer PAN -> track %u: %u (14bit) → forwarded to GUI\n", track, value14bit);
                }
                break;
            }

            case CMD_TRACK_SEND_A:
#if CMD_TRACK_SEND_A != CMD_TRACK_SEND_B
            case CMD_TRACK_SEND_B:
#endif
            {
                // Sends use 14-bit resolution (4 bytes: track, sendIndex, MSB, LSB)
                Serial.printf("Received CMD_SEND 0x%02X: payloadLen=%u\n", command, payloadLen);

                if (payloadLen < 4) {
                    Serial.printf("❌ Live: Send payload too short! Got %u bytes, need 4. Payload: ",
                                 payloadLen);
                    for (uint16_t i = 0; i < payloadLen; i++) {
                        Serial.printf("0x%02X ", payload[i]);
                    }
                    Serial.println();
                    break;
                }
                uint8_t track = payload[0] & 0x7F;
                uint8_t sendIndex = payload[1] & 0x7F;
                uint8_t msb = payload[2] & 0x7F;
                uint8_t lsb = payload[3] & 0x7F;

                guiInterface.sendMixerSend(track, sendIndex, msb, lsb);
                uint16_t value14bit = (msb << 7) | lsb;
                Serial.printf("✅ Mixer SEND -> track %u send %u: %u (14bit) → forwarded to GUI\n",
                             track, sendIndex, value14bit);
                break;
            }

            case CMD_TRACK_MUTE:
            case CMD_TRACK_SOLO:
            case CMD_TRACK_ARM:
            {
                // Mute/Solo/Arm use single state byte (2 bytes: track, state)
                if (payloadLen < 2) {
                    Serial.println("Live: Mute/Solo/Arm payload too short (need 2 bytes)");
                    break;
                }
                uint8_t track = payload[0] & 0x7F;
                uint8_t state = payload[1] & 0x7F;

                if (command == CMD_TRACK_MUTE) {
                    guiInterface.sendMixerMute(track, state);
                    Serial.printf("Mixer MUTE -> track %u: %s\n", track, state ? "ON" : "OFF");
                } else if (command == CMD_TRACK_SOLO) {
                    guiInterface.sendMixerSolo(track, state);
                    Serial.printf("Mixer SOLO -> track %u: %s\n", track, state ? "ON" : "OFF");
                } else {
                    guiInterface.sendMixerArm(track, state);
                    Serial.printf("Mixer ARM -> track %u: %s\n", track, state ? "ON" : "OFF");
                }
                break;
            }

            case CMD_TRACK_COLOR:
                if (payloadLen >= 4) {
                    uint8_t track = payload[0] & 0x7F;
                    uint8_t r = payload[1] & 0x7F;
                    uint8_t g = payload[2] & 0x7F;
                    uint8_t b = payload[3] & 0x7F;
                    guiInterface.sendTrackColor(track, r, g, b);
                    Serial.printf("Track color update track %u -> (%u,%u,%u)\n", track, r, g, b);
                } else {
                    Serial.println("Track color payload too short");
                }
                break;

            case CMD_TRACK_CROSSFADE:
                Serial.printf("Track-wide CMD 0x%02X len=%u\n", command, payloadLen);
                break;

            case CMD_SELECTED_TRACK:
            case CMD_SELECTED_SCENE: {
                uint8_t value = payloadLen ? (payload[0] & 0x7F) : 0;
                Serial.printf("Selection CMD 0x%02X -> %u\n", command, value);
                if (command == CMD_SELECTED_TRACK) {
                    guiInterface.sendSelectedTrack(value);
                    // Update selected track for encoder control
                    extern void setSelectedTrack(int trackIndex);
                    setSelectedTrack(value);
                }
                break;
            }

            case CMD_TEMPO: {
                // BPM sent as 16-bit value (MSB, LSB) in units of 0.1 BPM
                if (payloadLen >= 2) {
                    uint16_t bpm_fixed = (uint16_t)(((payload[0] & 0x7F) << 7) | (payload[1] & 0x7F));
                    float bpm = static_cast<float>(bpm_fixed) / 10.0f;
                    guiInterface.sendBPM(bpm);
                    Serial.printf("Tempo -> %.1f BPM\n", bpm);
                } else {
                    Serial.println("Tempo payload too short");
                }
                break;
            }

            case CMD_TRANSPORT_PLAY:
            case CMD_TRANSPORT_RECORD: {
                uint8_t value = payloadLen ? (payload[0] & 0x7F) : 0;
                bool isPlaying = (command == CMD_TRANSPORT_PLAY) ? (value > 0) : false;
                bool isRecording = (command == CMD_TRANSPORT_RECORD) ? (value > 0) : false;
                guiInterface.sendTransportState(isPlaying, isRecording);
                break;
            }

            // === INFORMATIONAL COMMANDS (silenced to reduce log spam and delay) ===
            case CMD_TRANSPORT_LOOP:
            case CMD_TRANSPORT_METRONOME:
            case CMD_TRANSPORT_SIGNATURE:
            case CMD_TRANSPORT_POSITION:
            case CMD_TRANSPORT_OVERDUB:  // Also known as CMD_ARRANGEMENT_RECORD
            case CMD_TRANSPORT_PUNCH:
            case CMD_RECORD_QUANTIZATION:
            case CMD_QUANTIZE_CLIP:
            case CMD_BACK_TO_ARRANGER:
            case CMD_RE_ENABLE_AUTOMATION:
            case CMD_TRANSPORT_QUANTIZE:
            case CMD_TRANSPORT:
            case CMD_STEP_SEQUENCER_STATE:
            case CMD_DEVICE_LIST:
            case CMD_DEVICE_ENABLE:
            case CMD_PARAM_VALUE:
            case CMD_CHAIN_SELECT:
            case CMD_DRUM_PAD_STATE:
            case CMD_LOOP_MARKERS:
            case CMD_CLIP_LOOP:
            case CMD_CLIP_MUTED: {
                // Silently process - these are high-frequency or informational commands
                break;
            }

            case CMD_DISCONNECT: {
                Serial.println("Live: Disconnect command received — clearing state.");
                liveConnected = false;
                liveConnectedAt = 0;
                gridSeen = false;
                gridRequestRetries = 0;
                gridRequestLastAttempt = 0;
                uint8_t clearFrame[TOTAL_KEYS * 3] = {0};
                neoTrellisLink.sendCommand(CMD_LED_GRID_UPDATE, clearFrame, sizeof(clearFrame));
                neoTrellisLink.sendCommand(CMD_DISABLE_KEYS, nullptr, 0);
                break;
            }

            case CMD_HANDSHAKE:
                Serial.println("Live: secondary handshake ping received");
                break;

            case CMD_SESSION_RING_METADATA: {
                // Bulk metadata: tracks and scenes with names and colors
                // Format: [num_tracks] [track0: len, name..., R, G, B] ... [track7: ...]
                //         [num_scenes] [scene0: len, name..., R, G, B] ... [scene3: ...]
                uint16_t offset = 0;

                if (payloadLen < 1) {
                    Serial.println("Live: Ring metadata bulk payload too short");
                    break;
                }

                // Parse tracks
                uint8_t numTracks = payload[offset++] & 0x7F;
                Serial.printf("📦 Ring metadata bulk: %u tracks\n", numTracks);

                for (uint8_t t = 0; t < numTracks && offset < payloadLen; t++) {
                    if (offset >= payloadLen) break;

                    uint8_t nameLen = payload[offset++] & 0x7F;
                    if (offset + nameLen + 3 > payloadLen) break;  // name + RGB

                    char trackName[64] = {0};
                    for (uint8_t i = 0; i < nameLen && i < 63; i++) {
                        trackName[i] = static_cast<char>(payload[offset++] & 0x7F);
                    }
                    trackName[nameLen] = '\0';

                    uint8_t r7 = payload[offset++] & 0x7F;
                    uint8_t g7 = payload[offset++] & 0x7F;
                    uint8_t b7 = payload[offset++] & 0x7F;

                    // Convert 7-bit to 8-bit
                    uint8_t r8 = r7 << 1;
                    uint8_t g8 = g7 << 1;
                    uint8_t b8 = b7 << 1;

                    guiInterface.sendTrackName(t, trackName);
                    guiInterface.sendTrackColor(t, r8, g8, b8);

                    if (t < GRID_TRACKS) {
                        size_t copyLen = strlen(trackName);
                        if (copyLen > MAX_TRACK_NAME_LEN - 1) {
                            copyLen = MAX_TRACK_NAME_LEN - 1;
                        }
                        memcpy(trackNameCache[t], trackName, copyLen);
                        trackNameCache[t][copyLen] = '\0';
                        trackNameValid[t] = true;
                    }
                }

                // Parse scenes
                if (offset < payloadLen) {
                    uint8_t numScenes = payload[offset++] & 0x7F;
                    Serial.printf("📦 Ring metadata bulk: %u scenes\n", numScenes);

                    for (uint8_t s = 0; s < numScenes && offset < payloadLen; s++) {
                        if (offset >= payloadLen) break;

                        uint8_t nameLen = payload[offset++] & 0x7F;
                        if (offset + nameLen + 3 > payloadLen) break;

                        char sceneName[64] = {0};
                        for (uint8_t i = 0; i < nameLen && i < 63; i++) {
                            sceneName[i] = static_cast<char>(payload[offset++] & 0x7F);
                        }
                        sceneName[nameLen] = '\0';

                        uint8_t r7 = payload[offset++] & 0x7F;
                        uint8_t g7 = payload[offset++] & 0x7F;
                        uint8_t b7 = payload[offset++] & 0x7F;

                        // Convert 7-bit to 8-bit
                        uint8_t r8 = r7 << 1;
                        uint8_t g8 = g7 << 1;
                        uint8_t b8 = b7 << 1;

                        guiInterface.sendSceneName(s, sceneName);
                        guiInterface.sendSceneColor(s, r8, g8, b8);
                    }
                }

                Serial.printf("✅ Processed ring metadata bulk (%u bytes)\n", payloadLen);
                break;
            }

            case CMD_SESSION_RING_CLIPS: {
                // Bulk clips: 32 clips with states and colors
                // Format: [clip0: state, R, G, B] [clip1: ...] ... [clip31: ...]
                // Order: column-major (track 0 scenes 0-3, track 1 scenes 0-3, ...)

                const uint8_t expectedBytes = 32 * 4;  // 32 clips × 4 bytes each
                if (payloadLen < expectedBytes) {
                    Serial.printf("Live: Ring clips bulk payload too short (got %u, need %u)\n",
                                 payloadLen, expectedBytes);
                    break;
                }

                Serial.printf("📦 Ring clips bulk: 32 clips\n");

                uint16_t offset = 0;
                for (uint8_t track = 0; track < GRID_TRACKS; track++) {
                    for (uint8_t scene = 0; scene < GRID_SCENES; scene++) {
                        if (offset + 3 >= payloadLen) break;

                        uint8_t state = payload[offset++] & 0x7F;
                        uint8_t r7 = payload[offset++] & 0x7F;
                        uint8_t g7 = payload[offset++] & 0x7F;
                        uint8_t b7 = payload[offset++] & 0x7F;

                        // Convert 7-bit to 8-bit for RGB
                        uint8_t r8 = r7 << 1;
                        uint8_t g8 = g7 << 1;
                        uint8_t b8 = b7 << 1;

                        // Send to NeoTrellis
                        int padIndex = scene * GRID_TRACKS + track;
                        if (padIndex >= 0 && padIndex < TOTAL_KEYS) {
                            uint8_t m4Data[] = {
                                static_cast<uint8_t>(padIndex),
                                r8, g8, b8
                            };
                            neoTrellisLink.sendCommand(CMD_LED_PAD_UPDATE, m4Data, sizeof(m4Data));

                            uint8_t statePayload[] = { static_cast<uint8_t>(padIndex), state };
                            neoTrellisLink.sendCommand(CMD_LED_CLIP_STATE, statePayload, sizeof(statePayload));
                        }

                        // Send to GUI (convert to 14-bit for full fidelity)
                        uint16_t r14 = r8 << 1;
                        uint16_t g14 = g8 << 1;
                        uint16_t b14 = b8 << 1;
                        guiInterface.sendClipState(track, scene, state,
                                                   (r14 >> 7) & 0x7F, r14 & 0x7F,
                                                   (g14 >> 7) & 0x7F, g14 & 0x7F,
                                                   (b14 >> 7) & 0x7F, b14 & 0x7F);
                    }
                }

                Serial.printf("✅ Processed ring clips bulk (%u clips)\n", 32);

                // Mark grid as seen and enable keys (like CMD_GRID_UPDATE does)
                if (!gridSeen) {
                    gridSeen = true;
                    Serial.println("Teensy: First grid seen (bulk clips) — enabling key scanning on M4");
                    neoTrellisLink.sendCommand(CMD_ENABLE_KEYS, nullptr, 0);
                }

                // Resend cached names to ensure GUI is in sync
                broadcastCachedNamesToGUI();
                break;
            }

            default:
                // Custom vendor messages (F0 7D ...) still go through processSysEx
                Serial.print("Unhandled Live CMD:0x");
                Serial.println(command, HEX);
                break;
        }
    }
}

void LiveController::updateClipState(int padIndex, int state) {
    if (padIndex < 0 || padIndex >= TOTAL_KEYS) return;
    
    uint32_t color = COLOR_EMPTY;
    switch (state) {
        case CLIP_STATE_EMPTY:     color = COLOR_EMPTY; break;
        case CLIP_STATE_STOPPED:   color = COLOR_LOADED; break;
        case CLIP_STATE_PLAYING:   color = COLOR_PLAYING; break;
        case CLIP_STATE_RECORDING: color = COLOR_RECORDING; break;
        case CLIP_STATE_QUEUED:    color = COLOR_TRIGGERED; break; // yellow for queued
        default:                    color = COLOR_EMPTY; break;
    }
    
    neoTrellisLink.setPixelColor(padIndex, color);
}

void LiveController::sendClipTrigger(uint8_t track, uint8_t scene) {
    if (!liveConnected) {
        Serial.println("Teensy: Ignoring clip trigger before Live handshake");
        return;
    }
    uint8_t data[] = {track, scene};
    sendSysExToAbleton(CMD_CLIP_TRIGGER, data, 2);
}

void LiveController::sendSysExToAbleton(uint8_t command, const uint8_t* data, int dataLength, bool requireLiveConnection) {
    if (requireLiveConnection && !liveConnected) {
        return;
    }
    if (dataLength < 0) dataLength = 0;

    static uint8_t sequence = 0;
    sequence = (sequence + 1) & 0x7F;

    const uint8_t lenMsb = (dataLength >> 7) & 0x7F;
    const uint8_t lenLsb = dataLength & 0x7F;
    const int messageSize = 4 + 4 + dataLength + 2; // header + cmd/seq/len + checksum + end
    uint8_t sysexMsg[messageSize];

    int index = 0;
    sysexMsg[index++] = SYSEX_START;
    sysexMsg[index++] = MANUFACTURER_ID;
    sysexMsg[index++] = DEVICE_ID;
    sysexMsg[index++] = 0x7F;
    sysexMsg[index++] = command & 0x7F;
    sysexMsg[index++] = sequence;
    sysexMsg[index++] = lenMsb;
    sysexMsg[index++] = lenLsb;

    for (int i = 0; i < dataLength; ++i) {
        uint8_t value = data ? data[i] : 0;
        sysexMsg[index++] = value & 0x7F;
    }

    uint8_t checksum = command ^ sequence;
    for (int i = 0; i < dataLength; ++i) {
        checksum ^= data ? data[i] : 0;
    }
    sysexMsg[index++] = checksum & 0x7F;
    sysexMsg[index++] = SYSEX_END;

    #ifdef DEBUG_LIVE_LOG
    Serial.print("SYSEX_OUT: ");
    for (int i = 0; i < index; i++) {
        Serial.print(sysexMsg[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
    #endif

    usbMIDI.sendSysEx(index, sysexMsg);
}

void LiveController::sendTransportCommand(uint8_t command) {
    sendSysExToAbleton(command, nullptr, 0);
}

// === HELPER FUNCTIONS ===

void LiveController::handleKeyPress(int board, int key) {
    int globalKey = (board * 16) + key;
    // Map 1D key index to (track, scene) using 8x4 layout
    int track = globalKey % GRID_TRACKS;
    int scene = globalKey / GRID_TRACKS;
    
    Serial.print("Pad "); Serial.print(globalKey);
    Serial.print(" (Track "); Serial.print(track);
    Serial.print(", Scene "); Serial.print(scene);
    Serial.println(") pressed");
    
    sendClipTrigger(track, scene);
    neoTrellisLink.setPixelColor(globalKey, COLOR_TRIGGERED);
}

void LiveController::handleKeyRelease(int board, int key) {
    int globalKey = (board * 16) + key;
    Serial.print("Pad "); Serial.print(globalKey); Serial.println(" released");
    neoTrellisLink.setPixelColor(globalKey, COLOR_LOADED);
}

int LiveController::getBoardFromKey(int globalKey) {
    return globalKey / 16;
}

int LiveController::getLocalKeyFromGlobal(int globalKey) {
    return globalKey % 16;
}

void LiveController::processSysEx(uint8_t* data, int length) {
    if (length < 5 || data[0] != SYSEX_START || data[length - 1] != SYSEX_END) return;
    if (data[1] != MANUFACTURER_ID || data[2] != DEVICE_ID) return;
    
    uint8_t command = data[3];
    switch (command) {
        case CMD_LED_CLIP_STATE:
            if (length >= 6) {
                int padIndex = data[4];
                int state = data[5];
                updateClipState(padIndex, state);
            }
            break;
    }
}

void LiveController::processHandshakeMessage(uint8_t* data, int length) {
    uint8_t command = 0;
    uint8_t sequence = 0;
    uint16_t payloadLen = 0;
    const uint8_t* payload = nullptr;
    if (!parseLiveSysExFrame(data, length, command, sequence, payloadLen, payload) || command != CMD_HANDSHAKE) {
        return;
    }

    if (!isHardwareReady()) {
        Serial.println("Teensy: Live handshake received but hardware not ready — deferring response");
        return;
    }

    if (payloadLen < 2) {
        Serial.println("Teensy: Handshake payload too short, continuing anyway");
    }

    Serial.println("Teensy: Ableton Live handshake detected. Responding...");
    sendHandshakeResponse();

    // Allow Live to re-establish state if it reopens ports
    liveConnected = true;
    liveConnectedAt = millis();
    gridSeen = false;
    gridRequestRetries = 0;
    gridRequestLastAttempt = 0;
    Serial.println("Teensy: ✓ Live connection established (ack sent).");
    broadcastCachedNamesToGUI();
    if (neoTrellisLink.isConnected()) {
        Serial.println("Teensy: Live connected — enabling NeoTrellis key scanning.");
        neoTrellisLink.sendCommand(CMD_ENABLE_KEYS, nullptr, 0);
    }
}

void LiveController::sendHandshakeResponse() {
    const uint8_t sequence = 0x01;
    const uint8_t payload[2] = {0x54, 0x53}; // "TS"
    const uint8_t lenMsb = 0x00;
    const uint8_t lenLsb = 0x02;
    uint8_t checksum = CMD_HANDSHAKE ^ sequence ^ payload[0] ^ payload[1];
    checksum &= 0x7F;
    uint8_t message[] = {
        SYSEX_START, MANUFACTURER_ID, DEVICE_ID, 0x7F,
        CMD_HANDSHAKE, sequence, lenMsb, lenLsb,
        payload[0], payload[1],
        checksum,
        SYSEX_END
    };
    usbMIDI.sendSysEx(sizeof(message), message);
    usbMIDI.send_now();
    Serial.println("Teensy: Handshake reply sent (TS).");
}

void LiveController::waitForLiveHandshake() {
    if (liveConnected) return;

    // This loop now only looks for one thing: the SysEx handshake from Live.
    // It's much faster and less prone to timing errors.
    while (usbMIDI.read()) {
        if (usbMIDI.getType() == usbMIDI.SystemExclusive) {
            uint8_t* data = usbMIDI.getSysExArray();
            uint16_t length = usbMIDI.getSysExArrayLength();
            
            // A valid handshake message will not be null and will have a minimum length.
            if (data && length >= 6) {
                // Pass the message to the dedicated handler.
                processHandshakeMessage(data, length);
            }
        }
    }
}

void LiveController::resendCachedNamesToGUI() {
    broadcastCachedNamesToGUI();
}

void LiveController::broadcastCachedNamesToGUI() {
    if (!guiInterface.isConnected()) {
        return;
    }

    for (int track = 0; track < GRID_TRACKS; ++track) {
        if (trackNameValid[track] && trackNameCache[track][0] != '\0') {
            guiInterface.sendTrackName(static_cast<uint8_t>(track), trackNameCache[track]);
        }
    }

    for (int pad = 0; pad < TOTAL_KEYS; ++pad) {
        if (clipNameValid[pad] && clipNameCache[pad][0] != '\0') {
            uint8_t track = pad % GRID_TRACKS;
            uint8_t scene = pad / GRID_TRACKS;
            guiInterface.sendClipName(track, scene, clipNameCache[pad]);
        }
    }
}
