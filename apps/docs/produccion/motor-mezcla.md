# Motor: mezcla de partida

> **Fecha:** 2026-11-08. **Estado:** aprobado por Juan el 2026-11-08 («inicia con los volúmenes… y la idea es tener el audio rack por rol»); primera entrega en prueba (capas 1 y 2). **Amplía:** `motor-asistencia-v2.md` §4, paso 7 (niveles por rol), regla 6. **Base:** `template-envios-valhalla.md` §5, `convenciones-workspace.md`. **No modifica** `00–06`.

## 1. Objetivo

Un punto de partida de mezcla por rol, para que Juan empiece a mezclar desde algo ordenado y no desde todos los faders a 0 dB. **No es una mezcla:** los niveles dependen de cuánto sale cada instrumento, y eso el motor no lo mide todavía. GrooveScope (The Room, modelo de Gibson) sirve para **ver** el resultado; no decide.

## 2. Capa 1: fader, paneo y envíos por rol

El rol se reconoce por el nombre (`convenciones-workspace.md`: familia y variante; un nombre parecido usa su prefijo).

| Rol | Fader | Paneo | Envíos (A Office · B Plate · C Hall · D Rumble · E Delay · F Echo) |
|---|---|---|---|
| KICK | −3 dB | C | D −18 |
| BASS SUB | −6 dB | C | — |
| BASS, BASS MID | −8 dB | C | — |
| DRUMS | −10 dB | C | A −20 · B −24 |
| PERC | −12 dB | 25 % izq. | A −16 · C −28 · E −28 |
| TOP | −16 dB | 30 % der. | A −18 |
| LEAD MAIN (LEAD, MELODY) | −9 dB | C | B −18 · E −24 · F −28 |
| LEAD AUX | −13 dB | 15 % der. | B −18 · E −24 · F −28 |
| CHORDS PLK (CHORDS) | −14 dB | 10 % izq. | B −20 · E −20 |
| CHORDS SUS (CHORDS 2) | −15 dB | C | C −20 |
| STABS | −14 dB | 20 % izq. | B −20 · E −20 |
| PLUCKS | −15 dB | 20 % der. | B −20 · E −20 |
| PAD | −18 dB | C | C −16 |
| ATMOS | −20 dB | C | C −16 |
| RISER, FX | −16 dB | C | C −20 |
| EAR CANDY | −16 dB | C | F −24 |
| ROLL | −12 dB | C | A −20 |
| ROLL TRIBE, FILL | −14 dB | 20 % izq. / der. | A −20 |
| SUBDROP | −8 dB | C | — |

**Por qué así:** el grave (kick, sub, bajo) va al centro y arriba: es la base y debe ser mono (Gibson, *The Art of Mixing*, 2.ª ed., 2005: volumen = adelante, paneo = lado a lado). Lo rítmico agudo se abre a los lados en pares opuestos (PERC izquierda, TOP derecha; stabs izquierda, plucks derecha), y lo que sostiene va atrás: menos fader y más Hall. Los envíos son los de `template-envios-valhalla.md` §5; los roles nuevos (riser, FX, ear candy, roll) siguen la regla de sus vecinos.

**No se tocan:** `ARRANGE`, `BRAIN` y `SC GHOST` (su salida es «Sends Only», con todo al mínimo, por diseño).

**Regla 6 (lo que movió Juan no se toca):** el motor recuerda lo que escribió. Una pista cuyo fader o paneo ya no coincide con eso, o que la primera vez no está en el valor por defecto de Live (fader a 0 dB y paneo al centro), se salta y se informa. Lo mismo vale para cada envío por separado (por defecto, −∞).

**Retornos:** se toman por orden, A a F. Si el set tiene menos de seis, los envíos que faltan se saltan y se informa.

## 3. Capa 2: cadenas de volumen en los racks del motor

Juan mezcla los drums con las cadenas del propio rack. Los racks que genera el motor nacen con su volumen de cadena:

| Rack | Cadenas |
|---|---|
| DRUMS | clave −4 · snare −3 · clap −2 · hat cerrado −10 · hat abierto −12 dB |
| ROLL | snare 0 · clap −3 dB |
| KICK, SUBDROP | 0 dB |

Los tops de Tribe no lo necesitan: la dinámica viene en el velocity del MIDI.

## 4. Capa 3 (siguiente): cadena de efectos por rol

Un Audio Effect Rack por rol con dispositivos de Live: **EQ Eight → Compressor → Saturator → Utility**, con macros (corte grave, corte agudo, compresión, drive, ancho). El motor lo clona desde una plantilla que Juan guarda una vez (como el drum rack) y ajusta los valores por rol. **Por qué los de Live:** sus parámetros están en el `.adg` y el motor los puede escribir; el estado de un plugin como FabFilter es un bloque cerrado. FabFilter sigue disponible para la mezcla final, encima.

## 5. Puente

`GFMotor` versión 4: `get_mixer` (fader, paneo y envíos de cada pista, y nombres de los retornos) y `set_mixer`. Live guarda el fader como un valor de 0 a 1; el script busca el valor cuyo texto («−6.0 dB») corresponde al pedido. Requiere reiniciar Live.

## 6. Cómo se valida

**Tests:** rol por nombre, tabla completa, regla 6 (fader movido, primera vez fuera del valor por defecto, envío propio), retornos faltantes, cadenas del rack en el `.adg`, conversión de dB del script. **Lo que decide es Juan.**
