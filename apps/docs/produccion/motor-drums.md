# Motor: rack de batería inicial

> **Fecha:** 2026-11-06. **Estado:** primera entrega, en prueba. **Pedido de Juan:** revisar el rack inicial con claves, snare, claps y hats abierto y cerrado. **Base:** `apps/docs/theory/percusion-kits-808-organica.md`, `percusion-groove.md` §3. **No modifica** `00–06`.

## 1. Alcance

Elegir los sonidos del rack y dejarlo puesto en la pista `DRUMS`. **No genera patrones todavía:** primero se revisan los sonidos.

## 2. Voces y notas

Las notas son las del **GF Perc Map** (`percusion-groove.md` §3), así que los patrones de `03 Drums MIDI` deberían sonar sin cambios en las voces que tengan.

| Voz | Nota | Selección |
|---|---|---|
| Clave | 47 | Una opción |
| Snare | 48 | Una opción |
| Clap | 49 | Una opción |
| Hat cerrado | 52 | Se elige junto con el abierto |
| Hat abierto | 53 | Se elige junto con el cerrado |

**Hats como un instrumento.** Un hat real es un solo objeto, y cerrar el pedal apaga lo que sonaba. Por eso comparten semilla de timbre y **grupo de choke** en el rack: el cerrado corta al abierto. Se eligen juntos, de modo que el contador es de **4 elecciones y 5 pads**.

## 3. Opciones

Cada tanda trae tres materiales por voz: **MADERA, PIEL y 808**. Son los modelos de `percusion-kits-808-organica.md` §4, con los que Juan ya aprobó los kits. La tanda 1 usa la versión base; las siguientes varían parámetros con la semilla.

**Tonalidad:** clave y snare se afinan a la tónica o a la quinta según `percusion-generos-tonalidad.md`. El clap y los hats no tienen nota definida.

**Riesgo conocido:** la madera sonó rara en el kick, sobre una base sostenida. Aquí no hay esa base, así que no tendría por qué repetirse, pero está sin oír.

## 4. El rack

Al completar las 4 elecciones, el motor crea `DRUMS <género> <raíz> <modo> <id> R<n>` en `GrooveForge_Library/00 Motor/` y lo pone en `DRUMS`, y borra los racks anteriores del motor en esa pista. Cambiar una elección después crea el siguiente rack: no hace falta volver a elegir todo. Un rack de Juan no coincide con el patrón de nombres y no se toca.

## 5. Escala

Juan cambió a **La lidia dominante** (1 2 3 #4 5 6 b7). Se añade como modo. Cambiar la tonalidad afecta solo a lo que se genere después; lo ya generado no se toca.

## 6. Pendiente de bajo (Juan, 2026-11-06)

Tras oír kick, bajo y rumble: le gusta la mezcla y el ritmo que ya generan. El bajo podría ir menos cargado de notas, o mezclar notas largas y cortas. Entra como nueva familia cuando se vuelva al bajo.

## 7. Cómo se valida

**Tests:** notas del mapa, choke solo en hats, hats con la misma semilla, opciones distintas entre sí, afinación de clave y snare. **Lo que decide es Juan:** si el rack le sirve como punto de partida.
