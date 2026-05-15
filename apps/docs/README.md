# 📚 GrooveForge Brain — OpenSpec Documentation Pack

> Complete spec documentation for the GrooveForge Brain boutique synthesizer.
> **Owner:** Juan Guerrero (GPROG) · Bogotá, Colombia
> **Date:** May 15, 2026

---

## 📂 Document Index

Los documentos están organizados jerárquicamente. Leélos en orden para entender el producto completo:

| # | Document | Description |
|---|---|---|
| 00 | [GFD v3.0 — Master Strategy](./00-master-strategy.md) | **Single Source of Truth** — strategy, vision, decisions |
| 01 | [OpenSpec v0.3 — Technical Spec](./01-architecture.md) | Hardware + software technical specification |
| 02 | [Bridge Protocol v0.1](./02-bridge-protocol.md) | Communication protocol Teensy ↔ ESP32 ↔ VST3 |
| 03 | [Filter Design Spec v0.1](./03-filter-design.md) | Discrete 2N3904 ladder filter spec |
| 04 | [AI Architecture v0.1](./04-ai-architecture.md) | 3-layer AI architecture (TinyML + Cloud + DAW) |
| 05 | [FX Architecture v0.1](./05-fx-architecture.md) | 12 signature effects specification |
| 06 | [Implementation Roadmap v0.1](./06-implementation-roadmap.md) | 8-phase implementation plan |
| 07 | [Claude Code Bootstrap Prompt](./07-claude-code-bootstrap-prompt.md) | Initial prompt to setup the monorepo |

---

## 🎯 Quick Reference

### Product

- **Name:** GrooveForge Brain
- **Tagline:** *"Una pieza de hardware. Cinco niveles de superpoderes. Tú decidís hasta dónde llegar."*
- **Retail price:** $599 USD
- **BOM cost (qty 100):** ~$110 USD
- **Margin:** 82% @ qty 100

### Architecture

- **Audio brain:** Teensy 4.1 (Cortex-M7 @ 600MHz, bare-metal, Teensy Audio Library)
- **Network/UI co-processor:** ESP32-S3 with GC9A01 display integrated (Waveshare module)
- **Audio codec:** SGTL5000
- **Analog filter:** Discrete 2N3904 transistor ladder (Moog Minimoog 1970 design)
- **Toolchain:** PlatformIO

### The 5 Levels (North Star)

```
NIVEL 0 — STANDALONE (out of the box)
NIVEL 1 — + TECLADO MIDI (USB-A host)
NIVEL 2 — + SLAVES via pogo (futuro v1.x)
NIVEL 3 — + WIFI (ESP32-S3 cloud)
NIVEL 4 — + DAW (GroovePilot VST3)
```

### Roadmap (15 months)

| Phase | Period | Outcome |
|---|---|---|
| Phase 0 — Foundation | May 2026 | Monorepo + tooling |
| Phase 1 — Audio Core | Jun 2026 | First engine + analog filter |
| Phase 2 — Multi-engine + FX | Jul-Aug 2026 | 3 engines + 6-8 FX |
| Phase 3 — UI + Display | Sep-Oct 2026 | Full UI working |
| Phase 4 — MIDI + TinyML | Nov-Dec 2026 | Level 1 unlocked |
| Phase 5 — WiFi + Cloud | Jan 2027 | Level 3 unlocked |
| Phase 6 — DAW Bridge | Feb-Mar 2027 | Level 4 unlocked |
| Phase 7 — PCB + Manufacturing | Apr-May 2027 | Prototypes done |
| Phase 8 — Shipping | Jun 2027+ | Batch 1 shipped |

---

## 🚀 How to Use This Documentation

### For new contributors

1. Start with **00-master-strategy.md** to understand the product vision
2. Read **01-architecture.md** for the technical contract
3. Pick your area of interest:
   - Audio/DSP → 05-fx-architecture + 03-filter-design
   - AI/ML → 04-ai-architecture
   - Communication → 02-bridge-protocol
   - Implementation order → 06-implementation-roadmap

### For Claude Code setup

Use **07-claude-code-bootstrap-prompt.md** as the initial prompt when bootstrapping the monorepo. It will set up CLAUDE.md, agents, skills, and the full project structure.

### For sprint planning

Each sprint in **06-implementation-roadmap.md** has:
- Theory section to document
- Implementation tasks
- Demo evidence to capture

Follow the educational-first philosophy: theory doc → code → demo → learnings.

---

## 🛡️ Document Authority

If there's a conflict between documents:

1. **00 GFD** wins on strategy/business decisions
2. **01 OpenSpec** wins on technical contracts
3. **03 Filter / 02 Bridge / 05 FX / 04 AI** are authoritative on their respective subsystems
4. **06 Roadmap** is a living document — adjust as needed but don't violate constraints from 01

---

## 📝 Living Documents

All these documents are versioned. As decisions evolve, increment the version (v0.1 → v0.2) and update the change log inside the doc. Never silently modify — always note what changed.

---

## 🎵 The North Star

> Standalone, the GrooveForge Brain is a complete boutique synthesizer.
> Connect a MIDI keyboard → it becomes intelligent.
> Connect slaves → it becomes an ecosystem.
> Connect WiFi → it accesses cloud AI.
> Connect to your DAW → it becomes a co-pilot.
>
> One piece of hardware. Five levels of superpowers. You decide how far to go.

---

*GrooveForge Brain · OpenSpec Documentation · Juan Guerrero (GPROG) · 2026*
