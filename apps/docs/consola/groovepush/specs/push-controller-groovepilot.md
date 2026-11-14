# GroovePilot Push Controller — OpenSpec v1.0

> **Document type**: OpenSpec (drives implementation phase by phase)
> **Product**: GroovePilot — AI Production Copilot for Electronic Music
> **Component**: Hardware Controller Integration (Push 3 clone: Teensy 4.1 + NeoTrellis M4 + Raspberry Pi 5)
> **Author**: Juan David Guerrero / GPROG
> **Date**: July 2026
> **Status**: Draft — ready for review
> **Companion doc**: `docs/push-controller-gui-design.md` (QML GUI brand & style)

---

## 1. Vision

Turn the DIY Push 3 clone into the **first hardware surface with an embedded AI mix copilot**.
Ableton Push gives you hands-on control of Live; the GroovePilot Push adds what no
commercial controller has: **live Mix Score on the display, one-knob AI fixes, and
AI pattern generation written straight into Live clips from a pad grid** — all powered
by the GroovePilot platform that already exists in this repo.

The controller does NOT reimplement any intelligence. It is a **thin, beautiful,
low-latency remote** for three systems that are already built:

| Existing system | What the hardware gets from it |
|---|---|
| VST3 Plugin (JUCE) | Live audio features, Mix Score, connection to the DAW |
| Remote Script (LOM) | Transport, tempo, parameter control, MIDI clip write/read, track scan |
| Backend (FastAPI + Bedrock) | Chat (SSE), MIDI generation, audit, AI tools, sound design recipes |

### Design principles (INVIOLABLE)

1. **Performance MIDI never touches the network.** Pads/encoders/faders reach Live
   as class-compliant USB MIDI from the Teensy. Latency budget: < 5 ms.
2. **The plugin owns the Remote Script connection.** The Remote Script TCP server
   (localhost:9877) has exactly one client: the plugin. The hardware talks to the
   plugin, which relays. No second client on 9877, ever.
3. **The RPi talks to the cloud directly for AI**, using the same `gp_sk_` API key
   auth that already exists (`get_plugin_user`). No new auth system.
4. **Audio never leaves the machine** — unchanged. The hardware only ever sees the
   ~30 numerical features the plugin already extracts.
5. **Tier gating is enforced server-side** (already true) and mirrored in the UI:
   the hardware is a Pro/Studio companion; Studio unlocks parameter control and
   Pattern Learner surfaces, same as the plugin.

---

## 2. Hardware Under Spec

```
┌────────────────────────────────────────────────────────────────┐
│  GroovePilot Push (DIY Push 3 clone)                           │
│                                                                │
│  ┌─────────────┐   UART/USB    ┌──────────────────────────┐   │
│  │ Teensy 4.1  │◄─────────────►│ Raspberry Pi 5           │   │
│  │             │               │                          │   │
│  │ • Encoders  │               │ • QML GUI (5" display)   │   │
│  │ • Faders    │               │ • gp-bridged daemon      │   │
│  │ • Buttons   │               │ • Wi-Fi / Ethernet       │   │
│  │ • USB MIDI  │               └──────────┬───────────────┘   │
│  └──────┬──────┘                          │                    │
│         │ I2C/Serial                      │                    │
│  ┌──────┴──────┐                          │                    │
│  │ NeoTrellis  │                          │                    │
│  │ M4 (pads,   │                          │                    │
│  │ 4x8 RGB)    │                          │                    │
│  └─────────────┘                          │                    │
└─────────────────────────┬─────────────────┼────────────────────┘
                          │ USB MIDI        │ LAN (TCP 9878)      │ HTTPS/SSE
                          ▼                 ▼                     ▼
                  ┌──────────────┐  ┌──────────────┐   ┌──────────────────┐
                  │ Ableton Live │  │ GroovePilot  │   │ GroovePilot      │
                  │ (MIDI input) │  │ VST3 Plugin  │   │ Backend (AWS)    │
                  │              │  │ Controller   │   │ api.groovepilot.co│
                  │ Remote Script│◄─┤ Bridge port  │   │ REST + SSE       │
                  │ TCP 9877     │  │ (relay)      │   └──────────────────┘
                  └──────────────┘  └──────────────┘
```

### Roles

| Board | Role | Firmware/Software |
|---|---|---|
| **Teensy 4.1** | Real-time I/O hub: scans encoders/faders/buttons, merges NeoTrellis pad events, presents itself as a class-compliant USB MIDI device to the host computer. Also streams control events to the RPi over UART for GUI feedback. | Arduino/PlatformIO C++, `usb_midi` |
| **NeoTrellis M4** | 4x8 RGB pad grid. Sends pad hits to Teensy (I2C/Serial); receives LED color frames back. | CircuitPython or Arduino |
| **Raspberry Pi 5** | Brain of the *intelligence* layer: runs `gp-bridged` (Python daemon) + QML GUI. Talks LAN to the plugin and HTTPS to the backend. **Not in the MIDI performance path.** | Raspberry Pi OS, Python 3.12, Qt 6 / QML |

### Two latency domains (do not mix)

| Domain | Path | Budget | Content |
|---|---|---|---|
| **Performance** | NeoTrellis → Teensy → USB MIDI → Live | < 5 ms | Notes, CC, transport MIDI-mapped |
| **Intelligence** | RPi ↔ Plugin (LAN) and RPi ↔ Backend (HTTPS) | 50 ms – 3 s | Features, score, session state, chat, MIDI generation, AI tools |

---

## 3. Communication Architecture

### 3.1 New component: Plugin **Controller Bridge** (port 9878)

The plugin gains a second TCP server thread (mirrors the existing
`TcpBridgeClient` code, reversed direction). It reuses the wire format already
implemented in `Source/Network/BridgeProtocol.h`:

```
Wire format (identical to Remote Script bridge):
  [4 bytes big-endian uint32 = UTF-8 body length] [UTF-8 JSON body]

Message envelope (identical to BridgeMessage):
  { "type": "...", "id": "<correlation-id>", "payload": { ... } }
```

**Why reuse this format?** One protocol implementation to test, and the plugin's
relay becomes almost mechanical: validate → forward → correlate response.

- Binding: `0.0.0.0:9878` **only when the user enables "Hardware Controller" in
  plugin settings** (default OFF — never open a LAN port silently).
- Discovery: plugin advertises `_groovepilot._tcp.local.` via mDNS/Bonjour
  (service TXT: `version`, `plugin_port=9878`). The RPi resolves it with Avahi —
  zero-config, no IP typing on a 5" screen.
- Pairing: first connection requires a 6-digit code displayed in the plugin GUI,
  typed on the controller. On success the plugin issues a `controller_token`
  (random 32-byte, stored on both sides). Subsequent connections present the token.
  This prevents any LAN device from driving the user's DAW.
- Single controller client at a time (same rule as the 9877 link).

### 3.2 Message catalog — Controller ↔ Plugin (TCP 9878)

**Controller → Plugin (relayed to Remote Script or answered locally):**

| `type` | Handled by | Notes |
|---|---|---|
| `hello` | plugin | `{token}` → `{ok, plugin_version, rs_connected, be_connected, user_plan}` |
| `get_session` | relay → RS | Returns tracks, devices, BPM, key, transport (already implemented) |
| `transport_play` / `transport_stop` / `transport_record` / `transport_loop` | relay → RS | Already implemented in `_dispatch` |
| `set_tempo` | relay → RS | `{bpm}` |
| `set_parameter` / `set_property` / `apply_param_batch` | relay → RS | **Studio only** — plugin checks plan before relaying |
| `write_midi_clip` | relay → RS | Used after MIDI generation; `{track_index, slot_index, notes, length_beats, overwrite}` |
| `read_midi_clip` | relay → RS | For ARRANGE-mode variations (`source_notes`) |
| `create_midi_track` | relay → RS | Target for generated patterns |
| `start_track_scan` / `cancel_track_scan` | relay → RS | Triggers per-track solo scan from hardware |
| `subscribe_events` | relay → RS | Controller subscribes to tempo/transport/track changes |
| `trigger_audit` | plugin | Plugin runs its existing audit flow, streams result back |
| `get_features` | plugin | Latest feature snapshot (spectrum 7-band, LUFS, stereo, transients) |
| `subscribe_features` | plugin | Push mode: plugin sends `features_frame` every N ms (default 100 ms, min 50) |
| `ping` | plugin | Keepalive |

**Plugin → Controller (push):**

| `type` | Content |
|---|---|
| `features_frame` | `{spectrum: [7], lufs_short, lufs_integrated, peak_db, stereo_width, correlation, is_playing}` — drives display meters and pad/button LED ambient feedback |
| `score_update` | `{score, dimensions[7], delta, genre}` — whenever the plugin receives a new score |
| `session_state` | Forwarded from Remote Script event subscriptions (tempo change, track add/remove, transport) |
| `audit_result` | Full audit payload (issues, priorities, suggested fixes with `fix_id`s) |
| `connection_state` | `{rs_connected, be_connected}` — mirrors plugin's bottom-bar dots |
| `error` | `{code, message}` |

### 3.3 Controller → Backend (direct HTTPS, no plugin in the path)

`gp-bridged` on the RPi authenticates with the **same API key** the user already
has (`gp_sk_...`, validated by the existing `get_plugin_user` dependency). Entered
once during onboarding on the controller (or pushed from the plugin during pairing
— see Phase 2).

Endpoints consumed (all already implemented in `services/backend/api/routes/`):

| Endpoint | Hardware feature |
|---|---|
| `POST /api/chat` (SSE) | Copilot screen: streamed AI answers on the 5" display. Context assembled on the RPi from latest `features_frame` + `session_state` — same shape the plugin sends. |
| `POST /plugin/midi/generate` | Pad-grid pattern generation: `{genre, type: drums\|percussion\|fill\|bassline\|chord_progression\|melody, bars, humanize, key, scale, source_notes?, variation_mode?}` |
| `POST /plugin/tools/call` | One-knob AI fixes: `gain_adjust`, `eq_sweep`, `sidechain_suggest`, `limiter_ceiling` |
| `POST /plugin/audit` | Full mix audit from a dedicated button |
| `POST /plugin/score` | Real-time scoring when plugin link is down but features were cached (degraded mode) |
| `GET /plugin/genres` | Genre selector on hardware |
| `GET /plugin/usage` | Quota display (analyses/chat/MIDI remaining this month) |
| `GET /plugin/latest-version` | Controller software update check |
| `POST /plugin/log` | Crash/error reporting (no PII, same masking rules) |

**Rule**: rate limits and plan gates are *already* enforced by these endpoints.
The controller UI must render 429/403 responses gracefully (show quota + upgrade
hint), never retry-loop.

### 3.4 Internal bus — RPi daemon ↔ QML GUI ↔ Teensy

```
┌───────────────────────── Raspberry Pi 5 ─────────────────────────┐
│                                                                  │
│  ┌────────────┐   ZeroMQ pub/sub    ┌─────────────────────────┐  │
│  │ gp-bridged │◄───(ipc socket)────►│ QML GUI (Qt 6)          │  │
│  │ (Python)   │                     │ QZmq / QWebSocket client │  │
│  └─────┬──────┘                     └─────────────────────────┘  │
│        │ UART 1 Mbaud (JSON lines)                               │
│        ▼                                                         │
│   Teensy 4.1 (control events in, LED/display hints out)          │
└──────────────────────────────────────────────────────────────────┘
```

- `gp-bridged` is the single owner of: plugin TCP link, backend HTTPS session,
  Teensy UART. The GUI is a pure view — it renders bus messages and publishes
  user intents. This keeps QML free of networking edge cases and lets us test
  the daemon headless with pytest.
- Teensy → RPi UART messages: `{"ev":"enc","idx":3,"delta":-2}`,
  `{"ev":"btn","idx":12,"state":1}`, `{"ev":"fader","idx":0,"val":0.72}`,
  `{"ev":"pad","row":2,"col":5,"vel":98}` (pads also go out USB MIDI in parallel).
- RPi → Teensy: LED frames for pads/buttons (`{"led":"pad","row":2,"col":5,"rgb":[0,212,170]}`),
  encoder ring hints, mode changes.

---

## 4. AI Capability Map — what each control surface gains

This is the product core of the spec: every capability below is backed by an
endpoint or bridge command that **exists today**.

### 4.1 Mix Score, always on (display + LEDs)

- The 5" display home screen shows the live Mix Score (0–100) with the 7-dimension
  breakdown, updated on every `score_update`.
- **Hardware-native twist**: the top row of 8 buttons doubles as a *score LED bar*
  in Mix mode — teal count (score/12.5 per LED), amber/red on regressions.
- Delta feedback: when a score improves after an applied fix, a brief teal sweep
  animation runs across the pad grid (LED frame from RPi).

### 4.2 One-knob AI fixes (encoders + `tools/call` + `apply_param_batch`)

Flow (Studio tier):
1. User presses **FIX** button → RPi calls `POST /plugin/tools/call` with the tool
   inferred from the worst-scoring dimension (e.g. LUFS off-target → `gain_adjust`).
2. Suggestion renders on display: current value, suggested value, dB delta, reason.
3. The **main encoder** becomes a "confidence dial": turning scales the suggestion
   0–120 % (display shows interpolated value); pressing applies via
   `apply_param_batch` through the plugin relay.
4. **UNDO** button reverts (`POST /recommendations/apply/{fix_id}/revert` exists,
   plus plugin-side prior-value cache).

Pro tier: steps 1–2 only (suggestion shown, apply gated with upgrade hint) —
mirrors plugin behavior.

### 4.3 AI pattern generation on the pad grid (`/plugin/midi/generate` + `write_midi_clip`)

The killer feature. GEN mode turns the 4x8 NeoTrellis into a pattern workstation:

- **Row mapping (drums, 16-step)**: 2 pad rows = 16 steps of one lane; lane
  selector on side buttons (kick/snare/hat/perc — matches the drum generator's
  16-step tick grid).
- **Generate**: user picks genre + type + bars on display (encoder navigation),
  presses GENERATE → RPi calls `/plugin/midi/generate` → notes preview on the
  grid (LED ghost pattern, velocity = brightness) → **audition** via USB MIDI
  local playback (Teensy sequencer clocked by Live via MIDI clock) → **COMMIT**
  writes the clip via `write_midi_clip` to a selected track/slot.
- **Variations without menus**: with a clip selected, three dedicated buttons map
  to the existing `variation_mode` values — **COMPLEMENT / VARIATION / RESPONSE**.
  RPi first pulls `read_midi_clip` for `source_notes`, then regenerates.
- **Key/scale awareness**: `get_session` provides key + BPM; harmonic types
  (chords/bass/melody) auto-fill `key`/`scale`, display shows "Am · 122 BPM"
  pulled live from the session.
- Free plan: chord progressions only (server enforces; UI shows lock icons on
  other types).

### 4.4 Scale-aware pad performance (session intelligence, zero cloud)

- PLAY mode lays the grid out as a Push-style isomorphic scale (4ths layout) using
  the session key from `get_session`. Root notes = teal LEDs, in-scale = dim white,
  out-of-scale = off (chromatic toggle available).
- This is pure Teensy+RPi logic — no backend call — but it's *fed* by GroovePilot
  session awareness, which standalone controllers don't have.

### 4.5 Copilot chat on hardware (`POST /api/chat`, SSE)

- COPILOT mode: quick-action buttons ("Diagnose my mix", "How's my low end?",
  "What should I fix first?") send pre-built prompts with live feature context.
- Response streams token-by-token onto the display (SSE chunks → QML text).
- Free-text entry is **not** a hardware goal (no keyboard); the quick-action set
  is curated and genre-aware. A "continue on phone/web" QR code hands the
  conversation off to app.groovepilot.co (chat sessions API already supports
  multi-session history).

### 4.6 Sound Design copilot (chat `mode: "sound-design"`)

- SOUND mode: pick a synth (serum, vital, diva, analog, operator, wavetable,
  massive_x — the launch set validated by the backend) + a target ("warm organic
  house pluck") from curated lists → recipe streams to display as a parameter
  checklist the user follows on their synth.
- Purple-accented UI (AI feature — brand rule).

### 4.7 Audit & track scan from the hardware

- **AUDIT** button → `trigger_audit` via plugin → results as a prioritized issue
  list on display; selecting an issue deep-links to the matching FIX flow (§4.2).
- **SCAN** (Studio): long-press AUDIT → `start_track_scan` relayed to the Remote
  Script's solo-scan machinery; progress bar on display (per-track updates via
  event subscription); cancel restores solo states (already handled RS-side).

### 4.8 Transport & tempo (Remote Script relay)

- Dedicated PLAY/STOP/REC buttons → `transport_*` (TCP relay, not MIDI-mapped,
  so they work even when Live's MIDI focus is elsewhere).
- Tempo encoder → `set_tempo` with local echo on display; also nudges Live via
  the relay rather than MIDI mapping for exact BPM values.

### 4.9 Degraded modes (must be explicit states, not bugs)

| State | Behavior |
|---|---|
| No plugin link (LAN down / plugin closed) | PLAY mode + local sequencer keep working (USB MIDI path unaffected). AI screens show "Plugin offline" with reconnect spinner. Direct-backend features (chat, gen preview) still work; clip COMMIT queues until relink. |
| No internet | Everything DAW-side works (transport, params, scale mode, clip write of *previously generated* patterns). AI screens show cached last score + offline banner. |
| Wrong tier | Feature renders locked with plan hint — server remains the source of truth. |

---

## 5. Security & Privacy

- Controller Bridge port (9878) is **opt-in**, LAN-only, pairing-code + token
  authenticated (§3.1). The plugin refuses `set_parameter`/`apply_param_batch`
  relays from an unpaired or stale-token client.
- API key on the RPi is stored in a root-owned file with mode 0600
  (`/etc/groovepilot/credentials`), never in the QML layer, never logged.
- All existing privacy rules inherit unchanged: no audio bytes on any link — the
  hardware only ever receives the ~30 features; logs mask emails; telemetry from
  `gp-bridged` reuses `POST /plugin/log` with the same anonymization.
- mDNS advertises no user-identifying data (no email, no user id in TXT records).

---

## 6. Implementation Phases

### Phase 1 — Controller Bridge in the plugin (C++)
- New `ControllerBridgeServer` (thread + juce::StreamingSocket) on 9878, opt-in
  setting, mDNS advertise, pairing code UI, token store in PropertiesFile.
- Relay layer: forward whitelisted message types to the existing
  `TcpBridgeClient`, correlate by `id`, plan-gate `set_parameter`/`apply_param_batch`.
- Push channels: `features_frame` (reuse MeterProcessor output), `score_update`,
  `connection_state`.
- Tests: protocol round-trip (reuse `BridgeProtocol` tests), pairing rejection,
  relay correlation under concurrent commands.

**Acceptance**: `nc`-level test client can pair, pull `get_session`, receive
`features_frame` at 10 Hz, and a `write_midi_clip` lands in Live.

### Phase 2 — `gp-bridged` daemon on RPi 5 (Python 3.12)
- Avahi discovery + pairing flow; plugin link with reconnect/backoff.
- Backend client: API-key auth, SSE chat consumer, midi/generate, tools/call,
  audit, usage. (Mirror the request shapes from `api/routes/plugin.py` schemas.)
- ZeroMQ pub/sub bus for the GUI; UART protocol with Teensy.
- Systemd unit, watchdog, `POST /plugin/log` error reporting.
- Tests: pytest with a fake plugin server + `respx`-mocked backend (deterministic,
  no network — same testing rules as the rest of the repo).

**Acceptance**: headless daemon on the bench passes an end-to-end scripted
session: pair → session state → generate drums → commit clip → score update.

### Phase 3 — Teensy & NeoTrellis firmware integration
- UART JSON-lines protocol both directions; LED frame renderer for pad grid.
- USB MIDI: pads (notes, velocity curve), encoders (CC relative), faders (CC),
  MIDI clock slave for local audition sequencer.
- GEN-mode step-grid editing (toggle steps locally, diff sent to RPi so COMMIT
  writes the *edited* pattern).

**Acceptance**: pad → Live note latency < 5 ms measured; LED preview of a
generated drum pattern matches the notes committed to the clip.

### Phase 4 — QML GUI modes (see companion design doc)
- Screens: HOME/Score, GEN, PLAY, COPILOT, SOUND, AUDIT, SETTINGS.
- Encoder-first navigation; SSE streaming text; usage/quota surfaces.

**Acceptance**: every AI flow in §4 drivable end-to-end from hardware alone.

### Phase 5 — Polish & resilience
- Degraded-mode matrix (§4.9) verified by pulling cables mid-flow.
- Update channel (`GET /plugin/latest-version` → controller OTA script).
- 4-hour soak: no crash, no LED desync, plugin CPU overhead of bridge < 0.5 %.

---

## 7. Open Questions (decide before Phase 2 ends)

1. Pairing hand-off of the API key: push from plugin over the paired link
   (convenient) vs. manual entry on controller (simpler threat model). Leaning:
   push over paired+tokened link, user confirms on plugin side.
2. `features_frame` rate: 10 Hz default is enough for the 5" display; is 20 Hz
   needed for smooth spectrum animation, and can the QML scene sustain it on RPi 5?
   (Benchmark in Phase 4.)
3. Does GEN audition use Live as the sound source via a dedicated armed track
   (simple, uses user's own sounds) or an onboard sample player on the RPi
   (works without Live focus)? Leaning: Live armed track — zero new audio code.
4. Multi-controller future (two units, one session) — out of scope v1; protocol's
   single-client rule keeps this cleanly deferred.
