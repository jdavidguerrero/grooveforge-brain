# DX7 six-operator wiring — 2026-11-10

Branch `codex/host-dx7`, base `2695044`. No changes to the USB checkout, contract 14,
or specs 00–06. No push. **All implementation and artifacts remain uncommitted**:
`git add` failed creating `.git/index.lock` with `Operation not permitted`.

## Delivered

- Host registry entry, six voices, note on/off/velocity, ±2-semitone bend,
  sustain, CC14/15/16/19/71/74, control-boundary edits, shared FX/GROOVE bus.
  DRIFT/GRIT/POCKET affect voice interpretation without rewriting saved VCED.
- Shared 151-descriptor table: OPS six levels; OP 1–6 banks of 7 tuning/sensitivity,
  8 EG, and 6 scaling controls; ALGO/FB, PITCH EG, LFO. The host adds 56 shared bus
  controls (207 total). Core numeric fields clamp/round exactly like firmware.
- VCED and complete VMEM bank discovery in preset directories, checksum validation,
  all 32 selectable voices. Standalone GFP6 plus JSON saves preserve raw voice and
  bus state; native GFP imports retain voice and initialize bus defaults.
- GUI uses real received IDs. All six columns, algorithms 1–32, operator focus,
  four-stage EG and keyboard scaling are live. FM 4-OP uses its real osc/mod IDs.
- Fake reads the compiled registry export committed as synth-metadata.json.
  CTest and pytest rebuild/compare metadata; routing flags and scaling curves are
  checked against compiled C++. No independent Python parameter table.
- JUCE 8.0.3 VST3 built locally; editor smoke visited every bank and exposed all
  151 core controls through the existing dx7_6op.<id> automation namespace.
- Docs 15/16 updated; no contract extension required.

## Verification and output

| Command | Result | Full output |
|---|---|---|
| cmake --build /tmp/gf-host-six-build -j4; ctest --test-dir /tmp/gf-host-six-build --output-on-failure -V | 6/6 pass; 918 parameter round-trips; 8-engine RT heap violations=0 | [host](dx7-wiring-ctest.txt) |
| python3 -m pytest apps/consola/gui-console/tests -q -rs | 204 passed, 13 skipped in 42.45 s | [GUI](../../../consola/gui-console/screenshots/dx7-wiring-pytest.txt) |
| PLATFORMIO_CORE_DIR=/tmp/gf-host-six-pio ~/.platformio/penv/bin/pio test -e native (apps/firmware-teensy) | 166/166 pass, 13 suites | [firmware](dx7-wiring-firmware.txt) |
| cmake --build /tmp/gf-host-six-vst -j4; ctest --test-dir /tmp/gf-host-six-vst --output-on-failure | VST3 builds; 14/14 pass | [VST](dx7-wiring-vst-ctest.txt) |
| git diff --check | Pass | — |

The GUI tests compile a fresh real host, enumerate every FM page/bank through
QLocalSocket, edit raw algorithm/EG/fixed frequency/enable, reset both engines,
and wait for matching IDs plus completion of pending requests. Contract tests
build an original 32-voice VMEM bank and a VCED voice without Yamaha ROM data;
exercise every imported voice, corrupt checksum rejection, GFP6 and bus reload.
The isolated DX7 audio test checks timestamps, sustain, measured ±2-semitone bend, all six CCs, audible
macros/FX, finite non-silent output and agreement with the firmware setter.

PlatformIO used a separate writable core cache, a copied local native platform,
and cached Unity 2.6.1; global cache writes are blocked. No downloads or flashing.

## Screenshots

Captured at 1280×720 from the real compiled C++ host, offline audio backend,
QLocalSocket attached to an inherited AF_UNIX socketpair; zero QML warnings.

- [FM 6-OP overview](../../../consola/gui-console/screenshots/dx7-6op-overview.png)
- [OP 6 envelope/scaling focus](../../../consola/gui-console/screenshots/dx7-6op-operator-focus.png)
- [Transport and SHA-256 manifest](../../../consola/gui-console/screenshots/dx7-wiring-manifest.json)

Reproduce: `python3 apps/consola/gui-console/tools/real_synth_review.py
/tmp/gf-host-six-build/gf-synth-host --screenshots apps/consola/gui-console/screenshots`.

## Remaining limits

- Sandbox forbids Unix pathname listeners: 12 GUI listener/broadcast tests skip.
  Existing fake protocol tests use pipes; new integration tests use real Unix
  socketpairs and QLocalSocket. Listener binding/duplicate-bind still need Pi or
  an unsandboxed run. One additional skip is optional local General Sans.
- Pi/PipeWire/JACK, physical MIDI, latency, thermal/xrun/60-fps gates and Yamaha
  calibration remain hardware work. Six voices, approximate AMD/AMS and no DAC
  emulation remain properties of the existing core.
- SysEx/GFP directories are scanned at startup; restart after dropping new files.
- Tape wow/flutter pitch is combined with the MIDI bend in the six-operator
  adapter. The EG time axis is a control sketch; keyboard scaling uses the core
  integer formula. HYBRID descriptions remain design samples.

## Files

- [apps/brain-plugin/tests/plugin_smoke.cpp](../../../../apps/brain-plugin/tests/plugin_smoke.cpp)
- [apps/consola/gui-console/components/FmEnvelope.qml](../../../../apps/consola/gui-console/components/FmEnvelope.qml)
- [apps/consola/gui-console/components/FmPanel.qml](../../../../apps/consola/gui-console/components/FmPanel.qml)
- [apps/consola/gui-console/components/KeyScaling.qml](../../../../apps/consola/gui-console/components/KeyScaling.qml)
- [apps/consola/gui-console/components/SynthGraph.qml](../../../../apps/consola/gui-console/components/SynthGraph.qml)
- [apps/consola/gui-console/components/SynthPlot.qml](../../../../apps/consola/gui-console/components/SynthPlot.qml)
- [apps/consola/gui-console/fake_synth_host.py](../../../../apps/consola/gui-console/fake_synth_host.py)
- [apps/consola/gui-console/fm_algorithms.py](../../../../apps/consola/gui-console/fm_algorithms.py)
- [apps/consola/gui-console/fm_visuals.py](../../../../apps/consola/gui-console/fm_visuals.py)
- [apps/consola/gui-console/screens/SynthHybrid.qml](../../../../apps/consola/gui-console/screens/SynthHybrid.qml)
- [apps/consola/gui-console/screenshots/dx7-6op-operator-focus.png](../../../../apps/consola/gui-console/screenshots/dx7-6op-operator-focus.png)
- [apps/consola/gui-console/screenshots/dx7-6op-overview.png](../../../../apps/consola/gui-console/screenshots/dx7-6op-overview.png)
- [apps/consola/gui-console/screenshots/dx7-wiring-manifest.json](../../../../apps/consola/gui-console/screenshots/dx7-wiring-manifest.json)
- [apps/consola/gui-console/screenshots/dx7-wiring-pytest.txt](../../../../apps/consola/gui-console/screenshots/dx7-wiring-pytest.txt)
- [apps/consola/gui-console/synth-metadata.json](../../../../apps/consola/gui-console/synth-metadata.json)
- [apps/consola/gui-console/synth_catalog.py](../../../../apps/consola/gui-console/synth_catalog.py)
- [apps/consola/gui-console/synth_state.py](../../../../apps/consola/gui-console/synth_state.py)
- [apps/consola/gui-console/tests/test_dx7_wiring.py](../../../../apps/consola/gui-console/tests/test_dx7_wiring.py)
- [apps/consola/gui-console/tests/test_synth.py](../../../../apps/consola/gui-console/tests/test_synth.py)
- [apps/consola/gui-console/tools/real_synth_review.py](../../../../apps/consola/gui-console/tools/real_synth_review.py)
- [apps/docs/consola/15-gui-synth.md](../../../../apps/docs/consola/15-gui-synth.md)
- [apps/docs/consola/16-synth-host.md](../../../../apps/docs/consola/16-synth-host.md)
- [apps/firmware-teensy/src/engines/dx7_6op_params.cpp](../../../../apps/firmware-teensy/src/engines/dx7_6op_params.cpp)
- [apps/synth-host/CMakeLists.txt](../../../../apps/synth-host/CMakeLists.txt)
- [apps/synth-host/src/engine.cpp](../../../../apps/synth-host/src/engine.cpp)
- [apps/synth-host/src/metadata.cpp](../../../../apps/synth-host/src/metadata.cpp)
- [apps/synth-host/src/metadata.h](../../../../apps/synth-host/src/metadata.h)
- [apps/synth-host/src/metadata_export.cpp](../../../../apps/synth-host/src/metadata_export.cpp)
- [apps/synth-host/src/params.cpp](../../../../apps/synth-host/src/params.cpp)
- [apps/synth-host/src/params.h](../../../../apps/synth-host/src/params.h)
- [apps/synth-host/src/server.cpp](../../../../apps/synth-host/src/server.cpp)
- [apps/synth-host/tests/contract_test.py](../../../../apps/synth-host/tests/contract_test.py)
- [apps/synth-host/tests/dx7_test.cpp](../../../../apps/synth-host/tests/dx7_test.cpp)
- [apps/synth-host/tests/metadata_test.py](../../../../apps/synth-host/tests/metadata_test.py)
- [apps/synth-host/tests/results/dx7-wiring-ctest.txt](../../../../apps/synth-host/tests/results/dx7-wiring-ctest.txt)
- [apps/synth-host/tests/results/dx7-wiring-firmware.txt](../../../../apps/synth-host/tests/results/dx7-wiring-firmware.txt)
- [apps/synth-host/tests/results/dx7-wiring-vst-ctest.txt](../../../../apps/synth-host/tests/results/dx7-wiring-vst-ctest.txt)
- [apps/synth-host/tests/rt_test.cpp](../../../../apps/synth-host/tests/rt_test.cpp)
