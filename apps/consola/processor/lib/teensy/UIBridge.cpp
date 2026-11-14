#include "UIBridge.h"
#include "MidiCommands.h"
#include "LiveController/LiveController.h"
#include "NeoTrellisLink/NeoTrellisLink.h"
#include "GUIInterface/GUIInterface.h"
#include "../../include/teensy/Hardware.h"

// Make the global liveController instance available to this file
extern LiveController liveController;
extern NeoTrellisLink neoTrellisLink;
extern GUIInterface guiInterface;

UIBridge::UIBridge() {
    // Constructor
}

void UIBridge::begin() {
    // Initialization logic for the UI Bridge
    lastSessionRingCheck = millis();
    sessionRingTrack = 0;
    sessionRingScene = 0;
    sessionRingWidth = 8;
    sessionRingHeight = 4;
}

void UIBridge::update() {
    // This function is now intentionally left empty.
    // The controller will no longer poll Live for updates.
    // It will rely exclusively on push messages from the remote script.
}

void UIBridge::processLiveSysEx(uint8_t command, const uint8_t* data, uint8_t length) {
    // Handle SysEx commands from Live that are relevant to the UI
    // For example, when Live sends a new session ring position.
    switch (command) {
        case CMD_RING_POSITION:
            // Payload: [track_msb, track_lsb, scene_msb, scene_lsb, width, height, overview]
            if (length >= 7) {
                int track = ((data[0] & 0x7F) << 7) | (data[1] & 0x7F);
                int scene = ((data[2] & 0x7F) << 7) | (data[3] & 0x7F);
                int width = data[4] & 0x7F;
                int height = data[5] & 0x7F;
                setSessionRingPosition(track, scene, width, height);
            }
            break;
    }
}

void UIBridge::setSessionRingPosition(int track, int scene, int width, int height) {
    sessionRingTrack = track;
    sessionRingScene = scene;
    sessionRingWidth = width;
    sessionRingHeight = height;

    // Optional: Log that we received a position update from Live
    Serial.print("UIBridge: Session ring position updated by Live -> T:");
    Serial.print(sessionRingTrack);
    Serial.print(" S:");
    Serial.println(sessionRingScene);

    sendSessionRingToGui();
}

void UIBridge::sendSessionRingPosition() {
    // This function is kept for potential future use (e.g., a manual refresh button)
    // but is no longer called periodically from update().
    byte data[] = {
        (byte)sessionRingTrack,
        (byte)sessionRingScene,
        (byte)sessionRingWidth,
        (byte)sessionRingHeight
    };
    liveController.sendSysExToAbleton(CMD_RING_POSITION, data, sizeof(data));
}

// Implement other member functions as needed, ensuring they are declared in the header.

void UIBridge::shiftSessionRing(int trackDelta, int sceneDelta) {
    // Send navigation commands to Live (0x68) per delta step
    auto sendNav = [&](uint8_t dir){ liveController.sendSysExToAbleton(CMD_RING_NAVIGATE, &dir, 1); };
    if (trackDelta > 0) for (int i=0;i<trackDelta;i++) sendNav(0x01); // Right
    if (trackDelta < 0) for (int i=0;i<-trackDelta;i++) sendNav(0x00); // Left
    if (sceneDelta > 0) for (int i=0;i<sceneDelta;i++) sendNav(0x03); // Down
    if (sceneDelta < 0) for (int i=0;i<-sceneDelta;i++) sendNav(0x02); // Up
}

void UIBridge::handleGridUpdateFromLive(uint8_t* colorData, int dataLen) {
    if (!colorData || dataLen <= 0 || !neoTrellisLink.isConnected()) {
        return;
    }

    uint8_t cmd = 0;
    if (dataLen == 96) {
        cmd = CMD_LED_GRID_UPDATE;
    } else if (dataLen == 192) {
        cmd = CMD_LED_GRID_UPDATE_14;
    }

    if (cmd != 0) {
        neoTrellisLink.sendCommand(cmd, colorData, dataLen);
    }
}

void UIBridge::handleUIStateFromLive(uint8_t panelId, bool isVisible) {
    sendUIStateToM4(panelId, isVisible);
}

void UIBridge::sendGridUpdateToM4(uint8_t* colorData, int dataLen) {
    handleGridUpdateFromLive(colorData, dataLen);
}

void UIBridge::sendUIStateToM4(uint8_t panelId, bool state) {
    if (!neoTrellisLink.isConnected()) {
        return;
    }
    uint8_t payload[2] = { static_cast<uint8_t>(panelId & 0x7F), static_cast<uint8_t>(state ? 1 : 0) };
    neoTrellisLink.sendCommand(CMD_LED_UI_STATE, payload, sizeof(payload));
}

void UIBridge::sendSessionRingToGui() {
    uint16_t track = static_cast<uint16_t>(sessionRingTrack);
    uint16_t scene = static_cast<uint16_t>(sessionRingScene);
    uint8_t payload[7] = {
        static_cast<uint8_t>((track >> 7) & 0x7F),
        static_cast<uint8_t>(track & 0x7F),
        static_cast<uint8_t>((scene >> 7) & 0x7F),
        static_cast<uint8_t>(scene & 0x7F),
        static_cast<uint8_t>(sessionRingWidth & 0x7F),
        static_cast<uint8_t>(sessionRingHeight & 0x7F),
        0x00 // overview placeholder
    };

    // Send to NeoTrellis (M4)
    if (neoTrellisLink.isConnected()) {
        neoTrellisLink.sendCommand(CMD_RING_POSITION, payload, sizeof(payload));
    }

    // Send to GUI (Raspberry Pi)
    guiInterface.sendSessionRingPosition(sessionRingTrack, sessionRingScene,
                                         sessionRingWidth, sessionRingHeight);
}

void UIBridge::updateSessionRingInLive() {
    // Deprecated: position is now sent by Live after navigation/select
}

void UIBridge::handleUARTCommand(uint8_t cmd, uint8_t* payload, uint8_t len) {
    switch (cmd) {
        case CMD_UI_BROWSER:
            if (len >= 1) {
                uint8_t state = payload[0] & 0x7F;
                liveController.sendSysExToAbleton(CMD_BROWSER_MODE, &state, 1);
            }
            break;
        case CMD_UI_DEVICE:
            if (len >= 1) {
                uint8_t state = payload[0] & 0x7F;
                liveController.sendSysExToAbleton(CMD_VIEW_STATE, &state, 1);
            }
            break;
        case CMD_UI_CLIP:
            if (len >= 1) {
                uint8_t state = payload[0] & 0x7F;
                liveController.sendSysExToAbleton(CMD_DETAIL_CLIP, &state, 1);
            }
            break;
        case CMD_UI_HOTSWAP:
            if (len >= 1) {
                uint8_t mode = payload[0] & 0x7F;
                liveController.sendSysExToAbleton(CMD_BROWSER_MODE, &mode, 1);
            }
            break;
        case CMD_UI_CREATE_TRACK:
            if (len >= 1) {
                uint8_t type = payload[0] & 0x7F;
                uint8_t createCmd = 0;
                if (type == 0) createCmd = CMD_CREATE_MIDI_TRACK;
                else if (type == 1) createCmd = CMD_CREATE_AUDIO_TRACK;
                else if (type == 2) createCmd = CMD_CREATE_RETURN_TRACK;
                if (createCmd != 0) {
                    liveController.sendSysExToAbleton(createCmd, nullptr, 0);
                }
            }
            break;
        case CMD_UI_CREATE_SCENE:
            liveController.sendSysExToAbleton(CMD_CREATE_SCENE, nullptr, 0);
            break;
        case CMD_UI_NAVIGATE:
            if (len >= 1) {
                uint8_t dir = payload[0] & 0x03;
                switch (dir) {
                    case 0: shiftSessionRing(0, -1); break;
                    case 1: shiftSessionRing(0, 1); break;
                    case 2: shiftSessionRing(-1, 0); break;
                    case 3: shiftSessionRing(1, 0); break;
                }
            }
            break;
        case CMD_GRID_SHIFT_LEFT:
            shiftSessionRing(-1, 0);
            break;
        case CMD_GRID_SHIFT_RIGHT:
            shiftSessionRing(1, 0);
            break;
        case CMD_GRID_SHIFT_UP:
            shiftSessionRing(0, -1);
            break;
        case CMD_GRID_SHIFT_DOWN:
            shiftSessionRing(0, 1);
            break;
        case CMD_GRID_BANK_UP:
            shiftSessionRing(0, GRID_SCENES);
            break;
        case CMD_GRID_BANK_DOWN:
            shiftSessionRing(0, -GRID_SCENES);
            break;
        case CMD_TRANSPORT_PLAY:
            if (len >= 1) {
                uint8_t playState = payload[0] & 0x7F; // 1=play, 0=stop
                liveController.sendSysExToAbleton(CMD_TRANSPORT_PLAY, &playState, 1);
            } else {
                liveController.sendSysExToAbleton(CMD_TRANSPORT_PLAY, nullptr, 0);
            }
            break;
        case CMD_TRANSPORT_RECORD:
            if (len >= 1) {
                uint8_t recState = payload[0] & 0x7F;
                liveController.sendSysExToAbleton(CMD_TRANSPORT_RECORD, &recState, 1);
            } else {
                liveController.sendSysExToAbleton(CMD_TRANSPORT_RECORD, nullptr, 0);
            }
            break;
        case CMD_CLIP_LAUNCH:
        case CMD_CLIP_TRIGGER:
            if (len >= 2) {
                uint8_t track = payload[0] & 0x7F;
                uint8_t scene = payload[1] & 0x7F;
                liveController.sendClipTrigger(track, scene);
            }
            break;
        case CMD_UI_CLIP_STOP:
            if (len >= 2) {
                uint8_t dataOut[2] = {
                    static_cast<uint8_t>(payload[0] & 0x7F),
                    static_cast<uint8_t>(payload[1] & 0x7F)
                };
                liveController.sendSysExToAbleton(CMD_CLIP_STOP, dataOut, 2);
            }
            break;
        case CMD_SCENE_LAUNCH:
            if (len >= 1) {
                uint8_t scene = payload[0] & 0x7F;
                liveController.sendSysExToAbleton(CMD_SCENE_FIRE, &scene, 1);
            }
            break;
        case CMD_UI_TRACK_STOP:
            if (len >= 1) {
                uint8_t track = payload[0] & 0x7F;
                // Send track stop by emulating stop for each scene within the visible grid
                for (uint8_t scene = 0; scene < GRID_SCENES; ++scene) {
                    uint8_t dataOut[2] = { track, scene };
                    liveController.sendSysExToAbleton(CMD_CLIP_STOP, dataOut, 2);
                }
            }
            break;
        case CMD_TRACK_SELECT:
            if (len >= 1) {
                uint8_t track = payload[0] & 0x7F;
                liveController.sendSysExToAbleton(CMD_TRACK_SELECT, &track, 1);
            }
            break;
        case CMD_LED_GRID_UPDATE:
            // Grid refresh request (payload ignored)
            if (!liveController.hasSeenGrid()) {
                Serial.println("UIBridge: Grid refresh requested — awaiting Live data");
            } else {
                Serial.println("UIBridge: Grid refresh request noted");
            }
            break;
        case CMD_UI_SHIFT:
            if (len >= 1) {
                bool pressed = (payload[0] != 0);
                guiInterface.sendShiftState(pressed);
                Serial.print("UIBridge: Shift state -> ");
                Serial.println(pressed ? "PRESSED" : "RELEASED");
            }
            break;
        case CMD_MIXER_BANK_CHANGE:
            if (len >= 1) {
                int bank = payload[0] & 0x7F;
                handleMixerBankChangeFromGUI(bank);
            }
            break;
        default:
            Serial.print("UIBridge: Unhandled UART cmd 0x");
            Serial.println(cmd, HEX);
            break;
    }
}
