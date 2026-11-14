# LEDs1 · implementation and verification

2026-11-10 · base `bcc4544` · destination branch `codex/leds1`.

Implementation is in `/private/tmp/grooveforge-leds1`. The sandbox grants read
access to the requested clone but write access only to grooveforge-brain and
temporary directories. The requested clone is unchanged and its worktree remains
clean. No `git add`, `git commit`, index mutation or hardware flash was performed.
`/private/tmp/grooveforge-leds1.patch` contains the full result, including previews.
Apply from the requested clone with `git apply /private/tmp/grooveforge-leds1.patch`.
The patch is checked using `git apply --check` against the original clone.

## Result and design

[Spec/theory23](../../../../docs/consola/23-leds-neotrellis.md) was written first.
It records the palette19 vs RGB21/22 contradiction, explicit LED23 opt-in,
compact per-pad state/animation flags, brightness, serials, recovery and budgets.
No other spec was edited. The Teensy loop renders at most once per34ms and
sends only changed96-byte frames through the nonblocking existing UART.
32pads, <2KiB fixed renderer, bounded integer computation, no heap/delay/IRQ masking.
100-byte framed UART at1Mbaud=2.94% wire duty at the maximum frame rate.
No pin/audio graph/ADC/priorities or Teensy Audio Library modifications.

DRUM renders audible-host-hit velocity flashes, selected pad and selected-lane
steps, page-aware playhead, velocity brightness, mute/solo/group/choke dimming,
record and queued pattern feedback. RT host publishes cumulative hit counters,
velocity and choke flags in its existing clock SPSC. It counts first nonzero ROOM
sample after output gates, and never fabricates hits from playhead/meters/pad UI.
SYNTH observes successful note output to retain original part/velocity, adds held
notes, release fades, scale/root layout, part selection and host-clock edge pulses.
The observer is presentation-only; it does not claim sample-accurate synth voice
telemetry or sustain-envelope state. Heartbeat2Hz is gated by fresh host telemetry,
local amber after1500ms; full snapshots1s/after reconnect and recovery, without
replaying historical drum hits. Boot/mode sweep480ms. Settings persist brightness
(default28/127≈22%, ceiling31/127≈24.4%) and animations ON/OFF through QSaveFile;
touch plus encoder5/6, preserving existing pad-velocity encoder4.

M4 exception, justified in23§6: bulk7 previously suppressed new frames for100ms
and double-attenuated small RGB values with gamma plus hardware brightness63.
Only `GF_CONSOLA_S_M4` bypasses that suppression and precompensates the existing
25% NeoPixel scaler for linear capped RGB7. Maximum physical PWM62/255=24.31%,
default maximum56/255=21.96%; malformed payloads remain capped. UART/commands
and other M4 environments stay compatible. To see this correction, deploy both
Teensy and consola M4 builds; no firmware was deployed here.

## Actual verification

| Check | Result | Evidence |
|---|---|---|
| `pio run -e consola_uno` in apps/firmware-teensy | PASS | [log](verification/consola_uno.log) |
| `pio test -e native` in apps/firmware-teensy | **188 passed** | [log](verification/native.log) |
| `python3 -m pytest apps/consola/gui-console/tests -q` | **469 passed,13 skipped**,148.57s | [log](verification/gui-pytest.log) |
| Release CMake host + CTest | **11/11 passed**,16.24s | [log](verification/host-ctest.log) |
| `pio run -e consola_s_m4` in apps/consola/processor | PASS | [log](verification/consola_s_m4.log) |
| Preview native renderer |396frames,3scenes; cap/cadence/amber/recovery assertions PASS| [frames](frames.json) |

Tests cover cap/hue, velocity decay, held-release fade, serial deduplication,
frame throttling/coalescing, reconnect, timer wrap, timeout/recovery, sweep<600ms,
animation-off states, playhead/STOP, M4 monotonic linear mapping; Pi channel/range,
state/diffs/snapshot, host freshness and hit baseline, epoch/choke, settings and
transport identity. The suite exercises LED settings soft keys by touch and MIDI.
Existing host RT allocation/free guard, audio, sampler, contracts and metadata pass.
Existing GUI skips concern local fonts and restricted Unix socket binding.

Build environment: local Teensyduino1.160.0 private USB/Wire overlays and cached
PlatformIO dependencies copied into `/private/tmp/leds-pio`; nothing changed in
~/.platformio. Registry download initially failed under restricted networking;
builds used already-installed Unity/Adafruit libraries instead. Host CMake initially
selected Python3.13 without soundfile; final configuration used installed Python3.14
with numpy/scipy/soundfile, then the entire11-test suite passed. No dependency
or product source workaround was added. An early parallel GUI run missed a frame
in an existing30ms keyboard test; isolated verification and final full suite pass.

## Preview

Open [interactive HTML](index.html), [SYNTH GIF](synth.gif), [DRUM GIF](drum.gif),
[animations OFF GIF](static.gif), or [contact sheet](contact-sheet.png).
Rebuild offscreen from repo root:

```sh
python3 apps/consola/gui-console/tools/preview_leds.py
```

The tool compiles the production `PadLedReturn.cpp` with a tiny stdin adapter and
feeds messages generated by the same Python LED protocol helpers. JavaScript only
plays recorded RGB frames; there is no second animation implementation. Each scene
has132frames at34ms (0..4454ms). Last heartbeat2040ms, local amber first3570ms,
recovery4250ms; three scenes also exercise SYNTH part changes and DRUM mute/choke,
record, queue, playhead and velocity. Contact sheet visually inspected. Display
gain×3 is explicitly labeled and can be disabled in HTML; it never changes bytes.
These are fixtures without audio/hardware, not evidence of light intensity or latency.

Settings changed only in its dedicated screen/soft-key actions/preferences;
no edits to TouchTarget.qml, BrowserList.qml, DrumBrowser.qml, drum_state.py,
synth_state.py, pad velocity or musical routing in either surface module.

## Physical gate still required

Flash the two built environments, verify NeoTrellis wiring/return, scope RGB/PWM,
measure renderer duration (target<100µs), stable `loop_max_us<2000`, UART loss and
MIDI responsiveness during audio USB, plus a prolonged xrun test on Pi. No bench
CPU/latency/current/light-quality claim can be made from native tests and builds.

## Changed files (complete manifest)

- `apps/docs/consola/23-leds-neotrellis.md`
- `apps/consola/surface/include/consola_s/PadLedReturn.h`
- `apps/consola/surface/src/PadLedReturn.cpp`
- `apps/consola/surface/src/Surface.cpp`
- `apps/consola/gui-console/pad_leds.py`
- `apps/consola/gui-console/local_settings.py`
- `apps/consola/gui-console/screens/Settings.qml`
- `apps/consola/gui-console/fake_data.py`
- `apps/consola/gui-console/synth_surface.py`
- `apps/consola/gui-console/drum_surface.py`
- `apps/consola/gui-console/main.py`
- `apps/consola/gui-console/tests/test_pad_leds.py`
- `apps/consola/gui-console/tests/test_drum.py`
- `apps/consola/gui-console/tests/test_synth_surface.py`
- `apps/consola/gui-console/tests/test_surface_controls.py`
- `apps/consola/gui-console/tools/led_preview.cpp`
- `apps/consola/gui-console/tools/preview_leds.py`
- `apps/firmware-teensy/test/test_pad_leds/test_main.cpp`
- `apps/synth-host/src/drum.h`
- `apps/synth-host/src/drum.cpp`
- `apps/synth-host/src/drum_service.cpp`
- `apps/synth-host/CMakeLists.txt`
- `apps/synth-host/tests/led_telemetry_test.cpp`
- `apps/consola/processor/include/shared/ConsolaLed.h`
- `apps/consola/processor/lib/neotrellis_m4/NeoTrellisController/NeoTrellisController.cpp`
- `apps/consola/gui-console/screenshots/leds/contact-sheet.png`
- `apps/consola/gui-console/screenshots/leds/drum.gif`
- `apps/consola/gui-console/screenshots/leds/frames.json`
- `apps/consola/gui-console/screenshots/leds/index.html`
- `apps/consola/gui-console/screenshots/leds/static.gif`
- `apps/consola/gui-console/screenshots/leds/synth.gif`
- `apps/consola/gui-console/screenshots/leds/verification/consola_s_m4.log`
- `apps/consola/gui-console/screenshots/leds/verification/consola_uno.log`
- `apps/consola/gui-console/screenshots/leds/verification/gui-pytest.log`
- `apps/consola/gui-console/screenshots/leds/verification/host-ctest.log`
- `apps/consola/gui-console/screenshots/leds/verification/native.log`
- `apps/consola/gui-console/screenshots/leds/REPORT.md`
