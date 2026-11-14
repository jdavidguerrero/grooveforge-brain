#include "UIPanelHandler.h"
#include "UIPanelCommands.h"
#include "MidiCommands.h"
#include "shared/Config.h"

UIPanelHandler::UIPanelHandler() {
    currentMode = MODE_SESSION;
    browserActive = false;
    devicePanelActive = false;
    hotswapMode = false;
    gridTrackOffset = 0;
    gridSceneOffset = 0;
}

void UIPanelHandler::begin() {
    Serial.println("M4: UI Panel Handler initialized");
    // Initialize any hardware-specific UI elements
    updateStatusLEDs();
}

void UIPanelHandler::handlePadPress(int padIndex, bool pressed) {
    if (!pressed) return;  // Only handle press events
    
    // Check for special combinations first
    if (checkSpecialCombinations(padIndex)) {
        return;  // Special combo handled
    }
    
    // Normal pad press - send to Live via Teensy
    // NEW MAPPING: 8 tracks × 4 scenes
    int track = padIndex % 8;  // Column (0-7) - 8 tracks horizontal
    int scene = padIndex / 8;  // Row (0-3) - 4 scenes vertical
    
    // Add grid offset to get absolute position
    int absoluteTrack = track + gridTrackOffset;
    int absoluteScene = scene + gridSceneOffset;
    
    Serial.printf("M4: Pad %d pressed (T%d S%d) -> Live T%d S%d\n", 
                  padIndex, track, scene, absoluteTrack, absoluteScene);
    
    // Send clip launch command to Teensy
    sendClipLaunchCommand(absoluteTrack, absoluteScene);
}

bool UIPanelHandler::checkSpecialCombinations(int padIndex) {
    // Example combinations for UI control:
    // Pad 0 + Pad 1 = Browser toggle
    // Pad 2 + Pad 3 = Device panel toggle
    // Pad 28 + Pad 29 = Create track
    // Pad 30 + Pad 31 = Hotswap mode
    
    static uint32_t lastComboTime = 0;
    static int lastPad = -1;
    uint32_t currentTime = millis();
    
    // Two-pad combo detection (within 500ms)
    if (lastPad >= 0 && (currentTime - lastComboTime) < 500) {
        bool comboHandled = handlePadCombo(lastPad, padIndex);
        lastPad = -1;  // Reset combo state
        return comboHandled;
    }
    
    // Store current pad for potential combo
    lastPad = padIndex;
    lastComboTime = currentTime;
    
    return false;  // No combo detected
}

bool UIPanelHandler::handlePadCombo(int pad1, int pad2) {
    // Sort pads for consistent combo detection
    if (pad1 > pad2) {
        int temp = pad1;
        pad1 = pad2; 
        pad2 = temp;
    }
    
    Serial.printf("M4: Combo detected: Pad %d + Pad %d\n", pad1, pad2);
    
    // NEW 8×4 GRID COMBOS:
    
    // Browser toggle (top-left corner: pads 0+1)
    if (pad1 == 0 && pad2 == 1) {
        toggleBrowserPanel();
        return true;
    }
    
    // Device panel toggle (top-right corner: pads 6+7)
    if (pad1 == 6 && pad2 == 7) {
        toggleDevicePanel();
        return true;
    }
    
    // Create track (bottom-left: pads 24+25)  
    if (pad1 == 24 && pad2 == 25) {
        sendCreateTrackCommand(0);  // 0 = MIDI track
        return true;
    }
    
    // Hotswap mode (bottom-right: pads 30+31)
    if (pad1 == 30 && pad2 == 31) {
        toggleHotswapMode();
        return true;
    }
    
    // Grid navigation combos (8×4 layout)
    // Left edge navigation
    if (pad1 == 0 && pad2 == 8) {  // Left edge up/down
        shiftGridLeft();
        return true;
    }
    if (pad1 == 7 && pad2 == 15) {  // Right edge up/down
        shiftGridRight();
        return true;
    }
    
    // SCENE LAUNCH COMBOS (Row-based):
    // Row 1 scene launch (pads 0-7 any two)
    if (pad1 >= 0 && pad1 <= 7 && pad2 >= 0 && pad2 <= 7) {
        launchScene(0);  // Launch scene 1
        return true;
    }
    // Row 2 scene launch (pads 8-15 any two)
    if (pad1 >= 8 && pad1 <= 15 && pad2 >= 8 && pad2 <= 15) {
        launchScene(1);  // Launch scene 2  
        return true;
    }
    // Row 3 scene launch (pads 16-23 any two)
    if (pad1 >= 16 && pad1 <= 23 && pad2 >= 16 && pad2 <= 23) {
        launchScene(2);  // Launch scene 3
        return true;
    }
    // Row 4 scene launch (pads 24-31 any two)
    if (pad1 >= 24 && pad1 <= 31 && pad2 >= 24 && pad2 <= 31) {
        launchScene(3);  // Launch scene 4
        return true;
    }
    
    return false;  // Unknown combo
}

void UIPanelHandler::toggleBrowserPanel() {
    browserActive = !browserActive;
    Serial.printf("M4: Browser panel %s\n", browserActive ? "ON" : "OFF");
    
    // Send command to Teensy
    uint8_t payload[] = {static_cast<uint8_t>(browserActive ? 1 : 0)};
    sendUARTCommand(CMD_UI_BROWSER, payload, 1);
    
    // Update status LEDs
    updateStatusLEDs();
}

void UIPanelHandler::toggleDevicePanel() {
    devicePanelActive = !devicePanelActive;
    Serial.printf("M4: Device panel %s\n", devicePanelActive ? "ON" : "OFF");
    
    // Send command to Teensy
    uint8_t payload[] = {static_cast<uint8_t>(devicePanelActive ? 1 : 0)};
    sendUARTCommand(CMD_UI_DEVICE, payload, 1);
    
    // Update status LEDs
    updateStatusLEDs();
}

void UIPanelHandler::toggleHotswapMode() {
    hotswapMode = !hotswapMode;
    Serial.printf("M4: Hotswap mode %s\n", hotswapMode ? "ON" : "OFF");
    
    // Send command to Teensy
    uint8_t payload[] = {static_cast<uint8_t>(hotswapMode ? 1 : 0)};
    sendUARTCommand(CMD_UI_HOTSWAP, payload, 1);
    
    // Auto-enable browser when entering hotswap
    if (hotswapMode && !browserActive) {
        toggleBrowserPanel();
    }
    
    // Update status LEDs
    updateStatusLEDs();
}

void UIPanelHandler::sendCreateTrackCommand(uint8_t trackType) {
    Serial.printf("M4: Creating track type %d\n", trackType);
    
    uint8_t payload[] = {trackType};  // 0=MIDI, 1=Audio, 2=Return
    sendUARTCommand(CMD_UI_CREATE_TRACK, payload, 1);
    
    // Visual feedback
    flashPadColor(28, UI_COLOR_GRID_FOCUS, 500);  // Flash creation pads
    flashPadColor(29, UI_COLOR_GRID_FOCUS, 500);
}

void UIPanelHandler::shiftGridLeft() {
    if (gridTrackOffset > 0) {
        gridTrackOffset--;
        Serial.printf("M4: Grid shifted left (track offset: %d)\n", gridTrackOffset);
        
        uint8_t payload[] = {2};  // Direction: left
        sendUARTCommand(CMD_GRID_SHIFT_LEFT, payload, 1);
        
        // Request grid update from Live
        requestGridUpdate();
    }
}

void UIPanelHandler::shiftGridRight() {
    gridTrackOffset++;  // No upper limit - Live will handle bounds
    Serial.printf("M4: Grid shifted right (track offset: %d)\n", gridTrackOffset);
    
    uint8_t payload[] = {3};  // Direction: right
    sendUARTCommand(CMD_GRID_SHIFT_RIGHT, payload, 1);
    
    // Request grid update from Live
    requestGridUpdate();
}

void UIPanelHandler::shiftGridUp() {
    if (gridSceneOffset > 0) {
        gridSceneOffset--;
        Serial.printf("M4: Grid shifted up (scene offset: %d)\n", gridSceneOffset);
        
        uint8_t payload[] = {0};  // Direction: up
        sendUARTCommand(CMD_GRID_SHIFT_UP, payload, 1);
        
        requestGridUpdate();
    }
}

void UIPanelHandler::shiftGridDown() {
    gridSceneOffset++;  // No upper limit
    Serial.printf("M4: Grid shifted down (scene offset: %d)\n", gridSceneOffset);
    
    uint8_t payload[] = {1};  // Direction: down  
    sendUARTCommand(CMD_GRID_SHIFT_DOWN, payload, 1);
    
    requestGridUpdate();
}

void UIPanelHandler::launchScene(int sceneIndex) {
    // Launch scene with offset
    int absoluteScene = sceneIndex + gridSceneOffset;
    Serial.printf("M4: Launching scene %d (absolute: %d)\n", sceneIndex, absoluteScene);
    
    uint8_t payload[] = {(uint8_t)(absoluteScene & 0x7F)};
    sendUARTCommand(CMD_SCENE_LAUNCH, payload, 1);
    
    // Visual feedback - flash entire row
    flashRowColor(sceneIndex, UI_COLOR_NAV_FEEDBACK, 300);
}

void UIPanelHandler::sendClipLaunchCommand(int track, int scene) {
    uint8_t payload[] = {
        (uint8_t)(track & 0x7F),   // Track index (0-127)
        (uint8_t)(scene & 0x7F)    // Scene index (0-127)  
    };
    
    sendUARTCommand(CMD_CLIP_LAUNCH, payload, 2);
}

void UIPanelHandler::updateStatusLEDs() {
    // Use corner pads as status indicators (8×4 grid)
    
    // Pad 0: Browser status (top-left)
    uint32_t browserColor = browserActive ? UI_COLOR_BROWSER_ON : COLOR_EMPTY;
    setCornerPadColor(0, browserColor);
    
    // Pad 7: Device panel status (top-right)
    uint32_t deviceColor = devicePanelActive ? UI_COLOR_DEVICE_ON : COLOR_EMPTY;
    setCornerPadColor(7, deviceColor);
    
    // Pad 31: Hotswap status (bottom-right)
    uint32_t hotswapColor = hotswapMode ? UI_COLOR_HOTSWAP_ON : COLOR_EMPTY;
    setCornerPadColor(31, hotswapColor);
    
    // Pad 24: Grid position indicator (bottom-left)
    uint32_t gridColor = (gridTrackOffset > 0 || gridSceneOffset > 0) ? 
                         UI_COLOR_GRID_FOCUS : COLOR_EMPTY;
    setCornerPadColor(24, gridColor);
}

void UIPanelHandler::setCornerPadColor(int padIndex, uint32_t color) {
    // This would call the NeoTrellis pixel update function
    // Assuming liveController.setPixelColor() exists
    extern void setTrellisPixel(int index, uint32_t color);
    setTrellisPixel(padIndex, color);
}

void UIPanelHandler::flashPadColor(int padIndex, uint32_t color, int durationMs) {
    // Store original color and schedule restore
    // This would need a timer/scheduler system
    setCornerPadColor(padIndex, color);
    
    // Schedule restore after duration (implementation depends on timer system)
    // For now, immediate restore after delay would work
}

void UIPanelHandler::flashRowColor(int rowIndex, uint32_t color, int durationMs) {
    // Flash entire row for scene launch feedback
    int startPad = rowIndex * 8;  // 8×4 grid: row starts at rowIndex * 8
    int endPad = startPad + 7;    // 8 pads per row
    
    Serial.printf("M4: Flashing row %d (pads %d-%d)\n", rowIndex, startPad, endPad);
    
    for (int pad = startPad; pad <= endPad; pad++) {
        setCornerPadColor(pad, color);
    }
    
    // Would need timer to restore original colors
}

void UIPanelHandler::requestGridUpdate() {
    // Request complete grid state from Live via Teensy
    uint8_t payload[] = {0};  // Request all grid data
    sendUARTCommand(CMD_LED_GRID_UPDATE, payload, 1);
}

void UIPanelHandler::sendUARTCommand(uint8_t cmd, uint8_t* payload, uint8_t len) {
    // UART message format: [F0][CMD][LEN][DATA...][CHECKSUM][F7]
    
    uint8_t message[40];  // Max message size
    uint8_t msgLen = 0;
    
    message[msgLen++] = 0xF0;      // Start byte
    message[msgLen++] = cmd;       // Command
    message[msgLen++] = len;       // Payload length
    
    // Calculate checksum as we build
    uint8_t checksum = cmd ^ len;
    
    // Add payload  
    for (int i = 0; i < len; i++) {
        message[msgLen++] = payload[i];
        checksum ^= payload[i];
    }
    
    message[msgLen++] = checksum;  // Checksum
    message[msgLen++] = 0xF7;      // End byte
    
    // Send via UART (implementation depends on UART library)
    Serial1.write(message, msgLen);
    
    #ifdef DEBUG_UI_COMMANDS
    Serial.printf("M4: Sent UART cmd 0x%02X, len %d\n", cmd, len);
    #endif
}