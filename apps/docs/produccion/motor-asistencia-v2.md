# Motor de asistencia v2: spec del flujo con Ableton Live

> **Fecha:** 2026-11-05. **Estado:** borrador para revisión de Juan; sin implementar.
> **Amplía:** `motor-asistencia.md`, cuya v1 no compone, no genera audio y no automatiza Live. El registro de decisiones y las
> reglas de audición se conservan. **No modifica** `00–06`.

## 1. Principio

> **El sistema hace lo repetitivo. El criterio creativo es de Juan.**

No es una IA que hace música por él. Genera puntos de partida editables, y lo que hace que un track sea suyo son las decisiones
que él toma al elegir, cambiar notas y cambiar sonidos.

## 2. Dónde está el límite

| El sistema lo hace | Juan lo decide |
|---|---|
| Crear la pista guía del arrangement según el género | Duración y orden final de las secciones |
| Crear pistas, poner tempo y fijar niveles iniciales | La mezcla y cualquier nivel definitivo |
| Generar sonidos y sus variantes | Qué sonido entra y cuál se descarta |
| Crear borradores MIDI de batería, bajo y demás partes | Cada nota: cambiar, quitar o reescribir |
| Preparar hasta 3 opciones y recordar qué se eligió | Qué opción gana, o conservar lo que ya hay |

## 3. Reglas que el motor no puede romper

1. **Sin botón de track completo.** Se trabaja una parte a la vez, y solo cuando Juan la pide.
2. **Todo es borrador editable:** MIDI y one-shots, nunca audio final mezclado.
3. **Nunca se sobrescribe lo que Juan editó.** Si un clip cambió desde que se generó, una nueva propuesta va en otro clip o en otra pista.
4. **No avanza solo.** Cada paso termina en una decisión de Juan.
5. **Nada se sube ni se envía fuera del Mac.**
6. **Automatizar no es decidir:** los niveles iniciales son un punto de partida, y una vez que Juan mueve un fader, el motor no lo vuelve a tocar.
7. **No crea ni modifica archivos `.als`.** Trabaja sobre el set abierto, y Juan decide cuándo guardar.

## 4. Flujo

| Paso | Automático | Criterio de Juan |
|---|---|---|
| 1. Partida | Propone BPM y duración según el género | Género, tonalidad y BPM |
| 2. Arrangement | Pista MIDI con bloques (intro, break, build, drop…) y presencia por sección | Mueve, alarga y elimina bloques |
| 3. Kick | Genera opciones afinadas | Elige y ajusta |
| 4. Percusión | Kit por género y borradores MIDI | Elige sonidos y edita notas |
| 5. Bajo | Borradores MIDI de sub y mid; presets del Brain | Elige, edita y toca |
| 6. Armonía y detalles | Borradores de chords, stabs, plucks, lead y ear candy | Elige, edita y descarta |
| 7. Armado | Pone niveles iniciales por rol; registra | Construye el arrangement final |

El kit de sonidos ya existe en `tools/perc-synth/`, y los patrones en `make_perc_patterns.py` y `bajo-groove.md`. Las plantillas de
arrangement deben salir de cómo trabaja Juan, a partir de un ejemplo suyo.

## 5. Arquitectura

```text
Interfaz web local ──► Motor local (Python) ──► Puente con Live ──► set abierto
                              │
                              └──► carpeta de sonidos visible en el navegador de Live
```

| Pieza | Responsabilidad | Origen |
|---|---|---|
| Motor | Generar sonidos y MIDI, catálogo, registro de eventos | `perc-synth`, `paleta-lab`, `gf_log.py` |
| Interfaz | Pasos, opciones, audición rápida y correcciones con palabras | Nueva; web local |
| Puente | Crear pistas, poner tempo, escribir y leer clips, poner niveles | Copia parcial del Remote Script de GroovePilot |

**Un device M4L** con `jweb` puede llevar la interfaz dentro de Live más adelante, sin rehacerla. **Del plugin de GroovePilot** no se reutiliza
nada en esta etapa: depende de nube y prohíbe guardar audio. `mix_analysis` queda para una etapa de mezcla.

## 6. Lo que Juan cambia es el dato más valioso

Cada borrador queda guardado. Cuando Juan lo edita, el motor lee el clip y registra qué cambió:

```text
borrador generado ──► edición de Juan ──► diferencia: notas movidas, quitadas, añadidas; sonido cambiado
```

Esa diferencia documenta su criterio con más detalle que una calificación, y con ella la IA local podría proponer **puntos de partida**
más cercanos a su gusto. No autoriza a decidir por él. El registro se guarda solo en local.

## 7. Primera entrega

**Elegir `organic`, La menor, 122 BPM → pista guía en Live → borrador de kick → 3 kicks → oírlos → enviar uno → editar → queda registrado.**

Criterios de aceptación:

1. El tempo y la pista guía aparecen en el set abierto.
2. Un Cmd+Z de Live puede revertir lo que el motor añadió, o se documenta cuándo no.
3. Un clip que Juan editó no se sobrescribe al pedir otra propuesta (test).
4. Cerrar el motor no afecta a Live ni a la reproducción.
5. El registro guarda contexto, opciones, elección y diferencias de notas.
6. **Juan confirma que le ahorró tiempo** frente a hacerlo a mano. Este criterio decide si se continúa.

## 8. Template de Live con envíos

**Decisión de Juan:** los niveles parten de un template con envíos de reverbs, echoes y delays.

Juan crea el template a mano una vez y lo guarda como set. Así se respeta la regla 7, y los plugins de terceros quedan cargados y con su
preset exacto. El motor no carga reverbs: al crear una pista, pone su nivel y sus envíos según el rol.

| Retorno | Efecto de partida | Roles que lo usan |
|---|---|---|
| A · Office | VintageVerb, Ambience | Hats, shakers, percusión corta |
| B · Plate | VintageVerb, Plate | Melodías, marimba, leads |
| C · Hall | VintageVerb, Concert Hall | Pads, atmósferas, arpegios |
| D · Rumble | VintageVerb, Dirty Hall | Solo kick y, si hace falta, bajo |
| E · Delay | ValhallaDelay, HiFi, 1/8 con puntillo, PingPong | Plucks, stabs, percusión aguda |
| F · Echo | ValhallaDelay, Tape, 1/4 | Golpes puntuales, voces, leads |

Los envíos por rol empiezan bajos y son una **tabla editable**, como los perfiles de género. Una vez que Juan mueve un envío, el motor no lo
toca (regla 6).

El template también trae las pistas `KICK` y `SC GHOST`, la pista MIDI muda que dispara el sidechain con notas cortas (técnica de Juan). `SC GHOST`
ya viene con un patrón a negras de Juan. Si un borrador de kick es a negras, el motor no toca esa pista. Si tiene otros golpes, o Juan edita el kick después,
el motor **propone** cambiarla y no cambia nada sin su decisión (reglas 3 y 4).

Valores exactos, rumble, pista fantasma y envíos de partida: `template-envios-valhalla.md`.

## 9. Puente con Live: estado

**Juan confirma que el Remote Script está probado:** lo creó y lo usa para GroovePilot y su clon de Push. Por eso ya no se plantea una prueba de
viabilidad general. Su código ya trae comandos para crear pistas MIDI, poner tempo, escribir y leer clips, pasos de undo y parámetros
(`set_parameter`, `set_property`, `call_method`).

Se comprueba al implementar, sin bloquear el spec:

- Escribir volúmenes y envíos desde el motor, con los comandos genéricos o uno nuevo.
- Cargar samples o Drum Racks automáticamente, a partir del `BrowserManager` de PushClone. Mientras tanto, se arrastran desde el navegador de Live.

## 10. Entradas de Juan

| Tema | Estado |
|---|---|
| Niveles | **Decidido:** template con envíos (§8) |
| Delay y echo | **Primera propuesta:** `template-envios-valhalla.md`, para corregir de oído |
| Ejemplos de arrangement | Pendiente: Juan los revisa |
| Borradores MIDI | **Decidido:** bases de drums por género creadas por Juan, más los patrones del laboratorio |
