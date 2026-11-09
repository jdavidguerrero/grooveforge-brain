#include "../../include/teensy/Hardware.h"
#include "LiveController/LiveController.h"
#include "../../lib/teensy/UartHandler/UartHandler.h"
#include "../../lib/teensy/UIBridge.h"
#include "../../lib/teensy/NeoTrellisLink/NeoTrellisLink.h"
#include "../../lib/teensy/GUIInterface/GUIInterface.h"
#include "../../lib/ButtonManager/ButtonManager.h"
#include "../../include/MidiCommands.h"
#include "../../lib/Encoders/Encoders.h"
#include "../../lib/Faders/Faders.h"
// #include "Neopixels.h"
// #include "Piezo.h"
// #include "Theremin.h"
// #include "CapButtons.h"
#include "../../lib/MidiHandler/MidiHandler.h"

// Create instances of all our hardware classes
LiveController liveController;
UartHandler uartHandler;
UIBridge uiBridge;
NeoTrellisLink neoTrellisLink;
GUIInterface guiInterface;
Encoders encoders;
Faders faders;
// Neopixels neopixels;
// Piezo piezo;
// Theremin theremin;
// CapButtons capButtons;
MidiHandler midiHandler;
ButtonManager buttonManager;

// ===== GLOBAL STATE =====
// Track seleccionado (actualizado por CMD_SELECTED_TRACK desde Ableton)
static int g_selectedTrack = 0;
// Valores de encoders [Pan, SendA, SendB, SendC] para el track seleccionado
static int g_encoderValues[4] = {8192, 0, 0, 0}; // Pan centrado, Sends en 0

// Función para actualizar track seleccionado (llamada desde LiveController)
void setSelectedTrack(int trackIndex) {
    if (g_selectedTrack != trackIndex) {
        g_selectedTrack = trackIndex;
        Serial.printf("🎯 Selected track changed: Track %d\n", g_selectedTrack);

        // Reset encoder values to neutral when changing tracks
        // This prevents sending old values from previous track
        g_encoderValues[0] = 8192;  // Pan center (50%)
        g_encoderValues[1] = 0;     // Send A off
        g_encoderValues[2] = 0;     // Send B off
        g_encoderValues[3] = 0;     // Send C off

        Serial.println("   ↳ Encoder values reset to neutral");
    }
}

// Función para manejar cambios de banco desde la GUI
void handleMixerBankChangeFromGUI(int bank) {
    // GUI envía número de banco (0, 1, 2...), necesitamos convertir a track offset
    int trackOffset = bank * 4;  // 4 tracks per bank
    Serial.printf("📤 Mixer bank change received from GUI: bank %d → track offset %d (tracks %d-%d)\n",
                  bank, trackOffset, trackOffset, trackOffset + 3);
    faders.setTrackBank(trackOffset);
}

namespace {
constexpr uint8_t SHIFT_UI_PANEL = 0x0F;

// Shift state tracking
static bool g_shiftHeld = false;

// Forward declarations
void handleMixerModeChange();

void handleEncoderButtonPress(uint8_t encoderIndex) {
    // Called when encoder buttons are pressed WITHOUT shift
    // (When shift is active, ButtonManager calls handleNavigationPress instead)
    // Fire scenes (original behavior)
    uint8_t scene = encoderIndex;
    if (scene < GRID_SCENES) {
        liveController.sendSysExToAbleton(CMD_SCENE_FIRE, &scene, 1);
        Serial.printf("🎬 Fire Scene %d (Encoder %d)\n", scene, encoderIndex);
    }
}

void handleNavigationPress(uint8_t direction) {
    // Called from encoder buttons when SHIFT is active
    // Direction mapping: 0=UP(Enc0), 1=DOWN(Enc1), 2=LEFT(Enc2), 3=RIGHT(Enc3)
    switch (direction) {
        case 0:  // UP = Encoder 0 = Previous track (LEFT in session ring)
            uiBridge.shiftSessionRing(-1, 0);
            Serial.println("🎯 Navigate: Previous Track (Encoder 0)");
            break;
        case 1:  // DOWN = Encoder 1 = Next track (RIGHT in session ring)
            uiBridge.shiftSessionRing(1, 0);
            Serial.println("🎯 Navigate: Next Track (Encoder 1)");
            break;
        case 2:  // LEFT = Encoder 2 = Previous scene (UP in session ring)
            uiBridge.shiftSessionRing(0, -1);
            Serial.println("🎯 Navigate: Previous Scene (Encoder 2)");
            break;
        case 3:  // RIGHT = Encoder 3 = Next scene (DOWN in session ring)
            uiBridge.shiftSessionRing(0, 1);
            Serial.println("🎯 Navigate: Next Scene (Encoder 3)");
            break;
        default:
            break;
    }
}

void handleTransportPress(ButtonID id) {
    switch (id) {
        case ButtonID::PLAY:    liveController.sendSysExToAbleton(CMD_TRANSPORT_PLAY, nullptr, 0); break;
        case ButtonID::STOP: {
            uint8_t off = 0;
            liveController.sendSysExToAbleton(CMD_TRANSPORT_PLAY, &off, 1);
            break;
        }
        case ButtonID::RECORD:  liveController.sendSysExToAbleton(CMD_TRANSPORT_RECORD, nullptr, 0); break;
        case ButtonID::LOOP:    liveController.sendSysExToAbleton(CMD_TRANSPORT_LOOP, nullptr, 0); break;
        case ButtonID::UNDO:    liveController.sendSysExToAbleton(CMD_UNDO, nullptr, 0); break;
        case ButtonID::METRONOME: handleMixerModeChange(); break;
        default: break;
    }
}

void handleBankChange(int8_t direction) {
    // DISABLED: User wants no bank navigation (8 tracks at a time)
    (void)direction;  // Suppress unused parameter warning
}

void handleShiftChange(bool pressed) {
    // SHIFT is now a TOGGLE (not hold) - only toggle on button press
    if (pressed) {
        g_shiftHeld = !g_shiftHeld;  // Toggle state
        Serial.printf("🔄 SHIFT toggled: %s\n", g_shiftHeld ? "ON" : "OFF");

        // Sync ButtonManager state so encoder navigation works
        buttonManager.setShiftState(g_shiftHeld);

        encoders.setShiftHeld(g_shiftHeld);
        guiInterface.sendUiState(SHIFT_UI_PANEL, g_shiftHeld);
        guiInterface.sendShiftState(g_shiftHeld);
    }
    // Ignore release events for toggle mode
}

void handleMixerModeChange() {
    // Ciclar entre los modos de parámetros
    static uint8_t currentModeIndex = 0;
    const FaderParamMode modes[] = {
        FaderParamMode::VOLUME_PAN,
        FaderParamMode::SENDS_AB,
        FaderParamMode::SENDS_CD,
        // FaderParamMode::MASTER_RETURNS  // Descomentarwhen implemented
    };
    const uint8_t numModes = sizeof(modes) / sizeof(modes[0]);

    currentModeIndex = (currentModeIndex + 1) % numModes;
    FaderParamMode newMode = modes[currentModeIndex];

    faders.setParamMode(newMode);
    guiInterface.sendMixerMode(static_cast<uint8_t>(newMode));

    const char* modeNames[] = {"VOL/PAN", "SEND A/B", "SEND C/CUE", "MASTER/RET"};
    Serial.printf("🎚️ Mixer mode changed to: %s\n", modeNames[currentModeIndex]);
}
} // namespace

void setupHardware() {
    uartHandler.begin();
    uiBridge.begin();
    liveController.begin();
    encoders.begin();
    faders.begin();
    // neopixels.begin();
    // piezo.begin();
    // theremin.begin();
    // capButtons.begin();
    midiHandler.begin();
    buttonManager.onEncoderButtonPress = handleEncoderButtonPress;
    buttonManager.onNavigationPress = handleNavigationPress;
    buttonManager.onTransportPress = handleTransportPress;
    buttonManager.onBankChange = handleBankChange;
    buttonManager.onShiftChange = handleShiftChange;
    buttonManager.begin();
    
    // Conectar callbacks de Faders
    faders.onVolumeChange = [](int trackIndex, int volume) {
        // Convertir 7-bit a 14-bit para mayor resolución
        int value14bit = volume * 129;  // 127 * 129 = 16383
        uint8_t valueMsb = (value14bit >> 7) & 0x7F;
        uint8_t valueLsb = value14bit & 0x7F;
        
        // Enviar vía LiveController
        uint8_t payload[] = {(uint8_t)trackIndex, valueMsb, valueLsb};
        liveController.sendSysExToAbleton(CMD_MIXER_VOLUME, payload, 3);
        
        Serial.printf("Fader → Track %d: vol=%d (14bit=%d, MSB=0x%02X LSB=0x%02X)\\n",
                     trackIndex, volume, value14bit, valueMsb, valueLsb);
    };
    
    // ===== ENCODERS: Controlan parámetros del TRACK SELECCIONADO (disposición vertical) =====
    // Encoder 0 → PAN
    // Encoder 1 → Send A
    // Encoder 2 → Send B
    // Encoder 3 → Send C
    encoders.onEncoderChange = [](uint8_t encoderIndex, int8_t delta) {
        if (encoderIndex >= 4) return; // Solo 4 encoders

        // Ajustar valor (paso de 128 ≈ 1 en 7-bit)
        g_encoderValues[encoderIndex] += delta * 128; // 128 ≈ 1/128 of 14-bit range
        g_encoderValues[encoderIndex] = constrain(g_encoderValues[encoderIndex], 0, 16383);

        // Convertir a MSB/LSB
        uint8_t msb = (g_encoderValues[encoderIndex] >> 7) & 0x7F;
        uint8_t lsb = g_encoderValues[encoderIndex] & 0x7F;

        if (encoderIndex == 0) {
            // Encoder 0 → PAN
            uint8_t payload[] = {(uint8_t)g_selectedTrack, msb, lsb};
            liveController.sendSysExToAbleton(CMD_MIXER_PAN, payload, 3);
            Serial.printf("Encoder 0 → Track %d PAN: %d (14bit)\n", g_selectedTrack, g_encoderValues[0]);
        } else {
            // Encoders 1-3 → Sends A, B, C
            uint8_t sendIndex = encoderIndex - 1; // 1→0 (SendA), 2→1 (SendB), 3→2 (SendC)
            const char* sendNames[] = {"SendA", "SendB", "SendC"};

            uint8_t payload[] = {(uint8_t)g_selectedTrack, sendIndex, msb, lsb};
            liveController.sendSysExToAbleton(CMD_MIXER_SEND, payload, 4);
            Serial.printf("Encoder %d → Track %d %s: %d (14bit)\n",
                         encoderIndex, g_selectedTrack, sendNames[sendIndex], g_encoderValues[encoderIndex]);
        }
    };
}

void loopHardware() {
    uartHandler.read();
    neoTrellisLink.update();
    uiBridge.update();
    midiHandler.processMidiMessages();
    liveController.read();
    buttonManager.update();
    encoders.read();
    faders.read();
    // piezo.read();
    // theremin.read();
    // capButtons.read();
    // neopixels.update();
}
