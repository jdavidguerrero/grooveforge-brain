# ADR-0002: Dual-mode architecture direction — Push Controller mode vs Standalone mode

> **Status**: Accepted (direction) — NOT yet a full spec; see "Not decided yet" below.
> **Date**: 2026-07-16
> **Deciders**: user (product owner), integration-architect

## Context

Every spec in this repo (`CLAUDE.md`, `specs/openspec/push-controller.md` (SPEC-PUSHCTRL),
`processor.md` (SPEC-PROC), `gui.md` (SPEC-GUI)) assumes **one mode only**: the device is a
real-time I/O hub that requires a host computer running Ableton Live — pads/encoders/faders
reach Live via USB-MIDI, and the Raspberry Pi 5 is "the brain of the intelligence layer"
but is explicitly **not in the MIDI performance path** and has **no audio I/O role** at all.

The user wants a second, future mode: the device should also work as a **standalone
instrument**, without a host computer. This ADR records the direction so future work
doesn't design against it, without yet fully specifying it (a lot remains open — see below).

## Decision

1. **Two modes, shared sensing layer**: pads/encoders/faders/piezo/IR/grid are identical in
   both modes. What differs is what the Teensy *does* with events and what the RPi5 hosts.
   - **Push Controller mode** (current, specced): host-dependent, USB-MIDI + SysEx to Live,
     RPi5 = AI-copilot view (per SPEC-PUSHCTRL/SPEC-GUI).
   - **Standalone mode** (new, direction only): no host required.
2. **The Raspberry Pi 5 is the "OS" for standalone mode** — it hosts the local
   sequencer/pattern engine (not the Teensy), because it has far more compute/storage than
   the Teensy 4.1 and can reuse the existing GUI (PLAY/GEN screens already model a
   step-grid). The Teensy remains the real-time sensing/LED layer in both modes.
3. **Audio I/O hardware, added to the BOM now**: **RaspiAudio "Audio+ V2"** — a Raspberry Pi
   HAT DAC (112 dB SNR, 32-bit/384 kHz PCM over I2S, 40-pin header, RPi5-compatible,
   3.5 mm + RCA stereo out). **Output only** — confirmed against the manufacturer's spec
   page; it has no mic/line input.
   - A companion **RaspiAudio "Mic+"** (audio input) is **planned but not yet acquired** —
     tracked as a future BOM addition, needed before standalone mode can have audio *input*
     and before "receive signals from other instruments" (tentative, controller-mode idea)
     is possible.
4. This HAT stack sits on the **Raspberry Pi 5's own 40-pin GPIO header** — it is a
   different physical location from the Teensy-side custom PCB designed in Phase 3
   (`hardware/*.kicad_sch`). It does **not** require KiCad schematic changes; it's tracked
   as a BOM/accessory item in `hardware/README.md`, not a net on the processor board.

## Not decided yet (explicitly out of scope for this ADR)
- The mode-switch mechanism (auto-detect USB host present? physical switch? menu toggle?).
- Whether the standalone sequencer/pattern engine lives entirely on the RPi5 or needs
  Teensy-side timing support (clock generation without a host to slave to).
- Whether standalone mode reuses the PLAY/GEN QML screens as-is or needs new ones.
- Whether local audio capture (via Mic+) in **Controller mode** interacts with or extends
  the "audio never leaves the machine" principle (C-4) — that principle constrains the
  *plugin → controller* link; local RPi5 capture is a new data path it doesn't yet address.
  **Must be written precisely, not assumed, when this is specced.**
- A full `SPEC-STANDALONE` OpenSpec — write via `/spec standalone` when ready to commit.

## Alternatives considered
- **Teensy-hosted standalone audio** (Teensy Audio Shield/Library, onboard synthesis) —
  rejected for now: far less compute/storage than the RPi5, and would duplicate a UI/engine
  the RPi5 can already host more capably.

## Consequences
- Positive: standalone direction is now written down so no spec/agent contradicts it;
  Audio+V2 is in the BOM; the modular PCB work from Phase 3 is unaffected (different header).
- Follow-up: add Mic+ to the BOM once acquired; write `SPEC-STANDALONE` before implementing
  any standalone-mode task; amend C-4's wording once the local-capture data path is specced.

## Revision 1 — GrooveForge Brain as an optional sound engine (2026-10-19)

**Change:** the standalone decision above stands (the Raspberry Pi 5 hosts the sequencer and
produces audio through the RaspiAudio Audio+ V2). On top of it, when a **GrooveForge Brain**
(`github.com/jdavidguerrero/grooveforge-brain`) is connected, sound generation can be
**handed off to the Brain**. Without the Brain, GroovePush is still a complete standalone
instrument.

**Why:** the Brain is a dedicated Teensy 4.1 synth (Model D, Juno, OB-6, Prophet, DX7 engines,
FX, presets) with sub-millisecond audio. It already exposes everything GroovePush needs:

- A **USB-A MIDI host port** (`firmware-teensy/src/usb/midi_host.*`) — GroovePush's Teensy,
  a class-compliant USB-MIDI device, plugs straight into it with no computer in between.
- A **fixed CC map** for every engine parameter, identical across engines
  (`apps/docs/variants/studio/theory/07-studio-midi-expresion.md`): e.g. CC 74/71 cutoff and
  resonance, 73/75/70/72 amp ADSR, 102–105 filter ADSR, 20–61 oscillator and mod pages. That
  document already names "the Push clone" as a full-panel controller for the Brain.

This is not the "Teensy-hosted standalone audio" alternative rejected above: the Brain is a
separate instrument, not GroovePush's own Teensy synthesizing.

**Gaps found (2026-10-19):**

1. **The Brain's USB host is receive-only.** `midi_host.cpp` never sends. Without a send path
   GroovePush cannot show the Brain's current values (preset load, edits made on the Brain's
   own panel). Needs a small change **in the Brain repo**: mirror the outgoing CCs it already
   sends on `usbMIDI` to the USB-host device as well.
2. **GroovePush needs a "Brain mode"**: device pages built from the Brain's CC map, and a
   BRAIN view in the GUI (ADR-0003).
3. **Clock:** with no Live to slave to, GroovePush is the clock master in standalone.

**Not decided yet (added to the list above):**

- **Topology of the Brain link.** Option A: GroovePush Teensy (USB device) → Brain USB-A host.
  Pads and encoders reach the Brain directly, lowest latency, works today; sequenced notes from
  the Pi 5 travel Pi → UART → Teensy → Brain. Option B: the Pi 5 hosts the Brain's USB device
  port and routes MIDI (ALSA); simpler for the sequencer, but puts pads through the Pi.
  Option A is the leading candidate.
- How the audio output is chosen when both the Audio+ V2 and the Brain are present
  (automatic on connect vs a setting).
