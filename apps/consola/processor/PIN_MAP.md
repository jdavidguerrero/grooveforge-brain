# Teensy 4.1 — Authoritative Pin Map (single source of truth)

> **This file is the source of truth for pin assignments.** `include/shared/Config.h`
> mirrors it. `PIN_DISTRIBUTION.md` is kept for power/LED/protocol notes but its old pin
> table is **superseded here** (see Block 0 / gap G-P1, G-P2, G-P3 in `../../GAPS_AND_IMPROVEMENTS.md`).
> Reconciled 2026-07-16; **re-mapped 2026-10-19 to the GrooveForge Brain carrier board**
> (gap G-H10): every Teensy pin below reaches the outside world through that board's
> connectors (see "Carrier connector contract"). Wire the protoboard to **this** map.

## Phase 1 scope (protoboard, current)
Building **now**: NeoTrellis M4 (UART), 8× encoders (EC11 with integrated switch), 4× faders,
2× MCP23017 (8 encoder switches + transport/function buttons; target 28 inputs, ADR-0004).
**Not** in this phase:
- ❌ Piezo, IR/theremin — **removed from this device** (future standalone e-drum module).
- ❌ Capacitive touch buttons — **removed**, replaced by MCP23017 pushbuttons.
- ⏸️ WS2812B LED strip — **deferred to PCB** (parts on hand are SMD, not protoboard-friendly).
Their pin reservations below are kept so the KiCad PCB design (Fase 3) stays valid, but
nothing is wired for these on the bench right now.

## Why the old tables were wrong
- Old docs put I2C on pins **4/5** — those are **not** a hardware I2C bus on Teensy 4.1.
  The firmware already uses the default `Wire` bus = **pins 18 (SDA) / 19 (SCL)**.
- Old encoder pins overlapped the IR analog pins (24/25) and used non-existent/awkward
  pins (33/34 in `ENCODER_PINS_EXPANSION`).
- Buttons are handled by **2× MCP23017 over I2C**, so they need **no** Teensy GPIO
  (frees the analog pins the old map double-booked as "capacitive buttons").

## Reserved / fixed
| Function | Teensy pin(s) | Carrier connector | Notes |
|---|---|---|---|
| Serial1 ↔ Raspberry Pi 5 (GUI) | RX1 = **0**, TX1 = **1** | J6 (2×5 Dupont) | 115200; `TEENSY_GUI_SERIAL`. Connect **UART + GND only** — the Pi 5 has its own supply |
| Serial5 ↔ NeoTrellis M4 | TX5 = **20**, RX5 = **21** | J15 (Shield socket) | 1 Mbps; `TEENSY_M4_SERIAL`. M4 side = its 4-pin JST port (`UART_CONNECTIONS.md`) |
| I2C `Wire` (MCP23017 ×2) | SDA = **18**, SCL = **19** | J15 (Shield socket) | 2.2 kΩ pull-ups on PCB-A (R1/R2; the carrier has none) |
| LED chain data (Choc keys) | **28** | J5.26 (`LED_DIN`) | through the carrier's U1 buffer (JP1) + R3 |
| Built-in LED | **13** | J5.18 | left free (spare) |
| USB | native | panel-mount cable | class-compliant USB-MIDI to the host, or to the Brain's USB-A host |

## Encoders (8 × quadrature A/B)
`ENCODER_PINS` = `{2,3, 4,5, 9,10, 11,12, 22,24, 26,30, 31,32, 34,35}` — all on the carrier's J5.

| Encoder | A | B | J5 pins (A / B) | Stage |
|---|---|---|---|---|
| Enc1 | 2 | 3 | 5 / 7 | active |
| Enc2 | 4 | 5 | 9 / 10 | active |
| Enc3 | 9 | 10 | 13 / 15 | active |
| Enc4 | 11 | 12 | 17 / 19 | active |
| Enc5 | 22 | 24 | 8 / 21 | expansion |
| Enc6 | 26 | 30 | 23 / 25 | expansion |
| Enc7 | 31 | 32 | 28 / 29 | expansion |
| Enc8 | 34 | 35 | 27 / 24 | expansion |

Encoder **push-switches** → MCP23017 `0x20` (GPA0–6 + GPB0), not Teensy GPIO.

## MCP23017 button allocation (verified against `hardware/processor.kicad_sch`)
Each MCP23017 has 16 GPIO (GPA0-7, GPB0-7), but **GPA7/GPB7 are output-only** (silicon
erratum) → **14 usable inputs/chip max**. Phase 1 wires **8 pins/chip**: GPA0-6 (7) +
GPB0 (1), library pin numbers 0-6 and 8 (Adafruit_MCP23X17: 0-7=GPA0-7, 8-15=GPB0-7).
**ADR-0004 (2026-10-19)** fills both chips to **14/14** with GPB1-GPB6 (library pins 9-14).
**Never use library pin 7 or 15** (GPA7/GPB7) for a button.

| MCP | Addr | Pin (lib #) | Function | Phase 1 status |
|---|---|---|---|---|
| #1 | 0x20 | 0–6, 8 | ENC_1..ENC_8 (encoder push-switches) | ✅ all 8 wired now |
| #2 | 0x21 | 0 | PLAY | ✅ populated now |
| #2 | 0x21 | 1 | STOP | ✅ populated now |
| #2 | 0x21 | 2 | RECORD | ✅ populated now |
| #2 | 0x21 | 3 | LOOP | ✅ populated now |
| #2 | 0x21 | 4 | UNDO | ✅ populated now — sends `CMD_UNDO` (0x4D) → `Song.undo()` |
| #2 | 0x21 | 5 | NEW | assigned 2026-10-19 (ADR-0004); firmware handler pending |
| #2 | 0x21 | 6 | SHIFT | ✅ populated now |
| #2 | 0x21 | 8 | METRONOME | ✅ populated now — **hoy reasignado** a cambio de modo mixer, no es click de metrónomo literal (pendiente de decisión, ver `GAPS_AND_IMPROVEMENTS.md`) |

### Final allocation — 3 MCP23017 (2026-10-19, supersedes the provisional table)
The panel boards split the keys across two PCBs, and PCB-K1 alone has 15 keys (one more than a
chip's 14 usable inputs), so there are **three** MCP23017. Authoritative table:
[`hardware/boards/CONNECTORS.md`](../../hardware/boards/CONNECTORS.md).

| MCP | Addr | Board | Lib pins 0–6 (GPA0–6) | Lib pins 8–14 (GPB0–6) |
|---|---|---|---|---|
| U1 | 0x20 | PCB-A (SSOP-28, between E4 and E5) | 0: ENC_5, 1: ENC_6, 2: ENC_7, 3: ENC_8 switches; 4–6 spare | 8: ENC_1, 9: ENC_2, 10: ENC_3, 11: ENC_4 switches; 12–14 spare |
| U1 | 0x21 | PCB-K1 | 0: OCT_UP, 1: OCT_DN, 2: BANK_R, 3: BANK_L; 4–6 spare | 8: METRO, 9: LOOP, 10: RECORD, 11: STOP, 12: PLAY; 13–14 spare |
| U2 | 0x22 | PCB-K1 | UNDO, BROWSE, MIX, DEVICE, NOTE, SESSION, SHIFT | 8: DELETE, 9: DUPLICATE, 10: QUANTIZE, 11: NEW; 12–14 spare |

The 0x20 and 0x21 orders look scrambled on purpose. 0x20's SSOP sits between E4 and E5: GPB (left
side) reads ENC_1–4, GPA (right side) ENC_5–8. 0x21's SSOP sits between METRO and BANK_L, so GPB (left side)
fans out to the transport keys and GPA (right side) to navigation, outermost pin to farthest key — a
planar fan-out with no crossings (rev. 2026-10-20). Firmware maps by name, not by order.

INT: 0x20 → Teensy 40; 0x21 + 0x22 share Teensy 41 (open-drain, 10 kΩ pull-up on PCB-K1).
`ButtonManager` still implements the 2-chip Phase 1 table above until the firmware task
for 3 MCPs lands.

**Parts:** protoboard = 20 ordinary tactile switches. PCB = 20 **Kailh Choc V2** with an
**SK6812-MINI-E** under each key (same parts as the GrooveForge Brain PCB-K, ADR-0004).
Encoder switches are built into the EC11s. See `lib/ButtonManager/ButtonManager.cpp`.

## Analog (12-bit ADC)
| Function | Analog | Teensy pin | J5 pin | Status |
|---|---|---|---|---|
| Fader 1–4 (ALPS B50K) | A0–A3 | 14, 15, 16, 17 | 16, 14, 12, 11 | ✅ Phase 1 |

Piezo and IR were removed from scope (gap G-P11).

## I2C device addresses
| Address | Device |
|---|---|
| 0x20 | MCP23017 #1 — encoder switches + 6 function buttons (INT → pin 40, optional) |
| 0x21 | MCP23017 #2 — transport + 6 function buttons (INT → pin 41, optional) |

## Decision: NeoTrellis link = UART to the NeoTrellis M4 (corrected 2026-10-19)
The pad grid is one **Adafruit NeoTrellis M4** (product 3938, 120 × 62 × 8.3 mm) running its own
firmware (`src/neotrellis_m4`, env `neotrellis_m4`), linked by UART @ 1 Mbps through the M4's
4-pin JST port (`UART_CONNECTIONS.md`). The July note that called this path deprecated in
favour of I2C Seesaw boards was **wrong** — those boards are not in the build (gap G-P2).
The link runs over the M4's **JST PH-4 "STEMMA I2C" port**, but as **UART** on its SDA/SCL pins, not
the I2C protocol. The M4's **micro-USB** (native SAMD51 USB, not a UART) is used for **5 V power only**
(power-only cable from the carrier). Cap the 32 NeoPixels' brightness in the M4 firmware.
Connector edge: micro-USB, JST and TRRS are all on the same long edge of the M4 PCB
(Adafruit CAD, `Adafruit-NeoTrellis-M4-PCB-and-Enclosure`): leave room for the cables there.

## Carrier connector contract (GrooveForge Brain `portadora`, used unmodified)
Source: `grooveforge-brain/apps/hardware/kicad/studio/portadora` netlist, checked 2026-10-19.
The Brain names (B1, ENCL_A…) are its own; GroovePush only relies on the Teensy pin numbers.

**J5 — panel IDC 2×15**

| J5 | Teensy | GroovePush | J5 | Teensy | GroovePush |
|---|---|---|---|---|---|
| 1, 2 | — | +5 V | 16 | 14 | Fader 1 (A0) |
| 3, 4, 6 | — | GND | 17 | 11 | Enc4 A |
| 5 | 2 | Enc1 A | 18 | 13 | spare |
| 7 | 3 | Enc1 B | 19 | 12 | Enc4 B |
| 8 | 22 | Enc5 A | 20 | 41 | MCP #2 INT (opt.) |
| 9 | 4 | Enc2 A | 21 | 24 | Enc5 B |
| 10 | 5 | Enc2 B | 22 | 40 | MCP #1 INT (opt.) |
| 11 | 17 | Fader 4 (A3) | 23 | 26 | Enc6 A |
| 12 | 16 | Fader 3 (A2) | 24 | 35 | Enc8 B |
| 13 | 9 | Enc3 A | 25 | 30 | Enc6 B |
| 14 | 15 | Fader 2 (A1) | 26 | 28 → U1 | LED_DIN |
| 15 | 10 | Enc3 B | 27 | 34 | Enc8 A |
| | | | 28 | 31 | Enc7 A |
| | | | 29 | 32 | Enc7 B |
| | | | 30 | — | +3.3 V |

**J6 — Raspberry Pi (2×5)**: pin 8 = Teensy 0 (RX1), pin 10 = Teensy 1 (TX1), pins 6/9 = GND.
Pins 2/4 carry +5 V: **leave them unconnected** in GroovePush (the Pi 5 has its own supply).

**J15 / J16 — Audio Shield sockets (empty in GroovePush)**: J15.6 = SDA (18), J15.7 = SCL (19),
J15.8 = pin 20 (TX5), J15.9 = pin 21 (RX5), J15.11 = pin 23, J15.12 = +3.3 V, J15.13 = GND;
J16.5 = pin 8, J16.6 = pin 7, J16.14 = GND. The two rows are 15.24 mm apart (6 × 2.54), so a
**PCB-S, the GroovePush shield adapter** (`hardware/boards/pcb-s`) plugs into both and can't sit offset: it only re-wires them to
keyed JST connectors, to the M4 (Serial5) and to PCB-A J2 (I2C). The 2.2 kΩ I2C pull-ups (4.7 kΩ in the first revision; changed after the 2026-10-22 audit, D07) ended up
on PCB-A (rev. 2026-10-20).
Pins 7, 8 and 23 stay as spares there.

**Power**: carrier USB-C (J10, power-only) → 3 A polyfuse → TVS → +5 V → Teensy VIN (the
Teensy's VUSB–VIN pads must be **cut**, as on the Brain). Feeds Teensy, MCPs, key LEDs and
the M4. The Pi 5 + 7" display use their own 27 W supply; grounds are joined through J6.

## Pin usage summary (no conflicts)
```
0,1    Serial1 → Pi 5 (J6)          14-17  Faders A0-A3 (J5)
2-5    Enc1-2 (J5)                  18,19  I2C → MCP ×2 (J15)
9-12   Enc3-4 (J5)                  20,21  Serial5 → NeoTrellis M4 (J15)
13     spare (onboard LED)          22,24,26,30,31,32,34,35  Enc5-8 (J5)
28     LED chain (J5.26, buffered)  40,41  MCP INT (J5, optional)
spare via J15/J16: 7, 8, 23         unavailable: 29 (Brain CV out, R18); 6, 25, 27, 33, 36-39 (NC)
```

## Consola E4 — entorno separado `consola_s` (2026-11-09)

El mapa GroovePush anterior sigue vigente para sus entornos. E4 usa protoboard
sin portadora: Juan autoriza cable directo a pines marcados NC arriba. La futura
placa principal debe rutear todas las señales y reservas de esta tabla.
Encoders, Wire 18/19, Serial5 20/21 y LED 28 conservan cableado.

| Uso E4 | Pines / canales |
|---|---|
| FSR velocity 1–4 | 14–17/A0–A3, faders trasladados al ADS |
| MASTER, CUE, IN | 23/A9, 38/A14, 39/A15 |
| ADS1115 #1 0x48 | Wire 18/19, ch0–3: faders 1–4 |
| ADS1115 #2 0x49 | ch0–1: expresión; ch2–3: Sharp GP2Y0A41 manos 1–2 |
| Enlace S↔A reservado | Serial2 RX2=7, TX2=8; sync=37 (cable directo; 13 queda para el LED de la placa) |
| MIDI DIN reservado | Serial1 RX1=0, TX1=1; Pi usa USB en E4 |
| Pedal reservado | 6 |
| Triggers reservados | 25/A11, 27/A13, 40/A16, 41/A17 |

Las reservas no se configuran ni leen. E4 sondea los MCP sin INT: por eso puede
reservar 40/41 para triggers futuros. El GPIO 29 sigue sin uso (CV heredado).
El mapa, MIDI y calibración están en `apps/docs/consola/06-teensy-s-e4.md`.
