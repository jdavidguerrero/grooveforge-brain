# Touch responsiveness and NeoTrellis M4 dynamics

2026-11-11. Work confined to `/private/tmp/grooveforge-touch1`, branch
`claude/monorepo-groovepush`, base `3d30b40`. The existing partial edit in
`fake_drum_host.py` is retained, including 30 Hz drum meters and the pipe harness.
No commits or normal staging. Final binary patch: `/private/tmp/grooveforge-touch1.patch`.

## Result

Every TouchTarget has immediate pressed feedback. Menu/navigation keys, soft keys,
tabs/choices and text keys outside scroll containers act on press. List items and
all controls inside scroll containers act on release. Qt's shared drag threshold
is 25 px, about 3 mm at this panel's ~210 dpi; Flickable can still steal a real drag.
Tests exercise genuine touch at QML90, jitter under the threshold and scrolling
beyond it. Hardware activation still calls the same `activate()` callback through
TouchController. Touch calibration remains identity, with QML handling rotation.

DRUM clock and meters have independent notifications and frame comparisons.
Neither rebuilds pads, parameters, lanes, sounds, rack/source lists or step models.
Playhead binds directly to clock and lane length, including polymeter and the
second step page. Pattern changes still replace the displayed step content;
play/record changes update roles in the existing soft-key model. Repeated visible
values emit no clock/meter notification. The rack summary is cached to avoid
copying it when a binding combines rack text with clock text.

`LOADING RACK`, `LOADING SAMPLE`, `LOADING PREVIEW`, `LOADING PRESET` and
`LOADING ENGINE` appear when the request is sent and clear on the matching host
reply/error or disconnection. An unrelated reply or telemetry cannot clear the
label. A new rack choice is sent even if another slow request is pending, and
an older reply cannot clear the newer rack's busy label. Identical pending DRUM
requests are deduplicated. STOP/navigation remain available. The host's measured
380–530 ms rack loading work is unchanged. A separate fake process delayed by
530 ms verifies the complete client path while clock and meters keep arriving.

## Measured main-thread work

Same replay harness before and after: `tools/profile_touch.py`, real rendered QML
screens at 90°, 128 sample rows and 8 rack rows, running sequencer. It injects one
clock and one `drum_meters` message per 33 ms tick through the presentation handler
and commits via SurfaceBatcher. Actual aggregate rate was 59.97–60.65 events/s
(~30 of each topic). One-second warmup, six measured seconds per screen; separate
processes for before/after. The final after run had no pytest process running.

Environment: macOS 27.0.1 arm64, Python 3.14.3, PySide6/Qt 6.11.1,
**offscreen/software**, no DSP/audio or connected hardware. cProfile was enabled
on the GUI thread in both runs. Event time is the handler+frame commit time of
a clock/meter pair divided by two; it includes synchronous QML notification work.
CPU is CPU time / elapsed time ×100, relative to **one core**. Main-thread CPU uses
`thread_time`; process CPU uses `process_time` and includes Qt rendering/event work.
Event time excludes JSON parsing, physical input delivery and display scanout.
The main-thread CPU number includes the complete event-loop interval.

| Screen | Mean ms/event before → after | p95 ms/event before → after | Main-thread CPU before → after | Process CPU before → after |
|---|---|---|---|---|
| DrumBrowser | 2.409 → 0.281 | 4.133 → 0.971 | 18.93% → 2.97% | 21.11% → 2.97% |
| DrumSteps | 5.240 → 0.988 | 11.789 → 3.513 | 46.52% → 8.20% | 48.69% → 8.59% |
| DrumMixer | 5.032 → 0.631 | 5.875 → 1.962 | 50.55% → 5.70% | 58.14% → 5.70% |

Raw metadata, event counts and top 25 cumulative cProfile entries:
[before.json](before.json), [after.json](after.json). Before was collected against
the starting presentation code, with the profiling harness added before fixes.
The baseline profiles show ~904k deepcopy calls per screen, repeated model builds,
and 10,738 TouchController unregister calls in the Mixer. These are consequences
of rebuilding presentation and QML list delegates at telemetry rate. After, the
telemetry-only regression test rejects structural builder calls and asserts zero
pad/cell/lane/sound model updates and zero broad presentation notifications.

Reproduce from `apps/consola/gui-console`:

```sh
python3 tools/profile_touch.py --output screenshots/touch/after.json
python3 tools/capture_touch.py
python3 -m pytest tests -q
```

On the Pi, run the same profiler with `--platform eglfs --duration 120`, using the
existing runner's EGLFS/KMS/evdev environment and identity calibration. Desktop
numbers do **not** certify Pi CPU, physical touch accuracy/latency, Qt6.8 behavior,
60 fps, audio xruns or thermal endurance. Recheck TRIBE browsing/scrolling and
menu/soft-key response with audio and the surface active on the Pi.

## Live velocity and accent

**SHIFT + fader 4 (CC27)** sets subsequent M4 hits to 1–127 in both DRUM and SYNTH.
TopBar continuously shows `M4 VEL N`. This modifier had no dedicated assignment;
it preserves all eight parameter encoders and their SHIFT fine behavior. Without
SHIFT, fader 4 keeps its normal METALS group / part 4 level function. Tradeoff:
the unshifted level remains an absolute fader mapping after releasing SHIFT.

One global local setting, default100, is shared by both modes. The existing
`drum_pad_velocity` key is retained for migration. Settings' 64/100/127 buttons
are quick choices for the same value and are labeled for both modes. The callback
updates the note-routing snapshot immediately; persistence/display use the GUI
frame and LocalSettings' atomic save. Sound does not wait for disk or a frame.

**SHIFT + M4 pad = 127** without changing the stored velocity. DRUM recording
receives that127 on MIDI or socket and records an accented step when REC+PLAY
are active and the quantized step is empty. The existing host overdub rule does
not overwrite an occupied step. No new protocol field or host DSP change is needed.
In the DRUM step rows, SHIFT+step writes127 while preserving probability/micro;
unshifted steps keep off/100 toggling. Holding MUTE/SOLO in Perform still wins over
musical hits. Variations' SHIFT+encoder-click CUE remains available: it is a click,
not a pad binding. FSR and external keyboard velocity remain measured/unscaled;
SHIFT does not accent them. Held SYNTH note releases retain their original route.

## Verification and artifacts

Full GUI pytest: **495 passed, 13 skipped, 0 failures in 181.60 s**; result
is recorded verbatim in [pytest.txt](pytest.txt). The final
suite includes the real C++ host integration: a SHIFT+M4 hit is recorded at127 and
saved in the rack, alongside the original FSR velocity102 assertion. Existing LED,
calibration, hardware navigation and soft-key regression tests run in the suite.
Pre-existing skips cover restricted Unix listeners and the unavailable General
Sans font. No failures are accepted. `git diff --check` is clean.

Added behavioral coverage includes press-only activation without double dispatch,
pressed visual visibility, jittered release with telemetry while the finger is
down, real scroll cancellation, immediate loading and success/error/disconnect,
matching versus unrelated replies, actual 530 ms process delay with telemetry,
changed-only notifications, rendered polymetric playhead and mixer meters without
recreating controls, live velocity persistence and immediate pre-frame note output,
Settings coherence, FSR invariance, step accent, Perform priority and real overdub.
Old SYNTH fake-pad expectations change102→100 to match the shared default; musical
release/reference counting and FSR expectations are unchanged.

Captures are synthetic preview data, QML90 at 720×1280, offscreen/software;
[manifest and provenance](captures.json):

- [Rack finger down](rack-pressed-90.png)
- [Rack request pending](rack-loading-90.png)
- [DRUM velocity64](drum-velocity-90.png)
- [SYNTH velocity64](synth-velocity-90.png)

No changes to `pad_leds.py`, LED output methods in the surfaces, LED23, evdev
calibration, or TouchController. The full diff includes all source/docs/test and
PNG artifacts, including the pre-existing fake-host edit. Specs00–06 remain intact.

[Theory and design rationale](../../../../docs/theory/consola-touch-velocity.md),
[updated touch deployment contract](../../../../docs/consola/13-gui-pi.md),
[SYNTH](../../../../docs/consola/21-modo-synth.md),
[DRUM](../../../../docs/consola/22-modo-drum.md),
[touch pending entry closed with measured desktop numbers](../../../../docs/consola/pendientes.md).
