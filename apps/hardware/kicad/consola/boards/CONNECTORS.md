# GroovePush panel boards — connector contract

> Single source of truth for every board-to-board connector. Both ends of each cable are drawn
> from this table. **All cables are 1:1** (pin n ↔ pin n). Positions on the panel:
> `../panel/panel-coordinates.csv`. Carrier side legacy: `../../apps/processor/PIN_MAP.md`.
> **Integración PCB-M aprobada 2026-11-12:** ver sección siguiente; el pinout físico de PCB-A no cambia.
> Las columnas Teensy de las tablas históricas corresponden a la portadora antigua; para PCB-M manda
> `pcb-m/README.md` y `apps/docs/consola/17-mapa-pines-teensy.md`.

## PCB-M ↔ PCB-A fabricada (D8/D9/D14/D15, aprobado por Juan 2026-11-12)

- J1_PCBA IDC 2×15 ↔ PCB-A J1 por cinta **1:1**, sin modificar el pinout de abajo. Los nombres
  ENC1–8 A/B se reparten en PCB-M según ENCODER_PINS del firmware; FADER1–4 van a ADS1115
  0x48 AIN0–3, no a los antiguos pines analógicos de la columna Teensy. Cross-reference vigente
  en [PCB-M README](pcb-m/README.md).
- En PCB-M **J1.18 NC** (spare), **.20 NC** (INT_K), **.22 NC** (INT_A), **.26 NC** (LED_DIN).
  Los MCP se sondean por I2C. LED_DIN_5V sale solo por J_PCB_K.6 hacia la nueva PCB-K única;
  no conectar simultáneamente la antigua ruta de teclas por PCB-A J4.
- J2_I2C ↔ PCB-A J2 mantiene GND / +3,3 V / SDA / SCL. **Desoldar R1/R2 de PCB-A**;
  quedan pull-ups seleccionables 2,2/4,7 kΩ en el lado B_A del PCA9517A de PCB-M (D18/D21). No alterar las otras pistas ni conectores de PCB-A.
- J_PCB_K (**10 hilos, D23**) desde PCB-M: **1 +3,3 V, 2 SDA, 3 GND, 4 SCL, 5 GND,
  6 LED_DIN_5V, 7 GND, 8 +5V_LED, 9 JOG_CLICK, 10 PEDAL_BTN**. Headers C158038 y mazo PHR-10 1:1. MCP 0x21/0x22/0x23 viven en PCB-K.
- J_5V_LED_IN (S2B-PH-K 2 vías): **1 +5V_LED_RAW, 2 GND**, desde cable USB-A del Pi.
  F1 **0,75 A** alimenta +5V_LED (incluye PCB-A J1.1/.2); +5V_TEENSY separado.
  Esta rama sustituye para PCB-M el antiguo contrato USB-C/3 A/TVS descrito al final.
  La BOM de PCB-M no incluye TVS de entrada adecuada: no se añade una pieza de señal como sustituto.
- Todos los jacks de PCB-M se montan con tuerca en el cuerpo y van por cable a un JST-PH por jack,
  pin a pin en el README de PCB-M; ninguno tiene detección en la captura actual. HP_VGND aislada.
  AGND/GND se unen solo por el net-tie NT1 junto al LDO de audio (posición a respetar en layout).

El resto de este archivo conserva los conectores **fabricados** y la topología histórica de
GroovePush (K1/K2, portadora y PCB-S); no define esa topología como vigente para PCB-M.

```
 Brain carrier J5 (IDC 2×15) ══ribbon══ PCB-A J1
 Brain carrier J15/J16 (Shield sockets) → PCB-S (shield adapter) ──JST-SH 4── PCB-A J2  (I2C)
                                                               └──JST-PH 4── NeoTrellis M4 (UART)
 PCB-A J3 (JST-SH 6)  ── PCB-F J1        faders
 PCB-A J4 (JST-SH 10) ── PCB-K1 J1       I2C, LED data, +5V, MCP INT  (~20–25 cm cable)
 PCB-K1 J2 (JST-SH 10) ── PCB-K2 J1      mode keys + LED data out/return
```

## PCB-S — shield adapter (plugs into carrier J15/J16)

Two 1×14 male headers (long pins down) sit over the carrier's empty Audio Shield sockets, pin for
pin; no parts besides the connectors. Only these socket pins are used, the rest stay unconnected:

| Header pin | Carrier socket | Teensy | Net | Goes to |
|---|---|---|---|---|
| P1.6 | J15.6 | 18 | SDA | J1.3 |
| P1.7 | J15.7 | 19 | SCL | J1.4 |
| P1.8 | J15.8 | 20 (TX5) | TX5 | J2.3 (M4 SCL = its RX22) |
| P1.9 | J15.9 | 21 (RX5) | RX5 | J2.4 (M4 SDA = its TX21) |
| P1.12 | J15.12 | — | +3V3 | J1.2 |
| P1.13 / P2.14 | J15.13 / J16.14 | — | GND | J1.1, J2.1 |

- **J1, JST-SH 4 vertical (BM04B-SRSS-TB) → PCB-A J2**, Qwiic order: GND, +3V3, SDA, SCL.
- **J2, JST-PH 4 vertical (B4B-PH-K-S) → NeoTrellis M4 CONN1**, 1:1 cable: GND, *n.c.*, TX5, RX5.
  M4 CONN1 is GND, 3.3 V, SDA, SCL (from its board file); pin 2 is the M4's own 3.3 V rail, so it
  stays unconnected here — the M4 is powered separately and only GND is shared.

## PCB-A J1 — IDC 2×15 box header (to carrier J5, ribbon 1:1)

| Pin | Net | Teensy | Pin | Net | Teensy |
|---|---|---|---|---|---|
| 1 | +5V | — | 2 | +5V | — |
| 3 | GND | — | 4 | GND | — |
| 5 | ENC1_A | 2 | 6 | GND | — |
| 7 | ENC1_B | 3 | 8 | ENC5_A | 22 |
| 9 | ENC2_A | 4 | 10 | ENC2_B | 5 |
| 11 | FADER4 | 17 (A3) | 12 | FADER3 | 16 (A2) |
| 13 | ENC3_A | 9 | 14 | FADER2 | 15 (A1) |
| 15 | ENC3_B | 10 | 16 | FADER1 | 14 (A0) |
| 17 | ENC4_A | 11 | 18 | — (spare, Teensy 13) | 13 |
| 19 | ENC4_B | 12 | 20 | INT_K (MCP 0x21/0x22) | 41 |
| 21 | ENC5_B | 24 | 22 | INT_A (MCP 0x20) | 40 |
| 23 | ENC6_A | 26 | 24 | ENC8_B | 35 |
| 25 | ENC6_B | 30 | 26 | LED_DIN (5 V, buffered on carrier) | 28 |
| 27 | ENC8_A | 34 | 28 | ENC7_A | 31 |
| 29 | ENC7_B | 32 | 30 | +3V3 | — |

## PCB-A J2 — JST-SH 4, I2C in (Qwiic / STEMMA QT order)

| 1 | 2 | 3 | 4 |
|---|---|---|---|
| GND | +3V3 | SDA | SCL |

PCB-A fabricada lleva pull-ups de 2,2 kΩ (R1/R2, 0603, C4190). **Al integrar PCB-M, desoldar R1/R2
de PCB-A**: el par activo de este segmento está en B_A del buffer de PCB-M: 2,2 kΩ por defecto, 4,7 kΩ seleccionables (D18/D21, Juan, 2026-11-12).
La configuración con la portadora anterior se conserva solo como referencia histórica.

## PCB-A J3 ↔ PCB-F J1 — JST-SH 6, faders

| 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|
| +3V3 | FADER1 | FADER2 | FADER3 | FADER4 | GND |

## PCB-A J4 ↔ PCB-K1 J1 — JST-SH 10, keys board

J4 sits on PCB-A's tab, next to J1 (rev. 2026-10-20). At the strip's left end its six lines ran
200 mm along the encoder strip and the board didn't route. The cable is therefore ~20–25 cm
(pre-crimped JST-SH, 28 AWG): it leaves the tab towards the M4 and runs under PCB-A / the M4 to
PCB-K1 J1 at the back of the edit block. At that length I2C (2.2 kΩ pull-ups on PCB-A) and the
buffered 800 kHz LED data are fine; +5V uses two pins and GND three. Esta ruta es histórica;
en la integración PCB-M, los datos LED van por J_PCB_K y R1/R2 de PCB-A se desueldan.

| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|
| +5V | +5V | GND | GND | +3V3 | SDA | SCL | LED_DIN | INT_K | GND |

## PCB-K1 J2 ↔ PCB-K2 J1 — JST-SH 10, mode keys

| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|
| +5V | GND | LED_K2_IN | LED_K2_RET | KEY_BROWSE | KEY_MIX | KEY_DEVICE | KEY_NOTE | KEY_SESSION | GND |

## MCP23017 allocation (supersedes the provisional table in PIN_MAP.md)

Each MCP reads the keys next to it (rev. 2026-10-20: the first K1 layout put both MCPs on the left
and Freerouting couldn't route 10 key lines along the 23 mm front strip). U1's keys are ordered
for a planar fan-out: GPB (the SSOP's left side) feeds the transport keys to its left and GPA (right
side) the navigation keys to its right, the pin nearest the front edge going to the nearest key.

| MCP | Addr | Board | GPA0–GPA6 | GPB0–GPB6 |
|---|---|---|---|---|
| U1 | 0x20 | PCB-A (SSOP-28, front band between E4 and E5) | GPA0–3: ENC5–ENC8 switches; GPA4–6 spare | GPB0–3: ENC1–ENC4 switches; GPB4–6 spare |
| U1 | 0x21 | PCB-K1 (SSOP-28, between METRO and BANK_L) | GPA0 OCT_UP, GPA1 OCT_DN, GPA2 BANK_R, GPA3 BANK_L; GPA4–6 spare | GPB0 METRO, GPB1 LOOP, GPB2 RECORD, GPB3 STOP, GPB4 PLAY; GPB5–6 spare |
| U2 | 0x22 | PCB-K1 (SOIC-28W, above UNDO) | UNDO, BROWSE, MIX, DEVICE, NOTE, SESSION, SHIFT | DELETE, DUPLICATE, QUANTIZE, NEW; GPB4–6 spare |

GPA7/GPB7 are never used (output-only erratum). Keys and encoder switches pull to GND; the MCP's
internal pull-ups are enabled in firmware. INT_K is shared by 0x21 and 0x22: configure both as
open-drain (`IOCON.ODR = 1`); PCB-K1 has a 10 kΩ pull-up.

## LED chain (SK6812MINI-EA, 20 LEDs, data at 5 V)

`LED_DIN` (carrier) → PCB-K1 → **PCB-K2** (indices 0–4: BROWSE, MIX, DEVICE, NOTE, SESSION) →
back to PCB-K1 → edit block (5–9: QUANTIZE, DUPLICATE, DELETE, UNDO, NEW) → front row (10–19:
SHIFT, PLAY, STOP, RECORD, LOOP, METRO, BANK_L, BANK_R, OCT_DN, OCT_UP). Cap brightness in
firmware: 20 × 60 mA at full white is 1.2 A.

## Power and handling contract (audit 2026-10-22, D03/D08)

- **Power the panel off when connecting or disconnecting any cable.** There is no inrush limiter, reverse
  block or hot-plug sequencing on these boards: the +5 V that feeds the key LEDs (20 × up to 60 mA, 1.2 A
  worst case, firmware caps brightness) comes from the carrier's USB-C input through its 3 A polyfuse and
  TVS; it is the only current limiter in the chain. Cables are 1:1 and polarised (JST-SH/PH are keyed).
- **ESD path.** Every encoder (PCB-A RE1–RE8) and fader (PCB-F RV1–RV4) mounting pad (MP) is tied to GND with
  a solid (non-thermal) zone connection; the metal shaft/frame of the part therefore drains into the GND
  plane at the pad. The enclosure must still give the front panel a short, wide bond to that plane (via a
  standoff) — to be measured on the first unit.
- **I2C**: start at 100 kHz and raise to 400 kHz only after measuring rise time (30–70 %) with the final
  harness; with the 2.2 kΩ pull-ups the bus tolerates ~160 pF at 400 kHz (300 ns).
