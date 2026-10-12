#!/usr/bin/env bash
# Deja studio-head en modo kiosko: arranca solo al encender, sin login ni consola encima.
# Uso (en la Pi, después de compilar):  sudo apps/studio-head/deploy/install-kiosk.sh
# Se puede volver a correr para actualizar el binario. Para deshacer: uninstall-kiosk.sh
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "Correr con sudo: sudo $0" >&2
  exit 1
fi

HERE="$(cd "$(dirname "$0")" && pwd)"
BIN="$HERE/../build/studio-head"
USER_NAME="${SUDO_USER:-juan}"
BOOT=/boot/firmware

[[ -x "$BIN" ]] || { echo "No encuentro $BIN: compila primero (cmake --build build)." >&2; exit 1; }

echo "1/5  Binario → /opt/studio-head/"
install -D -m 0755 "$BIN" /opt/studio-head/studio-head

echo "2/5  Regla udev (/dev/studio-touch y unidad de la tarjeta DRM)"
install -m 0644 "$HERE/99-studio-head.rules" /etc/udev/rules.d/99-studio-head.rules
udevadm control --reload
udevadm trigger --subsystem-match=input --subsystem-match=drm
udevadm settle

echo "3/5  Servicio systemd (usuario: $USER_NAME)"
sed "s/@USER@/$USER_NAME/" "$HERE/studio-head.service" > /etc/systemd/system/studio-head.service
systemctl daemon-reload
systemctl enable studio-head.service

echo "4/5  Consola fuera de la pantalla"
# Sin login en tty1: si no, el getty dibuja encima de la UI.
systemctl disable getty@tty1.service 2>/dev/null || true
# cmdline.txt: sin logo ni mensajes, sin cursor parpadeante y sin apagar la pantalla a los 10 min.
[[ -f "$BOOT/cmdline.txt.studio-bak" ]] || cp "$BOOT/cmdline.txt" "$BOOT/cmdline.txt.studio-bak"
for opt in quiet loglevel=3 logo.nologo vt.global_cursor_default=0 consoleblank=0; do
  grep -qw -- "$opt" "$BOOT/cmdline.txt" || sed -i "1 s/\$/ $opt/" "$BOOT/cmdline.txt"
done
# config.txt: sin el cuadro de colores del firmware al encender.
[[ -f "$BOOT/config.txt.studio-bak" ]] || cp "$BOOT/config.txt" "$BOOT/config.txt.studio-bak"
grep -q '^disable_splash=1' "$BOOT/config.txt" || printf '\n[all]\ndisable_splash=1\n' >> "$BOOT/config.txt"

echo "5/5  Arrancando"
pkill -x studio-head 2>/dev/null || true
systemctl restart studio-head.service
sleep 1
systemctl --no-pager --lines=0 status studio-head.service || true

cat <<EOF

Listo. studio-head arranca solo en cada encendido.
  Estado:      systemctl status studio-head
  Logs:        journalctl -u studio-head -b
  Detener:     sudo systemctl stop studio-head      (para correr la app a mano)
  Tiempo:      systemd-analyze critical-chain studio-head.service
  Deshacer:    sudo $HERE/uninstall-kiosk.sh
Reinicia (sudo reboot) para ver el arranque completo.
EOF
