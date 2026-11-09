# GroovePush

**DIY Ableton Push 3 clone + AI mix-copilot surface for the GroovePilot platform.**

A monorepo unifying the three layers of the controller:

| Layer | Path | Stack | Target |
|---|---|---|---|
| **Processor / firmware** | [`apps/processor`](apps/processor) | C++ / Arduino (PlatformIO) | Teensy 4.1 (brain) + NeoTrellis M4 (8×4 pads) |
| **GUI** | [`apps/gui`](apps/gui) | C++ / Qt 6 / QML | Raspberry Pi 5 + 5" DSI display |
| **Remote Script** | [`apps/remote-script`](apps/remote-script) | Python (Ableton LOM) | Ableton Live (MIDI/API integration) |
| **Hardware** | [`hardware/`](hardware) | KiCad | Main PCB / interconnect |

Planning & contracts live in [`specs/`](specs) and [`docs/`](docs).

---

## Architecture at a glance

```
     ┌───────────────┐   USB-MIDI (class-compliant, <5ms)   ┌──────────┐
     │  Teensy 4.1   │─────────────────────────────────────▶│  Ableton │
     │  (Processor)  │                                       │   Live   │
     │  pads·enc·    │◀───── SysEx / Remote Script ──────────│          │
     │  faders·piezo │                                       └──────────┘
     └──────┬────────┘
            │ UART 1 Mbps (binary framing / SysEx)
            │  + I2C to 2× NeoTrellis (pads/LEDs)
     ┌──────┴────────┐   Serial (QSerialPort)   ┌───────────────────────┐
     │ NeoTrellis M4 │                          │  Raspberry Pi 5 (GUI) │
     │  8×4 pad grid │◀────── UART link ───────▶│  Qt/QML 5" display     │
     └───────────────┘                          │  + cloud AI (GroovePilot)
                                                 └───────────────────────┘
```

- **Performance MIDI never touches the network** — pads/encoders/faders reach Live as USB-MIDI from the Teensy.
- **The RPi5 GUI** is a thin, low-latency remote + the AI mix copilot surface.
- Wire protocol between Teensy ↔ M4 ↔ RPi is documented in [`specs/`](specs) and `apps/processor/include/shared/`.

## Repo layout

```
GroovePush/
├── apps/
│   ├── gui/            # Qt6/QML GUI (Raspberry Pi 5) — imported with full git history
│   ├── processor/      # PlatformIO firmware (Teensy 4.1 + NeoTrellis M4) — full git history
│   └── remote-script/  # Ableton Remote Script (Python/LOM) — full git history
├── hardware/         # KiCad project (schematic + PCB)
├── specs/            # OpenSpec-style specifications (source of truth for implementation)
├── docs/             # Design specs, guides, ADRs
├── .claude/          # Shared AI context, agents, and SDD harness
├── Makefile          # No-Node fallback orchestration
├── package.json      # pnpm workspace orchestration
└── pnpm-workspace.yaml
```

## Getting started

```bash
# With pnpm (workspace orchestration)
pnpm install
pnpm processor:build      # build firmware (PlatformIO)
pnpm gui:build            # build the Qt GUI

# Or with make (no Node required)
make help
make proc-build-teensy
make gui-build
```

### Prerequisites
- **Processor**: [PlatformIO Core](https://platformio.org/) (`pip install platformio`)
- **GUI**: Qt 6.8+, CMake ≥ 3.16 (see [`apps/gui/README_SETUP.md`](apps/gui/README_SETUP.md))
- **Hardware**: KiCad 8+

## History

`apps/gui`, `apps/processor`, and `apps/remote-script` were previously three
standalone GitHub repos. They were merged into this monorepo **preserving their full
commit history** via `git subtree` merge. The Ableton folder copy of the Remote Script
is the live deploy target and was left in place. Original remotes and the migration
recipe are documented in `docs/MONOREPO_MIGRATION.md`.
