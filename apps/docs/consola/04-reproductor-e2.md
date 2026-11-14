# Consola: el reproductor de E2

> **Fecha:** 2026-11-09. **Estado:** diseño de la prueba E2 (`01-prototipo-mesa.md`). **No modifica** `00–06`.

## 1. Qué responde

¿Puede el Pi 5 sonar durante 30 minutos por el Audio+ **sin cortes**, mientras el motor trabaja al lado? Es la base del modo LIVE: si esto falla, nada de lo demás importa.

## 2. Cómo funciona

```
drum machine (gf_perc) ──WAV en memoria──┐
patrón de 16 compases (pasos) ───────────┼─▶ callback de audio ─▶ ALSA ─▶ Audio+ (PCM5102A)
                                         │   (bloque de 256 o 128 muestras a 48 kHz)
motor generando racks en bucle (carga) ──┘   mide: duración de cada bloque y cortes
```

- **El reloj es el callback de audio.** Cada vez que la tarjeta pide un bloque, el reproductor sabe en qué muestra va, dispara las notas que caen dentro de ese bloque **con precisión de muestra** y mezcla las voces que siguen sonando. No hay temporizador aparte: el tiempo musical sale del contador de muestras, que es lo más estable que tiene el Pi (`03-arquitectura-capas.md` §1).
- **Sonidos:** se generan al arrancar con la drum machine (kick, clave, snare, clap, hats), afinados a La, a 48 kHz, la misma frecuencia del Audio+, así que no hay remuestreo.
- **Patrón:** el groove del set de Juan: kick a negras, clave son 3-2, clap en 2 y 4, hats en las «y» y hat abierto al cierre de cada 4 compases (que corta al cerrado, como un choke).
- **Salida:** directo a ALSA (`hw` del Audio+) con PortAudio (`sounddevice`). Sin PipeWire en el medio en esta prueba: menos capas, y la medición es del Pi y no del servidor de audio.

## 3. Qué se mide

| Medida | Cómo | Pasa si |
|---|---|---|
| **Cortes (xruns)** | Bandera de *underflow* que PortAudio entrega en cada callback | **0** en 30 min con bloque de 256 |
| **Carga del callback** | Duración de cada callback ÷ duración del bloque (5,3 ms con 256) | Máximo < 50 % (margen para synths y FX más adelante) |
| **Bajo carga** | Mismo test con el motor generando racks de drums sin parar en otro proceso | Igual que arriba |
| Bloque de 128 (2,7 ms) | Repetir | Informativo: decide cuánto se puede bajar la latencia |

La **latencia de ida y vuelta** con el bucle Audio+ → SGTL5000 → Teensy → Pi pasa a **E3**, cuando la Teensy esté conectada.

## 4. Código

`tools/consola/reproductor.py`: lógica pura (patrón, programación por bloque, mezcla) probada en el Mac con tests; la parte de audio en vivo solo corre en el Pi.
