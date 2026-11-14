# ADR-0003: GUI on Qt 6 / QML for the 7" multitouch display, with the GrooveForge Brain visual identity

> **Status**: Accepted
> **Date**: 2026-10-19
> **Deciders**: user (product owner), gui-engineer, integration-architect
> **Related**: ADR-0002 (dual mode), [`openspec/gui.md`](../openspec/gui.md) (SPEC-GUI),
> GrooveForge Brain ADR-002 (Studio variant, `~/GROOVEFORGE/grooveforge-brain/apps/docs/adr/ADR-002-variante-studio.md`)

## Context

- The display changed. The 5" DSI 800×480 panel is replaced by a **Waveshare 7" DSI,
  1280×720 (16:9), 10-point capacitive multitouch**, already configured and working on the
  Raspberry Pi 5.
- The current GUI (`apps/gui`, Qt 6 / QML, ~4.4 k lines) only implements two Live-mirroring
  views (Session, Mix) and is laid out for 800×480.
- A sibling project, **GrooveForge Brain** (`github.com/jdavidguerrero/grooveforge-brain`), now
  has a much more finished instrument UI: `apps/studio-head` (C++17 + LVGL 8.4, ~7.9 k lines,
  12 screens, kiosk deployment). Its ADR-002 explicitly leaves Qt Quick "as an option for
  GroovePush".
- The two products are meant to read as one family. Their tokens disagree today:

  | | GroovePush (design spec v1.0) | Brain (`studio-head/src/ui/theme.h`) |
  |---|---|---|
  | Teal | `#00D4AA` | `#1D9E75` (+ `#5DCAA5`, `#0E5040`) |
  | Purple | `#8B5CF6` | `#534AB7` (+ `#7A6FE0`, `#2B2758`) |
  | Background | `#09090B` | `#0A0A0A` |
  | Typography | General Sans + JetBrains Mono | IBM Plex Mono only (7 sizes, 400/600) |

## Decision

1. **Stack stays Qt 6 / QML on the Raspberry Pi 5.** The new GUI is a rewrite of the screens
   inside `apps/gui`, keeping the existing C++ models (`ClipGridModel`, `TrackListModel`,
   `SceneListModel`, `MixerModel`) and `SerialController` until the bus client replaces it
   (SPEC-GUI R-GUI-8).
2. **Native canvas is 1280×720.** Components are laid out for 1280×720, not scaled up from
   800×480 (the aspect ratio differs: 5:3 vs 16:9). `Theme.scale = Screen.width / 1280`
   remains the only scaling seam for future panels.
3. **One visual identity with the Brain.** GroovePush adopts the Brain's palette and
   typography (`theme.h` hex values, IBM Plex Mono). This **supersedes** §2 (canvas) and §3
   (tokens, fonts) of [`push-controller-gui-design.md`](../push-controller-gui-design.md).
   The rest of that design spec (glanceability, soft-key bar, motion, "nothing bounces",
   brand don'ts) stays normative.
4. **Multitouch becomes a first-class input**, not only a shortcut. The encoders and
   buttons remain the primary path for everything done while playing; touch adds what a
   10-point panel does well (multi-finger faders, XY pads, on-screen strips, list
   scrolling, pinch in clip/step views).
5. **Screen set** (detailed in SPEC-GUI v0.2):
   - Push views: SESSION, NOTE, DEVICE, MIX, BROWSE.
   - AI views (GroovePilot): HOME/SCORE, GEN, COPILOT, SOUND, AUDIT, SETTINGS/PAIRING.
   - Standalone views: the local sequencer and a BRAIN view that mirrors the Brain's pages
     when it is connected (ADR-0002, Revision 1).

## Alternatives considered

| Option | Result | Why |
|---|---|---|
| **Qt 6 / QML + Brain identity** | ✅ | GPU scene graph on the Pi 5; full multitouch (`PointHandler`, `PinchHandler`, `MultiPointTouchArea`); reuses the existing models |
| Port `studio-head` (LVGL C++) | ❌ | Maximum code reuse with the Brain, but LVGL 8 is effectively single-pointer (wastes the 10-point panel), renders on the CPU (2.4× the pixels of 800×480), and its geometry is hard-coded to 800×480 |
| Keep the GroovePilot tokens | ❌ | The products would look unrelated |

## Consequences

- **Easier:** one look across both instruments and the video channel; design work done for
  the Brain (component sheets, engine illustrations, chips) is reusable as reference.
- **Harder:** two UI codebases in two toolkits implement the same identity. The Brain's
  `theme.h` is the source of truth for hex values; the QML `Theme` singleton mirrors it and
  must be updated when it changes.
- **Superseded:** SPEC-GUI R-GUI-1 / R-GUI-2 (old tokens and fonts), the 800×480 canvas.

## Open questions

- **Purple has two meanings.** In GroovePush purple is reserved for **AI-generated content**
  (design spec §4). In the Brain, purple is the **FX-mode chrome** (`mode_colors(fx)`).
  Unifying the palette means one of these has to give, or a shared rule has to cover both
  (e.g. "teal = what you play, purple = what the machine processes or generates"). **To be
  decided by the product owner** — not resolved here.
- Top-bar and soft-key-bar heights at 1280×720 (the Brain uses 40 + 96 px on 480 px).
