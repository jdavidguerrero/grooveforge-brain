# Motor: transiciones (roll, riser, fill y ear candy)

> **Fecha:** 2026-11-08. **Estado:** aprobado por Juan el 2026-11-08 («procede con esto y el tema transiciones, fills, roll, risers»); primera entrega en prueba. **Amplía:** `motor-asistencia-v2.md` §4, pasos 2 y 7. **Base:** decisiones del estudio indie (eventos del 2026-10-29), `motor-pads.md`. **No modifica** `00–06`.

## 1. Objetivo

Proponer lo que une las secciones, **en el lugar exacto del arreglo**: el motor lee los bloques de `ARRANGE` y escribe los clips antes de cada llegada. Reglas, sin ML; MIDI editable; nunca se pisa lo editado.

A diferencia de los demás pasos, aquí **no** se escribe un clip de 16 compases en el compás 1: cada elemento va **en todos los puntos** del arreglo donde aplica, en su propia pista.

## 2. Lectura del arreglo

Se leen los clips con nombre de `ARRANGE` (INTRO, BUILD, DROP, BREAK, OUTRO). Una sección termina donde empieza la siguiente con nombre; los clips sin nombre se ignoran. Sin `ARRANGE`, el paso pide crear la guía primero.

| Punto | Qué es |
|---|---|
| **Llegada al drop** | Inicio de cada DROP; la sección anterior es la que sube |
| **Cambio de sección** | Inicio de cualquier sección menos la primera |
| **Break** | Inicio de cada BREAK |

## 3. Elementos

**Roll** (pista `ROLL`, nota del snare del rack de drums, 48). Decisión de Juan en el estudio indie: empieza en negras y pasa a corcheas, semicorcheas y fusas; lo recortó a 4 compases.

| Patrón | Largo antes del drop | Velocidad |
|---|---|---|
| **ROLL 4** | 4 compases, uno por subdivisión | Sube de 50 a 120 |
| **ROLL 8** | 8 compases, dos por subdivisión | Ídem |
| **ROLL 2** | 2 compases, medio por subdivisión | Ídem |

**Riser** (pista `RISER`), durante la sección que sube, hasta 16 compases:

| Patrón | Qué toca | Sonido |
|---|---|---|
| **RISER NOTA** | La tónica sostenida: la subida la pone el preset | GF NOISE RISER, JD AIRE |
| **RISER ESCALERA** | Las notas del acorde de la tónica entran una a una y se sostienen hasta el drop | Un pad o un pluck largo |
| **RISER OCTAVAS** | La tónica salta de octava cada cuarto del tramo | Cualquier sonido sostenido |

**Fill** (pista `FILL`, notas del rack de drums), en el último compás antes de cada cambio de sección:

| Patrón | Qué toca |
|---|---|
| **FILL TRESILLO** | Clap en 3-3-2 que sube de fuerza |
| **FILL ÚLTIMO TIEMPO** | Snare en semicorcheas en el último tiempo y clap en la última |

**Ear candy** (pista `EAR CANDY`), golpes sueltos que se recuerdan:

| Patrón | Dónde | Qué toca |
|---|---|---|
| **IMPACTO** | Primer tiempo de cada drop, 2 compases | La tónica grave (sub drop) |
| **STAB ANTES DEL DROP** | La «y» del tercer tiempo del último compás antes del drop, y después silencio | Tríada de la tónica |
| **GOTA EN EL BREAK** | Inicio de cada break y su mitad | Tónica y quinta agudas, largas, para un eco |

Todo lo que tiene nota va a la tónica o al acorde de la tónica, por la regla de Juan.

## 4. Regla de no pisar

Cada elemento recuerda los clips que escribió. Si Juan editó o agregó algo en esa pista, la opción siguiente va a `<PISTA> propuesta`.

## 5. Sonido y mezcla

Roll y fill: una copia del rack de `DRUMS` o el snare de Tribe en la pista. Riser: `presets/pads/` (GF NOISE RISER, JD AIRE). Ear candy: stabs y plucks de `presets/stabs-plucks/`, con el envío **F · Echo**.

## 6. Cómo se valida

**Tests:** lectura de secciones, posición exacta antes de cada llegada, subdivisiones y crescendo del roll, notas en la tónica, regla de no pisar con varios clips. **Lo que decide es Juan.**

## 7. No cubre

Automatización (filtro, volumen, envíos) y efectos sobre audio, como el reverse: `GFMotor` aún no los escribe.
