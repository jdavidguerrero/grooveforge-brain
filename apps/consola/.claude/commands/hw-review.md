---
description: Review the GroovePush KiCad hardware design against the specs and firmware pinout
argument-hint: [schematic/area to focus on]
---

You are running a **hardware review** for GroovePush. Focus: $ARGUMENTS

Use the `hardware-engineer` agent's mindset and the KiCad MCP tools (`mcp__kicad__*`).

Do this:
1. Open the KiCad project under `hardware/` (`open_project` / `get_project_info`).
2. Cross-check the schematic against the authoritative pinout in
   `apps/processor/PIN_DISTRIBUTION.md`, `UART_CONNECTIONS.md`, and
   `include/shared/Config.h`. Every net/pin must agree with the firmware. Report any
   mismatch as a blocking finding — do not silently pick a side.
3. Verify per-IC: power rails, decoupling, I2C pull-ups (4.7kΩ), the WS2812B 5V rail +
   level shifter + bulk cap, piezo clamp/bleed, and 3.3V (not 5V) logic on Teensy pins.
4. Run `run_erc` and report every violation.
5. Confirm every integrated component is documented (ref, value, footprint, net) and
   JLCPCB-sourceable where possible.

Report findings ranked by severity (blocking → nice-to-have) with the exact net/ref, and
propose fixes. Don't modify the design unless asked.
