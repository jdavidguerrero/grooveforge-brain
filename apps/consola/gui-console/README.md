# GrooveForge Console · phase 2

QML/PySide6 desktop review catalog with deterministic fake snapshots. Runtime does
not connect to the motor, audio, Live, network or hardware. Phase 3 plans MVP wiring.
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
Mixer, Hands. **Tab / Shift+Tab** cycle all 35 entries. F2 opens mode selection;
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
remains disabled for deterministic review. Only Brain ENGINE/HYBRID and Settings
Surface have delivered content designs; other tabs retain their existing summary
or remain visual tabs (see the coverage doc).

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

The screenshot tool fails on QML warnings, renders 35 PNGs at 1280×720 with Qt's
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
