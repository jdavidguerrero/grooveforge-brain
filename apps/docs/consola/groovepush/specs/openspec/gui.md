# GUI — OpenSpec

> **Id**: SPEC-GUI
> **Status**: Draft
> **Owners**: gui-engineer
> **Parent**: [`push-controller.md`](push-controller.md) (SPEC-PUSHCTRL)
> **Normative design source**: [`../push-controller-gui-design.md`](../push-controller-gui-design.md) (GUI Design & Brand Spec v1.0),
> with §2 (canvas) and §3 (tokens, fonts) **superseded** by [ADR-0003](../adr/0003-gui-qt-brain-identity-7in-display.md):
> palette and typography come from GrooveForge Brain (`apps/studio-head/src/ui/theme.h`)
> **Target**: Qt 6 / QML on Raspberry Pi 5, **Waveshare 7" DSI 1280×720, 10-point multitouch** (`apps/gui`)
> **Version**: 0.2

## 1. Purpose & context
The GUI is the 7" instrument display. Today it is a **Live-mirroring controller UI**
(tabbed Session/Mix/Device/Browse/Note, `SerialController` + `ClipGridModel`,
`MixerModel`, `TrackListModel`, `SceneListModel`, `PushCloneTheme` singleton). The target
reframes it as an **encoder-first instrument surface** that carries both the Push views and
the AI-copilot screens, with a soft-key bar, and shares one visual identity with GrooveForge
Brain (ADR-0003). The full visual/brand contract is the **normative design source**; this spec assigns
requirement ids and the C++/QML architecture constraints.

## 2. Scope
- **In scope**: the Push views (SESSION, NOTE, DEVICE, MIX, BROWSE), the AI screens
  (HOME/Score, GEN, COPILOT, SOUND, AUDIT, SETTINGS/PAIRING), the standalone views
  (local sequencer, BRAIN), the persistent chrome (top bar + 8-cell soft-key bar), the Theme
  singleton, multitouch input, and the bus-client seam to `gp-bridged`.
- **Out of scope**: networking/AI (owned by `gp-bridged`); MIDI (owned by the Teensy).

## 3. Constraints (inviolable — from design source §1, §10)
- C-1: **Glanceable** — one primary value per screen, mono numerals, `bgBase` always.
- C-2: **Encoder-first** — every flow done while playing is completable with the encoders and
  buttons; 8 soft keys mirror the 8 physical encoders. Touch is first-class for what a
  10-point panel does well (ADR-0003 §4) but never the only path to a live-performance action.
- C-3: **Screen & LEDs are one language** — teal = audio/interactive; the meaning of purple
  (AI-only vs the Brain's FX chrome) is **open**, see ADR-0003.
- C-4: **No sockets/HTTP in QML** — the GUI is a pure view over the `gp-bridged` bus (§8).
- C-5: **No hardcoded hex/px** — Theme singleton + `Theme.scale` only. Native canvas is
  1280×720; `Theme.scale = Screen.width / 1280`.

## 4. Requirements

| Id | Requirement | Design § |
|----|-------------|----------|
| R-GUI-1 | A `Theme` singleton mirrors the Brain tokens (`theme.h`: BG `#0A0A0A`, TEAL `#1D9E75`/`#5DCAA5`/`#0E5040`, PURPLE `#534AB7`/`#7A6FE0`/`#2B2758`, SURFACE/LINE, RED/AMBER/ORANGE, Camelot colors) and `scale = Screen.width/1280`. | ADR-0003 |
| R-GUI-2 | Bundle **IBM Plex Mono** (weights 400/600) via `.qrc` FontLoader in Theme, with a type scale derived from the Brain's 7 roles for 720 px height; no system-font dependence. | ADR-0003 |
| R-GUI-3 | Persistent **top bar** (mode · session key/BPM · RS/BE/HW dots · quota chip) and **8-cell soft-key bar** mirroring the encoders, with focus/active/AI states. | §5 |
| R-GUI-4 | The **AI screens** per design §6 (HOME, GEN, COPILOT, SOUND, AUDIT, SETTINGS/PAIRING; PLAY merges into NOTE), switched by a StackLayout with a 180 ms slide+fade. | §6 |
| R-GUI-5 | HOME shows the **Mix Score ring** (Shape, count-up 1.5 s), 7-band spectrum (teal gradient, 10–20 Hz, `Behavior` smoothed), and 7 dimension rows. | §6.1, §7 |
| R-GUI-6 | GEN shows the **step grid mirroring pads 1:1**, purple ghost → teal committed, streaming GENERATE state, VARIATION overlay. | §6.2 |
| R-GUI-7 | COPILOT streams SSE chat token-by-token (purple accent, caret blink), curated quick-actions, QR hand-off; 429 → amber toast. | §6.4 |
| R-GUI-8 | The GUI is a **pure view over the bus**: subscribes to `features_frame`/`score_update`/`session_state`/chat; publishes intents. No networking in QML. | §8 |
| R-GUI-9 | Performance: 60 fps on RPi 5 with spectrum animating — `Shape`/scene-graph (no `Canvas`), no per-frame JS timers, fonts loaded once. | §8 |
| R-GUI-10 | Degraded-mode surfaces ("Plugin offline", offline banner, plan locks) render as explicit states. | design §6, parent §4.9 |
| R-GUI-11 | **Push views**: SESSION (8×4 clip grid + scene launch, mirrors the pads 1:1), NOTE (scale-aware layout, octave), DEVICE (8 parameter cells bound to the encoders, bank paging, curated pages for Diva / Serum / Vital), MIX (4 strips bound to the physical faders, bank of 4), BROWSE (category list + items, load into Live). Reachable from the physical mode buttons (ADR-0004). | ADR-0003 §5, ADR-0004 |
| R-GUI-12 | **Multitouch**: touch-draggable faders/strips, XY pads and list scrolling via `PointHandler`/`PinchHandler`; minimum target 48 px at 1280×720; never the only path to a live-performance action (C-2). | ADR-0003 §4 |
| R-GUI-13 | **Standalone views**: local sequencer and a BRAIN view mirroring the Brain's engine pages and values from its CC map when a Brain is connected. | ADR-0002 Rev. 1 |

## 5. Interfaces & contracts
- Bus client (new C++/Python bridge exposed to QML) ⇄ `gp-bridged` (ZeroMQ). Replaces the
  current `SerialController`-drives-everything model for AI data (transport/session may
  still use serial short-term — see gap G-G4).
- Theme singleton API: colors/fonts/sizes/scale — the only place hex/px live.

## 6. Acceptance criteria
- AC-1 (R-GUI-1/2): grepping components finds **no** inline hex or px; fonts render from qrc.
- AC-2 (R-GUI-4..7): each AI flow in normative §4 is drivable from soft keys + encoders.
- AC-3 (R-GUI-9): spectrum animates at ≥ 60 fps on RPi 5 at 1280×720 (measured).
- AC-5 (R-GUI-11): each Push view is reachable from its physical mode button and from touch.
- AC-6 (R-GUI-12): two fingers move two faders independently on the panel.
- AC-4 (R-GUI-8): with `gp-bridged` mocked, screens render from bus messages headless.

## 7. Open questions / risks
- Migration order: refit `PushCloneTheme` → `Theme` in place, or new `qml/Theme/` tree
  (design §8 layout) and port screens? (gap G-G1.)
- `features_frame` rate the QML scene can sustain on RPi 5 (parent §7.2).
- Purple semantics (AI-only vs FX chrome) — ADR-0003 open question.
- Chrome heights at 1280×720 (top bar, soft-key bar).

## 8. Change log
- 0.1 — initial draft from current GUI + design source v1.0.
- 0.2 — 2026-10-19: Waveshare 7" 1280×720 multitouch; Brain visual identity (ADR-0003);
  Push views, multitouch and standalone views added (R-GUI-11..13); R-GUI-1/2/4 revised.
