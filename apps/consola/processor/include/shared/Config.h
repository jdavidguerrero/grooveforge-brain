#pragma once

// === HARDWARE CONFIGURATION ===
// Grid Layout: 8 tracks (columns) × 4 scenes (rows) = 32 pads
#define GRID_TRACKS 8   // 8 tracks (columns) - horizontal
#define GRID_SCENES 4   // 4 scenes (rows) - vertical  
#define TOTAL_KEYS (GRID_TRACKS * GRID_SCENES) // 32 keys total

// Physical NeoTrellis mapping (8x4 orientation)
#define Y_DIM GRID_SCENES // 4 rows (scenes)
#define X_DIM GRID_TRACKS // 8 columns (tracks)
#define I2C_START_ADDR 0x2E // Legacy Seesaw address, unused: the pad grid is a NeoTrellis M4 over UART
// I2C bus = default Wire on Teensy 4.1: SDA=pin 18, SCL=pin 19 -> the two MCP23017 only.
// On the Brain carrier these reach the empty Audio Shield socket J15; the 4.7k pull-ups live on
// the GroovePush adapter, not on the carrier. See PIN_MAP.md.

// === MIDI CONFIGURATION ===
#define SYSEX_START 0xF0
#define SYSEX_END 0xF7
#define MANUFACTURER_ID 0x7F // Universal Non-Commercial (matches Live Remote Script)
#define DEVICE_ID 0x00       // Device channel (matches Live Remote Script)

// === LED BRIGHTNESS ===
#define LED_BRIGHTNESS 100 // 0-255
#define NEOPIXEL_PIN 28    // LED chain data: Brain carrier U1 buffer -> R3 -> J5.26 (LED_DIN). See PIN_MAP.md.

// === COLORS (RGB values) ===
#define COLOR_EMPTY 0x202020     // Dim white
#define COLOR_LOADED 0xFF4000    // Orange
#define COLOR_PLAYING 0x00FF00   // Green
#define COLOR_RECORDING 0xFF0000 // Red
#define COLOR_SELECTED 0x0080FF  // Blue
#define COLOR_TRIGGERED 0xFFFF00 // Yellow

// === UART COMMUNICATION ===
// Teensy port assignment on the GrooveForge Brain carrier (gap G-H10):
//   - Raspberry Pi 5 (GUI): Serial1, pins 0/1, carrier header J6.
//   - NeoTrellis M4 (pads): Serial5, TX5 = pin 20, RX5 = pin 21, via Audio Shield socket J15.
#define TEENSY_GUI_SERIAL Serial1
#define TEENSY_M4_SERIAL  Serial5
#define GUI_BAUD_RATE 115200
// Teensy <-> M4 link: 1 Mbps
#define UART_BAUD_RATE 1000000  // 1 Mbps - Reliable low-latency link
#define UART_BUFFER_SIZE 256
// Heartbeat ping from Teensy to M4
#define UART_PING_INTERVAL_MS 30000     // Send PING every 30 seconds
#define UART_LINK_TIMEOUT_MS 90000      // Consider link down after 90 seconds without RX

// === NEOTRELLIS M4 UART PINS (M4 side: its 4-pin JST port) ===
// The pad grid is an Adafruit NeoTrellis M4 linked by UART (gap G-P2, reversed 2026-10-19).
#define UART_RX_PIN 21  // M4 SDA pad, used as RX
#define UART_TX_PIN 22  // M4 SCL pad, used as TX

// === ABLETON INTEGRATION ===
#define ABLETON_MIDI_CHANNEL 1
#define ABLETON_SYSEX_TIMEOUT 1000

// === DEBUG FLAGS ===
// #define DEBUG_LIVE_LOG  // Enable Live command logging (disabled to reduce spam)

// === FADERS CONFIGURATION (Teensy only) ===
#define NUM_FADERS 4
#define FADER_PINS {A0, A1, A2, A3}  // 4 ALPS B50K faders
#define TRACKS_PER_BANK 4              // Tracks visible per bank (matches fader count)
#define FADER_PICKUP_THRESHOLD 3       // ±3 MIDI units for pickup mode (2.4% tolerance)

// === ENCODERS CONFIGURATION (Teensy only) ===
// Authoritative map: see PIN_MAP.md. All A/B pins come through the Brain carrier's panel
// IDC J5. Kept off Serial1 (0/1, GUI), Serial5 (20/21, M4), I2C (18/19), faders (14-17),
// LED data (28) and pin 13 (onboard LED). Encoder switches are on MCP23017 0x20.
#define NUM_ENCODERS_ACTIVE 4          // Currently wired encoders (build up to 8)
#define NUM_ENCODERS_MAX 8             // Maximum encoders supported
// Format: {Enc1_A, Enc1_B, Enc2_A, Enc2_B, Enc3_A, Enc3_B, Enc4_A, Enc4_B}
#define ENCODER_PINS_ACTIVE {2, 3, 4, 5, 9, 10, 11, 12}
// Encoders 5-8 (Enc5_A,B ... Enc8_A,B)
#define ENCODER_PINS_EXPANSION {22, 24, 26, 30, 31, 32, 34, 35}

// === MCP23017 I/O EXPANDER CONFIGURATION (Teensy only) ===
// Capacity: 16 GPIO/chip, but GPA7/GPB7 are output-only (erratum) -> 14 usable inputs/chip
// max. This build wires 8 pins/chip (GPA0-6 + GPB0), matching hardware/processor.kicad_sch
// exactly (verified via KiCad net lister 2026-07-16). See PIN_MAP.md.
#define MCP_ENCODER_BUTTONS_ADDR 0x20  // MCP23017 #1: encoder push-switches (EC11 integrated)
#define MCP_EXTRA_BUTTONS_ADDR 0x21    // MCP23017 #2: transport/extra buttons
// Interrupt lines (optional, not used by ButtonManager yet): J5.22 / J5.20 on the Brain carrier
#define MCP_ENCODER_BUTTONS_INT_PIN 40
#define MCP_EXTRA_BUTTONS_INT_PIN 41
#define NUM_ENCODER_BUTTONS 8          // 1:1 with the 8 encoders (was wrongly 4 - fixed)
#define NUM_EXTRA_BUTTONS 8            // 4 populated now (Play/Stop/Record/Loop), 4 free
#define TOTAL_MCP_BUTTONS (NUM_ENCODER_BUTTONS + NUM_EXTRA_BUTTONS)  // 16 total wired

// === BUTTON DEBOUNCE ===
#define BUTTON_DEBOUNCE_MS 50

// === ADC CONFIGURATION ===
#define ADC_RESOLUTION 12              // Teensy 4.1 supports 12-bit ADC
#define FADER_TOLERANCE 8              // Minimum change to trigger update (out of 4096)
