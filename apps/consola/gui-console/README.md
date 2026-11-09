# GrooveForge Console · phase 1

Desktop QML/PySide6 demo, fake snapshots, no hardware or network access at runtime.
Design and model contract: [09-gui-fase1.md](../../docs/consola/09-gui-fase1.md).

From the repository root, using system Python (PySide6 6.11.1 tested):

```sh
python3 apps/consola/gui-console/main.py
python3 apps/consola/gui-console/main.py --frameless --screen 8
python3 -m pytest apps/consola/gui-console/tests -q
python3 apps/consola/gui-console/tools/screenshot.py
```

Keys 1–9 select Boot, ModeSelect, Toasts, Settings, LiveSession, Layer, Transition,
Mixer, Hands. Left/right select an encoder; up/down turn it; Enter presses it.
O/E/L/N show offline/empty/loading/normal states; B switches fill/stripe; Escape
returns to Live. Soft keys accept touch/click and wheel turns.
Layer uses encoders 1–4 for cue/energy and 5–8 for Generate/Accept/Skip/Back;
this demo mapping differs from the export's pairs of encoders (see design conflict).
Controller stays in the mode selector because controller screens are outside phase 1.
The mode selector's eight-second auto-entry caption follows the artwork; its timer
is deferred so screen exploration and screenshots remain deterministic.

The screenshot tool selects Qt's offscreen/software renderer at scale factor 1,
waits for the single boot pulse, and saves nine PNGs in `screenshots/`.
`tools/render_assets.py` rebuilds baked PNG glows with QPainter gradients; no blur
runs in QML. Screenshots exercise desktop software rendering, not the Pi FPS gate.

Fonts are optional. See [fonts/README.md](fonts/README.md); the fetch script is
provided for a later network-enabled environment and has not been executed.
The app queries installed system font families before choosing fallback sans/mono.

`SnapshotModel.queue_update()` merges events and `flush()` emits one dataChanged
range per 16 ms per model. Roles and QObject properties are language-independent;
the future motor/surface adapter supplies snapshots and receives the public commands.
