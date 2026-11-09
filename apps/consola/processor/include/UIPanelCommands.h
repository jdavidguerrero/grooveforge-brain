#pragma once
#include "shared/Config.h"

// === UI PANEL INTEGRATION FOR 8x4 GRID ===

// Grid Layout: 8 tracks (columns) x 4 scenes (rows) = 32 pads
// Physical NeoTrellis: X_DIM=8, Y_DIM=4
// Live Mapping: pad_index = (scene * 8) + track

// === UI PANEL COMMANDS (M4 → Teensy → Live) ===
#define CMD_UI_BROWSER      0x80  // Toggle browser panel
#define CMD_UI_DEVICE       0x81  // Toggle device panel  
#define CMD_UI_CLIP         0x82  // Toggle clip panel
#define CMD_UI_CREATE_TRACK 0x83  // Create track (0=MIDI, 1=Audio, 2=Return)
#define CMD_UI_HOTSWAP      0x84  // Enter/exit hotswap mode
#define CMD_UI_NAVIGATE     0x85  // Navigate browser (0=up, 1=down, 2=left, 3=right)
#define CMD_UI_LOAD_DEVICE  0x86  // Load selected device
#define CMD_UI_CREATE_SCENE 0x87  // Create new scene
#define CMD_UI_SHIFT        0x88  // Shift button state

// === GRID NAVIGATION COMMANDS (M4 → Teensy → Live) ===
// NOTE: CMD_GRID_SHIFT_* commands are now defined in MidiCommands.h (0xB0-0xB3)
#define CMD_GRID_BANK_UP     0x8C  // Bank up (8 scenes up)
#define CMD_GRID_BANK_DOWN   0x8D  // Bank down (8 scenes down)

// === CLIP COMMANDS (M4 → Teensy → Live) ===
#define CMD_CLIP_LAUNCH     0x90  // Launch clip at pad position
#define CMD_UI_CLIP_STOP    0x91  // Stop clip at pad position  (renamed to avoid MIDI conflict)
#define CMD_SCENE_LAUNCH    0x92  // Launch scene at row
#define CMD_UI_TRACK_STOP   0x93  // Stop all clips in track (column) - UI Panel version

// === LED FEEDBACK COMMANDS (Teensy → M4) ===
#define CMD_LED_GRID_UPDATE 0xA0  // Update entire 8x4 grid colors
#define CMD_LED_PAD_UPDATE  0xA1  // Update single pad color
#define CMD_LED_UI_STATE    0xA2  // Update UI panel state LEDs
#define CMD_ENABLE_KEYS     0xA3  // Enable key scanning
#define CMD_DISABLE_KEYS    0xA4  // Disable key scanning
#define CMD_UART_CONFIRMATION_ANIMATION 0xA5 // Teensy -> M4 to trigger animation
// Extended 24-bit color support (14-bit per channel encoding: msb/lsb per color)
#define CMD_LED_GRID_UPDATE_14 0xA6  // Payload: 32 * (Rmsb,Rlsb,Gmsb,Glsb,Bmsb,Blsb) = 192 bytes
#define CMD_LED_PAD_UPDATE_14  0xA7  // Payload: [pad, Rmsb,Rlsb,Gmsb,Glsb,Bmsb,Blsb] = 7 bytes

// === UART MESSAGE STRUCTURE ===
// [START][CMD][LEN][DATA...][CHECKSUM][END]
// START = 0xF0, END = 0xF7 (SysEx compatible)
// CMD = Command byte (0x80-0xAF)  
// LEN = Payload length (0-32)
// DATA = Command payload
// CHECKSUM = XOR of CMD+LEN+DATA
// Total max size = 40 bytes (fits UART buffer)

// === GRID COORDINATE CONVERSION ===
// Hardware pad index to Live coordinates:
// track = pad_index % 8
// scene = pad_index / 8
//
// Live coordinates to hardware pad index:  
// pad_index = (scene * 8) + track

// === SESSION RING INTEGRATION ===
// The 8x4 grid represents a "window" into Live's session
// Grid position (0,0) maps to ring offset (ring_track, ring_scene)
// Navigation commands shift this window around the session

// === COLOR MAPPING FOR LIVE INTEGRATION ===
#define UI_COLOR_BROWSER_ON   0x00FF80  // Green - Browser panel active
#define UI_COLOR_DEVICE_ON    0x8000FF  // Purple - Device panel active  
#define UI_COLOR_HOTSWAP_ON   0xFF8000  // Orange - Hotswap mode active
#define UI_COLOR_GRID_FOCUS   0x0080FF  // Blue - Grid focused/selected
#define UI_COLOR_NAV_FEEDBACK 0xFFFF80  // Yellow - Navigation feedback

// === HARDWARE BUTTON ASSIGNMENTS (Future V2 Hardware) ===
// These would be mapped to physical buttons/encoders:
#define BTN_BROWSER    0  // Browser panel toggle
#define BTN_DEVICE     1  // Device panel toggle  
#define BTN_CLIP       2  // Clip panel toggle
#define BTN_CREATE     3  // Create track/scene
#define BTN_HOTSWAP    4  // Hotswap mode toggle
#define ENC_NAVIGATE   0  // Navigation encoder
#define ENC_BROWSE     1  // Browser navigation encoder

// === DEBUG SETTINGS ===
#define DEBUG_UI_COMMANDS     // Enable UI command logging
#define DEBUG_GRID_MAPPING    // Enable grid mapping debug
#define DEBUG_LED_FEEDBACK    // Enable LED update logging
