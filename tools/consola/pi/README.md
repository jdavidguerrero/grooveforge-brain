# Configuración del Raspberry Pi 5 de la consola

Rescatada el 2026-11-09 de la SD que funcionaba con la pantalla (Raspberry Pi OS del 2026-04-21).

| Archivo | Qué es | Cómo se aplica |
|---|---|---|
| `config-consola.txt` | Líneas propias de `/boot/firmware/config.txt`: I2S, I2C, Audio+ V2 (`hifiberry-dac`) y la pantalla 7" (`vc4-kms-dsi-waveshare-panel-v2,7_0_inch_a`) | `e0-preparar-pi.sh configurar` las agrega al final, si faltan, con respaldo |
| `cmdline-consola.txt` | Parámetros extra para `/boot/firmware/cmdline.txt`: la consola de texto rotada 90° (`fbcon=rotate:1`) y sin logo | Se **agregan** a la línea existente; nunca se copia `cmdline.txt` entero, porque lleva el `PARTUUID` de cada tarjeta |

**Historial de la SD anterior:** primero se probó `video=DSI-1:720x1280M@60,rotate=90` en `cmdline.txt`; la versión que quedó usa `fbcon=rotate:1`.

**Lo que no está aquí:** la rotación del **touch** y de las apps gráficas (Qt) no vive en la partición de arranque. Si en la SD anterior hubo reglas para el touch (por ejemplo en `/etc/udev/` o en variables de Qt), están en la partición del sistema, que el Mac no puede leer. La GUI de Qt rota con su propia variable (`QT_QPA_EGLFS_ROTATION`); se configura cuando se instale la GUI.

**Sin copiar a propósito:** `user-data` y `network-config` de la SD anterior (pueden tener la contraseña del Wi-Fi). La red se configura en Raspberry Pi Imager al grabar la nueva SD.
