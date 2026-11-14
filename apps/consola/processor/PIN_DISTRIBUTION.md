# Push Clone - Teensy 4.1 Pin Distribution

> ⚠️ **Pin assignments below are SUPERSEDED by [`PIN_MAP.md`](PIN_MAP.md)** (reconciled
> 2026-07-16, gap G-P1/G-P2/G-P3). `PIN_MAP.md` + `include/shared/Config.h` are the source
> of truth. This file is retained for the **power budget, NeoPixel distribution, and
> protocol/wiring notes**, which remain valid. The I2C-on-4/5 and old encoder pin rows here
> are wrong — use `PIN_MAP.md`.

## Hardware Overview
- **Microcontroller**: Teensy 4.1
- **Main Pad Grid**: NeoTrellis M4 (8x4 = 32 pads)  
- **Encoders**: 8 Rotary Encoders
- **Faders**: 4 Analog Faders
- **Sampler**: 4 Piezoelectric Sensors
- **Theremin**: 2 IR Sharp Distance Sensors
- **Transport**: 8 Capacitive Touch Buttons
- **Visual Feedback**: NeoPixel Strip (28 LEDs total)

---

## Pin Assignments

### Digital Pins

| Pin | Function | Component | Notes |
|-----|----------|-----------|-------|
| 0 | Encoder 1 A | Rotary Encoder | Encoder pair 1 |
| 1 | Encoder 1 B | Rotary Encoder | Encoder pair 1 |
| 2 | Encoder 2 A | Rotary Encoder | Encoder pair 2 |
| 3 | Encoder 2 B | Rotary Encoder | Encoder pair 2 |
| 4 | I2C SDA | NeoTrellis M4 | Hardware I2C |
| 5 | I2C SCL | NeoTrellis M4 | Hardware I2C |
| 6 | Encoder 3 A | Rotary Encoder | Encoder pair 3 |
| 7 | Encoder 3 B | Rotary Encoder | Encoder pair 3 |
| 8 | Encoder 4 A | Rotary Encoder | Encoder pair 4 |
| 9 | Encoder 4 B | Rotary Encoder | Encoder pair 4 |
| 10 | Encoder 5 A | Rotary Encoder | Encoder pair 5 |
| 11 | Encoder 5 B | Rotary Encoder | Encoder pair 5 |
| 12 | Encoder 6 A | Rotary Encoder | Encoder pair 6 |
| 13 | Built-in LED | System | Teensy onboard LED |
| 14 | Available | - | Free pin (was Serial MIDI) |
| 15 | Capacitive 1 | Touch Button | Play button |
| 16 | Capacitive 2 | Touch Button | Stop button |
| 17 | Capacitive 3 | Touch Button | Record button |
| 18 | Capacitive 4 | Touch Button | Loop button |
| 19 | Capacitive 5 | Touch Button | Track Previous |
| 20 | Capacitive 6 | Touch Button | Track Next |
| 21 | Capacitive 7 | Touch Button | Scene Previous |
| 22 | NeoPixel Data | LED Strip | WS2812B data line |
| 23 | Capacitive 8 | Touch Button | Scene Next |
| 24 | Encoder 7 A | Rotary Encoder | Encoder pair 7 |
| 25 | Encoder 7 B | Rotary Encoder | Encoder pair 7 |
| 26 | Encoder 8 A | Rotary Encoder | Encoder pair 8 |
| 27 | Encoder 8 B | Rotary Encoder | Encoder pair 8 |
| 28 | Encoder 6 B | Rotary Encoder | Encoder pair 6 |

### Analog Pins

| Pin | Function | Component | Range | Notes |
|-----|----------|-----------|-------|-------|
| A0 | Fader 1 | Volume Fader | 0-1023 | Track 1 volume |
| A1 | Fader 2 | Volume Fader | 0-1023 | Track 2 volume |
| A2 | Fader 3 | Volume Fader | 0-1023 | Track 3 volume |
| A3 | Fader 4 | Volume Fader | 0-1023 | Track 4 volume |
| A4 | Reserved | - | - | Future expansion |
| A5 | Reserved | - | - | Future expansion |
| A6 | Piezo 1 | Kick Drum | 0-1023 | Drum pad (MIDI C2) |
| A7 | Piezo 2 | Snare Drum | 0-1023 | Drum pad (MIDI C#2) |
| A8 | Piezo 3 | Hi-Hat | 0-1023 | Drum pad (MIDI D2) |
| A9 | Piezo 4 | Crash | 0-1023 | Drum pad (MIDI D#2) |
| A10 | IR Sensor 1 | Distance | 0-1023 | Theremin X-axis (CC 20) |
| A11 | IR Sensor 2 | Distance | 0-1023 | Theremin Y-axis (CC 21) |

---

## I2C Devices

| Address | Device | Function |
|---------|--------|----------|
| 0x2E | NeoTrellis Board 1 | Pads 0-15 (Left half) |
| 0x2F | NeoTrellis Board 2 | Pads 16-31 (Right half) |

---

## MIDI Configuration

### USB MIDI Interface
- **Connection**: Native USB MIDI (no additional pins required)
- **Latency**: ~1ms (vs ~3ms for Serial MIDI)
- **Bandwidth**: 480 Mbps (vs 31.25 kbps for Serial)
- **Format**: MIDI SysEx for Ableton integration

### MIDI Channel Mapping
- **Clip Triggers**: SysEx commands to Ableton
- **Encoder Parameters**: SysEx device parameter control
- **Fader Volumes**: SysEx mixer volume control
- **Drum Pads**: Standard MIDI notes (C2-D#2, channel 1)
- **Theremin**: MIDI CC 20-21, channel 1

---

## NeoPixel LED Distribution

**Total LEDs**: 28
- **Encoder Rings**: 8 LEDs (1 per encoder)
- **Button Indicators**: 16 LEDs (transport/nav buttons)
- **Fader Strips**: 4 LEDs (1 per fader)

### LED Index Mapping
```
Index 0-7:   Encoder ring LEDs
Index 8-15:  Transport button LEDs (Play, Stop, Record, etc.)
Index 16-23: Navigation button LEDs
Index 24-27: Fader indicator LEDs
```

---

## Power Requirements

| Component | Voltage | Current | Notes |
|-----------|---------|---------|-------|
| Teensy 4.1 | 3.3V/5V | ~150mA | USB powered |
| NeoTrellis (x2) | 3.3V | ~200mA | I2C powered |
| NeoPixels (28) | 5V | ~1.68A | At full brightness |
| Encoders | 3.3V | ~5mA each | Pull-up resistors |
| Sensors | 3.3V | ~10mA total | Analog inputs |

**Recommended PSU**: 5V/3A minimum for full system

---

## Wiring Notes

### Critical Connections
1. **I2C Pull-ups**: 4.7kΩ resistors on SDA/SCL lines
2. **NeoPixel Power**: Separate 5V supply recommended for LEDs
3. **Encoder Debouncing**: Hardware debouncing capacitors recommended
4. **Piezo Protection**: 1MΩ resistors to prevent damage
5. **Ground Plane**: Common ground for all components

### Shield Integration
- **Raspberry Pi Connection**: UART/USB interface for GUI communication
- **Audio Interface**: Consider I2S for high-quality audio if needed
- **Expansion Headers**: Use remaining pins for future features

---

## Software Configuration Files

- **Pin Definitions**: `include/Config.h`
- **MIDI Commands**: `include/MidiCommands.h` 
- **Hardware Classes**: `lib/*/` directories
- **Main Control**: `src/main.cpp` and `src/Hardware.cpp`