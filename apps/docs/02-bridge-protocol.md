# 🔌 GrooveForge Brain — Bridge Protocol v0.1

> **Communication protocol entre Teensy 4.1 ↔ ESP32-S3 ↔ GroovePilot VST3.**
> **Parent:** GFD v3.0 — Master Strategy & Spec
> **Sibling:** OpenSpec v0.3
> **Status:** v0.1 — Mayo 15, 2026

---

## 0. Overview

El Bridge Protocol es el lenguaje binario que conecta los tres componentes principales del ecosistema GrooveForge:

```
VST3 plugin ←─ USB-CDC ─→ Teensy 4.1 ←─ UART ─→ ESP32-S3 ←─ WiFi ─→ GroovePilot cloud
```

- **Teensy 4.1** es el master del protocolo
- **ESP32-S3** actúa como proxy network layer
- **VST3 plugin** se comunica con Teensy via USB-CDC virtual serial

Mismo protocolo binario sobre dos transports (UART y USB-CDC) para mantener la complejidad del firmware bajo.

---

## 1. Frame Format

### 1.1 Estructura del frame

```
┌──────┬──────┬──────┬─────────────────────┬──────┐
│ CMD  │ LEN  │ SEQ  │      PAYLOAD        │ CRC8 │
│ 1B   │ 1B   │ 1B   │     0-255 bytes     │  1B  │
└──────┴──────┴──────┴─────────────────────┴──────┘
```

| Field | Size | Description |
|---|---|---|
| CMD | 1 byte | Command ID (ver tabla 2) |
| LEN | 1 byte | Length of PAYLOAD in bytes (0-255) |
| SEQ | 1 byte | Sequence number (rolling 0-255, para ack matching) |
| PAYLOAD | 0-255 bytes | Command-specific data |
| CRC8 | 1 byte | Checksum CRC-8 polynomial 0x07 over [CMD, LEN, SEQ, PAYLOAD] |

### 1.2 Byte ordering

- **Little-endian** for multi-byte values (consistent con ARM Cortex-M7 nativo)
- Floats: IEEE 754 little-endian (4 bytes)
- Strings: UTF-8 encoded, null-terminated o length-prefixed (depending on command)

### 1.3 Transport details

**UART (Teensy ↔ ESP32-S3):**
- Baudrate: 921600
- 8N1 (8 data bits, no parity, 1 stop bit)
- Hardware flow control: none (CTS/RTS not connected)
- Buffer size: 1024 bytes RX, 1024 bytes TX

**USB-CDC (Teensy ↔ VST3):**
- Virtual serial port via TinyUSB CDC class
- No fixed baudrate (USB Full Speed)
- Buffer size: 1024 bytes per direction

---

## 2. Command Reference

### 2.1 System commands (0x00 - 0x0F)

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x00 | HEARTBEAT | empty | Periodic keepalive, 1Hz |
| 0x01 | ACK | [SEQ_acked: 1B] | Acknowledge previous command |
| 0x02 | NACK | [SEQ_nacked: 1B, REASON: 1B] | Reject previous command |
| 0x03 | GET_VERSION | empty | Request firmware version |
| 0x04 | VERSION | [major: 1B, minor: 1B, patch: 1B, build: 4B] | Response to GET_VERSION |
| 0x05 | RESET | empty | Soft reset receiver |
| 0x06 | GET_STATUS | empty | Request status |
| 0x07 | STATUS | [state: 1B, ...] | Response to GET_STATUS |

### 2.2 Engine commands (0x10 - 0x1F)

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x10 | SET_ENGINE | [engine_id: 1B] | Switch active engine (0=Moog, 1=Juno, etc) |
| 0x11 | GET_ENGINE | empty | Request current engine |
| 0x12 | ENGINE_CHANGED | [engine_id: 1B, name: string] | Notify engine changed |
| 0x13 | SET_PARAM | [param_id: 2B, value: 4B float] | Set engine parameter |
| 0x14 | PARAM_CHANGED | [param_id: 2B, value: 4B float] | Notify param change |
| 0x15 | LOAD_PRESET | [preset_id: 2B] | Load preset by ID |
| 0x16 | SAVE_PRESET | [preset_id: 2B, name: string] | Save current state as preset |
| 0x17 | PRESET_LOADED | [preset_id: 2B, name: string] | Notify preset loaded |

### 2.3 FX commands (0x20 - 0x2F)

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x20 | FX_ENABLE | [fx_id: 1B, layer: 1B, enable: 1B] | Enable/disable FX |
| 0x21 | FX_SET_PARAM | [fx_id: 1B, param_id: 1B, value: 4B float] | Set FX parameter |
| 0x22 | FX_PARAM_CHANGED | [fx_id: 1B, param_id: 1B, value: 4B float] | Notify FX param change |
| 0x23 | FX_CHAIN_LOAD | [chain_id: 1B] | Load FX chain (set of enabled FX + params) |
| 0x24 | FX_CHAIN_SAVE | [chain_id: 1B, name: string] | Save current FX chain |

### 2.4 MIDI/Audio commands (0x30 - 0x3F)

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x30 | NOTE_ON | [note: 1B, velocity: 1B, channel: 1B] | MIDI note on |
| 0x31 | NOTE_OFF | [note: 1B, channel: 1B] | MIDI note off |
| 0x32 | CC | [cc: 1B, value: 1B, channel: 1B] | MIDI CC message |
| 0x33 | PITCH_BEND | [value: 2B int14, channel: 1B] | MIDI pitch bend |
| 0x34 | TEMPO | [bpm: 2B (BPM × 10)] | Set tempo |
| 0x35 | TRANSPORT | [state: 1B] | 0=stop, 1=play, 2=pause |

### 2.5 UI commands (0x40 - 0x4F) - Teensy → ESP32

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x40 | DISPLAY_UPDATE | [region: 1B, data: bytes] | Update display region |
| 0x41 | DISPLAY_TEXT | [x: 1B, y: 1B, color: 2B, text: string] | Draw text |
| 0x42 | LED_SET | [led_id: 1B, r: 1B, g: 1B, b: 1B] | Set LED color |
| 0x43 | LED_PATTERN | [pattern_id: 1B, duration: 2B] | Run LED animation |

### 2.6 Cloud commands (0x50 - 0x5F) - ESP32 ↔ Teensy

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x50 | CLOUD_STATUS | [connected: 1B, latency: 2B] | WiFi/cloud connection status |
| 0x51 | PATCH_SEARCH | [query: string] | Request patch search |
| 0x52 | PATCH_RESULTS | [count: 1B, patches: array] | Search results |
| 0x53 | PROGRESSION_REQUEST | [context: bytes] | Request progression suggestion |
| 0x54 | PROGRESSION_RESPONSE | [chords: array] | Suggested chord progression |
| 0x55 | OTA_AVAILABLE | [version: bytes, size: 4B] | OTA update available |
| 0x56 | OTA_START | empty | Begin OTA update |
| 0x57 | OTA_PROGRESS | [percent: 1B] | OTA update progress |

### 2.7 DAW Bridge commands (0x60 - 0x6F) - Teensy ↔ VST3

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x60 | DAW_CONNECTED | [daw_name: string] | DAW handshake |
| 0x61 | MIX_SCORE | [score: 1B, ...] | Mix quality score from GP |
| 0x62 | FREQ_CONFLICT | [freq_low: 2B, freq_high: 2B] | Conflict detected in range |
| 0x63 | LAYER_SUGGEST | [type: 1B, preset_id: 2B] | Suggested layer type + preset |
| 0x64 | MACRO_BIND | [macro_id: 1B, target: bytes] | Bind macro to DAW parameter |
| 0x65 | MACRO_VALUE | [macro_id: 1B, value: 4B float] | Macro value from encoder |

### 2.8 Pogo/Slave commands (0x70 - 0x7F) - Future v1.x

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x70 | SLAVE_DETECTED | [slave_type: 1B, address: 1B] | New slave detected |
| 0x71 | SLAVE_REMOVED | [address: 1B] | Slave disconnected |
| 0x72 | SLAVE_DATA | [address: 1B, data: bytes] | Data from slave |

### 2.9 ML commands (0x80 - 0x8F) - TinyML inference results

| CMD | Name | Payload | Description |
|---|---|---|---|
| 0x80 | KEY_DETECTED | [key: 1B, confidence: 1B] | Key detection result (event-driven) |
| 0x81 | CHORD_DETECTED | [chord: 2B, confidence: 1B] | Chord recognition result (event-driven) |
| 0x82 | BEAT_DETECTED | [bpm: 2B, phase: 1B] | Beat tracker result (event-driven) |
| 0x83 | GROOVE_STATE | [activity: 12B, snap_event: 1B, snap_from: 1B, snap_to: 1B, beat_phase: 1B] | Periodic ML state @ 4Hz mientras `in_ai_mode` — chromagram EMA + snap rising-edge + bar phase. Spec completo en `apps/docs/sprints/33-ai-hero-viz.md` |
| 0x84 | GENRE_DETECTED | [genre: 1B, confidence: 1B] | Genre fingerprint result (reasignado en Sprint 33 desde 0x83) |

---

## 3. Communication Patterns

### 3.1 Request-Response with ACK

```
Sender (Teensy)            Receiver (ESP32)
     │                            │
     │── CMD 0x10 SEQ=42 ────────▶│
     │   Payload: engine=0        │
     │                            │
     │◀─ ACK SEQ=42 ──────────────│
     │                            │
     │ (state updated locally)    │
```

### 3.2 Async notification

```
Sender (Teensy)            Receiver (ESP32)
     │                            │
     │ (user turns encoder)       │
     │── CMD 0x14 SEQ=43 ────────▶│
     │   Payload: param_id=5,     │ (update display)
     │            value=0.75      │
     │                            │
     │ (no ACK expected)          │
```

### 3.3 Multi-frame transfers

For payloads >250 bytes (e.g. preset upload, OTA chunks):

```
0xF0 BEGIN_TRANSFER [total_size: 4B, transfer_id: 1B]
0xF1 CHUNK [transfer_id: 1B, chunk_index: 2B, data: bytes]
...
0xF2 END_TRANSFER [transfer_id: 1B, crc32: 4B]
```

---

## 4. Error Handling

### 4.1 NACK reasons

| Code | Meaning |
|---|---|
| 0x01 | CRC error |
| 0x02 | Invalid command ID |
| 0x03 | Invalid payload length |
| 0x04 | Invalid parameter ID |
| 0x05 | Out of memory |
| 0x06 | Operation timeout |
| 0x07 | Not connected (cloud) |
| 0x08 | Authentication failed |

### 4.2 Timeout handling

- **Default timeout:** 100ms for UART, 500ms for USB-CDC
- After timeout: retry up to 3 times with same SEQ
- After 3 retries: assume connection lost, raise error

### 4.3 Connection recovery

If heartbeat missed for >3 seconds:
1. Mark connection as DOWN
2. Cease sending until heartbeat received
3. On heartbeat received: send GET_VERSION to verify identity
4. Resume normal operation

---

## 5. Implementation Notes

### 5.1 Teensy 4.1 (master)

```cpp
// bridge/protocol.h
struct Frame {
    uint8_t cmd;
    uint8_t len;
    uint8_t seq;
    uint8_t payload[256];
    uint8_t crc8;
};

class BridgeMaster {
public:
    void send(Frame& frame);
    bool receive(Frame& frame, uint32_t timeout_ms = 100);
    void onCommand(uint8_t cmd, std::function<void(const Frame&)> handler);
};

// Usage:
bridgeMaster.onCommand(CMD_NOTE_ON, [](const Frame& f) {
    uint8_t note = f.payload[0];
    uint8_t vel = f.payload[1];
    engine.noteOn(note, vel);
});
```

### 5.2 ESP32-S3 (UART slave)

```cpp
// bridge/protocol.h - same struct as Teensy
class BridgeSlave {
public:
    void poll();  // Call in loop()
    void send(Frame& frame);
    void onCommand(uint8_t cmd, std::function<void(const Frame&)> handler);
};
```

### 5.3 VST3 plugin (USB-CDC client)

```cpp
// JUCE-based VST3
class BrainBridge : public juce::AudioProcessor {
    SerialPort port;  // OS-level serial USB-CDC

    void sendCommand(uint8_t cmd, const std::vector<uint8_t>& payload) {
        Frame f;
        f.cmd = cmd;
        f.len = payload.size();
        f.seq = nextSeq++;
        memcpy(f.payload, payload.data(), payload.size());
        f.crc8 = calculateCRC8(f);
        port.write(reinterpret_cast<uint8_t*>(&f), 4 + f.len);
    }
};
```

---

## 6. CRC-8 Implementation

```cpp
uint8_t calculateCRC8(const uint8_t* data, size_t length) {
    uint8_t crc = 0x00;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;  // polynomial 0x07
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}
```

---

## 7. Testing & Validation

### 7.1 Unit tests (firmware)

- CRC-8 calculation against known test vectors
- Frame serialization/deserialization roundtrip
- Buffer overflow protection (payload >255 rejected)
- Sequence number rollover (255 → 0)

### 7.2 Integration tests

- Teensy ↔ ESP32 UART loopback test
- Teensy ↔ VST3 USB-CDC roundtrip
- Connection recovery scenarios (cable unplug + reconnect)
- High-throughput stress (notes + display updates simultaneous)

### 7.3 Performance targets

- Frame send/receive: <500µs typical
- Round-trip latency Teensy ↔ ESP32: <2ms p95
- Round-trip latency Teensy ↔ VST3: <10ms p95
- Throughput: 1000+ frames/sec sustainable

---

## 8. Future extensions (v0.2+)

- Encryption: AES-128 for VST3 ↔ Teensy (prevent third-party spoofing)
- Compression: zlib for large preset transfers
- Streaming audio: dedicated channel for low-latency audio chunks
- BLE Bridge: add Bluetooth LE transport for mobile companion app

---

## 9. Documentos relacionados

- GFD v3.0 — Master Strategy (parent)
- OpenSpec v0.3 — Technical Spec
- AI Architecture v0.1 — Layer 2 cloud features use these commands
- Implementation Roadmap — Sprint 3.2 implements this protocol

---

*End of Bridge Protocol v0.1*
*GrooveForge Brain · Communication Spec · Juan Guerrero (GPROG)*
