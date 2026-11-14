# Block 0 — "Anatomy of a DIY MIDI controller" (content script)

> Paste this into the Block 0 Notion page. Milestone: source of truth + powered bench.

## Hook (first 10 s)
"I tore my Ableton Push clone apart — and before I re-solder a single wire, there's one
boring decision that will make or break the whole build: the pin map."

## What we build
Not a feature yet — the **foundation**: a Teensy 4.1 on a protoboard, powered, with a clean
conflict-free pin map that every future block depends on.

## Talking points (the concept)
- What the **Teensy 4.1** is and why it's the brain (native USB-MIDI, 12-bit ADC, lots of
  interrupt-capable pins).
- The **four buses** we'll use across the series: USB (to Ableton), I2C (pads + button
  expanders), UART (to the Raspberry Pi), and analog/ADC (faders, piezo, IR).
- Why a **pin map** matters: the old docs put I2C on pins that aren't an I2C bus, and put
  encoders on the same pins as the IR sensors. On real hardware that just doesn't work.
- The fix: one **authoritative map** (`PIN_MAP.md`) — encoders avoid Serial1 (0/1, the RPi
  link), I2C is the real `Wire` bus (18/19), buttons move onto I2C expanders so they don't
  steal analog pins.

## Shot list
1. **Bench tour**: the bare Teensy, the protoboard, the 5 V / 3 A supply, the 3V3/5V/GND rails.
2. **Screen**: the before/after pin map (show the conflicts crossed out).
3. **Demo**: power on → onboard LED blink → "it's alive."

## Code / files to show
- `apps/processor/PIN_MAP.md` (the source of truth).
- The encoder + I2C lines in `include/shared/Config.h`.
- A 5-line blink sketch (onboard LED on pin 13) as the "hello, board is alive."

## Deliverable
- The system block diagram (reuse `README.md`'s ASCII diagram, redraw it clean).
- A printable one-pager of `PIN_MAP.md`.

## Verification shown on camera
- `pio run -e teensy41` → **SUCCESS** (compiles with the new map).
- Onboard LED blinks on the powered protoboard.

## Notes
- This is the least flashy episode but sets up trust: "we measure twice, cut once."
- End with a teaser for Block 1: "Next time, one button becomes a note in Ableton."
