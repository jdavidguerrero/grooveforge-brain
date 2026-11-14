# Prompt complementario para Claude Design · Consola: páginas del synth (BRAIN / parts)

> Para pegar en el **mismo proyecto de Claude Design** donde está el diseño de la consola (export en
> `design/claude-design/`). Adjuntar: el export actual (o seguir en el mismo proyecto), capturas del VST del
> Brain (páginas OSC, ENV/LFO, GROOVE, FX, MACROS, HYBRID HOME) y `theme.h` de `apps/studio-head`.
> Fecha: 2026-11-09.

---

You already designed the **GrooveForge Console** (7" touchscreen, 1280×720, Push/MPC-style hardware: 8 endless
encoders with an 8-key soft-key bar under them, a 4×8 pad grid, 4 faders, FSR pads, two IR "hands" sensors).
Keep **everything** from that design: the design system, colours, role colours (KICK, DRUMS, PERC, TOP, BASS,
CHORDS, LEAD, PAD), fonts (Big Shoulders Display, IBM Plex Mono, Archivo, JetBrains Mono; GroovePilot brand with
General Sans only in menus/boot), the chrome geometry (TopBar 56 · Content 568 · SoftKeys 96; columns
x = 8 + i·160, w 152) and the "Layout notes for QML" style. **All UI text in English.**

This brief **completes the synth part**, which was left with empty tabs.

## What changed since the first design

- The synth engines of the GrooveForge Brain now also run **on the console itself** (a software twin of the
  Brain). A live set can have **four synth parts at once: BASS, CHORDS, LEAD and PAD**, each with its own engine
  and preset. The physical Brain (a Teensy) can still play **one extra live part** with its analog filter.
- Naming: **BRAIN** = the synth (engines, parts, presets). **ENGINE** = the generative motor that proposes
  patterns and options. Fix any text that says "Brain offline" when it means the generative engine.
- **ACCEPT / GENERATE / SKIP / DROP are not hardware buttons**: they are soft keys (encoder click under the key,
  or touch). Update the layout notes that say otherwise.
- The FM engine has **4 operators** (DX100/TX81Z family, 8 algorithms, feedback on operator 4), not 6. Fix
  "FM 6-OP · 8 voices" → "FM 4-OP · 6 voices". (A 6-operator mode is under evaluation; design the FM page so a
  2-column-per-operator layout could grow from 4 to 6 operators without redesign.)

## Engines (six + HYBRID)

| Engine card | After | Voices | Character |
|---|---|---|---|
| LADDER MONO | Moog Model D | mono, glide | 3 VCO, ladder filter, overdrive |
| CHORUS POLY | Juno-106 | 6 | DCO + sub, chorus I/II |
| VINTAGE POLY | Prophet-5 | 5 | 2 VCO, poly-mod, filter env |
| SEM POLY | OB-6 | 6 | 2 VCO, SEM filter with morph/BP, X-MOD, sync |
| FM 4-OP | DX7 (DX100 family) | 6 | 4 operators, 8 algorithms, feedback |
| MODULAR | ARP 2600 | 4 (duo) | 2 VCO, hard sync, ring mod, noise |
| HYBRID | two halves (A | B) | per half | two engines layered, MORPH between halves, 8 factory layers, per-half transpose/gain/delay |

Shared across engines: **MACROS** (DRIFT, GRIT, POCKET, MOVE), **GROOVE** (ARP, CHORD/STRUM, PUMP), and
**SYNTH FX** (DRIVE, TAPE with wow/flutter, PHASER, CHORUS, PING-PONG DELAY, PLATE and MODAL reverbs).

## Screens to design

### 1. PARTS (new, entry point of the synth)
The four synth parts of the set (BASS, CHORDS, LEAD, PAD) **plus** the optional live **BRAIN** part (the
hardware Teensy, show it as a distinct "hardware" card with its analog filter badge). Each part card shows:
role colour, engine, preset name, level meter, whether it is in the ROOM or in CUE, and a tiny activity
indicator. Encoders 1–5 select / scroll; press opens that part. This is how you know **which part you are
editing** — the selected part must stay visible on every synth page (header chip in role colour).

### 2. Generic PARAMETER PAGE (template)
One template for every engine page: **8 parameters under the 8 encoders**, each with name, value, unit and a
visual (arc knob, bipolar arc, stepped selector, on/off). Above them, a **graph area** whose content depends on
the page (see 3). Page tabs: OSC · FILTER · ENV · LFO · GROOVE · FX · MACROS (an engine may hide tabs it does not
have). Soft keys: page left/right, bank (when a page has more than 8 params: "1/2"), SHIFT-fine, and BACK.
Show how a page with 12 parameters splits in two banks. This template must work for all engines without
per-engine artwork except the graph.

### 3. Graph widgets for the page template
- **OSC**: waveform preview per oscillator, mix bars, sync/ring indicators.
- **FILTER**: response curve (LP/HP/BP, SEM morph), cutoff marker, resonance peak, key-tracking hint.
- **ENV**: ADSR shape with the edited segment highlighted; for filter vs amp envelope, two colours.
- **LFO**: waveform, rate (synced shows note value), destination chips.
- **FM 4-OP**: algorithm diagram (operators as boxes, carriers on the bottom row, feedback loop drawn on op 4),
  per-operator ratio/level/envelope mini-strip; selecting an operator focuses its 8 params.
- **GROOVE**: ARP pattern lane, CHORD shape, PUMP curve over one bar.
- **FX**: signal chain as chips (DRIVE → TAPE → PHASER → CHORUS → DELAY → REVERB) with wet amounts; the focused
  effect shows its 8 params.
- **MACROS**: four macro knobs large, with a list of what each one moves.

### 4. HYBRID detail
HOME with the two halves A | B (engine + preset each), MORPH as a big horizontal crossfade-like control (it is
**not** a DJ crossfader: it morphs timbre between halves), layer bank (8 factory + custom), per-half
transpose / gain / delay, and how you enter half A or B pages (which then use the template).

### 5. PRESETS browser (per part)
Filtered by **role** first (BASS shows bass presets), then engine, then tags (e.g. PLK, SUS, SUB, MID, STAB,
PAD, AIR). Columns or cards with name, engine, tags, a favourite star and "used in N sets". Audition with the
cue (preview without touching the room). Encoder 1 scrolls, encoder 2 filters, press loads to the part; touch
only to type a search with the on-screen keyboard. Show "MODIFIED" state and SAVE AS.

### 6. Missing settings pages
**Settings · Audio** (sample rate, buffer, outputs: Room = Audio+, Cue = Teensy headphones; input gain IN;
Brain USB link status with its counters), **Settings · Connections** (Ethernet to the Mac, Ableton Link peers,
MIDI devices, USB devices "GF Superficie" and the Brain), **Settings · System** (versions of GUI / engine /
two firmwares, storage on NVMe, temperature, update from the Mac, shutdown).

## Rules (same as before)
- Music is played with pads, encoders and faders; **touch is auxiliary** (menus, search, typing, picking).
- Glanceable from 60 cm, nothing bounces, no decorative motion. Glows and halos as static art, no live blur.
- Every new screen: the same chrome, a "Layout notes for QML" block with exact pixel geometry, and the empty /
  loading / offline states where they apply.
- Deliver as new pages in the existing project (or a new file `Console Synth Screens`) plus updated
  components (Parameter Page, Part Card, Algorithm Diagram, Preset Row).
