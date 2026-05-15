# GrooveForge Brain

> *"Una pieza de hardware. Cinco niveles de superpoderes. Tú decidís hasta dónde llegar."*

Boutique hybrid AI synthesizer — Teensy 4.1 + ESP32-S3 + discrete 2N3904 ladder filter.

**Retail: $599 USD · BOM: ~$110 · Margin: 82%**

---

## The 5 Levels (North Star)

```
NIVEL 0 — STANDALONE   → 6 synth engines + analog ladder filter + 12 signature FX
NIVEL 1 — + MIDI KB    → Scale Lock, Chord Recognition, Beat Follower (TinyML)
NIVEL 2 — + SLAVES     → E-Drum, Marimba, Air-Bamboo ecosystem (v1.x)
NIVEL 3 — + WIFI       → Patch search NL, progression suggester (cloud AI)
NIVEL 4 — + DAW        → GroovePilot VST3 mix-aware co-pilot
```

---

## Architecture

| Component | Chip | Role |
|---|---|---|
| Audio Brain | Teensy 4.1 (Cortex-M7 @ 600MHz) | Synth engines, FX, TinyML, MIDI |
| Network + UI | ESP32-S3 + GC9A01 1.28" display | WiFi, BLE, cloud sync, OTA |
| Audio Codec | SGTL5000 | 24-bit/48kHz ADC + DAC |
| Analog Filter | Discrete 2N3904 ladder (Moog 1970) | THE analog character |

Audio path: `Teensy → I2S → SGTL5000 → 2N3904 ladder → SGTL5000 ADC → USB Audio`

---

## Monorepo Structure

```
apps/
├── firmware-teensy/   C++ — Teensy 4.1 (PlatformIO)
├── firmware-esp32/    C++ — ESP32-S3 (PlatformIO)
├── bridge-protocol/   C headers — shared protocol
├── hardware/          KiCad PCB schematics
├── design/            Industrial design (3D + Figma)
├── training/          Python — TinyML model training
└── docs/              OpenSpec documentation (SSoT)
tools/
├── matching-jig/      Arduino — transistor Vbe matching jig
└── filter-cal/        Calibration scripts
```

---

## Quick Start

```bash
# Build Teensy firmware
cd apps/firmware-teensy && pio run -e teensy41 -t upload

# Build ESP32 firmware
cd apps/firmware-esp32 && pio run -e esp32s3 -t upload

# Run tests (native, no hardware needed)
pnpm test:native

# ML training
cd apps/training && uv sync && uv run pytest
```

---

## Documentation

All specs live in [`apps/docs/`](apps/docs/README.md):

| Doc | Content |
|---|---|
| [00-master-strategy.md](apps/docs/00-master-strategy.md) | Vision, pricing, North Star |
| [01-architecture.md](apps/docs/01-architecture.md) | Hardware + software spec |
| [02-bridge-protocol.md](apps/docs/02-bridge-protocol.md) | Teensy ↔ ESP32 ↔ VST3 protocol |
| [03-filter-design.md](apps/docs/03-filter-design.md) | Discrete 2N3904 ladder filter |
| [04-ai-architecture.md](apps/docs/04-ai-architecture.md) | 3-layer AI spec |
| [05-fx-architecture.md](apps/docs/05-fx-architecture.md) | 12 signature effects |
| [06-implementation-roadmap.md](apps/docs/06-implementation-roadmap.md) | Phase & sprint plan |

---

## Status

**Phase 0 — Foundation** · May 2026 · 🟡 In Progress

| Milestone | Status |
|---|---|
| Monorepo + CLAUDE.md + agents + skills | 🟢 Done |
| Sprint 1.1 — Hello Tone (440Hz via I2S) | 🔴 Pending |
| Sprint 1.2 — Multi-OSC + ADSR | 🔴 Pending |
| Sprint 1.3 — Transistor matching jig | 🔴 Pending |
| Sprint 1.4 — Filter protoboard | 🔴 Pending |

---

*GrooveForge Brain · Juan Guerrero (GPROG) · Bogotá, Colombia · 2026*
