# GroovePush — Gaps & Improvements

_Analysis date: 2026-07-16 · Contrasts the current code against the new specs:_
_[`specs/openspec/push-controller.md`](specs/openspec/push-controller.md) (adopts_
_[`push-controller-groovepilot.md`](specs/push-controller-groovepilot.md)),_
_[`specs/openspec/gui.md`](specs/openspec/gui.md) (adopts_
_[`push-controller-gui-design.md`](specs/push-controller-gui-design.md)),_
_[`specs/openspec/processor.md`](specs/openspec/processor.md)._

---

## 0. Executive summary

The current repo is a **mature Ableton session/mixer controller**: a Push-clone that
triggers clips, drives the mixer, and mirrors Live on a 5" screen over a SysEx-based
serial link. It works and is well-structured (`LiveController`, `GUIInterface`,
`NeoTrellisLink` on the Teensy; `SerialController` + Qt models on the GUI; a rich
Python Remote Script).

The new specs describe a **different product layered on top**: an AI mix-copilot surface.
A code survey found **zero** occurrences of every core target concept — `zmq`,
`gp-bridged`, port `9878`, `features`, `score`, `pairing`, `Avahi`, `backend`,
`groovepilot`, `purple`/`#8B5CF6` — across `apps/gui` and `apps/processor`. So the
intelligence layer is **not started**; the existing controller is the **foundation** to
extend, not throw away.

### Maturity vs target

| Capability | Status | Where |
|---|---|---|
| Session/mixer/clip control over SysEx | ✅ Built | `LiveController`, `MidiCommands.h`, Remote Script |
| GUI shell, models, serial link, handshake/ping | ✅ Built | `SerialController`, `*Model.{h,cpp}`, `Main.qml` |
| USB-MIDI to Live (SysEx + some note/CC) | 🟡 Partial | `LiveController.cpp` (SysEx), `Piezo`/`Theremin`/`CapButtons` (note/CC) |
| Class-compliant note/CC for pads/encoders/faders | ❌ Missing | needed for PLAY/GEN — R-PROC-1/2 |
| `gp-bridged` daemon (plugin link, backend, bus) | ❌ Missing | entire RPi intelligence layer — R-PUSHCTRL-4/5 |
| Controller Bridge in plugin (TCP 9878) | ❌ Missing (plugin repo) | R-PUSHCTRL-1..3 |
| ZeroMQ bus + QML-as-pure-view | ❌ Missing | GUI does its own serial — R-GUI-8 |
| 7 AI mode screens + soft-key bar | ❌ Missing | current UI is tabbed Live-mirror — R-GUI-3/4 |
| Theme tokens (teal/purple, scale, bundled fonts) | ❌ Mismatch | `PushCloneTheme` is cyan/Helvetica — R-GUI-1/2 |
| Mix Score / spectrum / FIX / AUDIT / COPILOT / SOUND / GEN | ❌ Missing | R-GUI-5/6/7, R-PUSHCTRL-7 |
| mDNS pairing + token + API-key store | ❌ Missing | R-PUSHCTRL-9 |

---

## 1. Processor (`apps/processor`)

**Obsolete / to reconcile**
- ✅ **G-P1 (RESOLVED 2026-07-16, Block 0)** — Single conflict-free pin map now in
  [`apps/processor/PIN_MAP.md`](apps/processor/PIN_MAP.md), mirrored in `Config.h`.
  Encoders `{2,3,4,5,6,7,8,9}` + expansion `{10,11,12,26,27,28,29,30}`, avoiding Serial1
  (0/1), I2C (18/19), faders (14–17), piezo/IR (20–25). Compiles (`teensy41 SUCCESS`).
- ⚠️ **G-P2 (REVERSED 2026-10-19)** — the July call ("NeoTrellis link = I2C 0x2E/0x2F, M4 UART
  deprecated") was **wrong**: the user's grid is a single **NeoTrellis M4**. The UART path
  (`NeoTrellisLink` on `Serial1` @ 1 Mbps, env `neotrellis_m4`, `UART_CONNECTIONS.md`) is the real
  one and stays. Docs corrected; `Config.h` comments still say "deprecated" until G-P14 lands.
- ✅ **G-P14 (FIXED 2026-10-19)** — encoder pins `{2,3,4,5,6,7,8,9}` put Enc3/Enc4 on **7 and 8 =
  `Serial2`, the GUI link** (introduced in Block 0; the original map avoided them). Fixed by the
  carrier re-map: encoders on J5 pins, GUI on `Serial1`, M4 on `Serial5`, ports centralised in
  `Config.h` (`TEENSY_GUI_SERIAL` / `TEENSY_M4_SERIAL`). `teensy41`, `test_encoders_teensy` and
  `neotrellis_m4` build.
- ✅ **G-P3 (RESOLVED, Block 0)** — Removed the orphan `include/Config.h` (dead, nothing
  included it, held contradictory values). `include/shared/Config.h` is authoritative.
- ✅ **G-P11 (RESOLVED 2026-07-16, Block 0 scope narrowing)** — Phase 1 protoboard scope
  reduced by the user: **piezo, IR/theremin, and capacitive touch buttons are removed**
  from this device (piezo/IR become a future standalone e-drum module; capacitive buttons
  are replaced by MCP23017 pushbuttons). `Piezo.cpp`/`Theremin.cpp`/`CapButtons.cpp` stay
  in the tree but remain commented out in `Hardware.cpp`; their undefined pin macros
  (`PIEZO_PINS`, `IR_PINS`, `CAPACITIVE_PINS`) are a non-issue now since nothing includes
  them. Revisit only if a future e-drum module reuses this codebase.
  **2026-07-17**: marked DNP (Do Not Populate) in `hardware/analog.kicad_sch` too —
  `J12-J15` (Piezo1-4 connectors), `R3-R6` (1MΩ bleed resistors), `J16/J17` (IR1/IR2
  Sharp connectors). Symbols/wiring kept intact (not deleted) so the sheet stays valid
  if reused for the future e-drum module; `RV1-RV4` (faders) and `J11` (shared
  interconnect to `processor.kicad_sch`) were left populated since faders are active
  Phase 1 hardware. Done via direct `dnp` field edits (KiCad MCP was disconnected);
  re-run ERC once the MCP server is back to confirm nothing else needs attention.
- ✅ **G-P12 (RESOLVED 2026-07-16, Block 0)** — **Bug found and fixed**: `ButtonManager.cpp`
  mapped `METRONOME` (MCP #2) and the disabled `ENC_8` (MCP #1) to **library pin 7**
  (GPA7) — the MCP23017's output-only erratum pin, which cannot reliably read as a button
  input. Fixed to use library pin 8 (GPB0) instead, matching `hardware/processor.kicad_sch`
  exactly (verified via the KiCad net lister). Also fixed `NUM_ENCODER_BUTTONS` (was `4`,
  now `8` — matches the 8 physical encoders and the schematic) and enabled all 8 encoder
  button mappings (`ENC_5..ENC_8` were `false`, now `true`, since the user confirmed EC11
  encoders with integrated switches).
- ✅ **G-P13 (RESOLVED 2026-07-17)** — Physical **UNDO** button added (MCP #2, was the
  unused BANK_LEFT slot). `CMD_UNDO` (0x4D) existed as a constant in both
  `MidiCommands.h` and `remote-script/consts.py` but had **no handler anywhere** — added
  `TransportManager.trigger_undo()` (calls `Song.undo()`) and wired it into
  `handle_transport_command`. One MCP #2 slot (lib pin 5) remains unassigned by user
  choice. `METRONOME` (lib pin 8) still triggers `handleMixerModeChange()` instead of a
  literal metronome toggle — `CMD_METRONOME`/`toggle_metronome()` already exist and work
  end-to-end in the Remote Script, so swapping is cheap, but **not done** pending user
  confirmation (would strand mixer-mode switching until the GUI soft-key bar exists).

**Missing (for the new specs)**
- **G-P4 (urgent)** — No **JSON-lines UART** to the RPi (R-PROC-3). The current
  Teensy↔GUI path is SysEx/binary via `GUIInterface`; the `gp-bridged` bus expects
  `{"ev":"enc",…}` lines. New adapter needed (keep SysEx to Live).
- **G-P5** — Pads only trigger clips via SysEx; no **class-compliant note-on/off with a
  velocity curve** (R-PROC-1). Blocks PLAY and GEN audition.
- **G-P6** — Encoders/faders don't emit **MIDI-mappable relative CC/CC** to Live
  (R-PROC-2); today they feed SysEx mixer commands only.
- **G-P7** — No **LED frame renderer** driven by RPi frames with teal/purple semantics
  (R-PROC-4); LED state is currently clip-state driven only.
- **G-P8** — No **MIDI-clock-slaved audition sequencer** (R-PROC-5) and no **GEN step
  editing/diff** (R-PROC-6).
- **G-P9** — No **scale-aware isomorphic PLAY layout** (R-PROC-7).

**Improvements**
- **G-P10** — `src/teensy/main.cpp` hand-parses serial with `strtok`/global buffers;
  as UART grows to JSON, move to a small framed line reader with tests (`test_*` env).

---

## 2. GUI (`apps/gui`)

**Obsolete / mismatch**
- **G-G1 (urgent)** — `PushCloneTheme.qml` is a **cyan/Helvetica/Monaco** theme with **no
  `scale`, no purple-AI semantics, no teal `#00D4AA`, no bundled fonts** — it contradicts
  design §3/§4 (R-GUI-1/2). Rework into a `Theme` singleton with the exact tokens and
  `scale = Screen.width/800`, or add a new `qml/Theme/` tree and migrate (decide: G-G1).
- **G-G2** — `Main.qml` uses **tab navigation** (Session/Mix/Device/Browse/Note) with
  Device/Browse/Note as "Coming soon" placeholders. The target is **encoder-first mode
  switching** with an 8-cell soft-key bar (R-GUI-3/4). The tab/NavigationBar model and the
  placeholders are obsolete for the new UX.
- **G-G3** — Stray duplicate `apps/gui/CMakeLists 2.txt` (carried from the old repo) — delete.

**Missing**
- **G-G4 (urgent)** — GUI owns its own serial link (`SerialController` + `QSerialPort`),
  violating "**no networking in QML / pure view over the bus**" (R-GUI-8). Needs a
  **ZeroMQ bus client** seam; `SerialController` becomes (at most) a fallback transport.
- **G-G5** — None of the **7 mode screens** exist (HOME/Score, GEN, PLAY, COPILOT, SOUND,
  AUDIT, SETTINGS/PAIRING) — R-GUI-4..7.
- **G-G6** — No **Mix Score ring / spectrum / dimension rows** (R-GUI-5); no `GpScoreRing`,
  `GpSpectrum`, `GpStepGrid`, `GpChatView`, `GpSoftKeyBar`, `GpConnectionDots`, `GpToast`,
  `GpQuotaChip` components (design §8).
- **G-G7** — No **SSE streaming chat** rendering, no **QR hand-off**, no quota/429 surfaces
  (R-GUI-7).
- **G-G8** — No **pairing/first-boot** flow (mDNS spinner, 6-digit code, API-key confirm) —
  design §6.7, R-PUSHCTRL-9.
- **G-G9** — No **General Sans / JetBrains Mono** in `resources.qrc` (R-GUI-2).

**Improvements**
- **G-G10** — Reusable component library is thin (`ClipPad`, `MixChannelStrip`,
  `TransportBar`, `NavigationBar`); build the `Gp*` set against the Theme singleton.
- **G-G11** — Verify 60 fps budget rules (no `Canvas`, no per-frame JS timers) as new
  meters land (R-GUI-9).

---

## 3. Remote Script (`apps/remote-script`)

The Remote Script is mature (transport, clip R/W, track scan, session ring). For the new
specs it is **consumed by the plugin relay**, so mostly it just needs contract alignment.

- **G-RS1** — Confirm the relay message catalog (parent §3.2: `get_session`,
  `write_midi_clip`, `read_midi_clip`, `create_midi_track`, `start_track_scan`,
  `subscribe_events`, `apply_param_batch`, …) is fully implemented in `_dispatch`; fill
  any gap. The spec claims these exist — **verify**, don't assume.
- **G-RS2** — Keep SysEx command IDs in `consts.py` in lockstep with firmware
  `MidiCommands.h` (3-endpoint contract). Add a conformance check (see G-I3).
- **G-RS3 (hygiene)** — 15 tracked `.py` files had **uncommitted edits** in the live
  Ableton folder at import; the monorepo now holds that state — reconcile and adopt the
  monorepo as source of truth, deploy via `update_pushclone.sh`.
- **G-RS4** — Large reference blobs (`lom_full.txt`, the 1.1 MB LOM PDF) live beside code;
  PDF is now gitignored — consider moving references under `docs/`.

---

## 4. Integration — new components that don't exist yet

These are net-new and the bulk of the work (parent §6 phases):

- **G-I1** — **Controller Bridge** in the VST3 plugin (TCP 9878): opt-in, mDNS advertise,
  pairing code, token store, whitelisted relay, plan-gating, push channels
  (`features_frame`/`score_update`/`connection_state`). Lives in the **plugin repo**
  (external), tracked here as a dependency (R-PUSHCTRL-1..3).
- **G-I2** — **`gp-bridged`** daemon on the RPi (Python 3.12): Avahi discovery + pairing,
  plugin link w/ reconnect, backend client (API-key, SSE chat, midi/generate, tools/call,
  audit, usage), ZeroMQ bus, UART to Teensy, systemd unit, `POST /plugin/log`. **New
  workspace** — suggest `apps/bridged` (R-PUSHCTRL-4/5).
- **G-I3** — **Protocol/contract conformance**: a single source-of-truth for command IDs
  and payloads across firmware / Remote Script / GUI / bridge, plus a test that fails when
  they drift (owned by `qa-hardware-test`).
- **G-I4** — **Security**: API key in root-owned `0600` `/etc/groovepilot/credentials`,
  never in QML/logs; mDNS TXT carries no PII (R-PUSHCTRL-9).
- **G-I5** — **Degraded-mode matrix** (parent §4.9) implemented and testable by pulling
  cables mid-flow (R-PUSHCTRL-8).

---

## 5. Hardware follow-ups (from Phase 3)

- **G-H1** — Assign KiCad **footprints** (schematic-only today) before PCB layout.
- **G-H2** — Map the **Teensy 4.1 socket** pins to the real footprint (J1 pins are logical).
- **G-H3** — Run **ERC in KiCad 10** (no `kicad-cli` in the analysis env).
- **G-H4** — Select a **3.3 V-compatible IR** sensor or add a divider (analog board).
- **G-H5** — `processor.kicad_sch` J2 is a "NeoTrellis I2C" connector: must become a **UART**
  connector to the NeoTrellis M4 (G-P2 reversed). Moot if the Brain carrier is adopted (G-H10).
- ~~**G-H4**~~ — moot: IR removed from scope (G-P11). `kicad-cli` 10.0.4 now available at
  `/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli` (unblocks G-H3).

**Found in the 2026-10-19 review (7" display, Brain integration, PCB order):**
- ✅ **G-H6 (resolved by G-H10)** — the Brain carrier already cuts VUSB–VIN and protects the
  input (USB-C power-only, 3 A polyfuse, TVS). Original note: **5 V back-feed.** `J4` (5V PWR IN) feeds the Teensy
  `VIN`. When the Teensy's USB is also plugged into a computer or into the Brain, the two
  supplies are tied together. Cut the Teensy's VIN–VUSB pads (or add a Schottky/ideal-diode)
  and decide which side powers the Teensy. The Pi 5 + 7" panel can't be powered from a USB-A
  port either (Pi 5 needs 5 V / 5 A).
- **G-H7 (urgent)** — **There is no PCB layout yet.** `hardware/GroovePush.kicad_pcb` is empty
  (0 footprints, 0 tracks). Only schematics exist, and they predate the 7" display, the DNP of
  piezo/IR and ADR-0004.
- **G-H8** — **Schematic out of date with ADR-0004**: break out GPB1–GPB6 of both MCP23017s;
  connectors J8/J20 grow; per-key SK6812-MINI-E chain for the Choc V2 keys.
- **G-H9** — Carrier board candidate for the shared JLCPCB order (with the GrooveForge Brain
  boards, in ~2–3 weeks): `processor` sheet only (Teensy socket, 2× MCP23017, R1/R2, C1–C3,
  power entry, IDC/JST connectors to the panels, Dupont header to the Pi 5), ≤ 100 × 100 mm.
  Panel boards wait for the enclosure layout. Apply
  `grooveforge-brain/apps/hardware/kicad/studio/docs/lecciones-para-groovepush.md` and reuse
  its scripts (`set_lcsc.py`, `set_idc_pinout.py`, `export_fab.py`).
- ✅ **G-H10 (ACCEPTED 2026-10-19, carrier used unmodified)** — **Reuse the Brain's carrier board** (`portadora`,
  97 × 100 mm) instead of designing G-H9: JLC ships ≥ 5 copies, the Brain needs 1–2. Checked
  against its netlist (2026-10-19):
  - Panel IDC `J5` (2×15) carries 23 Teensy GPIO: 2–5, 9–17, 22, 24, 26, 30–32, 34, 35, 40, 41
    + LED_DIN (pin 28, buffered) + 5 V / 3.3 V / GND. Enough for 4 faders on 14–17 (A0–A3, same
    as today) + 16 encoder A/B pins + 3 spare (13, 40, 41). **Encoder pins move** → `PIN_MAP.md`
    and `Config.h` change.
  - **I2C 18/19 is not on `J5`**: it only reaches the Audio Shield socket `J15` (pin 6 SDA, 7 SCL,
    12 3.3 V, 13 GND). GroovePush leaves the shield out and takes I2C from `J15` with a cable;
    the 4.7 kΩ pull-ups go on the GroovePush side (the carrier relies on the shield's).
  - UART to the Pi on `J6` (pins 0/1) — same as GroovePush.
  - Solves G-H6: VUSB–VIN pads cut, USB-C power-only input, 3 A polyfuse, TVS.
  - **Power budget:** the 3 A polyfuse can't feed a Pi 5 + 7" panel; power the Pi 5 from its own
    supply and connect only UART + GND on `J6`. Cap LED brightness in firmware.
  - Order the Brain carrier with **assembly quantity covering the GroovePush copy**; leave its
    audio jacks unsoldered.
- **G-X1 (cross-repo, GrooveForge Brain)** — the Brain's USB-A host (`midi_host.cpp`) only
  receives. Needed for Brain mode feedback (ADR-0002 Rev. 1, SPEC-PROC R-PROC-11).

---

## 6. Urgent refactorings (do first — they unblock everything else)

1. **G-P1 / G-P2 / G-P3** — settle the pin map, the NeoTrellis link, and the duplicate
   `Config.h`/`include` trees. Every firmware and hardware task depends on this truth.
2. **G-I2 scaffold** — stand up `apps/bridged` with the ZeroMQ bus and a fake plugin +
   mocked backend, so the GUI and firmware can develop against a real bus early.
3. **G-G1** — land the `Theme` singleton with correct tokens + `scale`; nothing else in the
   new GUI should be built on the old cyan theme.
4. **G-P4 + G-G4** — introduce the **JSON-lines UART ⇄ ZeroMQ** seam so QML stops owning
   the transport (R-GUI-8) and the daemon owns the links.
5. **G-P5/G-P6** — add the **class-compliant note/CC USB-MIDI** path (keeps <5 ms budget)
   so PLAY and GEN audition are possible.

## 7. Priority matrix

| Priority | Items | Theme |
|---|---|---|
| **P0 — unblockers** | G-P1, G-P2, G-P3, G-I2 (scaffold), G-G1 | source-of-truth + foundations |
| **P1 — core plumbing** | G-P4, G-G4, G-I1, G-I3, G-P5, G-P6 | the bus + performance MIDI |
| **P2 — copilot features** | G-G5..G-G8, G-P7..G-P9, G-I4, G-I5 | screens + AI flows |
| **P3 — polish/hygiene** | G-P10, G-G3, G-G10, G-G11, G-RS3/4, G-H1..H5 | cleanup + resilience |

> Next: [`specs/openspec/*.tasks.md`](specs/openspec/) will turn P0/P1 into ordered,
> verifiable tasks (SDD step 2 via `/tasks`). See the Phase 5 roadmap.
