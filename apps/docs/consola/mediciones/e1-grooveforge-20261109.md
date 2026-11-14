# E1 · el motor en el Pi 5 · 2026-11-09

Raspberry Pi 5 (8 GB), Debian 13, kernel 6.18 PREEMPT, Python 3.13, uv 0.13.

| Prueba | Resultado |
|---|---|
| `tools/perc-synth` (drum machine): suite completa | **467 pasan** en 43 s |
| `tools/motor`: suite completa | **221 pasan, 5 saltados** en 43 s. Los saltados piden plantillas de la User Library de Ableton (`808 Core Kit.adg`, `GF ROL.adg`), que no están en el Pi |
| Generar una tanda de kicks (8 colas) | **0,11 s** |
| Generar un rack de drums completo (15 voces: 5 voces × 3 materiales) | **0,34 s** |
| Generar los 3 subdrops | **0,04 s** |
| 3 progresiones de acordes × 10 patrones (30 clips) | < 0,01 s |

**Pasa:** objetivo «menos de unos 10 s por tanda de drums» (`01-prototipo-mesa.md` E1). Generar en vivo no es un cuello de botella: el plan B (precalcular en el Mac) no hace falta.

**Ajuste:** `python-rtmidi` no tiene paquete compilado para ARM64 con Python 3.13 y se compila al instalar; necesita `libasound2-dev` y `pkg-config`. Agregados a `e0-preparar-pi.sh`.

**Paso descartado (Juan, 2026-11-09):** abrir la web del motor desde el Mac. En la consola la interfaz es la app de Qt, que habla con el motor en el mismo Pi; la web sigue siendo la del Mac en el estudio. **E1 pasa.**
