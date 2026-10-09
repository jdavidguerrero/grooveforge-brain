#!/usr/bin/env bash
# Genera las fuentes LVGL de la escala tipográfica de Studio (UI v3: 7 tamaños, pesos 400 y 600).
# Uso: apps/studio-head/tools/gen-fonts.sh   (requiere node; usa npx lv_font_conv)
set -euo pipefail
cd "$(dirname "$0")/.."

REG=assets/fonts/IBMPlexMono-Regular.ttf
SB=assets/fonts/IBMPlexMono-SemiBold.ttf
OUT=src/fonts

# Texto completo: ASCII + acentos del español + símbolos que usa la UI (· × ° ± → ↔ −)
TEXT_RANGE="0x20-0x7E,0xB0,0xB1,0xB7,0xD7,0xC1,0xC9,0xCD,0xD1,0xD3,0xDA,0xE1,0xE9,0xED,0xF1,0xF3,0xFA,0xFC,0x2192,0x2194,0x2212"
# Tamaños hero (64 y 96): solo cifras, mayúsculas y los signos de los valores grandes
HERO_RANGE="0x20,0x23,0x25,0x2B,0x2D,0x2E,0x2F,0x30-0x39,0x3A,0x41-0x5A,0x2212"

gen() {  # gen <ttf> <size> <range> <name>
  npx --yes lv_font_conv --font "$1" --size "$2" --bpp 4 --range "$3" \
    --format lvgl --lv-include lvgl.h --no-compress -o "$OUT/$4.c"
  echo "ok $4"
}

gen "$REG" 12 "$TEXT_RANGE" gf_mono_12_r
gen "$REG" 16 "$TEXT_RANGE" gf_mono_16_r
gen "$SB"  12 "$TEXT_RANGE" gf_mono_12_sb
gen "$SB"  16 "$TEXT_RANGE" gf_mono_16_sb
gen "$SB"  20 "$TEXT_RANGE" gf_mono_20_sb
gen "$SB"  28 "$TEXT_RANGE" gf_mono_28_sb
gen "$SB"  40 "$TEXT_RANGE" gf_mono_40_sb
gen "$SB"  64 "$HERO_RANGE" gf_mono_64_sb
gen "$SB"  96 "$HERO_RANGE" gf_mono_96_sb
