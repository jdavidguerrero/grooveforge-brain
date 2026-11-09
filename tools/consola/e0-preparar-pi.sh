#!/usr/bin/env bash
# E0 del prototipo de mesa (apps/docs/consola/01-prototipo-mesa.md): preparar el Raspberry Pi 5.
# Se corre EN EL PI, desde la raíz del repo clonado. Dos fases, con un reinicio en medio:
#
#   bash tools/consola/e0-preparar-pi.sh configurar   # paquetes, uv, overlay del Audio+ (con respaldo)
#   sudo reboot
#   bash tools/consola/e0-preparar-pi.sh verificar    # tarjeta de audio, tono de prueba y latencia del sistema
#
# Overlay del Audio+ V2: `hifiberry-dac`, confirmado en la SD anterior de Juan (tools/consola/pi/). La pantalla 7"
# también sale de ahí. Si `verificar` no encuentra la tarjeta, se prueba otro con OVERLAY=<nombre>.
set -euo pipefail

OVERLAY="${OVERLAY:-hifiberry-dac}"
CONFIG=/boot/firmware/config.txt
CMDLINE=/boot/firmware/cmdline.txt
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
SALIDA="$REPO/apps/docs/consola/mediciones/e0-$(hostname)-$(date +%Y%m%d-%H%M).md"

configurar() {
  [[ "$(uname -m)" == "aarch64" ]] || { echo "Se esperaba un sistema de 64 bits (aarch64)"; exit 1; }
  grep -q "Raspberry Pi 5" /proc/device-tree/model || echo "Aviso: no parece un Raspberry Pi 5 ($(tr -d '\0' </proc/device-tree/model))"

  sudo apt-get update
  sudo apt-get install -y git rt-tests alsa-utils pipewire-bin pipx python3-dev libsndfile1
  pipx install uv || pipx upgrade uv
  pipx ensurepath

  marca="$(date +%Y%m%d-%H%M%S)"
  sudo cp "$CONFIG" "$CONFIG.bak-$marca"
  sudo cp "$CMDLINE" "$CMDLINE.bak-$marca"
  # Pantalla 7" y Audio+, rescatados de la SD anterior (tools/consola/pi/). Solo si faltan.
  if ! grep -q "vc4-kms-dsi-waveshare-panel-v2" "$CONFIG"; then
    { echo; echo "# --- GrooveForge consola (tools/consola/pi/config-consola.txt) ---"; cat "$REPO/tools/consola/pi/config-consola.txt"; } | sudo tee -a "$CONFIG" >/dev/null
  fi
  if ! grep -q "^dtoverlay=$OVERLAY" "$CONFIG"; then
    echo "dtoverlay=$OVERLAY" | sudo tee -a "$CONFIG" >/dev/null
  fi
  # El audio integrado se apaga para que el Audio+ sea la salida por defecto (documentación oficial del Pi).
  sudo sed -i 's/^dtparam=audio=on/#dtparam=audio=on/' "$CONFIG"
  # cmdline.txt es UNA línea con el PARTUUID de esta tarjeta: solo se agregan parámetros que falten.
  for p in $(cat "$REPO/tools/consola/pi/cmdline-consola.txt"); do
    grep -q -- "$p" "$CMDLINE" || sudo sed -i "1 s|\$| $p|" "$CMDLINE"
  done
  echo "Listo. Respaldos de config.txt y cmdline.txt junto a los originales. Reinicia (sudo reboot) y corre: $0 verificar"
}

verificar() {
  mkdir -p "$(dirname "$SALIDA")"
  {
    echo "# E0 · $(hostname) · $(date '+%Y-%m-%d %H:%M')"
    echo
    echo "| Dato | Valor |"
    echo "|---|---|"
    echo "| Modelo | $(tr -d '\0' </proc/device-tree/model) |"
    echo "| Sistema | $(. /etc/os-release; echo "$PRETTY_NAME") |"
    echo "| Kernel | $(uname -r) · $(uname -v | grep -o 'PREEMPT[_A-Z]*' | head -1) |"
    echo "| RAM | $(free -h | awk '/Mem:/{print $2}') |"
    echo "| Overlay | $OVERLAY |"
    echo
    echo "## Tarjetas de audio"
    echo '```'
    aplay -l || true
    echo '```'
  } > "$SALIDA"

  tarjeta=$(aplay -l | grep -i -m1 -E 'hifiberry|dac|audio\+|pcm5' | sed -E 's/^card ([0-9]+).*/\1/' || true)
  if [[ -z "$tarjeta" ]]; then
    echo "No encontré el Audio+. Revisa $SALIDA y prueba otro overlay: OVERLAY=<nombre> $0 configurar"
    exit 1
  fi
  echo "Tono de 440 Hz por la tarjeta $tarjeta durante 5 s: debe sonar limpio en ambos canales."
  timeout 6 speaker-test -D "plughw:$tarjeta,0" -c 2 -t sine -f 440 -l 1 >/dev/null 2>&1 || true

  echo "Latencia del sistema con cyclictest (60 s, como root). No muevas nada mientras corre."
  {
    echo
    echo "## cyclictest (60 s, prioridad 90, intervalo 200 µs; latencias en µs por núcleo)"
    echo '```'
    sudo cyclictest -m -S -p90 -i200 -D60 -q
    echo '```'
  } >> "$SALIDA"
  echo "Mediciones en: $SALIDA"
  echo "¿Sonó limpio el tono? Anótalo al final de ese archivo."
}

case "${1:-}" in
  configurar) configurar ;;
  verificar) verificar ;;
  *) echo "Uso: $0 configurar | verificar"; exit 1 ;;
esac
