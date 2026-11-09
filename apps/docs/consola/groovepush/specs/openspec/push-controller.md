# Push Controller — Integration OpenSpec

> **Id**: SPEC-PUSHCTRL
> **Status**: Accepted (normative source adopted)
> **Owners**: integration-architect
> **Normative source**: [`../push-controller-groovepilot.md`](../push-controller-groovepilot.md) (GroovePilot Push Controller OpenSpec v1.0)
> **Companion**: [`gui.md`](gui.md) (SPEC-GUI), [`processor.md`](processor.md) (SPEC-PROC)
> **Version**: 1.0

## 1. Purpose & context

This is the umbrella spec for turning the GroovePush controller into the AI mix-copilot
surface for GroovePilot. The full architecture, message catalog, AI capability map,
security model, and phase plan live in the **normative source**
[`push-controller-groovepilot.md`](../push-controller-groovepilot.md); this file adopts it
as binding and assigns stable requirement ids used by the tasks and the gap report.

## 2. Scope
- **In scope**: the seams — Controller Bridge (plugin TCP 9878), `gp-bridged` daemon on
  the RPi, the RPi↔GUI bus, the RPi↔Teensy UART, and the backend HTTPS/SSE consumption.
- **Out of scope**: the plugin, Remote Script, and backend internals (they exist already);
  the pad/encoder/fader **firmware I/O** (see SPEC-PROC) and **screens** (see SPEC-GUI).

## 3. Constraints (inviolable — from normative §Design principles)
- C-1: Performance MIDI never touches the network (USB-MIDI, < 5 ms).
- C-2: The plugin owns the Remote Script link (`localhost:9877`), exactly one client.
- C-3: The RPi talks to the cloud directly with the existing `gp_sk_` auth. No new auth.
- C-4: Audio never leaves the machine — only the ~30 numeric features cross links.
- C-5: Tier gating is server-side, mirrored (not enforced) in the UI.

## 4. Requirements (stable ids → normative sections)

| Id | Requirement | Normative § |
|----|-------------|-------------|
| R-PUSHCTRL-1 | Plugin exposes an opt-in **Controller Bridge** on TCP 9878 with mDNS discovery, 6-digit pairing, and a 32-byte token; single client. | §3.1 |
| R-PUSHCTRL-2 | Bridge speaks the existing length-prefixed JSON envelope `{type,id,payload}` and relays the whitelisted message catalog to the Remote Script, plan-gating `set_parameter`/`apply_param_batch`. | §3.1–3.2 |
| R-PUSHCTRL-3 | Plugin pushes `features_frame` (≤100 ms), `score_update`, `session_state`, `audit_result`, `connection_state`, `error` to the controller. | §3.2 |
| R-PUSHCTRL-4 | `gp-bridged` (RPi, Python 3.12) is the sole owner of the plugin TCP link, backend HTTPS session, and Teensy UART; the GUI is a pure view over a ZeroMQ pub/sub bus. | §3.4, §8 |
| R-PUSHCTRL-5 | `gp-bridged` consumes the backend endpoints (chat SSE, midi/generate, tools/call, audit, score, genres, usage, latest-version, log) with `gp_sk_` auth and renders 429/403 gracefully (no retry-loop). | §3.3 |
| R-PUSHCTRL-6 | Teensy↔RPi UART carries JSON-line control events up (`enc`/`btn`/`fader`/`pad`) and LED/mode frames down. | §3.4 |
| R-PUSHCTRL-7 | The AI capability map (Mix Score, one-knob FIX, GEN pattern gen + clip write, scale-aware PLAY, COPILOT chat, SOUND recipes, AUDIT/SCAN, transport/tempo) is drivable end-to-end from hardware. | §4 |
| R-PUSHCTRL-8 | The three degraded modes (no plugin link / no internet / wrong tier) are explicit UI states, not failures. | §4.9 |
| R-PUSHCTRL-9 | Security: 9878 opt-in + pairing + token; API key in a root-owned 0600 file, never in QML, never logged; mDNS carries no PII. | §5 |

## 5. Acceptance criteria
Adopt the per-phase acceptance gates in normative §6 (Phase 1–5). Umbrella done-when:
- AC-1 (R-PUSHCTRL-1..3): an `nc` client pairs, pulls `get_session`, receives
  `features_frame` at 10 Hz, and a `write_midi_clip` lands in Live.
- AC-2 (R-PUSHCTRL-4..6): headless `gp-bridged` passes the scripted session
  pair→state→generate drums→commit clip→score update (pytest, mocked backend).
- AC-3 (R-PUSHCTRL-7..8): every §4 flow works from hardware; cables pulled mid-flow
  degrade to the §4.9 states.

## 6. Open questions
Carry the four open questions in normative §7 (API-key hand-off, `features_frame` rate,
GEN audition source, multi-controller) — decide before Phase 2 ends.
