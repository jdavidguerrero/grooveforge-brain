# DRUM surface corrections — 2026-11-10

Scope: this checkout only, `codex/drum-fix1`, starting at `91d4688`. No push,
Pi configuration change, firmware change or host/CTest change.

## Root cause and behavior

`ConsoleState.refresh_keys()` reset the complete DRUM soft-key model on every
clock/meter update. Resetting destroyed the pressed QML delegate before release,
canceling touch clicks. Reproduced at QML90: press STEPS, deliver clock, release →
still RACK; the normal encoder click already navigated to STEPS. Updating roles
in place preserves delegates, for DRUM and the common SYNTH/legacy path.

DRUM additionally intercepted encoder clicks on PERFORM and VARIATIONS. Plain
notes80–87 now fire their labeled soft keys. PERFORM retains hold+pad mute/solo;
VARIATIONS uses SHIFT+click for candidate audition. BACK navigation is exercised.
PAD Layer now selects a layer; MIXER Level selects level. HYBRID Morph opens its
LAYER focus, and Layer opens presets. Disabled keys ignore hardware clicks too.

Settings → Surface / Hands has master/left/right toggles, all OFF by default.
Preferences are saved atomically in `~/.config/gf-console/settings.json`, honoring
`XDG_CONFIG_HOME` (test override `GF_CONSOLE_SETTINGS`). Disabled CC33/34 and
presence108/109 are dropped at ingress and before queued application in every
mode. Disabled hands clear presence and halos; re-enabling needs fresh presence.
Changing velocity or the other hand preserves an enabled hand's presence.

The same Settings screen selects global DRUM M4 velocity64/100/127, default100.
It affects immediate MIDI and socket pad hits. FSR velocity and step-grid
velocity remain independent. Rack format and host DSP are unchanged.

## Files changed

Under `apps/consola/gui-console/`:

- `fake_data.py`: stable shared key updates, disabled-key guard, Settings actions.
- `local_settings.py` (new), `main.py`: persisted preferences and QML context.
- `drum_state.py`, `synth_state.py`: missing/incorrect labeled actions and Settings return navigation.
- `drum_surface.py`, `synth_surface.py`, `surface_batch.py`: encoder dispatch, hand gates, M4 velocity.
- `screens/Settings.qml`, `ModeSelect.qml`, `Hands.qml`, `LiveSession.qml`: controls, Settings entry, halo gating.
- `tests/test_surface_controls.py` (new), `conftest.py`, `test_drum.py`, `test_synth_surface.py`: rotated rendered touch, decoded MIDI, effects, defaults, persistence, queued filtering, velocity and regressions.
- `tools/surface_controls_review.py` (new): reproducible captures, isolated defaults.
- `screenshots/04-Settings.png`, `53-SynthHybrid.png`, and this directory: updated captures and provenance.

Docs: `apps/docs/consola/21-modo-synth.md`, `22-modo-drum.md`.

## Validation

GUI: **461 passed, 13 existing skips** (116.63 s). Tools/consola: **454 passed**
(1.98 s). Results are recorded in `gui-pytest.txt` and `tools-pytest.txt` beside this
report. The ordinary GUI invocation rebuilt and exercised the unchanged C++ host;
the final run reuses that build via `GF_SYNTH_HOST_BUILD`.

`uv run pytest` could not initialize its restricted cache (`Operation not
permitted`); tools/consola was tested with the authorized `python3 -m pytest`
fallback. `git diff --check` and `bash -n` on both Pi launchers passed.

There are 18 deterministic silent-preview screenshots, including every DRUM
registry screen, Settings, HYBRID, Mode Select, and Rack/Settings at90°.
See [gallery](index.html) and [provenance](manifest.json). Captures used
Qt offscreen/software and are not physical Pi/audio/latency certification.

## Juan's Pi check

Restart with the existing `pi/run_drum.sh` / `pi/run_synth.sh`. Keep measured
identity touch (`rotate=0`, no `invertx`) and QML90. Test BACK/STEPS/BROWSE while
PLAY runs, both touch and encoder clicks. MENU opens Settings; enable master and
the intended hand only when wanted. Confirm the preferences survive GUI restart,
and test M4 at64/100/127 against a layered rack while FSR dynamics remain intact.
In VARIATIONS, use SHIFT+click to audition a card; plain click activates its key.
No firmware flash is required by these GUI changes.


## Git result

Staging failed with `.git/index.lock: Operation not permitted` in this sandbox.
All changes remain uncommitted on `codex/drum-fix1`; no push. Intended commit:

```text
Fix DRUM soft keys and persist surface controls

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>
```
