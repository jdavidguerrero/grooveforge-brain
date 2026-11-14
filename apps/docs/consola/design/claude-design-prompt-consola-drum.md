# Prompt complementario para Claude Design · Consola: modo DRUM (racks y jam)

> Para pegar en el **mismo proyecto de Claude Design** de la consola (export en `design/claude-design/` y
> `design/claude-design-synth/`). Adjuntar: capturas actuales de KIT, PADS y SEQUENCER STEPS del diseño, y si quieres
> capturas de Tribe o de un drum rack de Ableton como referencia de flujo (no de estética). Fecha: 2026-11-10.

---

You already designed the **GrooveForge Console** (7" touchscreen 1280×720; 8 endless encoders with an 8-key soft-key
bar under them; a 4×8 RGB pad grid; 4 faders; 4 velocity FSR pads; 2 IR "hands" sensors; a jog wheel) including the
SYNTH screens. Keep **everything** from that design system: colours, role colours, fonts (Big Shoulders Display, IBM
Plex Mono, Archivo, JetBrains Mono; General Sans only for GroovePilot menus/boot), chrome geometry (TopBar 56 ·
Content 568 · SoftKeys 96; columns x = 8 + i·160, w 152), states (empty / loading / offline) and the "Layout notes for
QML". **All UI text in English.** BRAIN = synth, ENGINE = generative motor.

This brief adds a new **DRUM mode**: use the console as a **drum machine to jam**, built around **racks (kits)** of
samples. Racks come from two sources: kits rendered by our own organic drum engine (modal synthesis of wood, skin and
808 bodies, tuned to the track's root — kits are pre-rendered, not synthesized live) and **imported sample folders**
(e.g. Tribe percussion — licence still to be checked; design a neutral "imported" source badge, no third-party
branding).

Rules (unchanged): music is played with pads, FSRs, encoders and faders; **touch is auxiliary** but everything that
looks tappable must work by touch (lists scroll by drag, tabs, buttons, cards). Glanceable from 60 cm, nothing bounces,
no live blur.

## Screens to design (new numbers 46–55)

### 46 DRUM · RACK (home of the mode)
The current rack: **16 pads** as a 4×4 or 2×8 visual mirror of the pad grid, each with name, source badge (ENGINE /
IMPORTED / RECORDED), material icon (wood / skin / 808 / metal / other), a tiny waveform, level and mute/solo state, and
the choke group as a coloured corner. Header: rack name, root/key it is tuned to, tempo, swing, MODIFIED flag. Encoders
1–8 edit the **selected pad** (TUNE, DECAY, TONE, START, PAN, SEND, VELOCITY CURVE, CHOKE). Soft keys: PADS / STEPS /
BROWSE / SAVE / MIXER / BACK.

### 47 DRUM · PAD EDIT (focus on one pad)
Big waveform with start/end and fade handles (moved by encoders; touch only selects which handle), pitch in semitones
and cents with the note name, amp envelope (attack/decay or hold/decay), filter (LP/HP + cutoff), velocity → level and
velocity → tone curves, choke group, output (room bus / group). A "layers" strip if the pad has up to 4 velocity layers
or round-robin variations (common in imported kits).

### 48 DRUM · BROWSER (build a rack)
Two panes: **sources** (ENGINE kits by genre/material, IMPORTED folders, FAVORITES, RECENT) and **sounds** list with
name, length, detected pitch/key, material/category tag, a waveform thumb and an audition button (audition goes to the
**cue** headphones, never to the room). Jog scrolls, encoder 1 filters by category (KICK, SNARE/CLAP, HAT, PERC, TOM,
FX), encoder 2 by material, press = load to the selected pad. Show the target pad clearly ("→ PAD 05 · CLAP").
Include the empty state (no imported folders yet) with the instruction to copy samples to the console's `/data`.

### 49 DRUM · IMPORT
Import a folder (from the Mac over Ethernet or a USB stick): progress, file count, sample-rate conversion to 48 kHz,
duplicates, unsupported files, total RAM the rack will use vs the budget, and the **licence note field** (free text,
e.g. "Tribe — licence pending") stored with the source. No upload anywhere; local only.

### 50 DRUM · STEPS (the jam view)
16-step lanes for the 16 pads (show 8 lanes at a time + scroll), the playhead, steps with velocity (bar height), ties
off; per-lane mute/solo/length (polymeter: lanes can have their own length 1–32), swing, step probability and
micro-timing as secondary layers (toggle which layer the encoders edit). Pattern slots A–H with the playing one and the
queued one (queued pattern switches at the next bar). Record arm: FSR/pad hits are recorded into the steps quantized
(show "REC" and the quantize amount). This is where Juan jams.

### 51 DRUM · PERFORM (pads view)
For playing live without editing: the 4×8 grid mirror where the **top two rows play pads 1–16** and the **bottom two
rows show the 16 steps of the selected pad** (as on the hardware), plus big FSR meters for the 4 force pads
(kick / snare / hat / perc by default) with their last velocity, the mute/solo state per pad and the pattern queue.

### 52 DRUM · MIXER
4 group faders (map to the 4 hardware faders: KICKS, SKINS, WOODS, METALS — editable grouping) and 16 pad strips
(8 visible, bank 1/2) with level, pan, send, mute/solo, meter. Same visual language as the existing MIXER screen.

### 53 DRUM · SAVE RACK / SAVE PATTERN
Name with on-screen keyboard, tags, tuned key, which source sounds it contains (and licence notes of imported ones),
overwrite warning.

### 54 DRUM · GENERATE VARIATIONS (engine racks only)
Not live synthesis: ask the drum engine to **render** 4–8 new variations of the selected pad (same material/role, tuned
to the rack's key) in the background; show them as cards to audition in cue and keep or discard. Progress state while
rendering (a few hundred milliseconds each).

### 55 MODE SELECT update
Add **DRUM** next to SYNTH, STANDALONE and CONTROLLER on the existing mode-select screen, with its status line (rack
loaded, samples RAM).

## Deliver
New pages in the existing project (or a file `Console Drum Screens`), each with its "Layout notes for QML" block, the
states (empty / loading / RAM budget exceeded / sample missing on disk), and updated components (Pad Cell, Step Lane,
Pattern Slot, Sound Row, Waveform with handles).
