# Consola: E3, la Teensy A conectada al Pi

> **Fecha:** 2026-11-09. **Estado:** en prueba. **Base:** `01-prototipo-mesa.md` (E3), `03-arquitectura-capas.md`. **No modifica** `00–06` ni el firmware normal del Brain.

## 1. Lo que encontró la conexión

El Pi ve la Teensy del Brain por USB de alta velocidad como **audio** (`card 3`), **MIDI** (cliente 28) y **serie** (`/dev/ttyACM0`). Pero el firmware del Brain (sketch 49) **no tenía salida de audio USB**: el Pi recibía silencio. La entrada USB (Pi → audífonos) existía, apagada, y se encendía con un combo del panel.

## 2. El cambio: entorno `consola` (`-DGF_CONSOLA`)

Mismo firmware del Brain; lo nuevo solo existe con la marca `GF_CONSOLA` (`apps/firmware-teensy/platformio.ini`, `[env:consola]`):

| Cambio | Para qué |
|---|---|
| `AudioOutputUSB` alimentado por un selector por lado | Que la Teensy le mande audio al Pi |
| Selector: canal 0 = mezcla de los synths (por defecto), canal 1 = entrada de línea | Con la entrada de línea se mide la latencia de ida y vuelta (Audio+ → cable → SGTL5000 → USB → Pi) |
| USB passthrough encendido al arrancar | El cue que manda el Pi sale por los audífonos, sin el combo del panel |
| Comandos `@GF RUTA SINTE` / `@GF RUTA LINEA` y `@GF CUE 0` / `@GF CUE 1` por el puerto serie | Controlarlo desde el Pi. **Solo para el prototipo:** en la consola final van por `bridge-protocol` |

El firmware normal (`sketch49`) compila igual y responde `@GF ERR CONSOLA` a esos comandos. Tests nativos: 157 (uno nuevo para el parseo).

## 3. Datos a tener en cuenta

- **Frecuencia:** el audio de la Teensy corre a 44,1 kHz nominales (`AUDIO_SAMPLE_RATE_EXACT` = 44100 en Teensyduino 1.60; corregido el 2026-11-09, antes decía 44 117,647 Hz) con su propio cristal; el Pi y el Audio+ van a 48 kHz. Pi y Teensy tienen **relojes distintos**: los posibles clics del cue en E3 vienen de ahí.
- **Nivel de entrada:** el código fija `lineInLevel(5)`. Su comentario dice que 0 es la máxima sensibilidad; la documentación de PJRC indica lo contrario (0 = 3,12 Vpp, la menos sensible). **Contradicción a revisar**; en la prueba la señal va a −20 dBFS para no saturar.

## 4. Pruebas

1. **Synths → Pi:** el Pi manda notas MIDI y graba por USB; debe haber señal limpia.
2. **Cue:** el Pi manda un tono por USB; se escucha en los audífonos del SGTL5000. Una hora sonando sala (Audio+) y cue (Teensy) a la vez, escuchando clics.
3. **Latencia:** `@GF RUTA LINEA`; el Pi toca un clic por el Audio+ y lo busca en lo que llega por USB.

## 5. Flasheo

Se compila en el Mac (`pio run -e consola`) y se programa **desde el Pi** con `teensy_loader_cli`, porque la Teensy ya está conectada a él: no se mueve al Mac ni se quita el aislador.
