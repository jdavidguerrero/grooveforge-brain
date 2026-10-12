# ADR-005: Engine DX7 de 4 operadores con un objeto de audio propio

**Status:** Accepted
**Date:** 2026-10-12
**Deciders:** Juan Guerrero (owner)
**Relacionados:** ADR-002 (variante Studio: página OSC), `auditoria-tecnica-2026-05.md` (DX7 no es FM real)
**Afecta specs:** `01-architecture.md` §4.1, fila DX7 ("AudioSynthWaveformPWM x6 (FM operators)").
No se edita hasta autorización explícita.

---

## Context

El proyecto tiene tres versiones distintas del DX7:

| Dónde | Qué dice |
|---|---|
| `01-architecture.md` §4.1 | 6 operadores, como el DX7 original, con `AudioSynthWaveformPWM x6` |
| Diseño de Claude Design y UI de Studio | 4 operadores ("FM · 4-OP") |
| Firmware (`src/engines/dx7.h`) | 2 operadores: modulador → portadora |

El engine actual tiene además tres problemas de sonido (auditoría 2026-05):

- Usa `frequencyModulation()` de `AudioSynthWaveformModulated` (oficial). El DX7 hace **modulación de
  fase**; con modulación de frecuencia el timbre cambia con la nota y hay aliasing en los agudos.
- Con 2 operadores no hay algoritmos.
- No hay feedback de operador, que es lo que da los timbres metálicos y de ruido del DX7.

Por qué la Teensy Audio Library no alcanza para un FM completo: cada operador sería un objeto del
grafo y el audio se procesa en bloques de 128 muestras (2.9 ms a 44.1 kHz).

- **Feedback:** un operador que se modula a sí mismo necesita su muestra anterior. En el grafo, un
  lazo llega con 128 muestras de retraso y deja de ser FM.
- **Algoritmos:** cambiar de algoritmo obliga a reconectar `AudioConnection` en vivo o a poner un
  mezclador delante de cada operador.
- (Además, `AudioSynthWaveformPWM`, el objeto que nombra el spec, es un oscilador de pulso: no sirve
  como operador FM.)

## Decision

1. El DX7 de GrooveForge es **FM de 4 operadores**, en la línea de los Yamaha DX100 / TX81Z / reface DX,
   no el DX7 original de 6 operadores y 32 algoritmos.
2. Se implementa como **un objeto de audio propio** (código custom, una subclase de `AudioStream`),
   que calcula las 6 voces × 4 operadores **muestra por muestra**:
   - Modulación de fase con tabla de seno e interpolación lineal; acumuladores de fase de 32 bits.
   - **8 algoritmos** de 4 operadores (los de la familia DX100 / TX81Z) como una tabla de ruteo.
   - **Feedback en el operador 4**, promediando las dos últimas salidas (el truco del DX7 para que el
     feedback alto no se vuelva inestable).
   - Por operador: RATIO (coarse + fine), LEVEL y una envolvente propia.
3. El filtro `MoogLadder4P` (custom) queda como opción por voz, apagado por defecto, como pide
   `01-architecture.md` (DX7: filtro OFF, "warmed FM" opcional).
4. La API del engine se mantiene (`noteOn`, `noteOff`, `update`, setters), así que la navegación, el
   bridge y la UI no cambian de forma.

## Options Considered

| Opción | Veredicto | Por qué |
|---|---|---|
| Quedarse en 2 operadores | ❌ | No es FM auténtico, sin algoritmos ni feedback |
| **4 operadores, objeto propio** | ✅ | FM real y editable con el panel de Studio; cubre E-pianos, bajos y campanas |
| 6 operadores, objeto propio (DX7 real) | ❌ por ahora | El doble de parámetros; editar 6 operadores con 5 encoders es justo lo que hizo difícil al DX7. Solo tiene sentido para cargar SysEx del DX7 original |
| 4 operadores con objetos de la librería | ❌ | Sin feedback real; algoritmos con mezcladores por operador; más CPU por overhead de bloques |

## Trade-off Analysis

- **CPU (estimación, a medir):** por muestra y por operador: avance de fase, lectura de tabla
  interpolada, envolvente y multiplicación (~25 ciclos). 6 voces × 4 operadores × 44 100 Hz × 25
  ciclos ≈ 26 M ciclos/s ≈ **4–5 %** de los 600 MHz de la Teensy 4.1. Con overhead y el filtro
  opcional, **≤10 %**: dentro del ~30 % del presupuesto de engines (`01` §4.1), con un solo engine
  activo a la vez.
- **Memoria:** una tabla de seno de 1024 muestras en flash (4 KB en float, 2 KB en int16) más el
  estado por voz. Despreciable.
- **Aliasing:** la modulación de fase lo reduce frente a la de frecuencia, pero los índices altos
  siguen generando parciales por encima de Nyquist. Se acota el índice máximo según la nota (como
  hacen los Yamaha) y se mide.
- **Costo de desarrollo:** un objeto de audio propio es más trabajo que encadenar objetos de la
  librería, pero es lo único que permite feedback correcto.

## Consequences

- La página OSC (diseño 11c) muestra **OP1–OP4 y el algoritmo**. NAV elige el operador; P1 / P2
  editan RATIO / LEVEL; con SHIFT, ALGORITHM / FEEDBACK (a validar en el diseño).
- La etiqueta de la UI pasa a "FM · 4-OP · POLY 6" (el engine tiene 6 voces, no 16).
- Los presets del DX7 original (SysEx de 6 operadores) **no** se pueden cargar. Si algún día hace
  falta, es otro ADR.
- El sprint del engine lleva su theory doc primero: modulación de fase vs. de frecuencia, los 8
  algoritmos, el feedback promediado y la envolvente por operador.

## Action Items

1. [ ] Autorización para actualizar la fila DX7 de `01-architecture.md` §4.1.
2. [x] Prompt de Claude Design (OSC · DX7) con 4 operadores.
3. [x] Etiqueta del DX7 en la UI de Studio.
4. [ ] Theory doc del engine FM de 4 operadores (`apps/docs/sprints/`).
5. [ ] Implementación + smoke test native (parámetros → salida sin NaN/Inf) + medición de CPU on-device.

## Referencias

- Chowning, J. M. "The Synthesis of Complex Audio Spectra by Means of Frequency Modulation".
  *Journal of the Audio Engineering Society* 21(7), 1973.
- Yamaha TX81Z / DX100 Owner's Manual — los 8 algoritmos de 4 operadores y el feedback del operador 4.
- Dexed / Music Synthesizer for Android (msfa), implementación open source del motor del DX7:
  modulación de fase en punto fijo y feedback promediado.
- PJRC, Teensy Audio Library — `AudioStream` (objetos propios) y `AudioSynthWaveformModulated`.
