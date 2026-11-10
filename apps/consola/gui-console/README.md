# GrooveForge Console

QML/PySide6 console GUI with a contract-14 synth host client and deterministic
review snapshots. Motor, Live and hardware connections remain future work.
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
Mixer, Hands. **Tab / Shift+Tab** cycle all 60 entries. F2 opens mode selection;
its Controller and Standalone cards enter the corresponding session screens.
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
The eight-second mode auto-entry caption is retained from the artwork; its timer
remains disabled for deterministic review. The old Brain ENGINE/HYBRID views remain available for comparison; their tabs
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

**F3** opens Parts; **F4** opens Audio settings. Tab/Shift+Tab cycle all 60 views.
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
