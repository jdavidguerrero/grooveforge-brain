# USB MIDI / UMP input — 2026-11-11

Base: `dde6043`, branch `claude/monorepo-groovepush`. Changes limited to synth-host,
its Pi helpers and consola docs 14/16/pendientes. No SSH, Pi changes or commit.

## Delivered

- Spec first in doc 16, pointer in contract 14; backlog marked resolved in code,
  physical Pi validation pending.
- PipeWire `pw_filter` input advertises UMP first plus a legacy MIDI1 EnumFormat.
  PipeWire 1.4.9 `filter.c` retains the locked format created from `format.dsp`
  and appends caller-provided formats. These are `application/control` with
  `SPA_FORMAT_CONTROL_types` flags; the DSP name alone is not the negotiated
  payload type. Runtime dispatch uses each sequence item's control type.
- Backend-independent C++17 decoder accepts native-endian UMP type 2 and type 4
  channel voice, truncating resolution to MIDI1 with MIDI2 Note On zero clamped
  to 1. MIDI1 Note On zero remains a release. Group and note attributes do not
  add engine destinations; optional program banks are discarded. Program and
  channel pressure decode correctly but have no engine action. No new DSP.
- No heap/locks in conversion; packet boundaries and a shared 2048 scan budget
  bound RT work. Ignored packet payloads cannot become notes. Existing synth
  channels 1–4 and drum channel 10 notes 0..15 retain sample offsets.
- `link_midi.py` uses pw-dump/pw-link to discover external physical MIDI outputs,
  exclude console/virtual feedback sources and skip existing links. Auto links
  all physical external inputs, not only a named keyboard. Rerun after hotplug.
  `run_on_pi.sh` calls it; doc 16 shows standalone/manual operation and disabling
  the GUI's existing `--external-midi-port` forwarding to prevent double notes.

## Verification

macOS arm64, AppleClang 17, C++17. Configure/build succeeded:

```bash
cmake -S apps/synth-host -B build -DPython3_EXECUTABLE=/opt/homebrew/bin/python3.14
cmake --build build
ctest --test-dir build --output-on-failure
```

**13/13 passed, 0 failed, 48.56 s.** Includes all 11 existing tests and the new
`host_midi` and `host_midi_links` tests. MIDI vectors cover raw MIDI1, UMP types
2/4, note on/off and zero velocity, CC, pitch bend endpoints/center/LSB, program,
channel pressure, ignored types, reserved fields, truncated/unaligned words,
concatenated packets, group/channel preservation and routing. The existing drum
integration test now decodes a MIDI2 note and checks its exact sample-37 pad
onset. The RT allocation hook also exercises conversion (no heap violations).
Helper fixtures cover auto discovery, selection, exclusions, existing links and
missing/ambiguous host errors. `bash -n tools/synth-host/run_on_pi.sh` and
`git diff --check` passed.

The first CTest run used CMake's default Python 3.13 and failed only the existing
`host_drum_contract` because `soundfile` was absent. Selecting the already
installed Python 3.14 (numpy/scipy/soundfile available) fixed the environment;
no production/test behavior was relaxed and no packages were installed.
Local ignored logs: `build/validation/{configure,build,ctest}.txt`.

## Pi gate (pending)

macOS has no PipeWire/JACK pkg-config packages; its build excludes those backend
branches. The PipeWire declaration and dispatch changes have been checked against
upstream 1.4.9 source, but are not compiled or exercised against a live graph here.
Run the commands/checklist in doc 16 on the Pi: build with PipeWire ≥1.4, inspect
EnumFormat, link a MiniLab and a legacy source, check notes/releases, sustain,
CC/bend, channels 1–4, channel 10 pads, reconnect/relink, xruns and audio latency.
Stop the old ALSA→socket helper and disable GUI external forwarding for the same
keyboard when using direct links. No automatic hotplug daemon is installed.

References and exact conversion policy: doc 16, “Entrada USB MIDI / UMP”.
