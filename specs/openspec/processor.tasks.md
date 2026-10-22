# Processor — Task List

> Derived from: `processor.md` (SPEC-PROC) + `../../ROADMAP.md`.
> Do tasks top to bottom, one at a time. Check the box only with verification evidence.

## Block 0 — Source of truth (P0 unblocker)

| Task | Satisfies | Agent | Verify by | Done |
|------|-----------|-------|-----------|:----:|
| T-B0-1 — Reconcile encoder pin map (single conflict-free map in `PIN_MAP.md` + `Config.h`) | G-P1 / SPEC-PROC C-3 | firmware-engineer | `pio run -e teensy41` **SUCCESS** (24 s, 2026-07-16); one coherent map | ☑ |
| ~~T-B0-2 — Decide NeoTrellis link = I2C (0x2E/0x2F); park UART-M4 path~~ **Reversed 2026-10-19**: the grid is a NeoTrellis M4 over UART (G-P2) | G-P2 | hardware-engineer | superseded by T-B0-7 | ✗ |
| T-B0-3 — Remove orphan `include/Config.h` (dead, contradictory values) | G-P3 | firmware-engineer | nothing includes bare `Config.h`; `pio run -e teensy41` SUCCESS | ☑ |
| T-B0-4 — Protoboard wiring diagram + powered bench (Teensy LED blinks) | G-H(bench) | industrial-designer | photo of powered protoboard; onboard LED blink sketch runs | ☐ |
| T-B0-5 — Narrow Phase 1 BOM/scope: drop piezo/IR/theremin (future e-drum) and capacitive buttons (replaced by MCP pushbuttons); defer LED strip to PCB | G-P11 | integration-architect | `PIN_MAP.md` "Phase 1 scope" section states it; user-confirmed | ☑ |
| T-B0-6 — Fix MCP23017 button pin allocation (avoid GPA7/GPB7 erratum) + `NUM_ENCODER_BUTTONS` 4→8 | G-P12 | firmware-engineer | `ButtonManager.cpp`/`.h` use lib pins 0-6+8 only; matches `hardware/processor.kicad_sch` net lister output; `pio run -e teensy41` SUCCESS | ☑ |
| T-B0-7 — Re-map to the GrooveForge Brain carrier: GUI on Serial1 (J6), NeoTrellis M4 on Serial5 (J15), encoders/faders on J5, LED data on 28; ports centralised in `Config.h`; fixes G-P14 | G-H10, G-P14 | firmware-engineer | `teensy41`, `test_encoders_teensy`, `neotrellis_m4` SUCCESS (2026-10-19); `PIN_MAP.md` J5 table matches the carrier netlist | ☑ |
| T-B0-8 — Shield adapter **PCB-S** for J15/J16 (`hardware/boards/pcb-s`, DRC 0/0/0, ordered with the panel boards): keyed JST to the M4 (Serial5) and PCB-A J2 (I2C); the pull-ups (now 2.2 kΩ) moved to PCB-A | G-H10 | hardware-engineer | M4 handshake + I2C scan finds 0x20/0x21 through the adapter | ☐ |
| T-B0-9 — Order the Brain carrier with an assembly quantity that includes the GroovePush copy; cut the Teensy VUSB–VIN pads | G-H10 | user | carrier in hand, Teensy powered from J10 | ☐ |

**Evidence (T-B0-1/3)**: `teensy41 SUCCESS 00:00:24` — FLASH 72976 B, RAM1 vars 22720 B.
**Evidence (T-B0-6)**: KiCad net lister confirms `U1`/`U2` both use pins `1,21-27` (lib
pins `8,0-6`) for `ENC_SW1-8`/`BTN1-8` — `ButtonManager.cpp` now matches exactly. Rebuilt
clean after a disk-space incident (see notes below): `teensy41 SUCCESS 00:29:21` — same
FLASH/RAM footprint (72976 B / 22720 B), confirming the fix is a logic-only change.

## Block 1 — MIDI 101 (next)

| Task | Satisfies | Agent | Verify by | Done |
|------|-----------|-------|-----------|:----:|
| T-B1-1 — One button → USB-MIDI note-on/off | R-PROC-1 | firmware-engineer | Ableton MIDI monitor shows the note | ☐ |
| T-B1-2 — Basic velocity curve | R-PROC-1 | firmware-engineer | different velocity by force/timing | ☐ |
| T-B1-3 — Measure pad→note latency | R-PROC-1 (AC-1) | qa-hardware-test | logged < 5 ms | ☐ |

> Blocks 2–12: generate their task rows with `/tasks processor` (and `/tasks gui`) as each
> block starts. Full plan in `../../ROADMAP.md`.

## Notes / decisions while implementing
- Authoritative pin map lives in `apps/processor/PIN_MAP.md`; `Config.h` mirrors it.
- I2C bus = default `Wire` (pins 18/19); buttons on 2× MCP23017 free the analog pins.
- Encoders avoid Serial1 (0/1 → RPi), I2C (18/19), faders (14–17), piezo/IR (20–25).
- **Phase 1 scope narrowed (2026-07-16)**: piezo/IR/theremin and capacitive buttons
  removed from GroovePush (future e-drum module); WS2812B strip deferred to PCB. See
  `PIN_MAP.md` "Phase 1 scope" and `GAPS_AND_IMPROVEMENTS.md` G-P11/G-P12.
- **Disk-space incident during this block**: the dev machine's real disk hit 100%
  (408/460 GB) mid-edit, causing one `Edit` to fail (ENOSPC) and a build to hang. Cleared
  `~/.platformio/packages` (4.4 GB, user-approved) to recover; killed a stale/stuck build
  process from before the cleanup to avoid a concurrent-write race on `.pio/build/`. No
  data was lost; the failed edit was retried cleanly.
