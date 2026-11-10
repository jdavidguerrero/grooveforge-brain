# Music Synthesizer for Android adaptation

Copyright 2012–2013 Google Inc. Apache License 2.0: LICENSE alongside this file.
Read-only source: refs/msfa/app/src/main/jni, commit f67d41d313b7dc85f6fb99e79e515cc9d208cfff (no Dexed sources).

- env: original curves/logic; instance sample-rate multiplier, live-edit targets and diagnostic accessors.
- pitchenv, lfo: original curves/logic; rate units become instance members, initialized state;
  negative left shifts replaced with multiplication; custom live pitch-EG editing.
- sin, exp2: original LUT generation/interpolation (unused polynomial/Tanh routines retained).
- freqlut: instance 64-bit LUT (also safe at 8 kHz); bounded shifts for extreme legal MIDI/ratio/pitch combinations.
- note_tables.h: dx7note.cc frequency, velocity, level/rate scaling and PMS tables;
  defined negative multiplication replaces a signed shift.
- ../dx7_6op_algorithms.cpp: fm_core.cc algorithm flags, OP6 first.
- ../dx7_6op_core.cpp: new scheduler/renderer uses fm_op_kernel gain interpolation and
  two-sample feedback equation; implements multi-operator loops absent upstream.
- ../dx7_6op_sysex.cpp: VMEM unpack field layout from patch.cc; new strict parser and packing.

All adapted files retain Google attribution. New integration code is GrooveForge custom.
Factory/ROM voice data are not included.
