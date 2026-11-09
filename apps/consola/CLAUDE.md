# GroovePush — Global Project Context

> This file is the shared source of truth for any AI agent working in this monorepo.
> Read it before touching code. Sub-app details live in `apps/*/README*.md`.

## What this is

**GroovePush** is a DIY **Ableton Push 3 clone** that doubles as the hardware surface
for the **GroovePilot** AI mix-copilot platform. It is a hardware + firmware + GUI
instrument, not a web app.

Three layers, one monorepo:

| Layer | Path | Stack | Runs on |
|---|---|---|---|
| **Processor / firmware** | `apps/processor` | C++/Arduino, PlatformIO | **Teensy 4.1** (brain) + **NeoTrellis M4** (8×4 pad grid) |
| **GUI** | `apps/gui` | C++/Qt 6/QML, CMake | **Raspberry Pi 5** + **Waveshare 7" DSI 1280×720, 10-point multitouch** (ADR-0003) |
| **Remote Script** | `apps/remote-script` | **Python** (Ableton LOM) | Ableton Live — transport, tempo, params, clip R/W, track scan |
| **Hardware** | `hardware/` | KiCad | Main PCB / interconnect (Phase 3) |

## Architecture & inviolable principles

```
Teensy 4.1 ──USB-MIDI (class-compliant, <5ms)──▶ Ableton Live
   │  ▲                                              │
   │  └──────── SysEx / Remote Script (LOM) ─────────┘
   │
   ├─ UART 1 Mbps (binary framing) ─▶ NeoTrellis M4 (pads + LEDs)
   └─ Serial (QSerialPort) ─────────▶ Raspberry Pi 5 GUI ─▶ cloud AI (GroovePilot)
```

1. **Performance MIDI never touches the network.** Pads/encoders/faders reach Live as
   class-compliant USB-MIDI directly from the Teensy. Latency budget **< 5 ms**.
2. **The plugin owns the Remote Script connection.** The Remote Script TCP server
   (`localhost:9877`) has exactly one client: the VST3 plugin. Hardware talks to the
   plugin, which relays. Never a second client on 9877.
3. **The RPi talks to the cloud directly for AI**, reusing the existing `gp_sk_` API key
   auth. No new auth system.
4. **Audio never leaves the machine.** The hardware only ever sees the ~30 numerical
   features the plugin already extracts.

**Future direction (not yet implemented — see [`specs/adr/0002-dual-mode-standalone-direction.md`](specs/adr/0002-dual-mode-standalone-direction.md))**:
a second **Standalone mode** is planned, where the device works without a host computer.
The RPi5 will host the local sequencer/pattern engine (not the Teensy) and gains audio I/O
via RaspiAudio HATs (Audio+V2 for output now in the BOM; Mic+ for input planned). When a
GrooveForge Brain is connected, sound can be handed off to it (ADR-0002 Revision 1). Do not
design against this — but it is **direction only**, not yet a spec; the mode-switch
mechanism, the standalone engine ownership, and how this interacts with principle 4 above
are explicitly undecided. All specs in this repo today describe **Push Controller mode**
only.

In-repo & related components:
- Ableton **Remote Script** (LOM, Python) now lives at `apps/remote-script`. It is
  **deployed** to `~/Music/Ableton/User Library/Remote Scripts/PushClone` (the live
  folder Ableton loads from) via `apps/remote-script/update_pushclone.sh`. Treat the
  monorepo as source of truth; the Ableton folder is the deploy target.
- **VST3 plugin** (JUCE) + **Backend** (FastAPI + Bedrock) — the GroovePilot platform
  (external to this repo, referenced by the specs).

## Hardware map (source of truth: `apps/processor/PIN_MAP.md`)

- **Grid**: 8 tracks × 4 scenes = **32 pads** on one **Adafruit NeoTrellis M4** (SAMD51,
  120 × 62 mm, own firmware in `src/neotrellis_m4`), linked to the Teensy by **UART @ 1 Mbps**
  through its 4-pin JST port. Not I2C Seesaw boards (corrected 2026-10-19, gap G-P2).
- **Controls**: 8 EC11 encoders with push (A/B to Teensy GPIO, switches to MCP `0x20`),
  4 ALPS B50K faders (A0–A3), **28 MCP23017 inputs** on `0x20`/`0x21` (8 encoder switches +
  20 function buttons: transport, modes, bank, octave, edit — ADR-0004). On the PCB the
  function buttons are Kailh Choc V2 with per-key SK6812-MINI-E LEDs.
- **Removed from scope** (gap G-P11): piezo pads, Sharp IR "theremin", capacitive buttons —
  kept as DNP in `hardware/analog.kicad_sch` for a future e-drum module.
- **Feedback**: per-pad RGB (NeoTrellis) + per-key LEDs and WS2812B chain (PCB phase only).
- **Links**: Teensy `Serial1` (0/1) ↔ RPi @ 115200; `Serial5` (20/21) ↔ NeoTrellis M4 @ 1 Mbps;
  I2C (18/19) ↔ the two MCP23017. Ports are set once in `Config.h`
  (`TEENSY_GUI_SERIAL`, `TEENSY_M4_SERIAL`).
- **Carrier board**: the GrooveForge Brain `portadora`, used unmodified (gap G-H10); its
  connector contract is in `PIN_MAP.md`.
- **Sibling instrument**: GrooveForge Brain (`~/GROOVEFORGE/grooveforge-brain`) — a Teensy
  synth that GroovePush can drive over USB (its USB-A host) in standalone (ADR-0002 Rev. 1).
  Shares GroovePush's visual identity (ADR-0003).

## Wire protocols

- **Teensy ↔ M4 / RPi**: binary framing `[SYNC=0xAA][CMD][LEN][PAYLOAD…][XOR-CKSUM]`
  (`apps/processor/include/shared/BinaryProtocol.h`) **and** legacy SysEx
  (`F0 … F7`) for Ableton. Shared constants: `apps/processor/include/shared/Config.h`.
- **GUI ↔ processor**: `SerialController` (`apps/gui/SerialController.{h,cpp}`) with a
  handshake + heartbeat/ping, feeding QML models (`ClipGridModel`, `TrackListModel`,
  `SceneListModel`, `MixerModel`).
- Contract reference: `apps/processor/api-contract-definition.md`.

## Repo conventions

- **Build**: `pnpm <app>:build` or `make help`. GUI = CMake/Qt6; processor = PlatformIO.
- **Firmware envs**: `teensy41`, `neotrellis_m4`, plus `test_*` bring-up envs.
- **Commits**: Conventional Commits (`feat:`, `fix:`, `chore:`, `hw:`, `spec:`).
- **Never** hardcode pixel sizes in QML — use `Theme.scale` (see gui-design spec).
- **Never** put performance-path MIDI through the network layer (principle #1).
- Shared constants change in `include/shared/` only; keep Teensy and M4 in sync.

## Spec-Driven Development (SDD) harness

This project is **spec-first**. Specs live in `specs/` (OpenSpec format). The flow:

1. **Spec** — write/adjust an OpenSpec in `specs/openspec/<feature>.md` (`/spec`).
2. **Tasks** — derive a small, ordered task list into `specs/openspec/<feature>.tasks.md` (`/tasks`).
3. **Implement** — code one task at a time; each task references its spec section.
4. **Verify** — exercise the real behavior (firmware bring-up test, GUI run, DRC for HW).
5. **Review** — `/hw-review` for schematics; `/code-review` for code.

See `specs/README.md` for the full workflow and templates.

## Agent roster (`.claude/agents/`)

| Agent | Use for |
|---|---|
| `hardware-engineer` | KiCad schematic/PCB, part selection, pinout, power, DRC/ERC |
| `firmware-engineer` | Teensy/M4 C++, PlatformIO, MIDI/UART protocols, drivers |
| `gui-engineer` | Qt6/QML, models, SerialController, RPi5 deployment |
| `remote-script-engineer` | Ableton Remote Script (Python/LOM), SysEx contract, Live control |
| `industrial-designer` | Enclosure, ergonomics, panel layout, materials, manufacturing |
| `integration-architect` | Plugin/Remote Script/backend integration, latency, data flow |
| `qa-hardware-test` | Bring-up tests, protocol conformance, regression, bench validation |

## Golden rules for agents

- Respect the four inviolable principles above.
- Prefer editing shared config over duplicating constants.
- When changing a protocol, update **both** ends and the contract doc in the same change.
- Cite pin numbers and I2C addresses from `apps/processor/PIN_MAP.md`; don't invent them.
- Keep the latency budget in mind: performance path is USB-MIDI, not serial-to-RPi.
