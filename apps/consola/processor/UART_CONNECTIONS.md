# UART Connections - NeoTrellis M4 ↔ Teensy 4.1

## Pin Connections

> **Updated 2026-10-19 (Brain carrier, gap G-H10):** the Teensy side moved from Serial1 (0/1)
> to **Serial5 (TX5 = 20, RX5 = 21)**, reached through the carrier's empty Audio Shield socket
> J15 (J15.8 = pin 20, J15.9 = pin 21). Serial1 now goes to the Raspberry Pi on J6. The M4 side
> is unchanged. See `PIN_MAP.md`.

| NeoTrellis M4 | Function | Teensy 4.1 | Function |
|---------------|----------|------------|----------|
| Pin 21 (SDA)  | RX       | Pin 20 (TX5)| TX      |
| Pin 22 (SCL)  | TX       | Pin 21 (RX5)| RX      |
| GND           | Ground   | GND        | Ground   |

## Why Use I2C Pins for UART?

The **Adafruit NeoTrellis M4** (https://www.adafruit.com/product/3938) **ONLY** exposes these pins physically:
- USB-C connector (for programming/power)  
- I2C jumper pads: SDA (Pin 21) and SCL (Pin 22)

Since no other pins are accessible, we implement bit-banging UART on the I2C pins:

- **Pin 21 (SDA)**: Configured as UART RX using bit-banging
- **Pin 22 (SCL)**: Configured as UART TX using bit-banging

⚠️ **IMPORTANT**: This disables I2C functionality on the NeoTrellis M4

## Communication Protocol

### NeoTrellis M4 → Teensy 4.1 (Pad Events)
```
F0 10 03 05 F7
│  │  │  │  └── End SysEx
│  │  │  └──── Scene 5
│  │  └─────── Track 3  
│  └────────── CMD_CLIP_TRIGGER
└─────────────── Start SysEx
```

### Teensy 4.1 → NeoTrellis M4 (LED Feedback)
```
F0 7D 00 80 18 02 F7
│  │  │  │  │  │  └── End SysEx
│  │  │  │  │  └──── State: PLAYING
│  │  │  │  └─────── Pad Index: 24
│  │  │  └────────── CMD_LED_CLIP_STATE
│  │  └─────────────── Device ID
│  └────────────────── Manufacturer ID
└───────────────────── Start SysEx
```

## Configuration

- **Baud Rate**: 2,000,000 (2 Mbps) - High speed for low latency
- **Data Bits**: 8
- **Parity**: None  
- **Stop Bits**: 1
- **Flow Control**: None
- **Bit Duration**: 0.5 microseconds per bit

## Bit-Banging UART Implementation

NeoTrellis M4 uses manual bit-banging to implement UART on I2C pins:

```cpp
// Configure pins
pinMode(UART_RX_PIN, INPUT_PULLUP);   // Pin 21 (SDA) as RX
pinMode(UART_TX_PIN, OUTPUT);         // Pin 22 (SCL) as TX
digitalWrite(UART_TX_PIN, HIGH);      // TX idle state

// Bit timing for 2 Mbps (0.5 microseconds per bit)
#define BIT_DELAY_CYCLES 60  // 60 CPU cycles @ 120MHz = 0.5μs

void writeByte(uint8_t data) {
    __disable_irq();  // Disable interrupts for precise timing
    
    // Direct port manipulation for maximum speed
    *txPortClr = txPinMask;  // Start bit (LOW)
    delayCycles(BIT_DELAY_CYCLES);
    
    // Unrolled loop for 8 data bits (LSB first)
    if (data & 0x01) *txPortSet = txPinMask; else *txPortClr = txPinMask;
    delayCycles(BIT_DELAY_CYCLES);
    // ... repeat for all 8 bits
    
    *txPortSet = txPinMask;  // Stop bit (HIGH)
    delayCycles(BIT_DELAY_CYCLES);
    
    __enable_irq();  // Re-enable interrupts
}
```

## Hardware Notes

- Ensure proper ground connection between both boards
- **Teensy 4.1**: Hardware UART (Serial1) easily handles 2 Mbps
- **NeoTrellis M4**: Optimized bit-banging with direct port manipulation
- **SAMD51 @ 120MHz**: Sufficient speed for precise 0.5μs bit timing
- **Interrupts disabled**: During bit transmission for timing accuracy
- **Unrolled loops**: Maximum speed, no branching overhead
- **Direct port access**: Bypasses Arduino digitalWrite() overhead
- You must solder wires to the I2C jumper pads on the NeoTrellis M4
- Consider pull-up resistors (1kΩ) on both data lines for signal integrity