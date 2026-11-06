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

**Tonalidad (regla de Juan):** todo va a la fundamental (`percusion-kits-808-organica.md` §17). Clave y snare, a su nota; clap y hats, alineando su resonancia principal con la tónica o su tercer armónico.

**Riesgo conocido:** la madera sonó rara en el kick, sobre una base sostenida. Aquí no hay esa base, así que no tendría por qué repetirse, pero está sin oír.

## 4. El rack

Al completar las 4 elecciones, el motor crea `DRUMS <género> <raíz> <modo> <id> R<n>` en `GrooveForge_Library/00 Motor/` y lo pone en `DRUMS`, y borra los racks anteriores del motor en esa pista. Cambiar una elección después crea el siguiente rack: no hace falta volver a elegir todo. Un rack de Juan no coincide con el patrón de nombres y no se toca.

## 5. Escala

Juan cambió a **La lidia dominante** (1 2 3 #4 5 6 b7). Se añade como modo. Cambiar la tonalidad afecta solo a lo que se genere después; lo ya generado no se toca.

## 6. Pendiente de bajo (Juan, 2026-11-06)

Tras oír kick, bajo y rumble: le gusta la mezcla y el ritmo que ya generan. El bajo podría ir menos cargado de notas, o mezclar notas largas y cortas. Entra como nueva familia cuando se vuelva al bajo.

## 7. Patrones (Juan, 2026-11-06)

**Idea de Juan:** en organic, la clave debe interactuar con el kick y el bajo. Por eso el motor **lee esas pistas en Live** cada vez que propone, incluidas sus ediciones, y calcula las claves. Son reglas, sin ML: la relación entre pistas se puede calcular con exactitud y todavía no hay ejemplos suyos de los que aprender.

**Lectura:** un golpe es **fuerte** si aparece en al menos la mitad de los compases. Si aparece menos veces es un **ghost**, y un golpe suelto, como un remate de kick, no cambia el análisis. Un paso **libre** no tiene kick, ni bajo fuerte, ni ghost.

| Opción | Regla | Relación esperada |
|---|---|---|
| **RESPUESTA** | Hasta 4 pasos libres, separados al menos 2 | Conversa en los huecos |
| **SON 3-2** | 0, 3, 6, 10, 12, sin importar el resto | Figura fija que coincide donde coincida; con un bajo en tresillo, en 3 y 6 |
| **CONTRA** | El primer paso libre tras cada golpe fuerte de bajo, a 2 pasos como máximo | Contesta justo después del bajo |

La clave ya no comparte frecuencias con kick y bajo (`Clave` va cerca de 1,76 kHz), así que coincidir no enmascara: es una decisión de ritmo. Si el análisis no deja pasos para una regla, esa opción no se ofrece.

**Base común, para editar:** clap en 2 y 4 (pasos 4 y 12), hat cerrado en las negras y hat abierto en la «y» (2, 6, 10, 14), que el cerrado corta. Es un punto de partida: Juan indicó que creará bases propias por género, y sustituirán a esta.

**Frase:** 16 compases, como kick y bajo. Acentos leves por compás y un golpe de snare de remate cada 4. En `DRUMS` rige la regla de no pisar lo editado: si el clip cambió, la propuesta va a `DRUMS propuesta`, que **no tendrá rack** hasta que se le cargue uno.

## 8. Cómo se valida

**Tests:** notas del mapa, choke solo en hats, hats con la misma semilla, opciones distintas entre sí, afinación de clave y snare. **Lo que decide es Juan:** si el rack le sirve como punto de partida.
