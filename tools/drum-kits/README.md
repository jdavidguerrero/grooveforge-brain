# DRUM kit service

Local offline worker for Console spec22. No network, no third-party assets in this
repository. Requirements: Python3.11+, numpy, scipy, soundfile (libsndfile), pytest
for tests. It imports the existing sibling `tools/perc-synth/gf_perc` APIs.

```sh
export GF_DRUM_DATA="$HOME/.local/share/gf-drum"
python3 tools/drum-kits/drum_kits.py render --genre organic --root A --mode menor --layers 4
python3 tools/drum-kits/drum_kits.py import /path/to/your/samples --licence-note 'Tribe — licence pending' --trim
python3 tools/drum-kits/drum_kits.py variations --rack "$GF_DRUM_DATA/racks/ID.json" --pad 0 --count 8
python3 -m pytest tools/drum-kits/tests -q
```

`--data-dir DIR` can precede the subcommand; the environment default is also used
by the host. On the Pi configure `/data/drum` explicitly. Source folders must be
separate from the data directory. Mono/stereo WAV/AIFF/FLAC/OGG are decoded by
libsndfile, resampled offline to48k float32 and named by SHA256 of the WAV bytes.
Imported folders exceeding16 files or the rack RAM budget produce multiple racks.
Unreadable files are skipped with reasons, license notes remain free text, and
original files are untouched. Note pending is information, not a license grant.

ENGINE renders13 gf_perc voices plus3empty pads, with1..4velocity layers or RR.
The existing tuning rules are preserved: root/scale context, kick register/optional
fifth and unpitched noise; see the contradiction/questions in spec22§9. Rendered
notes are recorded in each layer. Changing root requires an explicit new render.

The host launches `worker --job FILE` using posix_spawn, without a shell. Jobs
emit JSON progress and results on stdout and write an atomic `.result.json`.
Variations write new assets/candidates; they never load a sound or edit a rack.
GUI KEEP is an explicit, separate host command. Configure the host's
`--drum-kit-python` with an interpreter that has the dependencies, and
`--drum-kit-script` with the absolute path to this script. No packages are installed
by the CLI, host or GUI.
