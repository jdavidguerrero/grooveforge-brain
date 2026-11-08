# Motor: armado del arreglo

> **Fecha:** 2026-11-08. **Estado:** aprobado por Juan el 2026-11-08 («me parece buena opción… llenar y pues yo organizo después»); primera entrega en prueba. **Amplía:** `motor-asistencia-v2.md` §4, paso 7. **Base:** `motor-transiciones.md` (lectura de `ARRANGE`), eventos de presencia del estudio `est-acordes` (2026-10-29). **No modifica** `00–06`.

## 1. Objetivo

Repartir los materiales del set por los bloques de `ARRANGE`: cada elemento suena donde corresponde, entra y sale por capas, y el segundo drop crece. Es un **borrador para que Juan organice después**: el motor llena, Juan corta, mueve y decide.

## 2. Material

Cada pista con un clip en el compás 1 aporta su material: las notas de los primeros 16 compases (o del largo de su clip, si es más corto, como `SC GHOST`). Antes de llenar, el motor **registra el set** (registro con todas las notas y MIDI exportados) y guarda el material en su estado: si el clip del compás 1 se borra, no se pierde.

No se tocan: `ARRANGE`, las pistas de transiciones (`ROLL`, `ROLL TRIBE`, `RISER`, `FILL`, `EAR CANDY`, `FX`, `SUBDROP`), que ya están en su sitio, ni las pistas sin notas.

## 3. Cuadrícula de presencia

Filas: los elementos. Columnas: las secciones del arreglo, en orden. Cada casilla es **todo**, **nada** o **parcial**, y Juan la cambia con un clic. **Parcial** significa:

| En | Parcial es |
|---|---|
| INTRO | **Entra escalonado**: cada elemento entra en su turno, cada 8 compases |
| OUTRO | **Sale escalonado**: cada elemento sale en su turno, cada 8 compases |
| Otras | La primera mitad de la sección |

**Propuesta inicial** (convenciones de club y el estudio de acordes de Juan: bajo fuera de intro y break, acordes en todo, pluck solo en el drop):

| Elemento | INTRO | BUILD | DROP | BREAK | INTER | OUTRO |
|---|---|---|---|---|---|---|
| KICK, SC GHOST | entra (turno 1) | todo | todo | nada | nada | sale (turno 4) |
| PERC | entra (2) | todo | todo | todo | nada | sale (3) |
| DRUMS | entra (3) | todo | todo | nada | nada | sale (2) |
| TOP | entra (4) | todo | todo | nada | nada | sale (1) |
| BASS | nada | nada | todo | nada | nada | parcial |
| LEAD | nada | nada | todo | todo | nada | nada |
| CHORDS | nada | todo | todo | nada | nada | nada |
| CHORDS 2, PAD | todo | nada | nada | todo | nada | todo |
| ATMOS | todo | nada | nada | todo | todo | todo |
| AUX LEAD, STABS, PLUCKS | nada | nada | **solo desde el segundo drop** | nada | nada | nada |
| Otra pista | nada | nada | todo | nada | nada | nada |

Un nombre parecido usa la fila de su prefijo (`PAD 2` → `PAD`); `CHORDS SUS` (los acordes largos de Juan) va como `CHORDS 2`.

El segundo drop suma AUX LEAD, stabs y plucks: la pista crece en vez de repetirse.

## 4. Llenar

Para cada casilla con presencia, el material se repite desde el inicio del tramo, en clips del largo del material, y el último se corta en el final del tramo. Si el elemento no va en el compás 1, el clip original se **borra** (ya está guardado). Los clips se llaman `<ELEMENTO> · <SECCIÓN>`.

**Lo de Juan no se toca:** el motor recuerda los clips que escribió. Un tramo donde hay un clip que no es suyo se salta y se informa. Volver a llenar reemplaza solo lo que escribió el motor.

## 5. Puente

`GFMotor` versión 3 agrega `delete_clip` (borrar un clip del arreglo por su inicio, con `Track.delete_clip`). Requiere reiniciar Live, junto con `load_plugin` de la versión 2.

## 6. Cómo se valida

**Tests:** presencia por defecto, entradas y salidas escalonadas, segundo drop, cortes en el final de cada tramo, clips del largo del material, borrado del clip original, regla de no pisar y registro previo. **Lo que decide es Juan.**

## 7. No cubre

Automatizaciones (siguiente paso: envolventes de clip sobre los parámetros del VST, ver §8), niveles por rol y variaciones del material por sección.

## 8. Automatizaciones: lo que se puede

- **El VST expone a Live** cutoff, resonancia, ataque, release y las **macros por motor** (DRIFT en HYBRID, GRIT, POCKET, MOVE, GRIT RES, GRIT MOD, POCKET VEL): se automatizan como cualquier parámetro.
- **Desde el motor:** la API de Live no escribe la automatización de la pista en el arreglo, pero sí **envolventes de clip** sobre un parámetro de un dispositivo de la pista. Como `GFMotor` arma cada clip en Session y lo duplica al arreglo, la envolvente viaja con el clip. Así se podrían hacer el barrido de filtro del build o el MOVE del riser. **Sin probar en Live**: es la propuesta para la siguiente entrega.
