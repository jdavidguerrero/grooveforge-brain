# ADR-0001: Modular multi-board PCB topology

> **Status**: Accepted
> **Date**: 2026-07-16
> **Deciders**: user (hardware owner), hardware-engineer, integration-architect

## Context

The prototype was disassembled and is being rebuilt. The build is bench-mounted on a
protoboard first, then migrated to PCBs. The user's physical topology is:

- **NeoTrellis** pad grid lives on its **own board**, connected to the processor by **I2C**.
- The **Teensy 4.1** is the processor module and controls **all other peripherals**
  (encoders, faders, piezos, IR sensors, MCP23017 button expanders, NeoPixels).
- The **Raspberry Pi 5** is a separate module connected by **serial (UART)**.

We need a design that is **global** (one coherent system schematic) yet **modular**
(peripherals split so each can become its own PCB / daughterboard).

## Decision

Use a **KiCad hierarchical schematic**: one root sheet (global system view) with one
**sub-sheet per physical board / functional module**, connected through sheet pins and
hierarchical labels (buses for I2C, power, analog, encoders).

Modules:
1. `processor` — Teensy 4.1 carrier, 2× MCP23017, I2C pull-ups, power distribution,
   and all inter-board connectors (NeoTrellis I2C, RPi serial, encoder/analog/LED headers).
2. `encoders` — 8 rotary encoders (A/B quadrature to Teensy, push switches to MCP23017).
3. `analog` — 4 ALPS B50K faders, 4 piezo drum inputs (1MΩ bleed + clamp), 2 Sharp IR.
4. `leds` — WS2812B NeoPixel strip driver (level shifter, 5V rail, bulk cap, connector).

Each module exposes a header so it can be lifted onto its own PCB later. The root sheet
is the "protoboard-level" global wiring.

## Alternatives considered
- **Single main board integrating everything** — simpler ERC, but doesn't match the
  physical modular reality (separate NeoTrellis board) and makes panel-sized layout hard.
- **Fully separate KiCad projects per board** — maximal isolation, but loses the single
  global system view and duplicates the interface contract in many places.

## Consequences
- Positive: matches the physical build; each module → its own PCB; global view preserved;
  interfaces are explicit sheet pins (a real inter-board connector contract).
- Trade-off: hierarchical labels/sheet pins must stay in sync across sheets.
- Follow-ups: define the exact inter-board connector pinouts as part of `SPEC-HARDWARE`.

## Note: doc reconciliation (I2C vs UART NeoTrellis)

> **Correction (2026-10-19):** the conclusion below was wrong. The user's pad grid is a single
> **Adafruit NeoTrellis M4** (product 3938), linked by **UART** — exactly what `UART_CONNECTIONS.md`
> and the firmware (`NeoTrellisLink` on `Serial1`, env `neotrellis_m4`) describe. The `processor`
> sheet's "NeoTrellis I2C" connector (J2) must become a UART connector. Kept below for history.

`UART_CONNECTIONS.md` / `Config.h` describe a **NeoTrellis M4** linked by bit-banged
**UART** over its I2C jumper pads. The user's actual build uses **I2C NeoTrellis** boards
(Seesaw, addresses `0x2E`/`0x2F`) on the Teensy I2C bus, per `PIN_DISTRIBUTION.md`. This
schematic follows the **I2C** topology (the user's stated reality). The UART-M4 path is
recorded as an alternative and flagged in `GAPS_AND_IMPROVEMENTS.md`.
