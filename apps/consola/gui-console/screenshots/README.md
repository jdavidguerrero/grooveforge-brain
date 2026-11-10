# Screenshot review

All 35 PNGs were inspected against the supplied HTML geometry, sample content and colors.
Qt offscreen/software renderer, scale 1, 1280×720. This is visual review, not a pixel-diff certification.

[Offline gallery](index.html) · [Complete machine-readable list](manifest.json) · [Phase-2 report](../../../docs/consola/11-gui-fase2.md)

| Capture | Remaining visual difference / review note |
|---|---|
| [01-Boot.png](01-Boot.png) | Baked glyph/glow; no live shadow. |
| [02-ModeSelect.png](02-ModeSelect.png) | Auto-entry timer disabled; Controller remains reviewable with the offline sample. |
| [03-Toasts.png](03-Toasts.png) | Full LiveSession remains behind the two fixed toast samples. |
| [04-Settings.png](04-Settings.png) | Surface only; other sections retain phase-1 summaries. Sliders/calibration are previews. |
| [05-LiveSession.png](05-LiveSession.png) | Contract columns 152 px; simplified tags/glows. |
| [06-Layer.png](06-Layer.png) | Combined cue/energy mapping frees four soft actions; simplified tags. |
| [07-Transition.png](07-Transition.png) | Simplified type sketches and static takeover footer. |
| [08-Mixer.png](08-Mixer.png) | Four fixed groups, eight channels; meters have no audio clock/interpolation. |
| [09-Hands.png](09-Hands.png) | Static sensors; inherited C4 distance inconsistency retained. |
| [10-Search.png](10-Search.png) | Four fixed matches with a sample count of 38; preview fixed; 123 key simulates one digit. |
| [11-LiveArrange.png](11-LiveArrange.png) | Energy polyline and static rivers; dashed entry lines, no musical clock. |
| [12-SequencerSteps.png](12-SequencerSteps.png) | Static playhead; local batched velocity/probability/microtiming edits. |
| [13-SequencerPianoRoll.png](13-SequencerPianoRoll.png) | Legend says 14 user notes while export contains 16; pad frame follows export; touch only navigates. |
| [14-Pads.png](14-Pads.png) | FSR/expression snapshots; touch does not play the pads. |
| [15-BrainEngine.png](15-BrainEngine.png) | Engine illustrations are sketches pending final art; eight design macro values. |
| [16-BrainHybrid.png](16-BrainHybrid.png) | Simplified split-keyboard illustration; blue macros and original engine samples. |
| [17-Kit.png](17-Kit.png) | Triangular envelopes, dashed cue alternatives; generation is a local preview. |
| [18-Library.png](18-Library.png) | Table columns compressed to fit the inconsistent export widths. |
| [19-Log.png](19-Log.png) | Role labels use colored text in place of filled badges; export is a preview. |
| [20-Routing.png](20-Routing.png) | Routing matrix and connection status are fake; no actual route writes. |
| [21-Session.png](21-Session.png) | Six scenes, original track colors; selected/recording edges simplified. |
| [22-Note.png](22-Note.png) | 16 rack pads plus 16 steps; touch does not play music. |
| [23-Clip.png](23-Clip.png) | Clip velocity uses opacity; navigation has no Live connection. |
| [24-Device.png](24-Device.png) | Uniform device chips; original parameter arcs/response; no actual bank changes. |
| [25-Mix.png](25-Mix.png) | Stereo meters and faders are snapshots, with no Live control. |
| [26-Browse.png](26-Browse.png) | Category/item selection is local; preview remains the design sample. |
| [27-Scale.png](27-Scale.png) | 960×504 shared modal; no live CSS shadow. |
| [28-Repeat.png](28-Repeat.png) | Bottom modal; rate tiles align with the encoder-column contract. |
| [29-Tempo.png](29-Tempo.png) | 960×504 shared modal; tap/Link peers remain fixed sample data. |
| [30-Motor.png](30-Motor.png) | Soft keys replace hardware actions; option states change locally without writing clips. |
| [31-LiveSessionOffline.png](31-LiveSessionOffline.png) | ROOM/NEXT summary retained instead of the export’s full amber status banner. |
| [32-SessionNoLive.png](32-SessionNoLive.png) | Ghost grid and original three checks; Standalone switch works locally. |
| [33-LiveSessionEmpty.png](33-LiveSessionEmpty.png) | Empty state described in MVP notes; ROOM/NEXT preserved. |
| [34-LiveSessionLoading.png](34-LiveSessionLoading.png) | Loading state described in MVP notes; ROOM/NEXT preserved. |
| [35-NoteMelodic.png](35-NoteMelodic.png) | Controller note state described in notes: PADS geometry with Live Lead colors. |
