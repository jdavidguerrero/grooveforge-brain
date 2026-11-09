#pragma once

#include <Adafruit_MCP23X17.h>
#include "shared/Config.h"

// Enumeración de botones lógicos
// Pin allocation matches hardware/processor.kicad_sch exactly (verified via KiCad net
// lister): each MCP23017 uses GPA0-6 + GPB0 (8 usable inputs), never GPA7/GPB7
// (output-only erratum). See PIN_MAP.md and specs/openspec/processor.md.
enum class ButtonID : uint8_t {
    // Encoder push buttons (MCP #1 @ 0x20) — all 8 wired, 1:1 with the 8 encoders
    ENC_1 = 0,  // GPA0
    ENC_2,      // GPA1
    ENC_3,      // GPA2
    ENC_4,      // GPA3

    // Extra function buttons (MCP #2 @ 0x21) — PLAY/STOP/RECORD/LOOP/SHIFT/METRONOME/UNDO
    // populated in Phase 1 (all 8 pushbuttons). GPA5 (BANK_RIGHT) still unassigned/free.
    PLAY = 10,      // GPA0
    STOP,           // GPA1
    RECORD,         // GPA2
    LOOP,           // GPA3
    BANK_LEFT,      // enum value unused — pin GPA4 reassigned to UNDO, see below
    BANK_RIGHT,     // GPA5 — wired, unassigned/free (was bank nav; disabled, grid shows all 8 tracks)
    SHIFT,          // GPA6
    METRONOME,      // GPB0 (not GPA7 — erratum). Currently repurposed to mixer-mode toggle.

    // Encoders 5-8 (MCP #1)
    ENC_5 = 4,  // GPA4
    ENC_6,      // GPA5
    ENC_7,      // GPA6
    ENC_8 = 8,  // GPB0 (not GPA7 — erratum)

    // Reserved for future buttons (would need a 3rd MCP or PCB rework — both chips are
    // at their 8-pin-wired capacity; 14/chip is the theoretical max, see PIN_MAP.md).
    // Deliberately kept off hardware buttons — these live on the touchscreen/soft-key
    // bar instead (encoder-first, touch-second UX; see specs/openspec/gui.md).
    DELETE = 20,
    DUPLICATE,
    QUANTIZE,
    UNDO,   // Exception: mapped to a physical MCP #2 pin (GPA4) — see initializeMappings()
    REDO,
    // ... more future buttons
};

struct ButtonMapping {
    uint8_t mcpIndex;    // 0 o 1 (qué MCP23017)
    uint8_t gpioPin;     // 0-15 (pin dentro del MCP)
    ButtonID buttonID;   // ID lógico del botón
    bool enabled;        // Si está mapeado actualmente
};

class ButtonManager {
public:
    ButtonManager();

    void begin();
    void update();

    // Estado de botones
    bool isPressed(ButtonID id);
    bool isShiftHeld() const { return shiftPressed; }
    void setShiftState(bool state) { shiftPressed = state; }  // For external toggle control

    // Callbacks (se deben configurar externamente)
    void (*onEncoderButtonPress)(uint8_t encoderIndex);
    void (*onEncoderButtonRelease)(uint8_t encoderIndex);
    void (*onNavigationPress)(uint8_t direction);  // 0=UP, 1=DOWN, 2=LEFT, 3=RIGHT
    void (*onTransportPress)(ButtonID id);
    void (*onBankChange)(int8_t direction);  // -1=left, +1=right
    void (*onShiftChange)(bool pressed);

private:
    Adafruit_MCP23X17 mcp[2];  // Dos MCPs

    bool buttonStates[32];                // Estado actual de todos los botones
    bool lastButtonStates[32];            // Estado anterior para detección de cambios
    unsigned long lastDebounceTime[32];   // Tiempo del último cambio para debouncing

    bool shiftPressed;                    // Estado del botón shift

    ButtonMapping mappings[24];           // Mapeo de botones activos
    uint8_t mappingCount;

    void initializeMappings();
    void handleButtonChange(ButtonID id, bool pressed);
    uint8_t getMappingIndex(ButtonID id);
    void readMCP(uint8_t mcpIndex);
};
