# GroovePush — Hardware (KiCad)

Modular hierarchical schematic for the GroovePush controller. See
[ADR-0001](../specs/adr/0001-modular-pcb-topology.md) for the topology decision.

- **Project**: `GroovePush.kicad_pro`
- **KiCad**: v10 (files `version 20260101`)
- **Root sheet**: `GroovePush.kicad_sch` → 4 sub-sheets (one per module / future PCB):
  `processor.kicad_sch`, `encoders.kicad_sch`, `analog.kicad_sch`, `leds.kicad_sch`.

## System topology

```
        ┌────────────────── PROCESSOR BOARD ──────────────────┐
        │  Teensy 4.1 (J1 socket)                              │
 +5V ──▶│  ├─ I2C ─ 2× MCP23017 (U1 0x20 sw, U2 0x21 btn)      │
 PSU  J4│  ├─ 4.7k I2C pull-ups (R1,R2)                        │
        │  └─ decoupling C1/C2/C3                              │
        │   J2      J3       J5/J8      J6        J7      J20   │
        └───┼───────┼─────────┼─────────┼─────────┼───────┼────┘
        I2C │  UART │  ribbon │ ribbon  │  3-wire │ ribbon│
        ┌───▼──┐ ┌──▼───┐ ┌───▼────┐ ┌──▼─────┐ ┌─▼────┐ │ panel
        │Neo-  │ │ RPi5 │ │ENCODER │ │ ANALOG │ │ LED  │ │ buttons
        │Trellis│ │ GUI │ │ board  │ │ board  │ │board │
        └──────┘ └──────┘ └────────┘ └────────┘ └──────┘
```

Each sub-sheet is electrically self-contained and exposes a header — it can become its
own PCB. The inter-board wiring is the **connector contract** in the table below.

## Bill of Materials (initial)

### Processor board (`processor.kicad_sch`)
| Ref | Value | Part | Notes |
|-----|-------|------|-------|
| J1 | Teensy 4.1 socket | 2×24 female header | The Teensy is a module; socket pinout must be mapped to the real Teensy 4.1 footprint before layout |
| U1 | MCP23017 @ 0x20 | I²C 16-bit I/O expander | 8 encoder push-switches (GPA0–6 + GPB0) |
| U2 | MCP23017 @ 0x21 | I²C 16-bit I/O expander | 8 extra panel buttons (GPA0–6 + GPB0) |
| R1, R2 | 4.7 kΩ | I²C pull-ups | SDA / SCL to +3V3 |
| C1, C2 | 100 nF | Decoupling | 1 per MCP23017 |
| C3 | 10 µF | Bulk | +5V rail |
| J2 | 1×4 | NeoTrellis I²C | SDA, SCL, +3V3, GND |
| J3 | 1×4 | RPi5 UART | TX, RX, GND, GND |
| J4 | 1×2 | 5 V power in | +5V, GND |
| J5 | 2×8 | Encoder A/B ribbon | 16 quadrature lines |
| J6 | 1×12 | Analog board | 4 fader + 4 piezo + 2 IR + 3V3 + GND |
| J7 | 1×3 | LED board | LED_DATA, +5V, GND |
| J8 | 1×10 | Encoder switches | 8 switches + GND |
| J20 | 1×10 | Panel buttons | 8 buttons + 2 GND |

### Encoder board (`encoders.kicad_sch`)
| Ref | Value | Part | Notes |
|-----|-------|------|-------|
| RE1–RE8 | EC11 | Rotary encoder + switch | A/B quadrature + push |
| J9 | 2×8 | A/B to processor | mates J5 |
| J10 | 1×10 | Switches to processor | mates J8 |

### Analog board (`analog.kicad_sch`)
| Ref | Value | Part | Notes |
|-----|-------|------|-------|
| RV1–RV4 | ALPS B50K | Slide potentiometer (fader) | wiper → ADC |
| J12–J15 | 1×2 | Piezo element | signal + GND |
| R3–R6 | 1 MΩ | Piezo bleed | across each piezo |
| J16, J17 | 1×3 | Sharp IR sensor | +3V3, GND, OUT |
| J11 | 1×12 | To processor | mates J6 |

### LED board (`leds.kicad_sch`)
| Ref | Value | Part | Notes |
|-----|-------|------|-------|
| U3 | SN74LVC245A | Level shifter 3.3→5 V | data line to strip |
| R7 | 330 Ω | Series | data protection |
| D1 | WS2812B | First pixel (strip = 28) | DIN←data, DOUT→strip |
| C4 | 1000 µF | Bulk | +5V near LEDs |
| C5 | 100 nF | Decoupling | U3 |
| J18 | 1×3 | To LED strip | +5V, DATA, GND |
| J19 | 1×3 | To processor | mates J7 |

### Raspberry Pi 5 accessories (off-board — not on the custom PCB)
> See [ADR-0002](../specs/adr/0002-dual-mode-standalone-direction.md) — direction only,
> for the future **Standalone mode**. These sit on the RPi5's own 40-pin GPIO header, a
> different physical location from the processor board above; no KiCad schematic change.

| Item | Part | Notes |
|---|---|---|
| Audio output | RaspiAudio **Audio+ V2** | HAT DAC, I2S over 40-pin header, 112 dB SNR, 32-bit/384 kHz PCM, 3.5 mm + RCA stereo out. **Output only** (confirmed vs. manufacturer spec — no mic/line input). RPi5-compatible. **In BOM now.** |
| Audio input | RaspiAudio **Mic+** | Planned, not yet acquired. Needed for standalone audio input and any future controller-mode "listen to other instruments" idea. **Not yet in BOM.** |

## Inter-board connector contract

| Processor conn | Mates | Signals | Cable |
|---|---|---|---|
| J2 | NeoTrellis board | I2C_SDA, I2C_SCL, +3V3, GND | 4-wire |
| J3 | Raspberry Pi 5 | TEENSY_TX1→RPi_RX, TEENSY_RX1→RPi_TX, GND, GND | 4-wire (do **not** power the Pi from here) |
| J4 | 5 V PSU | +5V, GND | 5 V / 3 A min |
| J5 ↔ J9 | Encoder board | ENC1..8 A/B (16) | 16-way ribbon |
| J8 ↔ J10 | Encoder board | ENC_SW1..8, GND | 10-way ribbon |
| J6 ↔ J11 | Analog board | FADER1..4, PIEZO1..4, IR1..2, +3V3, GND | 12-way |
| J7 ↔ J19 | LED board | LED_DATA, +5V, GND | 3-wire |
| J20 | Panel buttons | BTN1..8, GND | 10-way |

## Power budget (from `apps/processor/PIN_DISTRIBUTION.md`)
| Rail | Load | Current |
|---|---|---|
| +5V | NeoPixels (28 @ full) | ~1.7 A |
| +5V | Teensy 4.1 | ~0.15 A |
| +3V3 | 2× NeoTrellis, MCP, encoders, sensors | ~0.25 A |
| **Total** | | **5 V / 3 A PSU minimum** |

## Verification status
Connectivity validated via the KiCad MCP net lister (all nets ≥ 2 pins; power rails carry
`PWR_FLAG`; unused MCP/level-shifter pins marked no-connect). Full ERC/netlist export via
`kicad-cli` was **not run in this environment** (no `kicad-cli` on PATH). Open the project
in KiCad 10 and run ERC before layout.

## Known issues / next steps (see `../GAPS_AND_IMPROVEMENTS.md`)
1. **Footprints not yet assigned** — this is schematic-only. Assign before PCB layout.
2. **Teensy socket pin mapping** — J1 pins are logical; map to the real Teensy 4.1
   footprint (verify each GPIO ↔ socket position) before layout.
3. **I2C vs UART NeoTrellis** — this design uses I²C NeoTrellis (0x2E/0x2F) per the user's
   build; the firmware's `UART_CONNECTIONS.md` (NeoTrellis **M4** over UART) is an
   alternative. Reconcile firmware `Config.h` accordingly.
4. **Sharp IR level** — powered from +3V3; select a 3.3 V-compatible rangefinder, or add a
   divider if using a 5 V GP2Y0A part (its output can exceed 3.3 V).
5. **MCP23017 GPA7/GPB7** are output-only (erratum) — intentionally not used as inputs.
6. **Encoder A/B pull-ups** — rely on Teensy internal pull-ups; add external 10 k if noisy.
