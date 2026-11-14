# GroovePilot Push — QML GUI Design & Brand Spec v1.0

> **Document type**: Design spec (drives QML implementation on the RPi 5)
> **Component**: 5" display GUI (Qt 6 / QML, Raspberry Pi 5)
> **Companion doc**: `docs/openspecs/push-controller.md` (integration architecture)
> **Brand source of truth**: `docs/design-system-rules.md` + `CLAUDE.md` brand section
> **Date**: July 2026

---

## 1. Design Stance

This is a **hardware instrument display**, not a web page. Three consequences:

1. **Glanceable over readable.** The producer looks at this screen for ≤ 1 second
   while their hands are on pads. Every screen must communicate its one primary
   value (score, step position, streamed answer) at arm's length. Big mono
   numerals, high contrast, minimal chrome.
2. **Encoder-first, touch-second.** Navigation follows the Push idiom: the bottom
   edge of the screen is a row of 8 "soft labels" that mirror the 8 physical
   buttons/encoders below the display. Touch is supported (DSI panel) but every
   flow must be completable without it.
3. **The screen and the LEDs are one design system.** Pad/button LED colors and
   screen colors must mean the same thing. Teal on screen = teal on a pad =
   audio/interactive. Purple = AI, everywhere, exclusively.

---

## 2. Canvas & Grid

- Target panel: 5" DSI, **800 × 480 px**, landscape, ~186 ppi.
  (If the final panel is 1280 × 720, all dimensions scale by 1.6 via a single
  `Theme.scale` factor — never hardcode px in components.)
- Root layout:

```
┌──────────────────────────────────────────── 800 ─┐
│ TOP BAR — 40 px                                   │  mode title · session (key/BPM) · link dots · quota
├───────────────────────────────────────────────────┤
│                                                   │
│ CONTENT — 384 px                                  │  one mode screen at a time
│                                                   │
├───────────────────────────────────────────────────┤
│ SOFT-KEY BAR — 56 px (8 cells of 100 px)          │  labels for the 8 encoders/buttons under the display
└───────────────────────────────────────────────────┘
```

- Spacing scale: 4 px base unit (4/8/12/16/24/32) — same as the web app.
- Corner radius: 8 px cards, 999 px pills. Border: 1 px `#27272A`.
- Minimum hit target (touch): 48 × 48 px.

---

## 3. Theme Tokens (QML singleton)

Create `qml/Theme/Theme.qml` as a singleton — the **only** place colors, fonts and
sizes live. Components import it; hex values never appear inline.

```qml
pragma Singleton
import QtQuick

QtObject {
    // Scale factor for panel variants (800x480 = 1.0)
    readonly property real scale: Screen.width / 800

    // ── Blacks / surfaces (identical to web + plugin) ─────────────
    readonly property color bgBase:      "#09090B"
    readonly property color bgCard:      "#141418"
    readonly property color bgElevated:  "#1A1A1F"
    readonly property color bgHover:     "#222228"
    readonly property color border:      "#27272A"
    readonly property color borderSoft:  "#3F3F46"

    // ── Brand accents ─────────────────────────────────────────────
    readonly property color teal:        "#00D4AA"   // audio / interactive / active
    readonly property color tealLight:   "#00F5C8"   // hover / glow / gradient top
    readonly property color tealDim:     "#00A88A"   // pressed / gradient bottom
    readonly property color purple:      "#8B5CF6"   // AI ONLY
    readonly property color purpleLight: "#A78BFA"
    readonly property color cyan:        "#06B6D4"   // sparingly, secondary accent

    // ── Text ──────────────────────────────────────────────────────
    readonly property color textPrimary:   "#FAFAFA"
    readonly property color textSecondary: "#A1A1AA"
    readonly property color textMuted:     "#71717A"
    readonly property color textDim:       "#52525B"

    // ── Semantic ──────────────────────────────────────────────────
    readonly property color success: "#10B981"
    readonly property color warning: "#F59E0B"
    readonly property color error:   "#EF4444"

    // ── Fonts (loaded via FontLoader in Theme; files bundled in qrc) ──
    readonly property string sansFamily: generalSans.name      // General Sans Variable
    readonly property string monoFamily: jetbrainsMono.name    // JetBrains Mono
}
```

**Font bundling**: ship `GeneralSans-Variable.ttf` and `JetBrainsMono[wght].ttf`
in the Qt resource file (`.qrc`) — same families the plugin bundles as binary
resources. No Fontconfig/system-font dependence: the device must render
identically on every flashed image.

**Type roles** (sizes at scale 1.0):

| Role | Font | Size / weight | Use |
|---|---|---|---|
| Display value | JetBrains Mono | 64 px / 700 | Mix Score hero, BPM |
| Big data | JetBrains Mono | 28 px / 500 | LUFS, dB values, step counts |
| Screen title | General Sans | 18 px / 600 | Top bar mode name |
| Body | General Sans | 15 px / 400–500 | Chat text, descriptions, list rows |
| Label / soft-key | General Sans | 12 px / 500, +0.05 em tracking, UPPERCASE | Soft-key bar, card labels |
| Micro | JetBrains Mono | 11 px / 400 | Timestamps, quota, units |

Rule (inherited): **every number is mono, every word is sans.**

---

## 4. Color Semantics — screen + LEDs as one language

| Meaning | Screen | Pad/Button LED |
|---|---|---|
| Audio signal / active / playing | teal `#00D4AA` | teal, brightness = level/velocity |
| AI feature, AI output, AI badge | purple `#8B5CF6` | purple (GEN preview ghost notes, COPILOT button) |
| Root note (PLAY mode) | teal filled | teal full |
| In-scale note | `#3F3F46` outline | dim white (10 %) |
| Step ON (sequencer) | teal cell | teal, velocity = brightness |
| Playhead | tealLight sweep | tealLight chase |
| Good / on-target | success green | green |
| Caution / near limit | warning amber | amber |
| Problem / clipping / regression | error red | red |
| Disabled / locked (plan gate) | textDim + lock glyph | off |

**Hard rules (from brand):**
- Purple appears **only** where an AI produced the content: generated-pattern
  preview, streamed chat text accent, AI badges, SOUND mode accents. Transport,
  meters, spectrum, navigation are never purple.
- Spectrum bars are the teal gradient `tealDim → tealLight`, **never red** even
  when hot — loudness problems are communicated by the LUFS card color, not by
  scaring the spectrum.
- Background is always `bgBase`. No light mode on hardware. Ever.

---

## 5. Persistent Chrome

### 5.1 Top bar (40 px)

```
│ ◉ GP  MIX SCORE      Am · 122 BPM       RS ● BE ● HW ●      CHAT 24/100 │
```

- Left: 20 px logo glyph + current mode name (General Sans 600, 18 px).
- Center: session context from `get_session` — key + BPM in mono (this is the
  ambient "GroovePilot knows your session" signal; dims to `textDim` if the
  plugin link is down).
- Right: three 8 px connection dots — RS (Remote Script), BE (backend), HW
  (plugin↔controller link). Green pulsing = connected (2 s breathe), red = down.
  Identical semantics to the plugin's bottom bar so users transfer knowledge.
- Far right: contextual quota chip (mono 11 px), e.g. `GEN 12/50` in GEN mode.
  Turns amber at ≤ 20 %, red at 0 (with reset date on tap).

### 5.2 Soft-key bar (56 px, 8 cells)

Mirrors the 8 encoders/buttons under the display — the core Push interaction.

- Cell anatomy: label (12 px UPPERCASE) + optional value line (mono 14 px) +
  2 px top edge that lights teal when the encoder is touched/turned.
- States: default (`textSecondary` on `bgBase`), focused (teal label + edge),
  active/latched (teal 12 % fill), disabled (`textDim`), AI action (purple label —
  e.g. GENERATE).
- Encoder turn feedback: the value line updates live; a thin radial arc (like
  Push's encoder ghosts) overlays the cell while turning, fades 300 ms after.

---

## 6. Mode Screens

### 6.1 HOME / SCORE (default)

The hardware's hero screen — equivalent of the plugin's Mix Score panel.

```
┌──────────────────────────────────────────────────────┐
│  ┌────────── 300 ──────────┐  ┌──────── 460 ───────┐ │
│  │                         │  │ SPECTRUM (7 bars)  │ │
│  │        87 /100          │  │ ▂▄▆█▆▄▂  + genre   │ │
│  │   (score ring, teal)    │  │ dashed targets     │ │
│  │      ▲ +4 vs last       │  ├────────────────────┤ │
│  │   Organic House         │  │ LUFS -9.2 │ ST 74% │ │
│  └─────────────────────────┘  │ 7 dimension rows   │ │
│                               └────────────────────┘ │
└──────────────────────────────────────────────────────┘
SOFT KEYS: GENRE · AUDIT · FIX · COMPARE · — · — · SCAN · SETUP
```

- **Score ring**: 220 px circular progress (stroke 10 px, teal gradient, round
  caps), score centered in 64 px mono 700. Count-up animation 1.5 s ease-out on
  new score (same motion spec as the plugin). Delta badge below: green `▲ +4` /
  red `▼ -3`, mono.
- **Spectrum**: 7 slim bars, teal gradient fill, 1 px dashed genre-target line
  per bar in `borderSoft`, peak-hold tick fading over 2 s. 10–20 Hz update from
  `features_frame`; animate bar height with 80 ms `Behavior on height` to avoid
  strobing.
- **Dimension rows** (7): label sans 12 px + score mono + 4 px mini-bar; bar color
  teal > 75, amber 50–75, red < 50 (identical thresholds to plugin).
- FIX soft-key is purple *only when an AI suggestion is pending*.

### 6.2 GEN (AI pattern generation)

```
┌──────────────────────────────────────────────────────┐
│ TYPE ▸ Drums     GENRE ▸ Organic House   BARS ▸ 4    │  ← param row (encoder-bound)
│ KEY Am (session) · HUMANIZE ✓                        │
├──────────────────────────────────────────────────────┤
│  STEP GRID PREVIEW  (16 × 4 lanes, mirrors pads)     │
│  kick  ●···●···●···●···                              │
│  snare ····●·······●···     ghost notes = purple     │
│  hat   ●·●·●·●·●·●·●·●·     committed   = teal       │
│  perc  ··●·····●·····●·                              │
├──────────────────────────────────────────────────────┤
│ ▶ AUDITION    Target: T3 "Drums" · Slot 1            │
└──────────────────────────────────────────────────────┘
SOFT KEYS: TYPE · GENRE · BARS · KEY · GENERATE · VARIATION · TARGET · COMMIT
```

- **The grid on screen mirrors the physical pads 1:1** — same rows/columns, same
  colors as the pad LEDs. Ghost (uncommitted, AI-generated) notes are purple at
  60 % opacity; once committed to Live they render teal. This purple→teal
  transition *is* the mental model: "AI proposal becomes your clip."
- GENERATE soft-key: purple, shows a 3-dot streaming shimmer while the backend
  call is in flight (target < 1.5 s; skeleton grid pulses `bgElevated`).
- VARIATION opens a 3-option overlay: COMPLEMENT / VARIATION / RESPONSE (purple
  pills), bound to encoders 5–7 while open.
- Playhead: tealLight column sweep synced to MIDI clock during audition.
- Plan gates: non-chord types on Free plan render with a lock glyph + "PRO" tag
  (amber); server 403 copy shown verbatim in a toast.

### 6.3 PLAY (scale mode)

- Full-screen minimal: giant key/scale readout (`Am · Dorian` mono 28 px), a
  keyboard ribbon highlighting in-scale notes, last-played note name large.
- Layout selector (4ths / 3rds / chromatic) on soft keys.
- No AI accents here — this screen is 100 % teal/white (performance surface).

### 6.4 COPILOT (AI chat)

```
┌──────────────────────────────────────────────────────┐
│  ⬤ AI  Your sub is sitting at -18 dB, which is 3 dB  │
│        under the organic-house target. Try…          │  ← streamed, purple accent bar
│                                                      │
│  YOU    How's my low end?                            │  ← right-aligned, bgElevated
├──────────────────────────────────────────────────────┤
│  [ Diagnose mix ] [ Low end? ] [ What next? ] [ ⧉QR ]│  ← quick actions (pills)
└──────────────────────────────────────────────────────┘
SOFT KEYS: DIAGNOSE · LOW END · NEXT STEP · REFS · — · SOUND · QR · CLEAR
```

- AI message: left aligned, 3 px purple left border, body sans 15 px, streamed
  chunk-by-chunk (SSE) with a purple caret blink while `complete: false`.
- User message: right aligned, `bgElevated` bubble, no border.
- Max ~9 lines visible; auto-scroll pinned to bottom during streaming; flick to
  scroll history.
- QR soft-key renders a QR deep-link to continue the same conversation in the
  web app (chat sessions API) — the escape hatch for long reads/typing.
- Rate-limit (429): amber toast "Chat limit reached — resets Aug 1" + quota chip
  turns red. Never silently drop.

### 6.5 SOUND (sound-design copilot)

- Synth picker: horizontal card carousel (Serum, Vital, Diva, Analog, Operator,
  Wavetable, Massive X) — encoder 1 scrolls, press selects.
- Recipe view: parameter checklist streamed from chat `mode: "sound-design"`;
  each row `PARAM → value` in mono with a check-off tap target (local state,
  helps the user track progress while patching their synth).
- Purple accents throughout (AI feature).

### 6.6 AUDIT

- Result list: prioritized issues, severity dot (red/amber/green) + one-line
  summary; selecting one opens detail + a purple **FIX WITH AI** action that
  jumps to the §6.1 FIX flow with context preloaded.
- SCAN progress (Studio): full-width track list with per-track state
  (pending / soloing / captured ✓), teal progress bar, always-visible CANCEL.

### 6.7 SETTINGS / PAIRING

- First-boot flow: Wi-Fi → "Searching for GroovePilot plugin…" (mDNS spinner) →
  6-digit pairing code entry (giant mono digits, encoder to dial, press to
  advance) → API key confirmation → done.
- Status page: plugin host name/IP, link latencies (mono), firmware versions
  (Teensy / Trellis / GUI), update check, "Unpair" (destructive → red confirm).

---

## 7. Motion Spec

| Animation | Duration / easing | Notes |
|---|---|---|
| Score count-up | 1500 ms, OutCubic | Same as plugin; number + ring together |
| Bar/meter changes | 80–100 ms, OutQuad | `Behavior on` value; never instant-jump |
| Mode switch | 180 ms slide + fade | Horizontal, direction = position in mode order |
| Soft-key focus | 120 ms | Edge light + label color |
| Connection dot | 2 s breathe loop | Opacity 0.5 ↔ 1.0 |
| SSE text | per-chunk append | Caret blink 530 ms while streaming |
| Toast | in 150 ms / hold 4 s / out 300 ms | Bottom center, above soft keys |
| Pad sweep (score up) | 400 ms left→right | LED-side, mirrored by a subtle screen glow |

Global rule: **nothing bounces**. This is a studio tool — motion is quick, damped,
functional. No spring/overshoot easings.

---

## 8. QML Architecture & Performance (RPi 5)

- **Structure**:

```
gui/
├── qml/
│   ├── Main.qml               # window, mode StackLayout, chrome
│   ├── Theme/ (singleton)     # Theme.qml + qmldir
│   ├── components/            # GpSoftKeyBar, GpScoreRing, GpSpectrum,
│   │                          # GpStepGrid, GpChatView, GpToast, GpQuotaChip,
│   │                          # GpConnectionDots, GpPill, GpCard
│   └── screens/               # HomeScreen, GenScreen, PlayScreen,
│                              # CopilotScreen, SoundScreen, AuditScreen,
│                              # SettingsScreen, PairingScreen
├── src/                       # thin C++/Python bus client exposed to QML
└── resources.qrc              # fonts, logo, icons
```

- The GUI is a **pure view over the gp-bridged bus** (see integration spec §3.4):
  it subscribes to `features_frame`, `score_update`, `session_state`, chat chunks;
  it publishes user intents. No sockets or HTTP in QML.
- Performance budget: 60 fps on RPi 5 with the spectrum animating. Rules:
  - No `Canvas` for meters — use `Rectangle`/`Shape` with GPU scene graph;
    the score ring is a `Shape { ShapePath }` with `PathAngleArc`.
  - No per-frame JS timers; drive meters from bus signals (10–20 Hz) and let
    `Behavior` interpolate.
  - Text on top of solid rects (no layered translucency stacks); avoid
    `layer.enabled` except for the mode-switch transition.
  - Fonts loaded once in the Theme singleton; no per-delegate FontLoader.
- Boot target: logo splash < 1 s, full GUI interactive < 6 s from power
  (Wayland/eglfs kiosk, no desktop shell).

---

## 9. Iconography & Assets

- Icons: 20/24 px inline SVG set, 1.75 px stroke, round caps — reuse the web
  app's icon shapes where they exist (play, spectrum, chat, spark/AI, lock,
  QR). AI spark icon is the only purple icon.
- Logo: white glyph variant in top bar; boot splash = glyph + wordmark centered
  on `bgBase` with a single teal pulse.
- All assets in `.qrc`; no runtime downloads (the GUI must fully render with
  zero connectivity).

## 10. Don'ts (brand police)

- ❌ Purple on anything a human played or set (steps you edited are teal).
- ❌ Red spectrum bars, red "loudness war" theatrics.
- ❌ Light backgrounds, pure black `#000000`, or opacity-tinted brand colors as
  new semantic colors.
- ❌ Sans-serif numbers in meters/scores (mono only).
- ❌ Touch-only flows — every action reachable by encoders + soft keys.
- ❌ Hardcoded hex/px in components — Theme singleton only.
