# Push Clone V2 - Hardware Layout con Scene Launching

## Diseño Final: 8 Tracks × 4 Scenes + Scene Launcher Separado

### Layout Físico Propuesto:

```
┌─────────────────────────────────────────────────────────────────┐
│  TRANSPORT & UI CONTROLS:                                       │
│  [PLAY][STOP][REC] [BROWSER][DEVICE][HOTSWAP] [TRACK+][SCENE+]  │
│                                                                 │
│  ENCODERS (9):                                                  │
│  [E1] [E2] [E3] [E4] [E5] [E6] [E7] [E8] [NAV]                 │
│  Vol  Pan  Send Send Dev1 Dev2 Dev3 Dev4  Browse               │
│                                                                 │
│  SCENE LAUNCHER (8 botones LED):                               │
│  [S1] [S2] [S3] [S4] [S5] [S6] [S7] [S8]                      │
├─────────────────────────────────────────────────────────────────┤
│  MAIN GRID (8×4 NEOTRELLIS):                                   │
│  Track1 Track2 Track3 Track4 Track5 Track6 Track7 Track8       │
│   [0]   [1]   [2]   [3]   [4]   [5]   [6]   [7]    Scene 1   │
│   [8]   [9]  [10]  [11]  [12]  [13]  [14]  [15]    Scene 2   │
│  [16]  [17]  [18]  [19]  [20]  [21]  [22]  [23]    Scene 3   │  
│  [24]  [25]  [26]  [27]  [28]  [29]  [30]  [31]    Scene 4   │
├─────────────────────────────────────────────────────────────────┤
│  FADERS & TRACK CONTROLS:                                       │
│  |F1| |F2| |F3| |F4| |F5| |F6| |F7| |F8|                      │
│  [M1] [M2] [M3] [M4] [M5] [M6] [M7] [M8]  (Mute)               │
│  [S1] [S2] [S3] [S4] [S5] [S6] [S7] [S8]  (Solo)               │
│  [A1] [A2] [A3] [A4] [A5] [A6] [A7] [A8]  (Arm)                │
└─────────────────────────────────────────────────────────────────┘
```

### Componentes Hardware V2:

#### MCU Principal:
- **Teensy 4.1** → USB MIDI + I2C master + coordinación

#### I/O Expansion:
- **2× MCP23017** → 32 GPIO adicionales para botones/LEDs
- **2× MCP23S17** → 32 GPIO SPI para encoder matrix

#### Grid Principal:
- **NeoTrellis M4** → 8×4 pads RGB via UART

#### Scene Launcher:
- **8× LED Buttons** → Scene launch dedicado
- **Conectados a MCP23017 #1**

#### Encoders & Faders:
- **9× Encoders** → Parameter control + navigation
- **8× Faders 60mm** → Track volumes
- **Conectados a MCP23S17 via SPI**

#### Additional Controls:
- **14× Buttons** → Transport, UI panels, modifiers
- **Conectados a MCP23017 #2**

### I2C/SPI Topology:

```
Teensy 4.1 (Master)
├── I2C Bus (pins 18/19)
│   ├── MCP23017 #1 (0x20) → Scene buttons S1-S8 + LEDs
│   ├── MCP23017 #2 (0x21) → UI buttons + Transport
│   └── (Raspberry Pi future)
│
├── SPI Bus (pins 11/12/13)
│   ├── MCP23S17 #1 (CS=10) → Encoders 1-8 matrix
│   └── MCP23S17 #2 (CS=9)  → Fader ADC + extra I/O
│
└── UART (Serial1)
    └── NeoTrellis M4 → 8×4 RGB pads
```

### Scene Launching Implementation:

#### Opción A: Botones Físicos Dedicados
```cpp
// MCP23017 #1 - Scene Launcher
#define SCENE_BTN_1    0  // Pin 0 - Scene 1
#define SCENE_BTN_2    1  // Pin 1 - Scene 2  
#define SCENE_BTN_3    2  // Pin 2 - Scene 3
#define SCENE_BTN_4    3  // Pin 3 - Scene 4
#define SCENE_BTN_5    4  // Pin 4 - Scene 5
#define SCENE_BTN_6    5  // Pin 5 - Scene 6
#define SCENE_BTN_7    6  // Pin 6 - Scene 7
#define SCENE_BTN_8    7  // Pin 7 - Scene 8

// LEDs en Port B del mismo MCP23017
#define SCENE_LED_1    8  // Pin 8 - Scene 1 LED
#define SCENE_LED_2    9  // Pin 9 - Scene 2 LED
// ... etc
```

#### Opción B: Solo NeoTrellis (Actual)
```cpp
// Row-based scene launching via pad combinations:
// Double-tap cualquier fila → Launch scene correspondiente  
// Long-press fila completa → Launch scene + stop others

// Row 1: pads 0-7   → Scene 1
// Row 2: pads 8-15  → Scene 2  
// Row 3: pads 16-23 → Scene 3
// Row 4: pads 24-31 → Scene 4
```

### Session Ring Navigation:

```
Live UI Panel - 8×4 Window:
┌─────────────────────────────────────────┐
│ Track Offset: 0    Scene Offset: 0      │
├─────────────────────────────────────────┤
│  T1   T2   T3   T4   T5   T6   T7   T8  │
│S1[0] [1]  [2]  [3]  [4]  [5]  [6]  [7] │ ← Pad Row 0  
│S2[8] [9] [10] [11] [12] [13] [14] [15] │ ← Pad Row 1
│S3[16][17][18] [19] [20] [21] [22] [23] │ ← Pad Row 2  
│S4[24][25][26] [27] [28] [29] [30] [31] │ ← Pad Row 3
└─────────────────────────────────────────┘

Navigation:
- Left/Right → Shift tracks (8-track window)
- Up/Down → Shift scenes (4-scene window)  
- Bank → Jump 8 tracks or 4 scenes
```

### Ventajas Layout 8×4:

✅ **8 tracks simultáneos** → Mejor para producción multi-track
✅ **Scene launching separado** → No interfiere con clip launching  
✅ **Navigation natural** → Horizontal = tracks, vertical = scenes
✅ **Compatible con Push workflow** → Misma orientación conceptual
✅ **Escalable** → Fácil agregar más NeoTrellis para 16×4 o 8×8

### Control Mapping Summary:

**Grid Principal (NeoTrellis):** Clip launching 8×4
**Scene Launcher:** 8 botones físicos OR pad combinations  
**Encoders:** Parameter control + browser navigation
**Faders:** Track volumes direct control
**UI Buttons:** Browser, Device, Hotswap panels
**Transport:** Play, Stop, Record, Loop

Este layout maximiza la funcionalidad manteniendo el hardware modular y cost-effective.