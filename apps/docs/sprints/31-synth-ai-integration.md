# Sprint 31 — Synth AI Integration

> **Fase:** 3B — UI + Display (continuación de Sprint 30)
> **Estado:** SPEC CERRADO — pendiente de implementación
> **Depende de:** Sprint 30 (`30-navigation-rmx-style.md`) — navegación RMX confirmada y archivada
> **Extiende:** view_07 FX MAIN (arc paradigm) → SYNTH y AI mode
> **Referencias:** `apps/docs/02-bridge-protocol.md §2.9` · `apps/docs/05-fx-architecture.md` · `apps/docs/04-ai-architecture.md §8`

---

## Resumen del sprint

Sprint 30 definió el paradigma de navegación para FX mode (view_07 con arco RMX-style). Este
sprint extiende el sistema de display a los dos modos restantes: SYNTH y AI.

El entregable es una jerarquía de 3 niveles para SYNTH mode (engine list → engine sub-home con
arcos → group view con animaciones por grupo), más el refinamiento de view_10 AI PROCESSING con
control de bypass individual por modelo.

**Este documento es SSoT para la arquitectura de navegación de SYNTH y AI mode.**

---

## 1. Theory

### 1.1 Jerarquía de 3 niveles: profundidad vs descubribilidad

El desafío de diseño de cualquier instrumento con muchos parámetros es el mismo que el de
cualquier menú de aplicación compleja: ¿cuántos niveles de jerarquía son correctos?

Un nivel plano (todos los parámetros en una lista de 12 items) es lo más simple para el
programador pero lo peor para el músico en performance: navegar de LFO_DEPTH a VCO_WAVE requiere
11 giros de encoder. La memoria muscular no ayuda porque la posición de cada parámetro en la
lista plana no tiene correlación física.

Una jerarquía de 4+ niveles (engine → sección → grupo → param → valor) tiene el problema inverso:
el usuario necesita recordar en qué nivel está antes de poder operar. Esto es **modal blindness**,
el mismo problema que Sprint 30 resolvió en FX mode (ver `30-navigation-rmx-style.md §2.1`).

Tognazzini, en *Tog on Interface* (Addison-Wesley, 1992), describe el principio de **spring
controls**: los controles que el usuario usa en performance deben estar siempre accesibles sin
necesidad de navegar hacia ellos. Los controles de edición detallada pueden estar detrás de un
nivel de navegación, pero solo uno.

Jef Raskin (2000, *The Humane Interface*, Addison-Wesley) lo complementa con el concepto de
**locus of attention**: el usuario tiene un solo punto de atención en cualquier momento. Un
sistema de 3 niveles donde el nivel 2 es siempre el "home operacional" (donde se toca en vivo)
y los niveles 1 y 3 son contextos de configuración satisface ambos principios:

```
Nivel 1 — Engine List
  (se visita una vez por sesión: elijo el engine)
       ↓↑
Nivel 2 — Engine Sub-Home  ← LOCUS DE ATENCIÓN EN PERFORMANCE
  (aquí se toca: cutoff + resonance siempre visibles)
       ↓↑
Nivel 3 — Group View
  (se visita cuando quiero editar un parámetro que no es cutoff/res)
```

La asimetría es deliberada: el nivel 2 es el home permanente, no el nivel 1. Un encoder click
en ENC L o ENC R desde el nivel 2 sube al nivel 1 (retrocede a la lista de engines). Un click
de ENC NAV en el nivel 2 baja al nivel 3 (accede a los grupos de parámetros).

Esta asimetría viene de la observación del flujo real de uso: el músico elige su engine al
principio del set, luego opera en el nivel 2 durante el resto. El acceso al nivel 3 (edición
de OSC, ENV, LFO) ocurre durante la programación de sonido, no durante la performance.

El Moog Subsequent 37 (2015) implementa exactamente este patrón físicamente: los controles del
filter (cutoff/resonance) están en el panel frontal siempre accesibles; los controles de OSC y
ENV tienen su propio panel físico con layouts dedicados. El Nivel 2 de GrooveForge Brain replica
esta lógica en un display circular.

### 1.2 Por qué Cutoff y Resonance en el sub-home

En cualquier síntesis substractiva (Moog Model D, Juno-106, Prophet-5), el filtro es el
parámetro de "voz en tiempo real". No es solo el más usado: es el único que el músico ajusta
mientras la nota está sonando, sin interrumpirla.

La razón es acústica, no arbitraria. El cerebro humano interpreta los cambios de cutoff como
movimiento y vida del sonido (Chowning, 1973, "The Synthesis of Complex Audio Spectra by Means
of Frequency Modulation"; Freed et al., 1993, "Perceptual Similarities between Acoustic and
Haptic Materials"). La resonancia agrega presencia y carácter. La combinación de los dos define
la "expresividad" del instrumento de la misma manera que el vibrato define la expresividad del
violín.

Esto crea una regla de diseño: cutoff y resonance deben ser accesibles con la mano dominante
sin mirar. En el GrooveForge Brain, esto significa que en el nivel operacional (nivel 2), ENC L
controla cutoff y ENC R controla resonance, exactamente como lo harían en un synth físico con
potenciómetros dedicados.

El precedente en la Teensy Audio Library es el mismo: `AudioFilterStateVariable` expone
`frequency()` y `resonance()` como los dos métodos principales de performance.

El reuso del arc paradigm de view_07 no es cosmético — es consistencia semántica. En FX mode,
ENC L y ENC R controlan los dos sub-parámetros críticos del FX activo. En SYNTH mode, lo mismo:
ENC L → cutoff (sub-parámetro más crítico del synth), ENC R → resonance. El músico que conoce
FX mode puede operar SYNTH mode por analogía, sin leer el manual.

### 1.3 Por qué animaciones por grupo en el nivel 3

El nivel 3 (Group View) muestra 4 grupos: OSC, ENV, FILTER, LFO. Cada grupo tiene 2-4
sub-parámetros que el usuario puede navegar y editar.

La decisión de animar cada grupo de manera diferente (waveform para OSC, ADSR graph para ENV,
LP curve para FILTER, sine wave para LFO) es una aplicación del principio de **cognitive
chunking** (Miller, 1956, "The Magical Number Seven, Plus or Minus Two", *Psychological Review*).
Miller demostró que la memoria de trabajo humana puede manejar 7±2 "chunks" de información,
donde un chunk es una unidad cognitiva, no un bit de datos.

El problema con mostrar 4 sub-parámetros como números (VCO_WAVE=SAW, VCO_OCT=3, VCO2_WAVE=TRI,
VCO2_DETUNE=7) es que el usuario necesita leer e interpretar cada número por separado. Son 4
chunks de información fragmentados.

La animación de waveform colapsa esos 4 parámetros en 1 chunk perceptual: el usuario ve la
forma de onda resultante y comprende el sonido del oscilador sin decodificación cognitiva.
Cuando VCO_WAVE cambia de SAW a SQR, la forma de onda animada cambia de rampa a cuadrada — el
usuario percibe el cambio inmediatamente por la forma, no por el número.

Exactamente el mismo principio justifica el ADSR graph para el grupo ENV. Un ADSR como cuatro
números (A=120ms, D=80ms, S=0.7, R=400ms) requiere experiencia en síntesis para imaginar el
sonido. Un ADSR graph donde el segmento de ataque se mueve y el usuario ve la curva cambiar es
comprensible para cualquier persona que haya visto una ola en el mar.

Este principio no es nuevo en instrumentos de hardware. El Sequential Prophet-6 (2015) tiene un
display que muestra el envelope gráficamente cuando el usuario ajusta los knobs de ENV. El
Arturia MatrixBrute (2016) tiene LEDs que dibujan la curva del filtro en un display matricial.
GrooveForge Brain aplica la misma idea a un display GC9A01 de 240px.

### 1.4 Por qué lv_canvas para las curvas

LVGL v8 no incluye un widget nativo para dibujar curvas arbitrarias. Tiene `lv_arc` para arcos
circulares, `lv_line` para segmentos rectos, y `lv_bar` para barras lineales — pero no tiene
spline rendering ni función de curva paramétrica.

Las opciones para dibujar la curva LP del filtro o la forma de onda del OSC son:

**Opción A — lv_line con array de puntos:**
Aproximar la curva como una polyline de N puntos. Para N=20, la curva LP del filtro se puede
representar con ~6KB de RAM (array de 20 `lv_point_t`). La limitación es que `lv_line` dibuja
líneas de 1px de grosor — no es posible hacer la curva más gruesa sin capas adicionales.

```
ventaja:  sin overhead de canvas, cada punto es dinámico
limitación: solo líneas rectas entre puntos, sin anti-aliasing
```

**Opción B — lv_canvas con pixel painting:**
Un `lv_canvas` de 200×120px (safe zone del display circular) con buffer de 200×120×2=48KB
(formato `LV_IMG_CF_TRUE_COLOR` 16bpp). Permite dibujar cualquier curva con anti-aliasing,
relleno de área bajo la curva, y control de grosor de línea.

```
ventaja:  pixel-level control, anti-aliasing, relleno de área
limitación: 48KB de RAM — revisar disponibilidad (ESP32-S3 tiene 512KB de SRAM interna)
```

**Opción C — lv_canvas con formato de 8bpp:**
El mismo canvas pero en `LV_IMG_CF_INDEXED_8BIT`, reduciendo el buffer a 24KB.

```
ventaja:  menor RAM que opción B, aún pixel-level control
limitación: paleta de 256 colores — suficiente para gradientes de teal y gris del theme
```

La decisión es **Opción A (lv_line) para formas de onda y ADSR** + **Opción B (lv_canvas con
TRUE_COLOR reducido a zona de interés)** para la curva LP del filtro, donde el relleno de área
bajo la curva agrega información de ganancia que justifica el overhead.

La justificación del split: la onda OSC y el ADSR son formas geométricas simples (cuadrada,
sierra, triángulo son perfectamente representables con 4-8 puntos de polyline). La curva LP del
filtro tiene una transición suave (rolloff -24dB/oct) y un bump de resonancia que se lee mejor
con área rellena que con una línea sola.

Para el ESP32-S3 con 512KB SRAM interna + 2MB PSRAM: un canvas de 160×80px en TRUE_COLOR usa
160×80×2=25.6KB — cómodo dentro del budget de RAM del display.

**Referencias:**
- Ilyés, G. et al. (LVGL documentation v8.3): "Canvas" widget — pixel-level drawing API,
  `lv_canvas_draw_line()`, `lv_canvas_draw_polygon()`, `lv_canvas_set_px()`.
- ESP32-S3 Technical Reference Manual (Espressif, rev. 1.3) §2.1.5 — SRAM: 512KB interno +
  2MB PSRAM accesible via octal SPI.

### 1.5 Por qué ENC L click / ENC R click para el bypass de AI

La pregunta es: dado que view_10 AI PROCESSING ya existe con una función (mostrar key/chord/BPM
detectados), ¿cómo agregar control de bypass de modelos individuales sin agregar hardware?

El principio de **contextual repurposing** (Cooper, Reimann et al., 2007, *About Face 3: The
Essentials of Interaction Design*, Wiley) establece que los controles pueden cambiar su función
cuando el contexto lo hace obvio, siempre que:
1. El cambio de contexto sea explícito y visible para el usuario
2. La función en el contexto nuevo sea análoga a la función en el contexto original

En view_10, ENC L click y ENC R click no tienen función asignada (están sin usar). Asignarlos
a bypass de Scale Lock y Beat Follower respectivamente satisface ambas condiciones:

1. El contexto es explícito: estás en view_10 AI PROCESSING, no en FX mode ni en SYNTH mode.
   El display lo muestra.
2. La función es análoga: en FX mode, ENC L click y ENC R click resetean el sub-parámetro al
   default. En AI mode, "resetear" un modelo se interpreta como "desactivarlo/activarlo" —
   un bypass es el equivalente AI del reset de un parámetro FX.

No se agrega hardware. No se agrega un gesto nuevo. El usuario que ya conoce ENC L/R click en
FX mode puede intuir su función en AI mode por la analogía del contexto.

Esta decisión es explícitamente diferente de agregar un botón físico para AI bypass, que sería
más claro pero requeriría hardware adicional no previsto en el BOM (`01-architecture.md §2.2`).
El enfoque de contextual repurposing es la solución correcta dado el constraint de hardware fijo.

### 1.6 Por qué Scale Lock y Beat Follower son bypasables independientemente

Los dos modelos de AI que afectan el sonido en tiempo real son Scale Lock y Beat Follower. Son
complementarios pero sirven casos de uso diferentes que pueden coexistir o usarse por separado:

**Scale Lock** (Sprint 5.3): mapea notas fuera de escala al semitono más cercano dentro de la
escala detectada. Es útil cuando el músico quiere explorar melódicamente sin preocuparse de
notas "wrongas". Pero hay contextos donde el músico intencionalmentequisiera una nota fuera de
escala: un blue note en blues (la b3 o b5 sobre una escala mayor), tensión cromática en jazz,
o cualquier estilo donde la "nota incorrecta" es el punto.

**Beat Follower** (Sprint 5.4): sincroniza los tiempos de delay y los LFOs al BPM detectado.
Útil cuando el músico toca en tiempo. Pero en impro libre, ambient, o drone music, no hay tempo
— el beat follower estaría sincronizando delays a un BPM inexistente, produciendo artifacts.

Si solo hubiera un bypass global de "AI on/off", el músico que quiere Scale Lock pero no Beat
Follower tendría que desactivar todo. Un bypass por modelo permite configuraciones mixtas:
Scale Lock on + Beat Follower off para una sesión de blues libre, o Scale Lock off + Beat
Follower on para un set de house donde el tempo está definido por el track.

El display de view_10 refuerza el estado visual:
- Modelo activo: opacidad 100%, valor de confianza visible
- Modelo bypaseado: opacidad 40%, label "OFF" superpuesto

Esto es consistente con el pattern de BYPASS en view_07 FX MAIN (sprint 30, §3.6.4): el
elemento bypaseado se atenúa, no desaparece. El usuario sabe que el modelo existe y puede
reactivarlo.

---

## 2. Navigation Architecture

### 2.1 SYNTH Mode — jerarquía de 3 niveles

```
┌─────────────────────────────────────────────────────────────┐
│  NIVEL 1 — Engine List (view_02 o nueva vista)              │
│                                                             │
│  Contenido: lista de engines                                │
│    • Moog Model D                                           │
│    • Juno-106                                               │
│    • Prophet-5                                              │
│                                                             │
│  ENC NAV rotate → navegar lista                             │
│  ENC NAV click  → seleccionar engine → ir a Nivel 2        │
└──────────────────────────┬──────────────────────────────────┘
                           │  ENC NAV click (enter)
                           │  ENC L click / ENC R click (back)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  NIVEL 2 — Engine Sub-Home (view_03, arcos estilo view_07)  │
│                                                             │
│  Contenido: engine name + dos arcos                         │
│    • Arc izquierdo (ENC L): Cutoff                          │
│    • Arc derecho (ENC R): Resonance                         │
│                                                             │
│  ENC L rotate   → ajustar Cutoff                            │
│  ENC R rotate   → ajustar Resonance                         │
│  ENC NAV click  → entrar a Nivel 3 (group view)            │
│  ENC L click    → volver a Nivel 1 (engine list)            │
│  ENC R click    → volver a Nivel 1 (engine list)            │
└──────────────────────────┬──────────────────────────────────┘
                           │  ENC NAV click (enter)
                           │  ENC L click / ENC R click (back)
                           ▼
┌─────────────────────────────────────────────────────────────┐
│  NIVEL 3 — Group View (view_04, animaciones por grupo)      │
│                                                             │
│  Contenido: 4 grupos — OSC | ENV | FILTER | LFO             │
│                                                             │
│  ENC NAV rotate → ciclar entre grupos                       │
│  ENC L rotate   → navegar sub-params dentro del grupo       │
│  ENC R rotate   → cambiar valor del sub-param seleccionado  │
│  ENC L click    → volver a Nivel 2 (engine sub-home)        │
│  ENC R click    → volver a Nivel 2 (engine sub-home)        │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 Sub-parámetros por grupo (Moog Model D como referencia)

| Grupo | Sub-params | Índice |
|-------|-----------|--------|
| OSC   | VCO_WAVE, VCO_OCT, VCO2_WAVE, VCO2_DETUNE | 0-3 |
| ENV   | ATK, DEC, SUS, REL | 0-3 |
| FILTER| FILT_CUT, FILT_RES | 0-1 |
| LFO   | LFO_RATE, LFO_DEPTH | 0-1 |

FILTER.FILT_CUT y FILTER.FILT_RES son los mismos parámetros que ENC L / ENC R en el sub-home
(Nivel 2). Si el usuario los edita en Nivel 3, el valor se refleja al volver a Nivel 2 — no hay
estado duplicado, comparten el mismo param_id del bridge.

### 2.3 Animaciones por grupo (lv_canvas + lv_line, LVGL v8)

| Grupo  | Animación                         | Technique     |
|--------|-----------------------------------|---------------|
| OSC    | Forma de onda que morfea entre sine/saw/square/tri según VCO_WAVE. Dos formas si VCO2 está activo | lv_line (polyline de 40 puntos, recalculado al cambiar VCO_WAVE) |
| ENV    | Gráfico ADSR con 5 puntos dinámicos (origin, attack peak, decay end, sustain end, release end). El segmento del param seleccionado se resalta en teal+ | lv_line (5 puntos, recalculado al cambiar ATK/DEC/SUS/REL) |
| FILTER | Curva LP con slope -24dB/oct y bump de resonancia en la frecuencia de corte. Curva se desplaza con cutoff, bump sube/baja con resonance | lv_canvas TRUE_COLOR 160×80px — área bajo la curva rellena |
| LFO    | Onda sine que avanza (timer scrollea la fase). Amplitud = depth, densidad de ciclos = rate | lv_line (polyline de 60 puntos), actualizado por lv_timer @60ms |

**Budget de RAM para canvas:**
El canvas de FILTER de 160×80px en TRUE_COLOR (16bpp) usa 160×80×2 = 25.6KB.
El ESP32-S3 tiene 512KB SRAM interna disponible. Después del stack de LVGL (~40KB) y los
buffers del display SRAM (~15KB), quedan ~450KB disponibles. 25.6KB es el 5.7% — dentro del
budget.

Solo un canvas existe a la vez (solo un grupo es visible en cualquier momento). No hay 4
canvas simultáneos.

### 2.4 AI Mode — view_10 con bypass por modelo

**Acceso:** hold ENC NAV 4s desde cualquier vista → view_10 AI PROCESSING completa.

La view_10 full (posterior al flash de entrada) muestra:
- Key detectada (ej: "Em")
- Chord detectado (ej: "Em7")
- BPM detectado (ej: "124")

**Nuevo: bypass por modelo**

| Control         | Acción                                         |
|-----------------|------------------------------------------------|
| ENC L click     | Toggle Scale Lock bypass (on ↔ off)            |
| ENC R click     | Toggle Beat Follower bypass (on ↔ off)         |

**Visual de bypass:**
- Modelo activo: opacidad 255, valor visible, sin label adicional
- Modelo bypaseado: opacidad LV_OPA_40 (~100/255), label "OFF" superpuesto en GF_COLOR_BYPASS_ORANGE

El layout de view_10 full:

```
         ← 240px →
    ╭─────────────────────╮
    │    AI PROCESSING    │  ← arc title CONTEXT 12px
    │                     │
    │  KEY    CHORD   BPM │  ← labels LABEL 14px
    │  Em     Em7    124  │  ← valores MAJOR 28px
    │                     │
    │ [SCALE LOCK]        │  ← status pill ENC L (teal+ si activo, dimmed si bypass)
    │  Scale: Em          │  ← escala activa CONTEXT 12px
    │                     │
    │ [BEAT FOLLOWER]     │  ← status pill ENC R (teal+ si activo, dimmed si bypass)
    │  BPM: 124  ♩=♩     │  ← tempo + sync indicator CONTEXT 12px
    │                     │
    │ L:bypass  R:bypass  │  ← hint controles CONTEXT 10px, gris dim
    ╰─────────────────────╯
```

---

## 3. Bridge Protocol — extensión para AI bypass

### 3.1 Nuevos param_ids (usados con GF_CMD_PARAM_CHANGED = 0x14)

Los param_ids especiales del Sprint 30 se extienden con dos entradas nuevas:

| param_id | Nombre          | Payload | Dirección     | Descripción |
|----------|-----------------|---------|---------------|-------------|
| `0x00F8` | SCALE_LOCK_BYPASS | 1B bool | ESP32 → Teensy | Toggle bypass del modelo Scale Lock. 0x01=bypass activo (modelo desactivado), 0x00=modelo activo |
| `0x00F9` | BEAT_FOLLOWER_BYPASS | 1B bool | ESP32 → Teensy | Toggle bypass del modelo Beat Follower. 0x01=bypass activo, 0x00=activo |

**Justificación del formato 2B para param_id:**

El frame format del Bridge Protocol (`02-bridge-protocol.md §1.1`) especifica param_id como 2B
en GF_CMD_PARAM_CHANGED (ver §2.2: `[param_id: 2B, value: 4B float]`). Los param_ids
especiales de Sprint 30 usaban 1B implícitamente (0xFB, 0xFA, etc.) porque están en el high
byte de un campo de 2B con el low byte en 0x00. Los nuevos param_ids siguen el mismo patrón:
`0x00F8` = high byte `0x00`, low byte `0xF8`.

El payload de bypass es 1B bool empaquetado en los 4B de value del frame: byte 0 = 0x00 (false,
modelo activo) o 0x01 (true, bypass activo). Bytes 1-3 son 0x00 (padding). No se usa float para
este param porque el valor es discreto, no continuo.

**Dirección del flujo:**

```
ESP32-S3 (ENC L click / ENC R click detectado en view_10)
    → GF_CMD_PARAM_CHANGED, param_id=0x00F8, value[0]=0/1
    → UART 921600 baud
    → Teensy 4.1 (bridge_master recibe)
    → ml_engine.set_scale_lock_bypass(bool)
    → Scale Lock ignora snap si bypass activo
```

El Teensy es el que decide si aplicar el snap de Scale Lock o no. El ESP32 solo reporta la
intención del usuario. El Teensy ejecuta o ignora el modelo según el flag de bypass.

**Reflexión del estado al display:**

Cuando el Teensy recibe el toggle, puede confirmar el nuevo estado enviando de vuelta un
GF_CMD_PARAM_CHANGED con el mismo param_id y el valor efectivo (0x00 o 0x01). El ESP32 actualiza
la opacidad del status pill al recibir la confirmación. Este roundtrip es opcional si el ESP32
asume que el toggle fue aplicado (optimistic update) — la latencia de UART es <2ms p95, por lo
que el roundtrip agrega <4ms de latencia de feedback visual, aceptable.

### 3.2 Nuevo param_id para navegación de Nivel 2 (SYNTH sub-home)

| param_id | Nombre        | Payload         | Dirección     | Descripción |
|----------|---------------|-----------------|---------------|-------------|
| `0x00F6` | SYNTH_LEVEL   | 1B (0/1/2)      | Teensy → ESP32 | Nivel activo en SYNTH mode. 0=engine list, 1=sub-home, 2=group view |
| `0x00F5` | SYNTH_GROUP   | 1B (0-3)        | Teensy → ESP32 | Grupo activo en Nivel 3. 0=OSC, 1=ENV, 2=FILTER, 3=LFO |
| `0x00F4` | ENGINE_SELECT | 1B (0-2)        | Teensy → ESP32 | Engine seleccionado. 0=Moog, 1=Juno, 2=Prophet |

Estos param_ids permiten que el display refleje el estado de navegación del Teensy sin necesitar
un CMD separado para cada transición de nivel.

---

## 4. Implementation Plan

### 4.1 Estructura de archivos — qué se crea y qué se modifica

**Archivos nuevos (Teensy):**

| Archivo | Propósito |
|---------|-----------|
| `apps/firmware-teensy/src/sketches/28-synth-navigator.cpp` | Sketch principal del sprint: máquina de estados para 3 niveles de SYNTH + bridge para los 3 engines + AI bypass |

**Archivos nuevos (ESP32):**

| Archivo | Propósito |
|---------|-----------|
| `apps/firmware-esp32/src/display/screens/views/view_03_engine_subhome.cpp` | Level 2: arcos ENC L (cutoff) + ENC R (resonance), reutiliza gf_arc_hero de Sprint 30 |
| `apps/firmware-esp32/src/display/screens/views/view_03_engine_subhome.h` | Header de view_03 |
| `apps/firmware-esp32/src/display/screens/views/view_04_group_view.cpp` | Level 3: 4 grupos con animaciones (lv_line + lv_canvas) |
| `apps/firmware-esp32/src/display/screens/views/view_04_group_view.h` | Header de view_04 |

**Archivos modificados:**

| Archivo | Cambio |
|---------|--------|
| `apps/firmware-esp32/src/display/screens/views/view_02_synth_main.cpp` | Refactorizar como Level 1 (engine list con cursor, 3 engines). Ya existe como editor de lista plana (Sprint 30) — reorientar para que la lista sea engines, no params |
| `apps/firmware-esp32/src/display/screens/views/view_10_ai_processing.cpp` | Agregar bypass pills (ENC L/R status), hint de controles, handlers para 0x00F8 / 0x00F9 |
| `apps/firmware-esp32/src/bridge/bridge_handlers.cpp` | Agregar handlers para 0x00F8 / 0x00F9 (SCALE_LOCK_BYPASS / BEAT_FOLLOWER_BYPASS); agregar setters/getters de estado bypass |
| `apps/firmware-esp32/src/bridge/bridge_handlers.h` | Declarar `bridge_get_scale_lock_bypass()`, `bridge_get_beat_follower_bypass()` |
| `apps/firmware-esp32/src/display/carousel/carousel.cpp` | Agregar VIEW_IDX_ENGINE_SUBHOME y VIEW_IDX_GROUP_VIEW al array s_views; rutear transiciones de nivel |
| `apps/bridge-protocol/include/protocol.h` | Documentar nuevos param_ids 0x00F4–0x00F6, 0x00F8–0x00F9 en el bloque de comentarios de param_ids especiales |
| `apps/firmware-esp32/src/display/ui_theme.h` | (verificar) GF_COLOR_BYPASS_ORANGE ya existe desde Sprint 30; agregar LV_OPA_AI_BYPASS si no está |

### 4.2 Máquina de estados del sketch 28 (Teensy)

```
Estado global en Sketch 28:

enum SynthLevel { LEVEL_ENGINE_LIST, LEVEL_SUBHOME, LEVEL_GROUP_VIEW };
enum SynthGroup { GROUP_OSC=0, GROUP_ENV=1, GROUP_FILTER=2, GROUP_LFO=3 };

SynthLevel g_synth_level = LEVEL_ENGINE_LIST;
int        g_engine_cursor = 0;        // 0=Moog, 1=Juno, 2=Prophet
SynthGroup g_group_cursor = GROUP_OSC;
int        g_group_param_cursor = 0;   // índice dentro del grupo activo
```

Transiciones de nivel:

```
LEVEL_ENGINE_LIST:
  ENC NAV rotate → mueve g_engine_cursor (0-2, wrap modular)
  ENC NAV click  → SET_ENGINE(g_engine_cursor), g_synth_level=LEVEL_SUBHOME
                   → bridge_send(PARAM_CHANGED, 0x00F4, engine_id)
                   → bridge_send(PARAM_CHANGED, 0x00F6, LEVEL_SUBHOME)

LEVEL_SUBHOME:
  ENC L rotate   → ajusta FILT_CUT del engine activo
                   → bridge_send(PARAM_CHANGED, PARAM_FILT_CUT, value)
  ENC R rotate   → ajusta FILT_RES del engine activo
                   → bridge_send(PARAM_CHANGED, PARAM_FILT_RES, value)
  ENC NAV click  → g_synth_level=LEVEL_GROUP_VIEW
                   → bridge_send(PARAM_CHANGED, 0x00F6, LEVEL_GROUP_VIEW)
  ENC L click    → g_synth_level=LEVEL_ENGINE_LIST
                   → bridge_send(PARAM_CHANGED, 0x00F6, LEVEL_ENGINE_LIST)
  ENC R click    → g_synth_level=LEVEL_ENGINE_LIST
                   → bridge_send(PARAM_CHANGED, 0x00F6, LEVEL_ENGINE_LIST)

LEVEL_GROUP_VIEW:
  ENC NAV rotate → mueve g_group_cursor (0-3, wrap modular)
                   → bridge_send(PARAM_CHANGED, 0x00F5, group_id)
  ENC L rotate   → mueve g_group_param_cursor dentro del grupo
  ENC R rotate   → ajusta valor del param en g_group_param_cursor del grupo activo
                   → bridge_send(PARAM_CHANGED, param_id_del_grupo[grupo][param], value)
  ENC L click    → g_synth_level=LEVEL_SUBHOME
                   → bridge_send(PARAM_CHANGED, 0x00F6, LEVEL_SUBHOME)
  ENC R click    → g_synth_level=LEVEL_SUBHOME
                   → bridge_send(PARAM_CHANGED, 0x00F6, LEVEL_SUBHOME)
```

Los 3 engines (Moog Model D, Juno-106, Prophet-5) ya existen como clases C++ en
`apps/firmware-teensy/src/engines/`. El sketch 28 instancia el engine correcto según
`g_engine_cursor` y delega los calls de parámetro al engine activo.

### 4.3 view_03 — Engine Sub-Home (Level 2)

Reutiliza el widget `gf_arc_hero` de Sprint 30. Layout:

```
         ← 240px →
    ╭─────────────────────╮
    │  SYNTH · MOOG MODEL │  ← arc title CONTEXT 12px (TOP_MID, ofs_y=+12)
    │                     │
    │ CUT    ╭───╮   RES  │  ← LABEL 14px izq/der
    │ 1.2k  ╱  --  ╲ 45% │  ← MAJOR 28px ENC L / MAJOR 28px ENC R
    │       │  [--]  │    │  ← arco central vacío (diferencia con FX: no hay wet/dry)
    │        ╲       ╱    │    placeholder visual para consistencia de layout
    │         ╰───╯       │
    │   ↓ NAV: grupos     │  ← hint CONTEXT 10px (CENTER, ofs_y=+60)
    │   ←→ atrás          │  ← hint ENC L/R click, CONTEXT 10px
    │       [SYNTH]       │  ← mode pill CONTEXT 12px (BOTTOM_MID, ofs_y=-10)
    ╰─────────────────────╯
```

Diferencia clave con view_07 FX MAIN: el arco central no muestra wet/dry (ese concepto no
existe en SYNTH mode). El arco central en el sub-home es un elemento visual de consistencia —
mantiene el patrón de 3 zonas (izq/centro/der) que el usuario reconoce del FX mode, pero el
centro está vacío (placeholder sin valor). Una alternativa es mostrar el volumen del engine en
el centro, pero eso agrega complejidad sin valor de performance claro.

### 4.4 view_04 — Group View (Level 3)

Layout base (el área de animación cambia por grupo, el frame es constante):

```
         ← 240px →
    ╭─────────────────────╮
    │  OSC   ENV  FLT LFO │  ← tabs CONTEXT 12px, grupo activo en teal+
    │  ───                │  ← underline bajo el tab activo
    │                     │
    │  ┌─────────────────┐│
    │  │  [ANIMACIÓN del ││  ← área de animación 160×80px, centrada
    │  │    grupo activo] ││    (lv_line o lv_canvas según grupo)
    │  └─────────────────┘│
    │                     │
    │  VCO_WAVE   SAW     │  ← param activo: LABEL 12px + valor MAJOR 24px
    │  ← ENC L navega →  │  ← hint CONTEXT 10px
    │  ↕ ENC R valor      │
    │       [SYNTH]       │  ← mode pill CONTEXT 12px
    ╰─────────────────────╯
```

**Tabs de grupo:** 4 tabs horizontales en la zona superior. El tab activo tiene:
- Texto en GF_COLOR_TEAL_PLUS
- Underline de 2px en GF_COLOR_TEAL_PLUS
Los tabs inactivos tienen texto en GF_COLOR_GRAY, sin underline.

Al rotar ENC NAV, los tabs no hacen slide — cambian instantáneamente porque el cambio de grupo
es una transición de contexto, no de posición en una lista. La animación de la zona de
animación sí transiciona: crossfade 200ms entre la animación del grupo anterior y la del nuevo.

**Animación OSC (lv_line):**

```cpp
// 40 puntos para una onda de una pantalla completa
lv_point_t osc_points[40];
// Para cada punto i en [0, 39]:
float x = (i / 39.0f) * 160.0f;   // ancho de 160px
float phase = (i / 39.0f) * 2.0f * PI;
float y_norm;
switch (vco_wave) {
    case WAVE_SINE:    y_norm = sinf(phase); break;
    case WAVE_SAW:     y_norm = 1.0f - (2.0f * (i / 39.0f)); break;
    case WAVE_SQUARE:  y_norm = (i < 20) ? 1.0f : -1.0f; break;
    case WAVE_TRI:     y_norm = (i < 20) ? (1.0f - 2.0f*(i/19.0f)) : (-1.0f + 2.0f*((i-20)/19.0f)); break;
}
osc_points[i] = { (lv_coord_t)(20 + x), (lv_coord_t)(40 - (int)(y_norm * 30.0f)) };
// Redibujar lv_line con el nuevo array de puntos
```

Cuando VCO_WAVE cambia, los 40 puntos se recalculan y `lv_line_set_points()` se llama. LVGL
invalida el área y redibuja en el próximo ciclo de refresh.

**Animación ENV (lv_line):**

5 puntos definen el ADSR. El punto en el segmento del param activo (ENC L posición) se resalta
con un círculo de 6px en GF_COLOR_TEAL_PLUS. Los otros segmentos están en GF_COLOR_TEAL_DIM.

```
origin (0,bottom) → attack_peak (Atk_x, top) → decay_end (Dec_x, Sus_y)
→ sustain_end (SusEnd_x, Sus_y) → release_end (Rel_x, bottom)
```

Al cambiar ENC R (valor del param activo), el punto correspondiente se mueve y `lv_line_set_points()`
se llama con el array actualizado.

**Animación FILTER (lv_canvas):**

El canvas de 160×80px se limpia (`lv_canvas_fill_bg()`) y se redibuja en cada cambio de
FILT_CUT o FILT_RES. La curva LP:

```
Para freq f en [20Hz, 20kHz]:
  magnitude_dB = 0 si f < cutoff
  magnitude_dB = -24 × log2(f / cutoff) si f > cutoff (rolloff -24dB/oct)
  + bump de resonancia: magnitude_dB += resonance × G × exp(-0.5 × ((f-cutoff)/BW)^2)
    donde G y BW son constantes visuales de escala, no parámetros de audio
```

El área bajo la curva se rellena con GF_COLOR_TEAL_DIM; la línea de la curva con GF_COLOR_TEAL_PLUS.

**Animación LFO (lv_line + lv_timer):**

Un `lv_timer` a 60ms actualiza la fase de la onda LFO:

```cpp
static float lfo_phase = 0.0f;
// En el timer callback:
float rate_normalized = lfo_rate / MAX_LFO_RATE;  // 0.0-1.0
lfo_phase += rate_normalized * 0.2f;               // avanza fase
if (lfo_phase > 2.0f * PI) lfo_phase -= 2.0f * PI;
// Recalcular 60 puntos de la onda sine con la nueva fase
// lv_line_set_points(...)
```

La amplitud de la onda (píxeles pico-a-pico) escala con `lfo_depth`.

### 4.5 view_10 — AI bypass pills

Agregar a view_10 full (posterior al flash de entrada de view_10):

```cpp
// En build_view_10():
// Pill Scale Lock
lv_obj_t* scale_lock_pill = lv_obj_create(screen);
lv_obj_set_size(scale_lock_pill, 100, 30);
lv_obj_align(scale_lock_pill, LV_ALIGN_CENTER, -55, +20);

lv_obj_t* scale_lock_label = lv_label_create(scale_lock_pill);
lv_label_set_text(scale_lock_label, "SCALE LOCK");

lv_obj_t* scale_lock_off_label = lv_label_create(scale_lock_pill);
lv_label_set_text(scale_lock_off_label, "OFF");
lv_obj_add_flag(scale_lock_off_label, LV_OBJ_FLAG_HIDDEN);  // visible solo si bypass

// Función de update al recibir 0x00F8:
void view_10_update_scale_lock(bool bypassed) {
    lv_obj_set_style_opa(scale_lock_pill, bypassed ? LV_OPA_40 : LV_OPA_COVER, 0);
    if (bypassed)
        lv_obj_clear_flag(scale_lock_off_label, LV_OBJ_FLAG_HIDDEN);
    else
        lv_obj_add_flag(scale_lock_off_label, LV_OBJ_FLAG_HIDDEN);
}

// Idem para Beat Follower pill (ENC R click → 0x00F9)
```

El hint de controles en el pie:
```
L:bypass  R:bypass
```
Se muestra en tipografía CONTEXT 10px, GF_COLOR_GRAY con opacity LV_OPA_60. Es visible pero
no compite con los valores de key/chord/BPM. El usuario lo lee la primera vez y luego lo ignora.

### 4.6 bridge_handlers — nuevos handlers

```cpp
// En bridge_handlers.h:
bool bridge_get_scale_lock_bypass(void);
bool bridge_get_beat_follower_bypass(void);
void bridge_send_scale_lock_bypass(bool bypassed);    // ESP32 → Teensy
void bridge_send_beat_follower_bypass(bool bypassed); // ESP32 → Teensy

// En bridge_handlers.cpp:
static bool s_scale_lock_bypass = false;
static bool s_beat_follower_bypass = false;

// Handler para 0x00F8 (recibido si Teensy confirma el estado):
static void handle_scale_lock_bypass(const bridge_frame_t* frame) {
    s_scale_lock_bypass = (frame->payload[0] != 0);
    view_10_update_scale_lock(s_scale_lock_bypass);
}

// ENC L click en view_10 llama:
void on_enc_l_click_in_ai_view(void) {
    s_scale_lock_bypass = !s_scale_lock_bypass;
    view_10_update_scale_lock(s_scale_lock_bypass);  // optimistic update
    bridge_send_scale_lock_bypass(s_scale_lock_bypass);
}
```

---

## 5. Demo

### 5.1 Evidencia requerida

**Batch B (sketch 28):** video de Serial monitor mostrando que al girar ENC NAV, ENC L y ENC R
en el sketch 28, los valores de cutoff/resonance y los parámetros de grupo cambian y se imprimen
como frames bridge correctamente formateados.

**Batch C (view_03):** video del display circular mostrando el sub-home con dos arcos (uno para
cutoff, uno para resonance) que se mueven al girar los encoders. Se ve el arc title con el nombre
del engine.

**Batch D (view_04):** video del display mostrando los 4 grupos con sus animaciones:
- OSC: forma de onda que morfea al cambiar VCO_WAVE
- ENV: gráfico ADSR con segmento resaltado al navegar con ENC L
- FILTER: curva LP que se desplaza al cambiar cutoff
- LFO: sine wave animada en loop

**Batch E (view_10):** video del display en view_10 mostrando:
- Estado normal: Scale Lock y Beat Follower activos (opacidad 100%)
- ENC L click: Scale Lock pasa a 40% + label "OFF"
- ENC R click: Beat Follower pasa a 40% + label "OFF"
- ENC L click de nuevo: Scale Lock vuelve a 100% + label "OFF" desaparece

**Batch G (demo end-to-end):** video completo del flujo:
1. Boot → view_02 engine list
2. ENC NAV rotate → navega engines
3. ENC NAV click → entra a sub-home (view_03) con arcos
4. ENC L rotate → cutoff cambia en el arco
5. ENC NAV click → entra a group view (view_04)
6. ENC NAV rotate → cycla grupos (tabs cambian)
7. ENC L rotate → navega sub-params del grupo
8. ENC R rotate → cambia valor (animación reactiva)
9. ENC L click → vuelve a sub-home
10. Hold ENC NAV 4s → view_10 AI PROCESSING
11. ENC L click → Scale Lock pasa a bypass (dimmed + "OFF")
12. ENC R click → Beat Follower pasa a bypass

### 5.2 Cómo reproducirlo

```bash
# Compilar y flashear Sketch 28 en Teensy:
cd apps/firmware-teensy
pio run -e sketch -t upload

# Compilar y flashear ESP32-S3:
cd apps/firmware-esp32
pio run -e esp32s3 -t upload

# Monitor serial Teensy (verificar frames bridge):
pio device monitor --baud 115200 -e sketch

# Monitor serial ESP32 (verificar recepción de frames):
pio device monitor --baud 115200 -e esp32s3
```

Pasos de verificación mínimos (sin hardware de audio):
1. Teensy conectado a USB (power + Serial)
2. ESP32-S3 conectado a USB (power + display)
3. UART conectado entre Teensy GPIO16(RX)/GPIO17(TX) y ESP32 GPIO17(TX)/GPIO16(RX)
4. Girar ENC NAV mientras el sistema está en SYNTH mode → verificar que view_02 muestra cursor moviéndose
5. ENC NAV click → verificar que view_03 aparece con arcos

```bash
# Test de protocolo (nativo, sin hardware):
cd apps/firmware-teensy
pio test -e native -f "test_bridge/"
# Los tests existentes de bridge deben seguir pasando con los nuevos param_ids documentados
```

---

## 6. Out of scope — qué queda para sprints futuros

| Feature | Motivo de exclusión | Sprint tentativo |
|---------|--------------------|--------------------|
| Juno-106 y Prophet-5 param maps en group view | Este sprint implementa el framework con Moog Model D como referencia. Los otros engines usan el mismo sistema pero sus param maps específicos van en un sprint de refinamiento | Sprint 33 |
| Persistencia de engine y params en EEPROM | Sprint 30 también dejó esto fuera; la persistencia de presets completos (engine + params) va junto con el sistema de presets | Sprint 32 (presets) |
| Touch display para bypassear modelos AI | El GC9A01 tiene touch driver; UX de touch para bypass alternativo a ENC L/R click | Sprint 35 (touch UX) |
| Chord Lock (extensión de Scale Lock a acordes) | Feature distinto, requiere su propio theory doc y diseño de estado | Sprint 6.x |
| Morphing entre engines (crossfade de parámetros) | Requiere interpolación de estado entre engines — complejidad alta, no en scope de display | Backlog |

---

## 7. Referencias

### Diseño de interacción y UX

- Tognazzini, B. (1992). *Tog on Interface*. Addison-Wesley. — Modal vs spring controls; capítulo "Modes". Fundamento teórico de la jerarquía de 3 niveles con Level 2 como home permanente.
- Raskin, J. (2000). *The Humane Interface*. Addison-Wesley. — Locus of attention; por qué un nivel de navegación por encima del locus es el máximo tolerable en contextos de performance.
- Cooper, A., Reimann, R., Cronin, D. (2007). *About Face 3: The Essentials of Interaction Design*. Wiley. — Contextual repurposing de controles; fundamento del bypass AI via ENC L/R click.
- Norman, D. A. (1988/2013). *The Design of Everyday Things*. Basic Books. — Affordances y feedback inmediato; base del principio de animaciones reactivas por grupo.

### Cognición y percepción

- Miller, G. A. (1956). "The Magical Number Seven, Plus or Minus Two: Some Limits on Our Capacity for Processing Information." *Psychological Review*, 63(2), 81-97. — Cognitive chunking; por qué las animaciones de grupo colapsan múltiples parámetros en 1 chunk perceptual.
- Chowning, J. M. (1973). "The Synthesis of Complex Audio Spectra by Means of Frequency Modulation." *Journal of the Audio Engineering Society*, 21(7), 526-534. — Fundamento de por qué el cutoff es el parámetro de expresividad central en síntesis substractiva.

### Síntesis substractiva

- Pirkle, W. C. (2019). *Designing Software Synthesizer Plug-Ins in C++* (2nd ed.). Routledge. — Capítulo 4: "Filters in Synthesis". Parámetros de performance vs parámetros de programación de sonido; justifica la separación cutoff/res en Level 2 vs parámetros OSC/LFO en Level 3.
- Moog, R. A. (1965). "Voltage-Controlled Electronic Music Modules." *Journal of the Audio Engineering Society*, 13(3), 200-206. — Arquitectura original del sintetizador substractivo; establece filter, envelope y oscillator como los tres subsistemas canónicos. Fundamento del grouping OSC/ENV/FILTER/LFO en Level 3.

### LVGL y rendering

- LVGL Documentation v8.3 (2023). "Canvas" — `lv_canvas_draw_line()`, `lv_canvas_draw_polygon()`, `lv_canvas_fill_bg()`. https://docs.lvgl.io/8.3/widgets/extra/canvas.html
- LVGL Documentation v8.3 (2023). "Line" — `lv_line_set_points()`, animaciones de polyline. https://docs.lvgl.io/8.3/widgets/core/line.html
- Espressif Systems (2023). *ESP32-S3 Technical Reference Manual* rev. 1.3, §2.1.5 — SRAM interna 512KB + PSRAM 2MB. Referencia de budget de RAM para lv_canvas.

### Productos de referencia

- Moog Subsequent 37 User Manual (Moog Music, 2019) — Layout físico: filter en panel frontal permanente, OSC/ENV en zonas separadas. Modelo de la jerarquía Level 2 (cutoff/res accesibles) vs Level 3 (resto de parámetros).
- Sequential Prophet-6 Owner's Manual (Sequential, 2015) — Display gráfico del envelope al ajustar parámetros ENV. Referencia de ADSR visual reactivo.
- Arturia MatrixBrute User Manual (Arturia, 2016) — Display matricial LED para curva de filtro. Referencia de visualización de curva LP reactiva.

### Specs del proyecto

- `apps/docs/02-bridge-protocol.md §2.2` — Formato GF_CMD_PARAM_CHANGED, param_id 2B, value 4B float.
- `apps/docs/04-ai-architecture.md §8` — Latencia de inferencia <20ms p99; independencia de los 3 modelos (key, chord, beat). Fundamento de bypass por modelo.
- `apps/docs/05-fx-architecture.md` — Lista de los 9 FX y sus parámetros; referencia del arc paradigm para view_07 que view_03 reutiliza.
- `apps/docs/sprints/30-navigation-rmx-style.md §2.1, §3.6.3, §3.6.4` — Modal vs spring controls; widget gf_arc_hero; BYPASS overlay pattern que view_10 reutiliza para los status pills de AI bypass.

---

## 8. Learnings

*Esta sección se completa después de la implementación. Qué salió distinto al plan, qué
sorprendió, qué estimación estuvo incorrecta, qué constraints de hardware aparecieron.*

---

**Status:** SPEC CERRADO — listo para handoff a Firmware Engineer.

**Orden de implementación recomendado:**
1. Batch B (sketch 28): máquina de estados en Teensy + bridge frames. Verificar con monitor serial.
2. Batch C (view_03): Level 2 sub-home con arcos. Verificar con hardware conectado.
3. Batch D (view_04): Level 3 group view con 4 animaciones. La animación FILTER (lv_canvas) va al final de este batch — es la más compleja.
4. Batch E (view_10): bypass pills. Implementar handlers de bridge primero, luego el visual.
5. Batch F (bridge params): documentar en protocol.h (puede ir en paralelo con cualquier batch).
6. Batch G (demo): integración end-to-end con los dos firmwares corriendo simultáneamente.
