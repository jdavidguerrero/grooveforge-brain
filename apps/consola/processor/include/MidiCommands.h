#pragma once
#include "shared/Config.h"
#include "LiveControllerStates.h"

// ==============================================================
// Tabla de comandos SysEx (0x00-0x7F) totalmente 7-bit-safe
// Basada en la nueva asignación del script PushClone
// ==============================================================

// === SISTEMA & NAVEGACIÓN (0x00-0x0F) ===
#define CMD_HANDSHAKE        0x00
#define CMD_HANDSHAKE_REPLY  0x01
#define CMD_DISCONNECT       0x02
#define CMD_PING_TEST        0x03
#define CMD_SWITCH_VIEW      0x04
#define CMD_VIEW_STATE       0x05
#define CMD_SELECTED_TRACK   0x06
#define CMD_SELECTED_SCENE   0x07
#define CMD_DETAIL_CLIP      0x08
#define CMD_BROWSER_MODE     0x09
#define CMD_RING_NAVIGATE    0x0A
#define CMD_RING_SELECT      0x0B
#define CMD_RING_POSITION    0x0C
#define CMD_TRACK_SELECT     0x0D
#define CMD_SCENE_SELECT     0x0E
#define CMD_SESSION_MODE     0x0F

// === CLIP & SCENE (0x10-0x1F) ===
#define CMD_CLIP_STATE           0x10
#define CMD_CLIP_TRIGGER         0x11
#define CMD_SCENE_FIRE           0x12
#define CMD_CLIP_STOP            0x13
#define CMD_CLIP_NAME            0x14
#define CMD_CLIP_LOOP            0x15
#define CMD_CLIP_MUTED           0x16
#define CMD_CLIP_WARP            0x17
#define CMD_CLIP_START           0x18
#define CMD_CLIP_END             0x19
#define CMD_SCENE_STATE          0x1A
#define CMD_SCENE_NAME           0x1B
#define CMD_SCENE_COLOR          0x1C
#define CMD_SCENE_IS_TRIGGERED   0x1D
#define CMD_CLIP_PLAYING_POSITION 0x1E
#define CMD_CLIP_LENGTH          0x1F

// === MIXER & TRACK (0x20-0x2F) ===
#define CMD_MIXER_STATE        0x20
#define CMD_MIXER_VOLUME       0x21
#define CMD_MIXER_PAN          0x22
#define CMD_MIXER_MUTE         0x23
#define CMD_MIXER_SOLO         0x24
#define CMD_MIXER_ARM          0x25
#define CMD_MIXER_SEND         0x26
#define CMD_TRACK_NAME         0x27
#define CMD_TRACK_COLOR        0x28
#define CMD_TRACK_PLAYING_SLOT 0x29
#define CMD_TRACK_FIRED_SLOT   0x2A
#define CMD_TRACK_FOLD_STATE   0x2B
#define CMD_TRACK_CROSSFADE    0x2C
#define CMD_TRACK_CUE_VOLUME   0x2D
#define CMD_TRACK_METER        0x2E
#define CMD_CPU_USAGE          0x2F

// === DEVICE & RACK (0x30-0x3F) ===
#define CMD_DEVICE_LIST        0x30
#define CMD_DEVICE_SELECT      0x31
#define CMD_DEVICE_PARAMS      0x32
#define CMD_PARAM_CHANGE       0x33
#define CMD_PARAM_VALUE        0x34
#define CMD_DEVICE_ENABLE      0x35
#define CMD_DEVICE_PREV_NEXT   0x36
#define CMD_PARAM_PAGE         0x37
#define CMD_RACK_MACRO         0x38
#define CMD_DEVICE_CHAIN       0x39
#define CMD_CHAIN_SELECT       0x3A
#define CMD_CHAIN_MUTE         0x3B
#define CMD_CHAIN_SOLO         0x3C
#define CMD_CHAIN_VOLUME       0x3D
#define CMD_CHAIN_PAN          0x3E
#define CMD_CHAIN_SEND         0x3F

// === TRANSPORTE & AUTOMACIÓN (0x40-0x4F) ===
#define CMD_TRANSPORT_PLAY       0x40
#define CMD_TRANSPORT_RECORD     0x41
#define CMD_TRANSPORT_LOOP       0x42
#define CMD_TRANSPORT_TEMPO      0x43
#define CMD_TRANSPORT_SIGNATURE  0x44
#define CMD_TRANSPORT_POSITION   0x45
#define CMD_TRANSPORT_METRONOME  0x46
#define CMD_TRANSPORT_OVERDUB    0x47
#define CMD_TRANSPORT_PUNCH      0x48
#define CMD_TRANSPORT_STATE      0x49
#define CMD_NUDGE                0x4A
#define CMD_RE_ENABLE_AUTOMATION 0x4B
#define CMD_BACK_TO_ARRANGER     0x4C
#define CMD_UNDO                 0x4D
#define CMD_REDO                 0x4E
#define CMD_CAPTURE_MIDI         0x4F

// === NOTAS & SECUENCIADOR (0x50-0x5F) ===
#define CMD_NOTE_ON                 0x50
#define CMD_NOTE_OFF                0x51
#define CMD_SCALE_CHANGE            0x52
#define CMD_SCALE_INFO              0x53
#define CMD_OCTAVE_CHANGE           0x54
#define CMD_OCTAVE_INFO             0x55
#define CMD_STEP_SEQUENCER_STATE    0x56
#define CMD_STEP_SEQUENCER_NOTE     0x57
#define CMD_STEP_SEQUENCER_RESOLUTION 0x58
#define CMD_STEP_SEQUENCER_PAGE     0x59
#define CMD_STEP_SEQUENCER_HOLD     0x5A
#define CMD_STEP_EDIT_PARAMS        0x5B
#define CMD_STEP_SEQUENCER_INFO     0x5C
#define CMD_STEP_CLEAR_ALL          0x5D
#define CMD_STEP_COPY_PAGE          0x5E
#define CMD_LOOP_MARKERS            0x5F

// === GRID, GROOVE & QUANTIZE (0x60-0x6F) ===
#define CMD_GRID_UPDATE             0x60
#define CMD_GRID_SINGLE_PAD         0x61
#define CMD_GRID_PAD_PRESS          0x62
#define CMD_SESSION_OVERVIEW        0x63
#define CMD_SESSION_OVERVIEW_GRID   0x64
#define CMD_DRUM_RACK_STATE         0x65
#define CMD_DRUM_PAD_STATE          0x66
#define CMD_GROOVE_AMOUNT           0x67
#define CMD_GROOVE_TEMPLATE         0x68
#define CMD_GROOVE_POOL             0x69
#define CMD_RECORD_QUANTIZATION     0x6A
#define CMD_TRANSPORT_QUANTIZE      0x6B
#define CMD_MIDI_CLIP_QUANTIZE      0x6C
#define CMD_QUANTIZE_CLIP           0x6D
#define CMD_QUANTIZE_NOTES          0x6E
#define CMD_CUE_POINT               0x6F

// === ACCIONES DE CANCIÓN / CLIP (0x70-0x7F) ===
#define CMD_CREATE_AUDIO_TRACK      0x70
#define CMD_CREATE_MIDI_TRACK       0x71
#define CMD_CREATE_RETURN_TRACK     0x72
#define CMD_CREATE_SCENE            0x73
#define CMD_DUPLICATE_TRACK         0x74
#define CMD_DUPLICATE_CLIP          0x75
#define CMD_CLIP_DELETE             0x76
#define CMD_CLIP_COPY               0x77
#define CMD_CLIP_PASTE              0x78
#define CMD_CLIP_DUPLICATE_RESULT   0x79
#define CMD_CLIP_DELETE_RESULT      0x7A
#define CMD_CLIP_COPY_RESULT        0x7B
#define CMD_CLIP_PASTE_RESULT       0x7C
#define CMD_MIDI_NOTES              0x7D
#define CMD_MIDI_NOTE_ADD           0x7E
#define CMD_MIDI_NOTE_REMOVE        0x7F

// === LED / GRID (Teensy ↔ NeoTrellis) ===
#define CMD_LED_GRID_UPDATE    0xA0
#define CMD_LED_PAD_UPDATE     0xA1
#define CMD_LED_UI_STATE       0xA2
#define CMD_ENABLE_KEYS        0xA3
#define CMD_DISABLE_KEYS       0xA4
#define CMD_UART_CONFIRMATION_ANIMATION 0xA5
#define CMD_LED_GRID_UPDATE_14 0xA6
#define CMD_LED_PAD_UPDATE_14  0xA7
#define CMD_LED_CLIP_STATE     0x80
#define CMD_LED_TRACK_STATE    0x81
#define CMD_LED_TRANSPORT_STATE 0x82
#define CMD_LED_DEVICE_STATE   0x83
#define CMD_LED_RGB_STATE      0x84
#define CMD_SHIFT_STATE        0x88

// === ALIAS / COMPATIBILIDAD ===
#define CMD_PING CMD_PING_TEST
#define CMD_NEOTRELLIS_CLIP_GRID CMD_GRID_UPDATE
#define CMD_NEOTRELLIS_GRID      CMD_GRID_SINGLE_PAD
#define CMD_TRANSPORT            CMD_TRANSPORT_STATE
#define CMD_METRONOME            CMD_TRANSPORT_METRONOME
#define CMD_AUTOMATION_RECORD    CMD_TRANSPORT_OVERDUB
#define CMD_ARRANGEMENT_RECORD   CMD_TRANSPORT_OVERDUB
#define CMD_CLIP_DUPLICATE       CMD_DUPLICATE_CLIP

// Compatibilidad con código legado (alias)
#define CMD_TRACK_VOLUME   CMD_MIXER_VOLUME
#define CMD_TRACK_PAN      CMD_MIXER_PAN
#define CMD_TRACK_MUTE     CMD_MIXER_MUTE
#define CMD_TRACK_SOLO     CMD_MIXER_SOLO
#define CMD_TRACK_ARM      CMD_MIXER_ARM
#define CMD_TRACK_SEND_A   CMD_MIXER_SEND
#define CMD_TRACK_SEND_B   CMD_MIXER_SEND

// === ALIAS PARA GUI INTERFACE ===
#define CMD_TEMPO              CMD_TRANSPORT_TEMPO
#define CMD_PLAY               CMD_TRANSPORT_PLAY
#define CMD_STOP               CMD_CLIP_STOP
#define CMD_RECORD             CMD_TRANSPORT_RECORD

// === MIXER MODE CONTROL ===
#define CMD_MIXER_MODE         0x98  // Mixer parameter mode change (Volume/Pan, Sends, etc.)
#define CMD_MIXER_BANK_CHANGE  0x99  // GUI → Teensy: bank change notification for fader pickup

// === BULK SESSION RING COMMANDS ===
#define CMD_SESSION_RING_METADATA 0x9A  // Bulk session ring metadata (tracks/scenes names+colors) (Live → Hardware)
#define CMD_SESSION_RING_CLIPS    0x9B  // Bulk session ring clips (32 clips states+colors) (Live → Hardware)

// === NAVEGACIÓN DE GRID (para GUI) ===
#define CMD_GRID_SHIFT_LEFT    0xB0
#define CMD_GRID_SHIFT_RIGHT   0xB1
#define CMD_GRID_SHIFT_UP      0xB2
#define CMD_GRID_SHIFT_DOWN    0xB3
