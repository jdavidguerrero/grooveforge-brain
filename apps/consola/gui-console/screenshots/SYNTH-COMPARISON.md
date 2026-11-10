# Synth visual review · design 31–45

25 PNGs at **1280×720**, Qt 6.11.1 offscreen/software, scale 1. Big Shoulders,
IBM Plex Mono, Archivo, JetBrains Mono and local General Sans loaded; no QML
warnings. [Gallery](synth-index.html) · [manifest](synth-manifest.json).
The numeric PNG prefix is the registry ID; the design number is listed separately.
The first 35 legacy IDs are unchanged.

The bundled export was decoded from its JSON `__bundler/template`, including
inner CSS and `renderVals()` sample data. Review used each screen's supplied
“Layout notes for QML”; the independent renderer is not a pixel-equivalence claim.

| Design | Registry / PNG | Comparison and remaining differences |
|---|---|---|
| 31 | 36-SynthParts | 44 px header, five 152×392 cards, 472 px detail, two bus cards. Eight engine choices include DX7. Hardware and CUE are marked unavailable; output control replaced by supported mute. Voice counts omitted because host does not report them. |
| 31b | 37-SynthPartsStates | Loading bar, empty card, amber offline outline, hardware help. Explicit offline preview; contract cannot report empty/hardware states. Live timeout marks software parts offline. |
| 32 | 38-SynthFilter | Header, 296 px graph, 168 px cells and exact eight-column geometry. Log cutoff, resonance, bipolar and enum cells derive from host values. Cutoff ring is linear per metadata; response drawing is illustrative. |
| 32b | 39-SynthNotResponding | Amber status card, Retry, last cells at 45%, BRAIN dot amber. Retry backoff is automatic; design's countdown text omitted. |
| 32c | 40-SynthLoading | Static loading bar and dashed cells with ellipses. Unsupported Cancel and next-bar guarantee omitted. |
| 33 | 41-SynthOsc | Separate two-cycle oscillator tiles, mix/routing readouts, two banks. Uses the software ARP part; hardware control is outside contract. Bank order follows list_params, not the export's interleaved example. |
| 34 | 42-SynthEnv | Amp teal and filter white, two envelopes from values, first eight controls. Segment handles and selected-segment fill simplified. |
| 34b | 43-SynthEnvBank2 | Remaining four controls plus four dashed free cells. Graph remains visible. |
| 35 | 44-SynthLfo | Two cycles, synchronized note rate, delay and signed destinations. Grid/side-card destination treatment simplified; absent eighth destination remains a free cell. |
| 36 | 45-SynthFm | Table-driven 4-op diagram, eight algorithm buttons, four 312 px strips aligned to encoder pairs. Algorithm/feedback controls use a second metadata bank. |
| 36b | 46-SynthFmFocus | Operator highlight, editable tuning/velocity/ADSR; envelope shown beside the diagram. Click the focus strip to return to all operators. |
| 36 DX7 | 47-SynthDx7 | Added 6-op mode, 32 algorithm buttons, actual carriers/edges/feedback endpoints. First four operator strips; same column contract. |
| 36 DX7 bank 2 | 48-SynthDx7Bank2 | Operators 5–6, algorithm and feedback controls, two free cells. Diagram retains all six operators across banks. |
| 36b DX7 | 49-SynthDx7Focus | Added four-stage rate/level envelope with R1–R4/L1–L4 and handles. Other banks expose tuning, keyboard scaling, velocity and rate scaling. Envelope time axis is illustrative, not DX7 DSP timing. |
| 37 | 50-SynthGroove | 16-step arp, chord strum offsets and pump gain lane. Pump shown as adjacent bars; notes are an illustrative pattern, not live MIDI telemetry. |
| 38 | 51-SynthFx | Fixed six-effect chain, wet bars, selected-effect bank and detail card. Character sketch simplified; real effect transfer functions unavailable. |
| 39 | 52-SynthMacros | Four large macro knobs and target lists. Metadata has four contiguous controls; free cells are at the end, rather than forcing odd-encoder placement absent from contract. Mapping amounts are not DSP telemetry. |
| 40 | 53-SynthHybrid | Nine layer chips, blue morph band with bar marker, A/B panels and eight host controls. Swap/link/layer-save unavailable; half engine/preset descriptions are design samples. |
| 41 | 54-SynthHybridA | Same template in blue; SEM LP/notch/HP morph and BP. Half key flips A/B without losing selected part. |
| 41 B | 55-SynthHybridB | Extra review view for the second half, using its own prefixed parameters and filter type. |
| 42 | 56-SynthPresets | Role filter, engine/tag encoder filters, scroll/load, current preset and Save As. No cue audition, favorite mutation, usage counts or replace-in-place Save in contract. |
| 42b | 57-SynthSaveAs | 960×488 window at absolute x160/y96, 24-character field and touch keyboard with digits/punctuation. Tags omitted because save_preset has no tags field. Duplicate-name error stays in the dialog. |
| 43 | 58-SynthSettingsAudio | General Sans/JetBrains OS theme, 240 px navigation, two card columns. Rate/block come from hello; changes disabled. USB/input/cue meters are labeled preview data or unavailable. |
| 44 | 59-SynthSettingsConnections | Ethernet/Link/USB/MIDI cards. Samples labeled; no pretend rescan/network detection. Two-USB drawing conflicts with ADR-008 and remains a documented design sample. |
| 45 | 60-SynthSettingsSystem | Versions, storage, update, temperature, performance and power. Added real host CPU/xruns card; update/restart/shutdown disabled. 256 GB storage is explicitly sample data, conflicting with ADR-008's 128 GB. |

All screens use the existing theme/components; no live blur. The top bar includes
separate ENGINE and BRAIN indicators. Legacy transport status remains demo data;
BRAIN on synth screens follows the client. Unknown host parameter IDs still create
editable controls; specialized visualization is marked unavailable when its
semantic binding cannot be identified. See the exact contract proposals in
[15-gui-synth.md](../../../docs/consola/15-gui-synth.md).

No Pi GPU/audio/physical touch claims follow from these software screenshots.
