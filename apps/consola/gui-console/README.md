# GrooveForge Console

QML/PySide6 console GUI with a contract-14 synth host client and deterministic
review snapshots. SYNTH connects the physical surface and synth host; Motor and Live remain local design demos.
Coverage, absent designs and visual review: [11-gui-fase2.md](../../docs/consola/11-gui-fase2.md).

```sh
python3 apps/consola/gui-console/main.py
python3 apps/consola/gui-console/main.py --frameless --screen SequencerPianoRoll
python3 apps/consola/gui-console/main.py --screen Session
python3 apps/consola/gui-console/main.py --list-screens
python3 -m pytest apps/consola/gui-console/tests -q
python3 apps/consola/gui-console/tools/screenshot.py
python3 apps/consola/gui-console/tools/screenshot.py --screen Kit
```

Desktop verification uses Python 3.14 / PySide6 6.11.1. Pi setup uses apt Qt 6.8 / Python 3.13;
see [13-gui-pi.md](../../docs/consola/13-gui-pi.md) and `pi/`. Runtime needs no network.
`--screen` accepts a case-insensitive registry name or a legacy numeric ID.

1–9 retain Boot, ModeSelect, Toasts, Settings, LiveSession, Layer, Transition,
Mixer, Hands. **Tab / Shift+Tab** cycle all 61 entries. F2 opens mode selection;
Controller and Standalone enter their session screens; Synth enters PARTS.
Left/right select an encoder, up/down turn it, Enter presses its soft key.
Escape returns to the mode's session, or closes a shared Scale/Repeat/Tempo window.
O/E/L/N select offline/empty/loading/normal live data; B selects fill/stripe.

Piano roll: drag/pinch only scroll/zoom; Shift+arrows move the 8×4 pad window.
The `pressPad(column,row)` model slot simulates a hardware pad. Encoders edit note,
position, length, velocity and microtiming; Vary changes only engine notes. Editing
an engine note makes it yours. Steps also supports pad-window navigation and
batched velocity/probability/microtiming edits. Touch never draws or plays notes.

Layer and Motor use four cue keys plus Generate/Accept/Skip/Back. Motor actions only
change local option states; Accept never writes a Live clip. Library/export, audio
and calibration controls are visual previews, with no persistence or backend.
The old Brain ENGINE/HYBRID views remain available for comparison; their tabs
lead into the new synth catalog described below.

`registry.py` is the screen source of truth; `fake_data.py` owns commands and MVP
models; `review_data.py` supplies `SnapshotModel` lists with a stable `entry` role.
`review_samples.json` is deterministic sample content extracted from the supplied
Standalone/Controller HTML, including nested steps, notes and clips. No export JS
or HTML runs in the application. Standalone model timers start on pending changes. In a GUI window, `SurfaceBatcher`
owns a thread-safe FIFO, wakes the window on new input, drains at `afterAnimating`,
and flushes each model once before scenegraph sync. Independent model timers are disabled.
The engine owns the state QObject to keep its lifetime safe across engine teardown.

[fonts/README.md](fonts/README.md) describes local FontLoaders, explicit variable
weight axes, static Plex weights and licenses. General Sans stays gitignored;
its absence uses a system fallback and skips its CI font test. OFL files are bundled.

The screenshot tool fails on QML warnings, renders all registry PNGs at 1280×720 with Qt's
offscreen/software backend and produces [an offline gallery](screenshots/index.html)
and [a complete manifest](screenshots/manifest.json). These captures and desktop
tests do not establish the Pi 5 performance gate required by ADR-007.

Pi rotation: `main.py --rotate 90 --rotation-method qml`. Qt Quick requires a QML
transform; EGLFS's rotation environment variable only affects software widgets.
`--rotation-method eglfs` is an explicitly unverified diagnostic comparison.

```sh
python3 apps/consola/gui-console/tools/perf_gate.py --duration 3 --warmup 0 --offscreen --output /tmp/gf-gate.json
```

The harness returns 1 for smoke/FAIL/UNVERIFIED and 0 only for a complete Pi gate.
It saves JSON and Qt logs, checks frame pacing, QSG p99, enqueue-to-swap latency
and single-core CPU. `pi/run_perf.sh` also records temperature/throttling.

## Synth host and screens 31–45

The synth client uses contract-14 JSON lines through `QLocalSocket`. The generative
motor and audio graph stay separate. The original 35 catalog IDs are preserved;
the 25 new `Synth*` entries include DX7 overview, second bank, operator focus, and
both Hybrid halves. Their design IDs are recorded in `registry.py`.

```sh
# Separate development host process, managed by the GUI; no audio synthesis.
python3 apps/consola/gui-console/main.py --fake-synth --screen SynthParts
# Real host, or an already running fake on the same contract.
python3 apps/consola/gui-console/main.py --synth-socket /tmp/gf-synth.sock --screen SynthParts
# Offline deterministic visual review, including the 6-operator envelope.
python3 apps/consola/gui-console/main.py --preview --screen SynthDx7Focus
python3 apps/consola/gui-console/tools/screenshot.py --synth
```

Without `--preview` or `--fake-synth`, the GUI connects to
`$XDG_RUNTIME_DIR/gf-synth.sock` (`/tmp/gf-synth.sock` when unset). The client
reconnects with bounded exponential backoff and locks edits after 500 ms without
an answer. It resubscribes and refreshes all parts after reconnecting. The real
host replaces the fake without changing the control UI. Specialized graphs need
the semantic IDs described in [15-gui-synth.md](../../docs/consola/15-gui-synth.md);
unknown metadata still builds usable generic controls, with visualization marked
unavailable. No fake values are substituted into a live host's control model.

**F3** opens Parts; **F4** opens Audio settings. Tab/Shift+Tab cycle all 61 views.
PageUp/PageDown change parameter pages; encoder click 3 changes bank. Encoder turns
edit the corresponding metadata cell; SHIFT gives ×0.1 steps for continuous and
bipolar parameters, with integral enum/bool steps. Clicks fire the independent soft
key. Click an operator to focus it, click its focus strip to return to all operators,
and use Bank for rate/level and scaling controls. DX7 has all 32 algorithm routes,
including actual feedback endpoints and six selectable operators. Parts encoders
1–4 select/open a part, encoder 6 changes engine, 7 toggles mute, and 8 changes level.
Preset encoder 1 scrolls/loads, 2 filters engine, 3 filters tag, and 6 opens Save As.
The keyboard supports letters, digits, punctuation, backspace and cancel.

Host events and surface events commit in the same `afterAnimating` batch. Encoder
bursts send one final absolute value per parameter per frame. Meter updates do not
reset the controls or preset list. The fake's four parts, metadata, presets, meters
and CPU are simulated; presets persist only for that process's lifetime.
Unsupported settings, CUE audition/routing, SysEx import, favorites, layer save and
hardware-part control remain unavailable pending a host/service contract.

The sandbox may prohibit Unix socket binding (`Operation not permitted`). Socket
integration tests then skip explicitly; a separate Python-process pipe harness
(`fake_synth_host.py --stdio`, test-only transport injection) exercises the same
JSON-lines client parser, all requests/events, 500 ms timeout and reconnect. Pipes
are a protocol test seam; the production GUI continues to use Unix sockets.
Review [synth screenshots](screenshots/synth-index.html),
[comparison notes](screenshots/SYNTH-COMPARISON.md), and
[test output](screenshots/synth-test-results.txt). Pi GPU/audio performance still
requires the ADR-007 hardware gate. GeneralSans.ttf remains local and gitignored.

## SYNTH performance mode and auxiliary touch

Spec and hardware equivalence table: [21-modo-synth.md](../../docs/consola/21-modo-synth.md).
No motor, phrases or ACCEPT in this mode. Surface MIDI is decoded with
`tools/consola/superficie.py`; notes go straight to the host MIDI output before
visual feedback is queued. Control edits use the existing once-per-frame seam.
`--note-transport auto` prefers MIDI and reports a dedicated JSON-lines fallback
when the host MIDI graph port is unavailable through the MIDI backend. Use
`--note-transport midi` to require MIDI or `--note-transport socket` to force the fallback.
No automatic graph wiring, installation or firmware flashing is performed.

```sh
# Offline Mac development: 32 physical keyboard pad substitutes; no audio.
python3 apps/consola/gui-console/main.py --preview --synth-mode --fake-surface
# Pi: calibrated touch and 90-degree QML image rotation; host already running.
# GF_SURFACE_CDC must identify the Teensy via /dev/serial/by-id/ (M4 can take ttyACM0).
bash apps/consola/gui-console/pi/run_synth.sh --surface-cdc "$GF_SURFACE_CDC" \
  --room-node 'AUDIO_PLUS_NODE_ID'
# Explicit note socket fallback if no ALSA/JACK-visible host MIDI output exists.
bash apps/consola/gui-console/pi/run_synth.sh --note-transport socket
python3 apps/consola/gui-console/tools/synth_mode_review.py
```

Preinstalled optional dependencies: `mido`, `python-rtmidi`; `pyserial` for CDC.
The surface and host MIDI names must each match exactly one port. Configure the
PipeWire/ALSA bridge or JACK graph to expose the host destination; do not connect
the raw console note map directly to the synth. `run_synth.sh` defaults to
`--external-midi-port auto`: all external inputs, excluding the surface, Midi Through,
and host/GUI ports. Use `GF_EXTERNAL_MIDI_PORT='Arturia'` to select a substring or
`GF_EXTERNAL_MIDI_PORT=none` to disable discovery. Explicit repeatable
`--external-midi-port` flags override the launcher default. Inputs are discovered at startup.
The GUI uses SCHED_OTHER; audio requests RT through PipeWire and defaults to 256 frames.
See [RT permissions and per-thread verification](../../docs/consola/16-synth-host.md#corrección-pi-con-gui-y-prioridad-rt-2026-11-10)
before trying `--block 128` on the host.
MASTER uses `wpctl set-volume` with the configured `--room-node`; CUE/IN send existing `@GF HP/IN` CDC
commands. `@GF MASTER` is absent from firmware18 and is never sent.

F3 enters SYNTH; F2 returns to modes. Arrows select/turn encoders, Enter clicks
the soft key, SHIFT is fine, [/] changes octave. In `--fake-surface`, four rows
1–8 / QWERTYUI / ASDFGHJK / ZXCVBNM, map to all 32 pads (release sends off).
Numbers select catalog screens only when fake surface is off. ALT+left/right and
ALT+Enter traverse/activate the same targets as SHIFT+jog and jog click.
Touch knobs select focus, bool switches toggle, list rows select, and browsers
support drag/flick; screen pads and piano keys never play music. SYNTH SCALE
changes the real pad model through touch, encoders or soft keys. Existing screens
without backend services show explicit local selection/detail, not successful
system/audio operations. Notes remember their original part/pitch until release;
STOP, mode exit, MIDI input loss and close release held notes.

The LED output protocol is a proposal (doc21); a Teensy/M4 receiver and physical
validation remain open. [SYNTH screenshots](screenshots/modo-synth/index.html) use
fake host data. No desktop test certifies Pi/audio latency, touch calibration or
ADR-007 performance.

## DRUM mode (spec22)

```sh
python3 main.py --preview --drum-mode --screen DrumRack --fake-surface
python3 main.py --fake-drum --drum-mode --fake-surface
python3 main.py --drum-mode --drum-socket "$XDG_RUNTIME_DIR/gf-synth.sock" --surface-port 'GF Consola'
bash pi/run_drum.sh
python3 tools/drum_mode_review.py
```

Run from this directory. Preview is deterministic and silent; the separate fake
also has no audio. The real path uses the same `gf-synth-host` process as SYNTH.
`--note-transport auto` prefers graph MIDI channel10 notes0..15; the announced
fallback uses an independent socket and a background ACK reader. Never connect
raw GF Consola channel1 directly to the host while in DRUM: surface notes overlap
synth notes/CCs. One surface reader routes between SYNTH and DRUM; there is no
second capture of the hardware.

Top2M4rows play16pads, bottom2rows toggle16steps of the selected lane/page.
FSR1..4 resolve kick/snare/hat/perc by category. Faders control the four drum
groups. Encoders use the host's `list_pad_params` pages; all note hits precede
frame-batched presentation. Touch selects pads/steps/handles, double tap opens
pad edit, and does not play musical notes. In variations, plain encoder click
cues its candidate; SHIFT+click activates the bottom soft key. The conflict in
the design is recorded in spec22§9. ALT+arrows/Enter retain auxiliary focus.
Space toggles transport in DRUM; F5 enters DRUM. Desktop fake-surface pad keys
use the same32-key layout as SYNTH, with their new DRUM meanings.

The browser auditions only in cue. Pad audition uses the pad DSP; browser sound
preview uses its raw layer. Engine generation/import runs in the separately
spawned kit service. Enter a local folder and free-text licence note on IMPORT;
the path keyboard is auxiliary, and jobs continue when returning to the jam.
Completed jobs only update the catalog: LOAD/KEEP is a separate choice. Imported
folders larger than16sounds produce multiple racks. No third-party samples or
GeneralSans are included by this change.

The RGB return is the existing SYNTH21 proposal (channel16 CC20..22 then pad
note48..79). `surface/PadLedReturn.h` and `Surface.cpp` now consume it and forward
a bounded/coalesced32-pad snapshot over the existing M4 binary protocol, <=30Hz,
max31/127 brightness. `consola_uno` was built without flashing. Actual LED return,
M4 firmware compatibility, PipeWire routing, touch and latency need the Pi/Teensy
validation in the [report](screenshots/modo-drum/REPORT.md). The old E5 palette
proposal is still a documented question, not an implicit firmware remap.

[All14captures and source manifest](screenshots/modo-drum/index.html) use fake
snapshots, including explicit RAM/missing/empty examples; they are not audio or
hardware measurements. The real GUI integration test uses freshly rendered
own ENGINE audio and inherited Unix sockets to the compiled host.

Tap the DRUM BPM readout to open the auxiliary tempo control (encoder1 or touch
−/+); transport must be stopped. Browser results use bounded catalog pages and a
ListView, so jog selection scrolls into view and large imports do not create all
rows at once. Catalog refreshes discard replies from an older browsing request.
