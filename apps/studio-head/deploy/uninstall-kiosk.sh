#!/usr/bin/env bash
# Deshace install-kiosk.sh: quita el servicio y la regla udev, vuelve el login en tty1 y restaura
# cmdline.txt y config.txt desde las copias .studio-bak.
# Uso:  sudo apps/studio-head/deploy/uninstall-kiosk.sh
set -euo pipefail

if [[ $EUID -ne 0 ]]; then
  echo "Correr con sudo: sudo $0" >&2
  exit 1
fi
BOOT=/boot/firmware

systemctl disable --now studio-head.service 2>/dev/null || true
rm -f /etc/systemd/system/studio-head.service /etc/udev/rules.d/99-studio-head.rules
systemctl daemon-reload
udevadm control --reload
systemctl enable getty@tty1.service

for f in cmdline.txt config.txt; do
  if [[ -f "$BOOT/$f.studio-bak" ]]; then
    mv "$BOOT/$f.studio-bak" "$BOOT/$f"
    echo "Restaurado $BOOT/$f"
  fi
done
echo "Listo. El binario sigue en /opt/studio-head (borrarlo a mano si no se usa). Reinicia para aplicar."
