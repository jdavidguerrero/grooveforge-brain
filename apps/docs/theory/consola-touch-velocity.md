# Touch responsiveness and M4 dynamics — implementation contract

2026-11-11. Scope approved by Juan's touch/velocity request. No DSP or pin changes.

Qt dispatches input and Python property notifications on the GUI thread. A broad
notify signal makes every dependent QML binding run, including QVariantList
Repeaters which destroy their delegates. Comparing the resulting model after
building it does not avoid this work. Clock and meters therefore need separate
notify signals and frame flags. Meter values and playhead are bound separately
from rack/source/sample/step models; pattern changes remain structural updates.
Keep the existing soft-key model and its incremental role updates.

Every TouchTarget draws a pressed overlay without animation. Navigation, soft
keys and choices outside scroll containers activate on press. Any target under a
Flickable (including a ListView contentItem) activates on release. A shared Qt
startDragDistance of ~3 mm (25 px at the 7-inch 720×1280 panel's ~210 dpi) allows
finger jitter while allowing Flickable to steal a genuine drag. Do not use
preventStealing or pressDelay, which would harm scrolling or immediate feedback.
QML rotation handles hit testing; evdev remains identity.

Load intentions publish a separate busy label before sending the async request;
only the matching reply/error/disconnection clears it. Keep browsing/STOP usable.
Do not claim to shorten the host's 380–530 ms decode/hash/resample work.

SHIFT + fader 4 (CC27) is a new modifier binding in DRUM and SYNTH, leaving all
unmodified faders and eight parameter encoders available. Map absolute fader
position to 1–127 M4 velocity. Snapshot the choice before the next note, persist
on the GUI thread with LocalSettings' atomic save, and display its value. Use one
global local velocity for both modes; existing 64/100/127 Settings buttons are
quick choices for the same value (legacy settings key retained). Tradeoff: shifted
absolute fader movement does not move its normal group/part level.

SHIFT + M4 hit = 127 (accent), captured in the MIDI callback before frame batching.
No existing SHIFT+pad action exists: Variations uses SHIFT+encoder *click*, and
Perform's MUTE/SOLO hold still wins over musical hits. FSR/external velocities are
unchanged. Host overdub records the incoming velocity (127 means accent); no new
host protocol field. Lower DRUM rows remain step toggles off/100 normally and
write 127 with SHIFT, preserving probability/microtiming.

Validation: rendered 90-degree synthetic touch press/release, jitter and real
scroll, held delegates during 30 Hz clock+meters; deferred host replies/errors;
immediate note output, persistence and host-recorded accent. Profile real QML
screens with 30 clock + 30 meters events/s, measure event/commit ms and main-thread
and process CPU separately. Desktop offscreen results cannot certify Pi/evdev or
physical touch latency. Repeat the same harness on the Pi for the physical gate.

References: [Qt MouseArea](https://doc.qt.io/qt-6/qml-qtquick-mousearea.html),
[Qt Flickable](https://doc.qt.io/qt-6/qml-qtquick-flickable.html),
[QStyleHints startDragDistance](https://doc.qt.io/qt-6/qstylehints.html#startDragDistance-prop),
local contracts 13, 17, 21, 22 and 23; host `Drum::hit` / `DrumService::drain`.
