#!/bin/sh
# Compila la validación offline de los engines con los núcleos DSP reales del firmware.
set -e
cd "$(dirname "$0")"
SRC=../../src
clang++ -std=c++17 -O2 -I"$SRC" -o validate_engines validate_engines.cpp \
    "$SRC/engines/prophet_osc_core.cpp" "$SRC/engines/dx7_core.cpp" "$SRC/dsp/lfo.cpp"
mkdir -p out
echo "ok: ./validate_engines out"
