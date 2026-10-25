# Studio · Página GROOVE: reloj común, PUMP, ARP, CHORD, UNISON y macros

> **Fecha:** 2026-10-25 · **Estado:** APROBADO por Juan ("sí aprobado con las 3 opciones, implementa") ·
> **Depende de:** `docs/theory/caracter-sonoro.md` §4 fase 1, §7 y §8 · theory/04 (navegación, 4 encoders) ·
> theory/06 (presets) · theory/07 (CC).

## 1. Reloj común (`dsp/groove_clock.h`, lógica pura)

Un solo reloj para PUMP, ARP, DELAY (fase 4) y el SYNC del LFO. Da **BPM** y **posición en beats** desde el arranque
(continua: `pos = 3.25` = segundo cuarto del cuarto beat), que es lo que hace falta para estar **en fase** con el kick.

| Fuente | Prioridad | Cuándo está activa | Fase |
|---|---|---|---|
| Clock MIDI (24 ppq) con START / CONTINUE / STOP / SONG POSITION | 1 | Hubo un clock en los últimos 300 ms | START = beat 0; SONG POSITION en semicorcheas; entre ticks se interpola con el último intervalo |
| Beat de la IA (`BEAT_DETECTED`: BPM + fase) | 2 | Hubo un beat en los últimos 2 s | La fase del detector |
| BPM interno + TAP | 3 | Siempre (respaldo) | Avanza con el BPM; cada TAP pone la fase en un beat entero |

- **TAP:** BPM = promedio de los últimos intervalos (2 a 4 toques, cada uno entre 0.25 y 2 s); un toque aislado solo
  re-fasa. El BPM interno arranca en 120.
- El usuario puede forzar la fuente (AUTO / MIDI / IA / INTERNO) en la parada PUMP.
- **Studio hoy:** el sketch49 no corre la IA, así que en la práctica usa MIDI e INTERNO. La fuente IA queda lista para
  cuando se integre.

## 2. PUMP (`dsp/pump.h`, lógica pura + `audio/pump_stage.h`)

Ducking del sonido del synth, **después de toda la cadena de SYNTH FX** (bombean también las colas de delay y reverb,
como un sidechain en el canal).

- Ciclo de `RATE` beats (1/4 = cada beat, 1/8 = medio beat, 1/2 = cada dos beats). En el inicio del ciclo
  (corrido por `OFFSET` ms) la ganancia cae a `1 − DEPTH` y se recupera en `RELEASE` ms (sin pasar del ciclo).
- Formas: **SUAVE** (coseno), **EXP** (recuperación exponencial), **KICK** (cae en ~3 ms, se mantiene ~25 ms y
  recupera exponencial: como un compresor con el kick).
- La ganancia se calcula a 1 kHz y el objeto de audio la interpola por muestra dentro del bloque (sin escalones).

## 3. ARP (`dsp/arp_core.h`, lógica pura)

Avanza con la posición del reloj (no con milisegundos propios): siempre en fase con PUMP y DELAY.

- Modos: UP, DOWN, UP-DOWN, RANDOM, **SMART** (paseo aleatorio por las notas sostenidas con preferencia por pasos
  cortos, sin repetir la misma nota dos veces seguidas).
- Divisiones: 1/4, 1/8, 1/16, 1/8T, 1/16T, 1/8. (puntillo).
- GATE 5–100 % del paso; OCTAVAS 1–3; **SWING** 50–75 % (retrasa los pasos impares); **LATCH** (las notas quedan
  sostenidas al soltar; tocar con todas sueltas reemplaza el acorde).
- Velocity: la de la última nota tocada.

## 4. CHORD (`dsp/chord_memory.h`, lógica pura)

- **MEMORIA:** con el acorde apretado, NAV push lo graba como intervalos desde su nota más grave. Después cada tecla
  dispara ese acorde transpuesto. Por defecto: tríada menor (0, 3, 7) + octava, típica de stab de house.
- **EN TONALIDAD:** cada tecla dispara la tríada diatónica de la tonalidad elegida (24: C mayor … B menor); una tecla
  fuera de la escala se lleva a la nota de la escala más cercana.
- **INVERSIÓN** 0–3 (sube a la octava las notas de abajo); **STRUM** 0–100 ms entre notas (de grave a agudo).
- CHORD va antes que ARP: con los dos encendidos, el arpegio recorre las notas del acorde.

## 5. UNISON (en cada engine polifónico)

- VOCES ×1 / ×2 / ×3 / ×6 por nota con DETUNE 0–50 ¢ repartido simétrico. **Cuesta polifonía:** 6 voces / ×N (tabla
  en caracter-sonoro §4). Al soltar la nota se liberan todas sus voces.
- Engines: Juno, Prophet, OB-6, DX7, ARP (en poly). El Model D ya tiene 3 VCO en una voz: no aplica.
- **Sin SPREAD estéreo en esta revisión:** los engines salen mono (salvo el chorus del Juno); repartir voces en
  estéreo exige mezclar por voz en L / R. Queda para la fase de capas.

## 6. Macros (desplazamientos sobre el patch, no lo pisan)

| Macro (CC) | 0 % → 100 % |
|---|---|
| **DRIFT** (14) | Por nota: desafinación aleatoria hasta ±12 ¢ y cutoff ±5 % por voz |
| **GRIT** (15) | Drive del filtro +, resonancia +20 % del rango, más FM en el DX7 (+ nivel de moduladores), POLY-MOD en el Prophet y X-MOD en el OB-6 |
| **POCKET** (16) | Decay y release de AMP y FILTER hacia 150–250 ms, FLT ENV AMT hacia 0.8, y la velocity entra al brillo también en Juno / Model D (que la ignoran en neutro) |
| **MORPH** (17) | Balance A / B de las capas: **sin efecto** hasta la fase de capas |

## 7. Página GROOVE (SHIFT + B3) y protocolo

Página 12 (`studio::GROOVE`, mismo id en la cabeza). NAV elige la parada, NAV push enciende / apaga (en CHORD, si
hay notas apretadas, las graba), los 4 encoders editan con capa SHIFT, **B4 = TAP**. NAV largo vuelve a PERFORM.

| Parada | P1 | P2 | ENC L | ENC R | SHIFT P1 | SHIFT P2 |
|---|---|---|---|---|---|---|
| PUMP | DEPTH 0–100 % | SHAPE (SUAVE/EXP/KICK) | RATE (1/4, 1/8, 1/2) | OFFSET −50…+50 ms | RELEASE 30–400 ms | SOURCE (AUTO/MIDI/IA/INT) |
| ARP | MODE | DIV | GATE 5–100 % | OCT 1–3 | SWING 50–75 % | LATCH |
| CHORD | TYPE (MEM/KEY) | INV 0–3 | STRUM 0–100 ms | KEY (C … B, mayor / menor) | — | — |
| UNISON | VOICES (×1/×2/×3/×6) | DETUNE 0–50 ¢ | — | — | — | — |
| MACRO | DRIFT | GRIT | POCKET | MORPH | — | — |

**Parámetros:** tres grupos nuevos de `gf_params.h` con la misma forma que MOD (parada · 8 + slot):
`GF_GROUP_GROOVE_A` (8) = PUMP, ARP · `GF_GROUP_GROOVE_B` (9) = CHORD, UNISON · `GF_GROUP_GROOVE_C` (10) = MACRO
(0–7) y encendidos (8 PUMP, 9 ARP, 10 CHORD, 11 UNISON). El param_id sigue siendo
`((0x10 + engine) << 8) | (grupo << 4) | idx`. Los ajustes de GROOVE son **por engine** y van en el preset
(`PRESET_FORMAT` 3: los presets de formato 2 se regeneran de fábrica, como en el cambio anterior).

**UI_STATE:** `page` = 12 y `cursor` = parada. **BPM a la cabeza:** el `BEAT_DETECTED` que ya se manda, ahora con el
BPM del reloj común (cualquier fuente), una vez por segundo y al cambiar.

**PERFORM:** chips de estado PUMP · ARP · CHORD (encendido) y el BPM con su fuente.

## 8. Cómo se prueba

| Qué | Cómo |
|---|---|
| Reloj: BPM y posición con clock MIDI, START, SONG POSITION; TAP; prioridad de fuentes | test native |
| PUMP: cae en el beat, recupera en RELEASE, DEPTH 0 = 1.0, OFFSET corre el ciclo | test native |
| ARP: pasos por división (tresillos y puntillo), swing, gate, octavas, modos, latch | test native |
| CHORD: memoria, transposición, inversión, tríadas en tonalidad | test native |
| Página GROOVE: navegación, toggles, TAP, edición por encoders, CC 14–17, preset ida y vuelta | test native |
| Bombeo en fase con el kick de Ableton, arpegio con swing, stab de acordes | en el hardware |

## 9. Qué no cubre

SPREAD estéreo del unison, MORPH (fase de capas), la fuente IA en el sketch49, ratchets / probabilidad en el ARP,
secuenciador de pasos.
