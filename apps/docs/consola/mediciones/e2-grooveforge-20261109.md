# E2 · sonido de sala desde el Pi 5 · 2026-11-09

Reproductor `tools/consola/reproductor.py` (samples de la drum machine, 122 BPM, reloj en el callback) por ALSA directo al Audio+ (PCM5102A), 48 kHz. Kernel 6.18 PREEMPT normal. Datos crudos: `e2-grooveforge-20261109-1038.txt`.

| Prueba | Bloques | **Cortes** | Callback medio | p99 | **Máximo** | **Carga máx.** | Temperatura |
|---|---|---|---|---|---|---|---|
| 30 min, bloque 256, sin carga | 337 510 | **0** | 40 µs | 78 µs | 156 µs | **2,9 %** | 41 °C |
| 30 min, bloque 256, **con carga** (3 procesos generando racks sin pausa) | 337 521 | **0** | 92 µs | 252 µs | 660 µs | **12,4 %** | 58 °C |
| 5 min, bloque 128, con carga | 112 506 | **0** | 86 µs | 237 µs | 764 µs | **28,6 %** | 58 °C |

Sin reducción de velocidad por temperatura en ninguna prueba (`throttled=0x0`).

## Lectura

- **E2 pasa:** 0 cortes en 65 minutos, incluso con tres núcleos ocupados generando sonidos, y con el **kernel normal**. **No hace falta PREEMPT_RT** por ahora (`03-arquitectura-capas.md` §3).
- **Margen:** con carga, el peor bloque usó el 12 % de su tiempo (bloque de 256) y el 29 % (bloque de 128). Queda espacio para mezclar más capas, ecualizadores y filtros del mixer.
- **La carga sube la temperatura** a 58 °C en 30 min, sin llegar al límite. En la caja hará falta ventilación o un disipador (el del Pi 5 activo es suficiente).
- **Corrección pendiente:** la latencia de salida informada fue 5,3 ms también con bloque de 128: PortAudio no bajó el buffer del dispositivo. Para medir la latencia real con bloques chicos hay que fijarla explícitamente; se hace en E3 con el bucle de ida y vuelta.
