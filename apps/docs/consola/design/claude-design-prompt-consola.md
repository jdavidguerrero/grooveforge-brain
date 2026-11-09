# Prompt para Claude Design — Consola GrooveForge (UI completa)

> **Cómo usarlo:** copiar todo lo que está debajo de la línea en Claude Design.
> Adjuntar como referencia: capturas de **Ableton Push 3** y **Akai MPC Live 3**; `apps/studio-head/src/ui/theme.h` (tokens del Brain); y, si se quiere, `~/Downloads/GrooveForge Studio UI (standalone).html` (diseño de la cabeza Studio del Brain).
> Fuente de las decisiones: `apps/docs/consola/07-gui.md`. Última actualización: 2026-11-09.

---

## Rol y objetivo

Eres diseñador o diseñadora de interfaces para **instrumentos musicales de hardware**. Vas a diseñar la UI completa de la **Consola GrooveForge**: un instrumento standalone tipo Ableton Push 3 / Akai MPC Live 3 con un **motor de IA local** que genera música en el estilo de su creador (house orgánico, indie y afro, 122–126 BPM). Su creador la usa para **producir más rápido** y para **tocar sets en vivo**, y la muestra en video. Tiene que verse **profesional, con carácter y nada genérica**: un instrumento que da ganas de tocar, no una app.

Diseña **todas las vistas** de este documento. El primer desarrollo implementará solo las marcadas **MVP**, pero el sistema visual tiene que cubrir el conjunto.

## Contexto del producto

- **Dos modos**, como una familia de Push:
  - **CONTROLADOR**: clon de Push 3 conectado a Ableton Live (rejilla de clips, notas, dispositivos, mezcla). También controla el motor de IA que corre en el Mac (modo «estudio»).
  - **STANDALONE**: sin computador. El motor genera las **capas** de cada track (kick, drums, perc, top, bajo, acordes, lead, pad) y el músico las toca como un **DJ de música generada**: escucha la siguiente opción por audífonos (**cue**), la **acepta** y entra al inicio de la siguiente frase de 8 compases. Siempre hay **dos tracks**: el que suena (**SALA**) y el **SIGUIENTE**. La transición entre tracks se hace **por capas** (intercambio, por el break o corte en la frase). Hay un **mixer interno** (volumen por grupo, ecualizador aislador de 3 bandas, filtro de una perilla, envíos a echo y reverb, cue por canal).
- También tiene un **synth propio** (el «Brain»: 6 motores inspirados en Moog Model D, Juno-106, Prophet-5, OB-6, DX7 y ARP 2600, más capas HYBRID que combinan dos) y una **drum machine** que genera sonidos afinados a la tónica.

## Hardware y rendering

- **Pantalla:** 7" DSI, **1280×720 (16:9)**, touch capacitivo de 10 puntos. Brillo alto, se ve en escenario oscuro y en estudio.
- **Render:** Qt 6 / QML en Raspberry Pi 5 (GPU). Se permiten degradados suaves y animación fluida a 60 fps, pero con criterio: la pantalla se mira **menos de un segundo** con las manos en los pads. Nada de efectos que distraigan.
- **Nada rebota.** Movimiento corto y con propósito (≤ 200 ms en cambios de vista; el playhead y los medidores, continuos).
- **Sin modo claro.** Siempre oscuro.

## Controles físicos (la UI se maneja PRIMERO con estos)

| Control | Cantidad | Relación con la pantalla |
|---|---|---|
| **Encoders** bajo la pantalla | 8, con clic | La **barra inferior de 8 teclas** muestra qué hace cada uno; su celda se ilumina al girarlo |
| **Pads RGB** (NeoTrellis) | 8 columnas × 4 filas | **Mismo color y mismo orden que la pantalla**: las 8 columnas son las 8 columnas de la rejilla |
| **Pads con fuerza** (FSR) | 4 | Golpes con velocity: kick, perc, golpes de percusión |
| **Expresión** (FSR) | 2 | Presión continua: filtro, envíos |
| **Sensores de mano** (IR, 4–30 cm) | 2 | Tipo theremin: filtro, envíos, riser o notas en la escala. En pantalla, un **halo** en el borde izquierdo y derecho |
| **Faders** | 4 | Volumen por grupo: RITMO, BAJO, MÚSICA, FX (con banco para los otros 4) |
| **Perillas** | 3 | MASTER (sala), CUE (audífonos; con Shift = mezcla de cue), IN (entrada) |
| **Botones con LED** (Choc) | 20 | Transporte (PLAY, STOP, REC, TAP) · modo y navegación (MODO, SHIFT, ◀ ▶ ▲ ▼, ATRÁS) · motor (CUE, ACEPTAR, SALTAR, GENERAR, BUILD, DROP, BREAK) · edición (DUPLICAR, BORRAR, DESHACER, FILL) |

**El touch es auxiliar:** menús, elegir algo puntual, buscar y escribir. **La música no se toca en la pantalla.** Todo flujo de directo tiene que poder hacerse sin tocarla.

## Design system

**Parte de la identidad del Brain** (respeta estos hex; puedes extender la escala):

| Token | Hex | Uso |
|---|---|---|
| Fondo | `#0A0A0A` | Base |
| Superficies | `#141414`, `#1C1C1C` · líneas `#242424` | Paneles, celdas |
| Verde azulado | `#1D9E75` · claro `#5DCAA5` · oscuro `#0E5040` | **Lo que suena / lo que tocas** |
| Violeta | `#534AB7` · claro `#7A6FE0` · oscuro `#2B2758` | **Lo que el motor propone o genera** |
| Azul HYBRID | `#1E90C0` · claro `#6CC4EA` · oscuro `#0E3F55` | Capas HYBRID del Brain |
| Texto | `#F2F2F2` · secundario teñido `#9FE1CB` · apagado `#5A5A6E` | |
| Estado | rojo `#E84A4A` · ámbar `#E8B84A` · naranja `#FF6A35` | Clip, cerca del límite, grabando |

**Lo que pedimos que propongas (esto es lo «arriesgado»):**
1. **Un color por capa / rol**, como los colores de pista de Ableton, que sea el mismo en pantalla, pads y LEDs: KICK, DRUMS, PERC, TOP, BASS, CHORDS, LEAD, PAD (y FX). Vivos, distinguibles de lejos y en daltonismo común, y que convivan con verde azulado = suena y violeta = propuesta del motor (por ejemplo: el color del rol en el relleno; el estado en el borde o en la intensidad).
2. **Tipografía con carácter:** IBM Plex Mono para valores y datos, más **una fuente de display** para números grandes, nombres de modo y la cuenta de frase. Que no se vea genérica.
3. **Botones y teclas como hardware:** las 8 teclas inferiores tienen que leerse como teclas físicas iluminadas (color del rol, estado encendido o apagado), no como una barra de texto plano.
4. **La marca GroovePilot** (verde `#00D4AA`, violeta `#8B5CF6`, fondo `#09090B`, General Sans + JetBrains Mono) solo en arranque, menús y ajustes: el «sistema operativo», distinto de la música.
5. Escala de espaciado de 4 px, radios contenidos, objetivos táctiles de al menos 48 px.

## Datos disponibles para la UI (no inventes otros)

Tonalidad y modo · BPM y estado de Ableton Link · compás, frase (n de 8) y sección (INTRO, BUILD, DROP, BREAK, INTER, OUTRO) · por capa: nombre, color, nivel, si suena, patrón (notas por paso), sonido o preset, opciones del motor (patrón + sonido + energía) · track siguiente: origen (generado o de la biblioteca), compatibilidad de tonalidad y BPM · transición: tipo y paso · mixer: volumen, ecualizador de 3 bandas, filtro, envíos, cue, medidores por grupo y master · sensores: distancia de cada mano, destino, rango · conexiones: Brain, Link, Live, red · en CONTROLADOR: pistas, clips, escenas, colores y dispositivos de Live.

## Pantallas a diseñar (1280×720, todas)

**Estructura común:** barra superior (modo, tonalidad, BPM, frase, conexiones) · contenido · **barra inferior de 8 teclas** (encoders). Ventanas encima para TRANSICIÓN, ESCALA, REPETIR, TEMPO y avisos.

### Comunes
1. **Arranque** (MVP) — marca GroovePilot / GrooveForge.
2. **Selector de modo** (MVP) — CONTROLADOR o STANDALONE, con el estado de conexiones.
3. **Avisos** (MVP) — mensajes cortos encima de todo: «BASS entra en la frase», «Link: 3 equipos».
4. **Ajustes** (MVP mínimo) — Audio (salidas, cue, niveles) · Conexiones (Ethernet, Link, USB, MIDI) · Superficie (brillo de LEDs, calibración de pads de fuerza y sensores de mano) · Sistema (temperatura, disco, apagado).
5. **Buscar / Browser** — sonidos, presets, samples y tracks, con **teclado en pantalla**.

### CONTROLADOR (clon de Push 3 con Live)
6. **SESSION** — rejilla de clips (8 pistas × escenas), color de pista, miniatura de forma de onda por clip, clip sonando y en cola.
7. **NOTE** — pads como teclado en escala o como drum rack; escala y raíz visibles.
8. **CLIP** — editar notas de un clip de Live: pasos y piano roll.
9. **DEVICE** — 8 parámetros por página como arcos grandes; cadena de dispositivos.
10. **MIX** — volumen, paneo y envíos por pista; medidores.
11. **BROWSE** — el navegador de Live.
12. **ESCALA · REPETIR · TEMPO** — ventanas encima.
13. **MOTOR** — el motor de IA del Mac: sus pasos (Partida, Arrangement, Kick, Bajo, Drums, Tops, Motivo, Acordes, Pads, Stabs, Transiciones, Armado, Mezcla, Ideas) y sus opciones, para elegir sin tocar el Mac.

### STANDALONE (estilo Push 3 standalone / MPC Live 3)
14. **LIVE · vista SESIÓN** (MVP, la principal) — **rejilla**: columnas = capas (en el color de su rol), filas = **SALA**, **SIGUIENTE** y opciones del motor; qué suena, qué está en cue, qué entra en la próxima frase. Cuenta de frase grande.
15. **LIVE · vista ARREGLO** — los **«ríos de capas»**: un carril por capa con su forma de onda corriendo, una línea AHORA, el track siguiente entrando desde la derecha, un **anillo de frase** y la **curva de energía** del set arriba.
16. **CAPA** (MVP) — al elegir una capa: sus opciones del motor como cartas (mini piano roll, sonido, energía), la que está en cue, aceptar o pedir otra.
17. **TRANSICIÓN** (MVP, ventana) — elegir intercambio, break o corte; progreso paso a paso; «el motor lo hace, si tocas un fader mandas tú».
18. **MIXER** (MVP) — tiras por grupo y canal: volumen, ecualizador aislador de 3 bandas, filtro de una perilla (curva que se dobla), envíos, cue por canal, medidores.
19. **MANOS** (MVP) — los dos sensores: qué controla cada uno, rango, escala del modo theremin, y la mano en vivo.
20. **SECUENCIADOR** — dos vistas, como en Push; se cambia entre ellas con una tecla de la barra:
    - **PASOS**: pasos por carril, velocity, probabilidad y microtiming por paso; el motor rellena o varía. Para batería y percusión.
    - **PIANO ROLL**: para bajo, lead, acordes y arpegios. La rejilla de pads muestra una ventana del piano roll (8 pasos × 4 notas) y la pantalla dibuja esa ventana resaltada dentro de un rango más amplio. Tocar un pad pone o quita una nota; los encoders mueven la nota seleccionada, su largo, velocity y microtiming. **El touch solo navega** (zoom, desplazarse, elegir rango de notas y compases); no dibuja ni borra notas. Las notas que puso el motor se ven en otro tono que las de Juan, y el motor nunca pisa una nota que Juan editó. Teclado lateral con la escala activa resaltada.
21. **PADS** — tocar en la escala con los pads y los pads de fuerza.
22. **BRAIN** — páginas del synth: motor (selector con ilustración de cada uno), osciladores, filtro, envolventes y LFO, FX, capas HYBRID, presets.
23. **KIT** — drum machine: materiales (madera, piel, 808), afinación, generar sonidos.
24. **BIBLIOTECA / SETS** — tracks del estudio cargados por capas, listas de set.
25. **REGISTRO** — lo que se aceptó en el set, para exportar a Live.
26. **RUTEO** (modo híbrido) — qué capa suena en la consola y cuál en Live, estado de Link y MIDI.

Para cada pantalla: el estado normal y, donde aplique, vacío, cargando y sin conexión.

## Qué NO hacer

- No diseñar para tocar música con el dedo en la pantalla.
- No usar el violeta para nada que no venga del motor (transporte, medidores y navegación nunca son violeta).
- No pintar el espectro ni los medidores de rojo «para asustar»: el problema se comunica con un indicador, no tiñendo todo.
- No llenar de texto: lo principal se lee a un brazo de distancia.
- No copiar la apariencia exacta de Push ni de MPC: son referencias de flujo, no de marca.

## Entregables

1. **Página de design system:** colores (incluida la paleta por rol), tipografía (mono + display), espaciado, componentes: tecla inferior, celda de rejilla en todos sus estados, tira del mixer, carta de opción, anillo de frase, halo de mano, aviso, ventana, barra superior.
2. **Las 26 pantallas** a 1280×720, con las **9 del MVP** primero y con más detalle.
3. Para las vistas principales (LIVE · SESIÓN, LIVE · ARREGLO, MIXER, SESSION de Live): una secuencia corta que muestre **una transición por capas** de principio a fin.
4. Notas de layout por pantalla (medidas y jerarquía) para implementarlas en QML.
