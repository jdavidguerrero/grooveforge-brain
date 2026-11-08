# Motor: stabs y plucks

> **Fecha:** 2026-11-08. **Estado:** aprobado por Juan el 2026-11-08 («sí me parece… procede»); primera entrega en prueba. **Amplía:** `motor-asistencia-v2.md` §4, paso 6 (stabs y plucks). **Base:** `motor-acordes.md`, `motor-pads.md`. **No modifica** `00–06`.

## 1. Objetivo

Sumar el detalle armónico y rítmico que falta entre los acordes y el lead, **sin duplicar lo que ya hace otra pista**. Reglas, sin ML; MIDI editable; clip de 16 compases; nunca se pisa lo editado.

| Parte | Qué es | Es armonía o melodía |
|---|---|---|
| **Stab** | Un acorde entero, corto y acentuado. Pocos golpes, suele ser el gancho | Armonía con función rítmica |
| **Pluck arpegio** | El acorde tocado nota a nota, rápido | Armonía en movimiento |
| **Pluck riff** | Una frase corta que se repite y responde al lead | Melodía: contramelodía |

## 2. Lectura: qué papel ya está cubierto

Juan (2026-11-08): si los acordes ya están punteados, el motor debe decir que ese papel no hace falta. **Avisa, no prohíbe:** la decisión sigue siendo suya.

| Se lee | Cómo | Aviso |
|---|---|---|
| Pistas `CHORDS*` | Duración media de nota y golpes por compás | Si alguna es **punteada** (duración media ≤ 1 tiempo): «tu `CHORDS` ya hace el papel de pluck; un arpegio lo duplicaría». Si además marca 4 golpes o más por compás: «un stab solo suma si es escaso» |
| `LEAD` (o `MELODY`, `MOTIVO`) | Huecos: pasos de semicorchea en un ciclo de 2 compases donde el lead calla en al menos 3 de cada 4 vueltas | Si quedan menos del 30 % de pasos libres: «tu lead deja pocos huecos para un riff» |
| `KICK` | Golpes fuertes | Los stabs no caen en el kick |

**Pregunta y respuesta:** stabs y riff caen **donde el lead calla**. Si suenan a la vez, compiten; si se alternan, conversan (Butler, *Unlocking the Groove*, 2006, sobre capas que se reparten el espacio rítmico).

## 3. Patrones

**Stabs** (pista `STABS`, tríada en Do4–Do5, golpe de 0,3 tiempos):

| Patrón | Dónde cae |
|---|---|
| **GANCHO** | Una figura fija de 2 compases con hasta 3 golpes en los huecos del lead, fuera del kick, prefiriendo la «y» y las síncopas. Repetida igual: así se recuerda |
| **RESPUESTA** | Un golpe en el primer hueco después de cada frase del lead, como máximo uno cada 2 compases |
| **ANTICIPO** | En la última semicorchea «y» de cada 2 compases, con el acorde que viene |

**Plucks** (pista `PLUCKS`, por encima de `CHORDS 2`, MIDI 64–84):

| Patrón | Qué toca |
|---|---|
| **ARPEGIO 16** | 1 3 5 8 5 3… en semicorcheas, sobre el acorde de cada compás |
| **ARPEGIO 3-3-2** | Las notas del acorde subiendo, en el 3-3-2 dos veces por compás |
| **RIFF EN HUECOS** | Frase de 2 compases solo en los huecos del lead, con notas del acorde. Con semilla: volver a leer da otra |

## 4. Sonido

Para el VST (`gf_presets.py hacia-vst` → `GrooveForge_Library/presets/stabs-plucks/`): FRENCH STAB, GF BRASS HIT, BRASS STAB, GF FILTER STAB, GF FM DISCO STAB, JD RAP STAB P5; JD PLK WOOD, JD PLK BELL, GF SUNRISE PLUCK, GF TALKING PLUCK, KALIMBA, MARIMBA (también para la capa sobre el lead).

**Mezcla:** stabs y plucks al envío **E · Delay** (`motor-asistencia-v2.md`, tabla de envíos).

## 5. Cómo se valida

**Tests:** notas en la escala, registro, stabs fuera del kick y en huecos del lead, avisos cuando el papel ya está cubierto, semilla del riff, regla de no pisar. **Lo que decide es Juan.**

## 6. No cubre

Stabs o plucks sueltos como ear candy: van en `motor-transiciones.md`.
