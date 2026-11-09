# ADR-0004: Panel buttons — +12 function buttons on the MCP23017s, Kailh Choc V2 with per-key LEDs

> **Status**: Accepted
> **Date**: 2026-10-19
> **Deciders**: user (product owner), hardware-engineer, firmware-engineer
> **Related**: ADR-0001 (modular PCB), [`apps/processor/PIN_MAP.md`](../../apps/processor/PIN_MAP.md),
> GrooveForge Brain `apps/docs/variants/studio/theory/02-studio-pcb.md` (PCB-K, Choc V2)

## Context

- Phase 1 wires 8 inputs per MCP23017: encoder push-switches on `0x20`, transport/function on
  `0x21` (PLAY, STOP, RECORD, LOOP, UNDO, SHIFT, METRONOME, one free slot on GPA5).
- Controlling Live devices and VSTs (Diva, Serum, Vital) from the encoders is already done the
  Push way — 8 parameters per page, paged (`DeviceManager.change_parameter_page`). More encoders
  are not needed for that. What is missing is fast **physical navigation**: mode switching,
  bank paging, octave, and the core editing actions.
- Each MCP23017 has **14 usable inputs** (GPA7 and GPB7 are output-only, silicon erratum), so
  6 more per chip are available without adding chips.
- GrooveForge Brain already designed, measured and prepared for fabrication a key board with
  **Kailh Choc V2** switches and **SK6812-MINI-E** LEDs mounted on the underside (PCB-K).

## Decision

1. **No extra encoders.** 8 encoders + paging, as on the Push.
2. **+12 function buttons**, using GPB1–GPB6 (library pins 9–14) on both MCP23017s, plus the
   free GPA5 slot on `0x21`. Both chips end at **14 / 14 inputs**. Any further button needs a
   third MCP23017 at `0x22` on the same I2C bus.

   | Group | Buttons |
   |---|---|
   | Modes | SESSION · NOTE · DEVICE · MIX · BROWSE |
   | Navigation | BANK ◀ · BANK ▶ · OCTAVE ▲ · OCTAVE ▼ |
   | Editing | DELETE · DUPLICATE · QUANTIZE |
   | GPA5 (`0x21`) | NEW (new clip / scene) |

   Totals: 8 encoder switches + 20 function buttons = 28 MCP inputs. The pin-to-function
   assignment per chip is **provisional** until the panel layout exists (routing follows
   physical proximity) — see `PIN_MAP.md`.
3. **On the PCB, function and transport buttons are Kailh Choc V2 with an SK6812-MINI-E under
   each key**, same part, footprint and LCSC numbers as the Brain's PCB-K. Keys light up by
   state (playing, recording, active mode), as on the Push.
4. **The 32 pads stay on the NeoTrellis silicone grid.**
5. **On the protoboard, buttons are ordinary tactile switches.** Same MCP pins and same
   firmware; only the part changes on the PCB.

## Alternatives considered

| Option | Result | Why |
|---|---|---|
| **Choc V2 + per-key LED** | ✅ | Matches the Brain; per-key state lighting; footprint, LED window (4.93 mm from centre) and stack heights already measured |
| Silicone caps over tactile switches | ❌ | Cheaper and quieter, but per-key backlighting is hard and it breaks uniformity with the Brain |
| More encoders (jog / master volume) | Not now | Not required for device control; can be added later on free Teensy GPIO (20–25, 31–41) |

## Consequences

- **LEDs come back into scope for the PCB.** Per-key SK6812-MINI-E (≈20) join the deferred
  WS2812B chain. Follow the Brain lessons: one chain on one pin, ~330 Ω series resistor at the
  first DIN, level shifter (the `leds` sheet already has an SN74LVC245A), bulk capacitor on +5V.
- **Cost:** the SK6812-MINI-E is "Standard assembly only" at JLCPCB, which forced Standard
  assembly on the Brain's PCB-K (estimated US$25–40 more there).
- **Mechanics:** Choc V2 needs a 1.2–1.3 mm plate to clip into (the aluminium top panel with a
  0.8 mm pocket, as on the Brain) — this ties the button board to the enclosure design.
- **Firmware:** `ButtonManager` grows from 16 to 28 mappings and gains the mode / bank / octave
  / edit handlers; the matching SysEx commands must exist on both ends (Remote Script).
- **Schematic:** `processor.kicad_sch` must break out GPB1–GPB6 of both MCPs; connectors J8
  and J20 grow accordingly.
