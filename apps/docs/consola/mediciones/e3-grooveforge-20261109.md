# E3 · la Teensy A (Brain) conectada al Pi 5 · 2026-11-09

Firmware: entorno `consola` (`05-e3-teensy-a.md`), programado desde el Pi con `teensy_loader_cli` (primer intento: «error writing to Teensy»; el reintento en HalfKay funcionó).

| Prueba | Resultado |
|---|---|
| El Pi ve la Teensy | Audio (`card 3`, 44,1 kHz), MIDI (cliente 28) y serie (`/dev/ttyACM0`), USB de alta velocidad |
| Comandos `@GF RUTA` / `@GF CUE` | Responden `@GF OK …` |
| **Brain → Pi** (notas MIDI del Pi, audio de vuelta por USB) | ✅ Silencio digital sin notas (−240 dBFS); con notas −31 dBFS de nivel medio, −18 dBFS de pico, L = R |
| **Pi → Teensy (cue)** | ✅ Tono de 440 Hz a −12 dBFS enviado por USB: aparece en la mezcla de la Teensy a **−12,1 dB** (sin pérdida) y **Juan lo escucha** por la salida del SGTL5000 |
| Audio+ → entrada de línea de la Teensy | ✅ Conexión confirmada por Juan |
| CPU del Brain (DX7) | 9 % (máx. 9,6 %); memoria de audio máx. 26 de 128 bloques |
| **Latencia de ida y vuelta** | ⚠️ **No válida todavía:** 30/30 clics, mediana 2,9 ms, pero mínimo −4,6 ms y máximo 17,8 ms. El método con marcas de tiempo por callback no es preciso; hay que medir grabando salida y entrada a la vez |

**Pendiente de E3:** corregir la medición de latencia; una hora de sala + cue a la vez escuchando clics (dos relojes distintos); revisar la contradicción de `lineInLevel`.
