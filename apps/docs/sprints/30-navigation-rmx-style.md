# Sprint 30 — Navigation RMX-Style

> **Fase:** 3 — UI + Display
> **Estado:** SPEC CERRADO — pendiente de implementación
> **Depende de:** Sprint 29 (`29-hardware-integration.md`) — hardware físico operativo
> **Reemplaza:** UX de sketch 27 (`27-fx-selector.cpp`) — SHIFT + double-click eliminados
> **Referencias:** `apps/docs/01-architecture.md §3.3` · `apps/docs/02-bridge-protocol.md §1`

---

## 1. Context — Por qué reemplazamos la UX de sketch 27

### El problema: tres gestos apilados en un encoder

Sketch 27 implementó la navegación de FX con tres gestos en ENC NAV:

1. **Single push** → toggle BYPASS del FX activo
2. **Double push** → toggle global MODE (FX ↔ SYNTH ↔ AI)
3. **Hold 4 s** → arm AI mode (con feedback de ring visual)

Además, el acceso a la lista de parámetros del FX requería que B1 actuara como "SHIFT" tácito:
mientras se mantenía presionado B1, ENC NAV cicla parámetros en lugar de wet/dry.

Este modelo colapsó en práctica de performance por dos razones estructurales:

**Fragilidad temporal del double-click.** El debounce de doble-click depende de una ventana de
tiempo (150–250 ms típicamente). En condiciones de performance en vivo — dedos mojados, atención
dividida, adrenaline — la distinción entre "rápido pero intencional" y "single antes del double"
es inconsistente. El resultado es MODE TOGGLE accidental durante performance.

**Modal blindness.** El modo SHIFT de B1 no tiene indicador visual permanente cuando está activo.
Bret Victor, en "A Brief Rant on the Future of Interaction Design" (2011), llama a esto el
problema de los controles modales invisibles: el usuario pierde el track de qué modo está activo
y los gestos producen resultados inesperados. Victor argumenta que los controles físicos deben
tener estados que el cuerpo pueda sentir (spring, ratchet, detent), no estados que el software
mantiene en silencio.

El nuevo modelo elimina ambos problemas:

- **Cero double-click** en cualquier control
- **Cero SHIFT modal** (ningún botón cambia el significado de otro)
- **Hold 4 s** solo para AI, con feedback visual desde t=200 ms (no ciego)
- **B1–B4 globales y constantes** — misma semántica en cualquier pantalla (spring controls)

---

## 2. Theory

### 2.1 Modal vs spring controls

Bruce Tognazzini, en "Tog on Interface" (Addison-Wesley, 1992), establece la distinción
fundamental entre dos clases de controles de UI:

- **Modal control:** el control cambia su significado dependiendo del estado actual del
  sistema. SHIFT en un teclado es el ejemplo canónico: la misma tecla "A" produce "a" o "A"
  según el estado invisible del sistema.

- **Spring control:** el control vuelve siempre al mismo efecto, independientemente del estado.
  Un botón de pausa en un reproductor de audio es un spring: siempre pausa, siempre. No hay
  estado que cambie su semántica.

Donald Norman refuerza esto en "The Design of Everyday Things" (Basic Books, 1988/2013) con el
concepto de **affordance natural**: un control bien diseñado hace visible su función sin necesitar
instrucción. Un encoder rotativo grande y central "quiere ser girado para el parámetro principal".
Un botón físico dedicado tiene un affordance implícito de función constante.

**Aplicación en GrooveForge Brain:**

Los cuatro botones Kailh Choc (B1–B4) son spring controls globales. Su función no varía con el
modo del display. El usuario puede encontrarlos por tacto (están en posición fija en el panel)
y ejecutar sin mirar la pantalla. Esto es crítico en performance: los DJs y productores en vivo
raramente miran el instrumento — operan por músculo.

ENC NAV como modal (sketch 27) violaba este principio: su single-push significaba BYPASS en modo
FX pero SELECT en modo SYNTH, y su doble-push era MODE TOGGLE global. El usuario necesitaba saber
en qué modo estaba para predecir el resultado del gesto.

En el nuevo modelo, ENC NAV tiene solo dos gestos — push corto y hold 4 s — y sus efectos son
predecibles sin depender del modo: push corto siempre es "confirmar/bypass", hold 4 s siempre
abre AI. La adaptación al contexto (bypass en FX vs select en SYNTH) es mínima y lógica, no
arbitraria.

### 2.2 Fitts' Law y la separación espacial de encoders

Fitts (1954, "The information capacity of the human motor system in controlling the amplitude of
movement", Journal of Experimental Psychology) establece que el tiempo de movimiento para
alcanzar un target es:

```
MT = a + b · log2(2D / W)
```

donde D es la distancia al target y W es el ancho del target. Más grande y más cerca = más
rápido de alcanzar.

En un layout de 3 encoders físicos (ENC L, ENC NAV, ENC R), los tres parámetros críticos de FX
(sub-1, wet/dry, sub-2) están bajo los dedos simultáneamente: el MT es mínimo porque la distancia
D es cero — la mano ya está posicionada sobre cada encoder.

En el modelo alternativo de "un encoder que navega parámetros" (el esquema de menú de Sketch 27),
el usuario primero navega al parámetro (gira ENC NAV hasta llegar), luego entra (push), luego edita
(gira ENC NAV). Esto es tres acciones en secuencia con un MT no trivial para la navegación.

El Pioneer RMX-1000 (Roland, 2012) es el referente de producto que valida este enfoque: tiene
tres knobs fijos de izquierda a derecha (Scene FX Rate/Depth y el Wet/Dry central grande). Un DJ
puede ajustar los tres simultáneamente con una mano. No hay menú, no hay cursor, no hay
navegación. Esto es posible porque los efectos del RMX tienen exactamente 2 sub-parámetros
relevantes en performance — el mismo número que los performance picks de cada FX en GrooveForge.

El Elektron Digitakt (2017) usa la misma lógica en cada "página": 8 encoders físicos, cada uno
mapeado a un parámetro fijo de la página activa. No hay cursor dentro de la página — todos los
parámetros están físicamente accesibles al mismo tiempo. La navegación ocurre entre páginas, no
dentro de ellas.

Native Instruments Maschine (2009–presente) adopta el mismo principio: en "performance mode"
los 4 encoders superiores se mapean a los 4 parámetros críticos del instrumento activo. El modo
"browse" (con cursor y lista) está reservado para edición de preset, no para performance.

### 2.3 Por qué cursor en SYNTH pero no en FX

El paradigma de lista con cursor es correcto para SYNTH porque el contexto es distinto:

En **FX performance**, el usuario está tocando en vivo. El tiempo de respuesta motor importa.
Los parámetros que cambia son siempre los mismos (wet/dry, sub-1, sub-2 del FX activo). No hay
"exploración" de parámetros — hay ejecución de un mapeo memorizado. La lista con cursor
introduciría latencia de navegación donde no debe haberla.

En **SYNTH edit**, el usuario programa un sonido. El tiempo de respuesta motor no importa:
puede tomarse 30 segundos para encontrar el parámetro correcto. Además, el Moog Model D y
engines equivalentes tienen 12+ parámetros (VCO wave, VCO octave, VCO detune × 2 oscs,
ENV A/D/S/R, FILT cutoff, FILT resonance, LFO rate, LFO depth). Tres encoders no pueden
cubrir 12 parámetros simultáneamente — el cursor en lista plana es la solución correcta.

La distinción performance/edit no es nueva: el Moog Minimoog (1970) tiene controles físicos
para todos sus parámetros (performance first). El Prophet-5 REV2 (2018) tiene un encoder de
navegación + botones de página para parámetros extendidos (edit mode). GrooveForge implementa
ambas filosofías según el contexto.

### 2.4 Hold gestures con feedback progresivo

Apple Human Interface Guidelines (sección "Long Press") especifica que un hold gesture requiere
dos condiciones para ser usable:

1. **Threshold visible:** el usuario debe ver progreso antes de que se cumpla el threshold. Sin
   feedback visual, el hold es ciego — el usuario no sabe cuánto falta y abandona antes de tiempo.

2. **Umbral mínimo de confirmación de intención:** feedback visual que inicia solo después de un
   delay inicial (típicamente 200–500 ms) para no contaminar los single-press con animación
   indeseada. Si el feedback arranca en t=0, un push rápido produce un flash de ring que
   desorienta.

El patrón Siri (activación hold en botón lateral del iPhone) implementa exactamente esto:
nada visible en los primeros ~200 ms, luego animación de carga visible hasta el threshold de
activación. El usuario puede cancelar en cualquier punto antes del threshold.

La implementación del ring de AI en GrooveForge Brain sigue este patrón:

```
t=0 ms       Usuario presiona NAV — sin feedback visual (single-push candidato)
t=200 ms     Vista actual atenúa a 60%, ring vacío aparece en borde del display,
             core central empieza a "respirar" (pulsar) en teal
t=200→4000ms Ring se llena linealmente. Core respira más rápido cada segundo.
t=4000 ms    Ring completo → flash blanco → entra a view_10 AI PROCESSING
```

La ventana de 0–200 ms cubre el caso de single-push (no contamina con animación). La animación
de 200 ms–4000 ms es el feedback progresivo que informa "sigues en camino al AI mode". La
atenuación de la vista al 60% es una señal adicional de que algo diferente está pasando —
el usuario no puede confundirlo con un push normal.

Si el usuario suelta entre t=200 ms y t=4000 ms: ring se desvanece en 200 ms, vista recobra
100% opacidad. El bridge envía el command 0xFB con valor -1.0 (cancel ya implementado en
bridge_handlers.cpp). No hay efecto secundario.

Si el usuario suelta antes de t=200 ms: fue single-push. En FX mode ejecuta BYPASS toggle.
En SYNTH mode ejecuta SELECT/enter. El ring nunca apareció.

---

## 3. Mapping completo

### 3.1 B1–B4 — Spring controls globales (misma función en cualquier modo)

| Botón | GPIO | Función | Comportamiento |
|-------|------|---------|----------------|
| B1 | 2 | HOME | En FX mode: vuelve a vista 07 FX MAIN desde cualquier subvista. En SYNTH mode: vuelve a lista de engines (view_02 tope). |
| B2 | 3 | MODE TOGGLE | Alterna entre FX mode y SYNTH mode. Si se está en PARAM_EDIT de SYNTH, sale de PARAM_EDIT automáticamente antes de cambiar. |
| B3 | 4 | TAP TEMPO | Universal: estampa BPM para FX time-based (ghost echo time, granular rate, phase chorus rate) y para SYNTH LFO/arpeggiator. Mínimo 2 taps para calcular BPM. |
| B4 | 5 | PANIC | All notes off inmediato (Teensy). FX clear buffer (resetea delay lines, reverb tail, granular grains). Sin animación — acción instantánea. |

Nota: B1–B4 no tienen función "hold" en este sprint. Push corto = acción única. No hay
ambigüedad temporal.

### 3.2 ENC NAV — dos gestos globales

| Gesto | Umbral | Función en FX mode | Función en SYNTH mode |
|-------|--------|-------------------|----------------------|
| Push corto | t < 200 ms | BYPASS toggle del FX activo | SELECT (entra al item bajo cursor / confirma param edit) |
| Hold | t >= 4000 ms | AI Siri-like → view_10 AI PROCESSING | AI Siri-like → view_10 AI PROCESSING (mismo flow) |

ENC NAV rotación es diferente por modo — ver secciones 3.3 y 3.4.

### 3.3 FX mode — layout RMX

El modo FX no tiene cursor ni PARAM_EDIT. Los tres encoders mapean directamente a los
tres parámetros de performance del FX activo.

| Control | Gesto | Acción |
|---------|-------|--------|
| ENC L | Rotar | Sub-parámetro 1 del FX activo (ver tabla 3.5) |
| ENC NAV | Rotar | WET / DRY del FX activo (0.0–1.0) |
| ENC R | Rotar | Sub-parámetro 2 del FX activo (ver tabla 3.5) |
| ENC L | Push corto | Reset sub-1 al valor default del FX |
| ENC NAV | Push corto | BYPASS toggle |
| ENC R | Push corto | Reset sub-2 al valor default del FX |
| B1 | Push | HOME: vuelve a view_07 FX MAIN si se está en view_08 FX SELECT |
| B2 | Push | MODE TOGGLE → SYNTH mode |

**Selección de FX (view_08 FX SELECT):**

Acceso desde B1 cuando ya se está en view_07 FX MAIN (HOME hace sentido porque "ya estás en home,
quiero ir al nivel superior de la home — el selector"). En la práctica el flujo más natural es:

```
FX performance → B1 → view_08 FX SELECT
  En view_08:
    ENC NAV rotar → cyclea entre los 9 FX (sin push por ahora — el cursor ya indica)
    ENC NAV push  → confirma FX seleccionado → vuelve a view_07 FX MAIN
    B1 push       → cancela, vuelve a view_07 sin cambiar FX
```

No hay doble-push, no hay SHIFT. La selección es: navegar con ENC NAV rotate, confirmar con
ENC NAV push, cancelar con B1.

### 3.4 SYNTH mode — editor de lista plana

El modo SYNTH usa un cursor de lista plana que recorre todos los parámetros del engine activo.
ENC L tiene un atajo permanente a cutoff (parámetro más usado en performance de SYNTH).

| Control | Gesto | Acción |
|---------|-------|--------|
| ENC L | Rotar | Cutoff del filtro — atajo permanente, sin necesidad de cursor |
| ENC NAV | Rotar (sin item seleccionado) | Mueve el cursor por la lista de engines o lista de params |
| ENC NAV | Rotar (con param seleccionado) | Edita el valor del param bajo cursor |
| ENC R | Rotar | Edita el valor del param/item bajo cursor (equivalente a ENC NAV en edit) |
| ENC L | Push corto | Reset cutoff al valor default |
| ENC NAV | Push corto | SELECT: entra al engine / confirma param edit / sale de param edit |
| ENC R | Push corto | Reset del param bajo cursor al valor default |
| B1 | Push | HOME: vuelve al tope de la lista de engines (sin cambiar engine) |
| B2 | Push | MODE TOGGLE → FX mode |

**Lista plana de parámetros (orden canónico para Moog Model D):**

```
 1/12  VCO_WAVE     (waveform oscilador 1)
 2/12  VCO_OCT      (octava oscilador 1)
 3/12  VCO2_WAVE    (waveform oscilador 2)
 4/12  VCO2_DETUNE  (detune en cents oscilador 2)
 5/12  ENV_ATK      (envelope attack)
 6/12  ENV_DEC      (envelope decay)
 7/12  ENV_SUS      (envelope sustain)
 8/12  ENV_REL      (envelope release)
 9/12  FILT_CUT     (cutoff — duplica ENC L permanente)
10/12  FILT_RES     (resonance)
11/12  LFO_RATE     (LFO rate)
12/12  LFO_DEPTH    (LFO depth)
```

El display muestra "X/12" con dots de posición en la parte baja. El usuario no necesita
pensar en páginas — la lista es continua y el scroll es libre.

### 3.5 Performance picks por FX — ENC L / ENC R

Estos son los dos sub-parámetros que se exponen en los encoders laterales para cada FX.
La elección prioriza los parámetros que se ajustan más frecuentemente en performance en vivo.

| FX | ENC L (sub-1) | ENC R (sub-2) | Justificación |
|----|--------------|--------------|---------------|
| Ghost Echo | time | feedback | Time define el groove; feedback controla la densidad. Los dos en performance. |
| Modal Reverb | size | damping | Size cambia el espacio percibido; damping controla la calidez/brillo. |
| Phase Chorus | rate | depth | Rate define la velocidad del movimiento; depth la intensidad del efecto. |
| Bit Crusher | bits | sample-rate | Los dos parámetros definen el carácter lo-fi. Interdependientes. |
| Tape Sat | drive | warmth | Drive = saturación; warmth = EQ de baja frecuencia del saturador. |
| Glitch | rate | chaos | Rate = frecuencia de corte; chaos = variación aleatoria del corte. |
| Filter Sweep | cutoff | resonance | Los dos parámetros fundamentales del filtro, clásicos en performance. |
| Granular | grain-size | density | Grain-size define la textura; density el volumen relativo de grains activos. |
| Convolver | IR-mix | predelay | IR-mix = balance entre seco y convolución; predelay = sensación de espacio. |

Nota: estos performance picks son hardcoded en este sprint. La reasignación por usuario
queda fuera de scope (ver sección 6).

### 3.6 AI hold flow — diagrama temporal

```
Tiempo    Estado del sistema
─────     ──────────────────────────────────────────────────────────
t=0       NAV pressed. Empieza timer de hold.
          Sin acción, sin feedback visual. Sistema espera.

t<200ms   NAV soltado → single-push. Ejecuta BYPASS (FX) o SELECT (SYNTH).
          Sistema vuelve a estado normal. Ring nunca apareció.

t=200ms   Umbral de intención confirmado.
          Vista actual atenúa a 60% opacidad (transición suave ~100ms).
          Ring vacío aparece en borde del display GC9A01 (radio máximo).
          Core central empieza "respiración": pulsa entre teal claro y teal oscuro.

t=200ms   Ring empieza a llenarse linealmente.
→4000ms   Velocidad de respiración del core aumenta 1× por segundo
          (1Hz en t=200ms, 2Hz en t=1200ms, 3Hz en t=2200ms, 4Hz en t=3200ms).

200ms<t<  NAV soltado antes de completar.
4000ms    Ring se desvanece en 200ms.
          Vista recobra 100% opacidad en 200ms.
          Bridge envía 0xFB con value=-1.0 (cancel AI arm).
          Sistema vuelve a estado normal.

t=4000ms  Ring completo (360°).
          Flash blanco en todo el display (~80ms).
          Transición a view_10 AI PROCESSING.
          Bridge envía 0xFB con value=1.0 (AI arm confirmed).
```

### 3.7 Casos edge

| Situación | Comportamiento esperado |
|-----------|------------------------|
| Usuario rota ENC NAV mientras hace hold (t>200ms) | La rotación es ignorada. El hold continúa. Al completar 4s, entra a AI mode. Al cancelar, el valor de wet/dry no cambia. |
| B2 MODE TOGGLE mientras cursor en SYNTH PARAM_EDIT | Sale de PARAM_EDIT automáticamente (cursor vuelve a lista), luego cambia a FX mode. No se queda en estado PARAM_EDIT al cambiar de modo. |
| AI hold en SYNTH mode | Mismo flow Siri-like idéntico al de FX mode. Bridge envía 0xFB igual. |
| B4 PANIC mientras AI hold en progreso (t>200ms) | PANIC se ejecuta primero (all notes off + clear buffers). El hold se cancela (equivalente a soltar NAV). Bridge envía 0xFB=-1.0. |
| B1 HOME en view_07 FX MAIN (ya en home) | Abre view_08 FX SELECT (el HOME cuando ya estás en home sube un nivel a la selección). |
| B1 HOME en view_08 FX SELECT | Cancela FX SELECT, vuelve a view_07 FX MAIN sin cambiar FX. |
| ENC L / ENC R push mientras AI hold | Ignorados. El hold tiene prioridad sobre todos los encoders durante t>200ms. |
| Boot frío (sin EEPROM) | Arranca en FX mode, Ghost Echo (FX índice 0), wet/dry 0.5, sub-params a sus defaults. |

### 3.8 UI Visual Specs

Esta sección cierra el diseño visual del sprint. La navegación RMX ya define qué controla
cada encoder (§3.3–3.5); lo que sigue define cómo se muestra esa información en la pantalla
circular de 240px. El principio rector es legibilidad a un metro de distancia con iluminación
de escenario variable — el mismo criterio de diseño que usan los racks de DJ profesional.

#### 3.6.1 Principios de jerarquía tipográfica

Un display circular de 240px de diámetro es pequeño. El error de diseño más común en este
form factor es intentar mostrar demasiada información a tamaño similar, resultando en un
"ruido visual" donde el usuario no puede extraer el dato crítico rápidamente. El modelo de
Tufte ("The Visual Display of Quantitative Information", 2001) prescribe que el diseñador
debe maximizar el data-ink ratio: cada píxel que no comunica información debe eliminarse.

La jerarquía tipográfica fuerza que el ojo vaya primero al dato más crítico (el valor que
el encoder está modificando ahora) y segundo a su contexto (qué es ese valor). El nombre del
parámetro es siempre menos importante que su valor numérico en contexto de performance.

Blanco roto (#F2F2F2) en lugar de blanco puro (#FFFFFF): Apple Human Interface Guidelines
(sección "Color and Contrast", actualización 2023) documenta que el blanco puro en pantallas
OLED/IPS de alto brillo genera "white glare" perceptible en periferia de visión, fatigando
al usuario en sesiones de 30+ minutos. #F2F2F2 reduce la luminancia máxima un 5% sin pérdida
de legibilidad percibida, resultado que Barten (2003, "Contrast Sensitivity of the Human Eye")
confirma en el rango de frecuencias espaciales de texto de 32–64px en pantallas a 30 cm.

| Jerarquía | Uso | Tamaño px | Font | Color |
|-----------|-----|-----------|------|-------|
| HERO_XL | Valor central wet/dry en FX MAIN; valor principal SYNTH cuando se edita | 64 | Montserrat Bold | GF_COLOR_HERO_WHITE (#F2F2F2) |
| HERO | Valor principal SYNTH en modo browse (sin edición activa) | 56 | Montserrat Bold | GF_COLOR_HERO_WHITE |
| MAJOR | Sub-parámetros de FX (TIME, FEEDBACK y equivalentes según §3.5) | 32 | Montserrat Medium | GF_COLOR_TEAL_PLUS / GF_COLOR_PURPLE_BRIGHT según modo |
| LABEL | Nombre del parámetro posicionado encima del valor | 16 | Montserrat Medium UPPERCASE, +1 letter-spacing | GF_COLOR_GRAY |
| CONTEXT | Arc title en top, breadcrumb, contadores de posición ("4 / 12") | 12 | Montserrat Regular | GF_COLOR_GRAY |

Regla de precedencia: nunca dos elementos HERO_XL simultáneos en la misma vista. Si hay
colisión de jerarquía durante una transición, el elemento entrante hace fade-in desde opacidad
0 mientras el saliente hace fade-out, duración 150ms cada uno en paralelo.

#### 3.6.2 Safe zone en el display circular

El GC9A01 es una pantalla circular de 240px de diámetro físico. El driver LVGL reporta un
canvas rectangular de 240×240 con las esquinas físicamente fuera del cristal. Cualquier
elemento renderizado en las esquinas del canvas rectangular es invisible para el usuario pero
consume GPU del ESP32-S3. Más importante: elementos de texto posicionados cerca del borde del
canvas pueden aparecer recortados parcialmente por la curvatura del cristal.

La safe zone es el círculo inscrito que garantiza visibilidad completa de cualquier elemento
de texto o UI contenido en su interior.

```
         ← 240px →
    ╭───────────────────╮
   ╱  ·  ·  ·  ·  ·  ·  ╲     ← área del canvas, esquinas fuera del cristal
  │ ·  ┌─────────────┐  · │
  │ ·  │             │  · │
  │ ·  │  SAFE ZONE  │  · │   ← círculo inscrito 200px diámetro
  │ ·  │  200px dia  │  · │
  │ ·  │             │  · │
  │ ·  └─────────────┘  · │
   ╲  ·  ·  ·  ·  ·  ·  ╱
    ╰───────────────────╯
    20px margin arriba y abajo dentro de la safe zone:

         y=0px   ─── borde canvas top
         y=20px  ─── arc title / modo pill superior (CONTEXT)
         y=20–200px  zona libre para contenido HERO/MAJOR/LABEL
         y=200px ─── mode pill inferior (CONTEXT)
         y=220px ─── borde canvas bottom (fuera del cristal en las esquinas)

    x centrado en 120px (horizontal)
    margen lateral mínimo: 20px desde borde del canvas (= 20px desde cada lado)
```

Todos los textos críticos (HERO_XL, HERO, MAJOR) deben tener su bounding box completo dentro
del área delimitada por y=20px, y=200px, x=20px, x=220px. El arc title y el mode pill pueden
vivir en el margen de 20px porque son CONTEXT (tipografía 12px, ninguna letra supera la zona
recortada del cristal en la práctica).

Para lv_label con texto HERO_XL de 64px: la altura del glyph más alto (mayúsculas + descenders)
es aproximadamente 80px. Un label HERO_XL centrado en y=120px ocupa y=80–160px, bien dentro
de la safe zone. Verificar con lv_obj_get_coords() en runtime durante desarrollo.

#### 3.6.3 Wireframes por vista

Los wireframes usan coordenadas de LVGL (origen top-left del canvas 240×240, y crece hacia
abajo). Los offsets son relativos al anchor de alineación especificado.

---

**(a) view_07 FX MAIN performance — layout RMX**

El objetivo de esta vista es que el DJ/productor pueda leer wet/dry con un vistazo periférico,
sin bajar la vista del set. El arco circular hace que el dato sea reconocible por forma (no solo
por número), similar a los VU meters de hardware analógico que se leen por posición de aguja.

```
         ← 240px →
    ╭─────────────────────╮
    │  FX · GHOST ECHO  ↺ │  ← arc title CONTEXT 12px + dot bypass blink
    │                     │     (lv_align TOP_MID, ofs_y=+12)
    │ TIME     ╭───╮  FBK │
    │ 240ms   ╱ 85  ╲ 45% │  ← MAJOR 32px izq/der, HERO_XL 64px centro
    │        │  WET  │    │     arco teal+/teal-dim diam ~140px grosor 8px
    │         ╲     ╱     │
    │          ╰───╯      │
    │         [WET]       │  ← LABEL 16px (CENTER, ofs_y=+46 desde centro)
    │ ▂▃▅▃▂▄▃▂            │  ← spectrum 8 barras, max 24px altura
    │      [FX]           │  ← mode pill CONTEXT 12px
    ╰─────────────────────╯
```

| Elemento | Jerarquía | lv_align | ofs_x | ofs_y | Widget | Color |
|----------|-----------|----------|-------|-------|--------|-------|
| "FX · GHOST ECHO" | CONTEXT | TOP_MID | 0 | +12 | lv_label | GF_COLOR_GRAY |
| Dot bypass blink | — | TOP_RIGHT | -18 | +12 | lv_obj (círculo 6px) | naranja-rojo si bypass activo |
| "TIME" label | LABEL | LEFT_MID | +20 | -38 | lv_label | GF_COLOR_GRAY |
| "240ms" valor | MAJOR | LEFT_MID | +20 | -10 | lv_label | GF_COLOR_TEAL_PLUS |
| "FBK" label | LABEL | RIGHT_MID | -20 | -38 | lv_label | GF_COLOR_GRAY |
| "45%" valor | MAJOR | RIGHT_MID | -20 | -10 | lv_label | GF_COLOR_TEAL_PLUS |
| Arco circular fondo | — | CENTER | 0 | -10 | lv_arc (indicator OFF) | GF_COLOR_TEAL_DIM |
| Arco circular fill | — | CENTER | 0 | -10 | lv_arc (indicator ON) | GF_COLOR_TEAL_PLUS |
| "85" wet valor | HERO_XL | CENTER | 0 | -10 | lv_label | GF_COLOR_HERO_WHITE |
| "WET" label | LABEL | CENTER | 0 | +46 | lv_label | GF_COLOR_GRAY |
| Spectrum 8 barras | — | BOTTOM_MID | 0 | -28 | custom widget | GF_COLOR_TEAL_DIM / TEAL_PLUS |
| "FX" mode pill | CONTEXT | BOTTOM_MID | 0 | -10 | lv_label | GF_COLOR_GRAY |

Nota sobre el arco: `lv_arc` tiene dos arcos superpuestos — el de background (teal-dim, arco
completo 0→360°) y el de indicator (teal+, ángulo proporcional al wet/dry). El widget nativo
lv_arc soporta exactamente esta configuración. El diámetro de ~140px es el límite superior
que deja espacio para los MAJOR de izquierda y derecha dentro de la safe zone de 200px.

Spectrum reducido de 42px max a 24px max: la altura máxima de las barras baja para ceder
espacio vertical al HERO_XL de 64px. El spectrum sigue siendo legible porque la información
que comunica (actividad de frecuencias relativa) no requiere resolución vertical alta.

Animación del arco al cambiar wet/dry: el ángulo del indicator de lv_arc se anima en 200ms
ease_out. No se anima el número HERO_XL (se actualiza directo — la animación del número
requeriría contador interpolado y agrega latencia visual perceptible al girar el encoder).

---

**(b) view_02 SYNTH editor — HERO híbrido + tira de contexto prev/current/next**

La vista SYNTH tiene un challenge diferente: 12 parámetros en una pantalla pequeña. La
decisión de diseño (aprobada por el usuario en sesión 2026-05-25) es un modelo **híbrido**
que combina lo mejor de dos patrones:

- **HERO grande del param actual** (estilo Korg Wavestate o Elektron Digitakt en edit mode):
  el valor que estás editando ocupa el peso visual del centro.
- **Tira contextual de 3 nombres arriba** (estilo Ableton Push 2 "device strip"): muestra
  prev / current / next param para que el usuario sepa hacia dónde se mueve el cursor sin
  tener que rotar para descubrirlo.

Esto resuelve el dilema entre "veo el valor que estoy editando" (HERO domina) y "veo el
contexto de navegación" (tira contextual). El HERO comunica el dato puntual; la tira
comunica la estructura de la lista. Coste: 14px verticales adicionales del display.

```
         ← 240px →
    ╭─────────────────────╮
    │  SYNTH · SUB GENESIS│  ← arc title CONTEXT 12px (TOP_MID, ofs_y=+12)
    │                     │
    │ VCO_OCT  WAVE  ENV_A│  ← tira contextual: prev (gris dim) · CURRENT (blanco) · next (gris dim)
    │           ●         │  ← dot indicador bajo el current name
    │                     │
    │     SAW             │  ← HERO 56px valor actual (CENTER, ofs_y=+10)
    │                     │
    │  ████████░░░░  4/12 │  ← progress bar 120px ancho, CONTEXT "4/12"
    │                     │
    │  cutoff 1200 Hz     │  ← CONTEXT 12px monitor permanente ENC L
    │       [SYNTH]       │  ← mode pill CONTEXT 12px
    ╰─────────────────────╯
```

| Elemento | Jerarquía | lv_align | ofs_x | ofs_y | Widget | Color |
|----------|-----------|----------|-------|-------|--------|-------|
| "SYNTH · SUB GENESIS" | CONTEXT | TOP_MID | 0 | +12 | lv_label | GF_COLOR_GRAY |
| Prev param name (item N-1) | LABEL_DIM | CENTER | -64 | -50 | lv_label 12px opacity 100/255 | GF_COLOR_GRAY |
| **Current param name** (item N) | LABEL | CENTER | 0 | -50 | lv_label 14px UPPERCASE | GF_COLOR_HERO_WHITE |
| Next param name (item N+1) | LABEL_DIM | CENTER | +64 | -50 | lv_label 12px opacity 100/255 | GF_COLOR_GRAY |
| Dot indicador bajo current | — | CENTER | 0 | -34 | lv_obj círculo 4px | GF_COLOR_TEAL_PLUS |
| "SAW" valor actual | HERO | CENTER | 0 | +10 | lv_label | GF_COLOR_HERO_WHITE |
| Progress bar fondo | — | CENTER | 0 | +55 | lv_bar (bg) | GF_COLOR_TEAL_DIM, w=120px |
| Progress bar fill | — | CENTER | 0 | +55 | lv_bar (indicator) | GF_COLOR_TEAL_PLUS |
| "4 / 12" contador | CONTEXT | CENTER | 0 | +70 | lv_label | GF_COLOR_GRAY |
| "cutoff 1200 Hz" monitor | CONTEXT | BOTTOM_LEFT | +14 | -28 | lv_label | GF_COLOR_MONITOR_GRAY |
| "SYNTH" mode pill | CONTEXT | BOTTOM_MID | 0 | -10 | lv_label | GF_COLOR_GRAY |

**Tira contextual — wrap modular como en view_08:** los nombres prev/next se calculan con
la misma fórmula de wrap (`((cursor + delta) + N) % N`) donde N es la cantidad total de
params del engine activo. En el primer param (cursor=0), el "prev" muestra el último param
de la lista; en el último, el "next" muestra el primero. Consistencia con view_08.

**Animación al rotar ENC NAV:** los 3 nombres de la tira hacen slide horizontal 200ms
ease-out (el current sale por izq/der, prev/next ocupan su lugar). El HERO del valor hace
crossfade 150ms (fade-out del valor viejo + fade-in del nuevo) — sin slide, porque el HERO
no se mueve espacialmente, solo cambia de dato.

**Progress bar + contador:** comunican posición global en la lista (4/12). Reemplazan los
12 dots del wireframe original §4.4 — 12 dots de 9px con espaciado serían ~120px de ancho
solo para los dots, sin diferenciación visual entre items distantes (dot 3 y dot 4 igualito
de informativos). La barra de progreso es lineal-continua, más legible.

**Monitor de cutoff** en la esquina inferior izquierda es el recordatorio permanente de que
ENC L siempre controla cutoff, independientemente de donde esté el cursor. Tipografía
CONTEXT (12px) en GF_COLOR_MONITOR_GRAY (#5A5A6E) — visible pero no compite con el HERO del
param actual. El usuario que opera en performance sabe que ENC L está siempre disponible;
el monitor es un safety net para usuarios nuevos.

**Truncado de nombres en la tira:** los params pueden tener nombres largos
(`VCO2_DETUNE` = 11 chars). La tira contextual usa nombres abreviados (máx 7 chars) para
que los tres quepan en la safe zone de 200px sin solaparse. Tabla de abreviaciones se
mantiene en `view_02_synth_main.cpp` como `static const char* PARAM_NAMES_SHORT[][N]`.
El HERO/LABEL del centro sí muestra el nombre completo.

---

**(c) view_08 FX SELECT — list infinite con wrap modular**

El wrap modular elimina el caso de "lista vacía en borde" que ocurre cuando el cursor está
en el primer o último FX y el usuario sigue girando. La fórmula de acceso al elemento de
cada fila es:

```
fx_i = ((cursor + (row - 2)) + NUM_FX) % NUM_FX
```

donde `row` va de 0 a 4, `cursor` es la posición actual (0–8 para 9 FX), y `NUM_FX = 9`.
La suma de `NUM_FX` antes del módulo garantiza que el resultado nunca es negativo en C++.

Ejemplo: cursor=0 (Ghost Echo), row=0 (la fila más arriba, dos posiciones antes):
`fx_i = ((0 + (0-2)) + 9) % 9 = ((-2) + 9) % 9 = 7 % 9 = 7` → muestra Granular (el
segundo-último FX). El usuario percibe una lista circular sin fin.

```
         ← 240px →
    ╭─────────────────────╮
    │                     │
    │  Granular Cloud     │  ← row 0, opacity 60/255 (~24%), LABEL 16px
    │  Spring Plate       │  ← row 1, opacity 100/255 (~40%), LABEL 18px
    │ [Ghost Echo    ]    │  ← row 2 (cursor), opacity 255, MAJOR 32px + bracket
    │  Modal Reverb       │  ← row 3, opacity 100/255, LABEL 18px
    │  Phase Chorus       │  ← row 4, opacity 60/255, LABEL 16px
    │                     │
    │     1 / 9    ↺      │  ← CONTEXT 12px, flecha indica sentido último giro
    ╰─────────────────────╯
```

| Elemento | Fila | Tipografía | Opacity (LVGL 0-255) | Alineación |
|----------|------|-----------|---------------------|------------|
| FX extremo arriba (row 0) | cursor-2 | LABEL 16px | 60 | CENTER, ofs_y variable |
| FX semi-atenuado arriba (row 1) | cursor-1 | 18px Medium | 100 | CENTER |
| FX activo / cursor (row 2) | cursor | MAJOR 32px | 255 | CENTER con bracket |
| FX semi-atenuado abajo (row 3) | cursor+1 | 18px Medium | 100 | CENTER |
| FX extremo abajo (row 4) | cursor+2 | LABEL 16px | 60 | CENTER |
| Bracket selección | row 2 | — | 255 | rodea el texto de row 2 |
| "N / 9 ↺" contador | — | CONTEXT 12px | 200 | BOTTOM_MID, ofs_y=-14 |

La altura de fila es 28px. Las cinco filas ocupan 140px verticales, centradas en el display
(y=50px a y=190px). El bracket de selección es un rectángulo con borde 1px en GF_COLOR_TEAL_PLUS,
sin fill, que rodea el texto de la fila central.

Animación de slide al rotar ENC NAV: al recibir un tick de encoder, todas las filas (5 labels
+ bracket) se animan simultáneamente con `lv_anim` sobre la propiedad `lv_obj_set_y`, desplazando
+28px (giro CCW, sube la lista) o -28px (giro CW, baja la lista), duración 200ms, easing
ease_out. El texto de las filas se actualiza al instante con los nuevos valores de fx_i (el
texto nuevo aparece ya en su posición de destino), evitando la complejidad de crear un sexto
label temporal para el elemento que "entra por el borde".

La flecha rotativa "↺" en el contador cambia de sentido a "↻" según el último giro detectado,
reforzando el feedback visual de que la lista es circular e infinita.

---

**(d) view_10 AI overlay — Siri-like durante hold 0→4s**

Esta vista es un overlay: no destruye la vista anterior (FX MAIN o SYNTH editor), sino que
la atenúa y renderiza encima. Esto implica que el carousel NO debe hacer `lv_obj_del()` de
la vista anterior durante el AI hold — la vista debe permanecer en memoria y en el árbol de
objetos LVGL, solo atenuada.

El mecanismo de atenuación es `lv_obj_set_style_opa(parent_screen, LV_OPA_60, 0)` sobre la
vista anterior, donde la vista de overlay es un lv_obj hijo del screen raíz con z-order superior.

```
         ← 240px →
    ╭─────────────────────╮   ← pantalla durante t=200ms–4000ms
    │░░░░░░░░░░░░░░░░░░░░░│
    │░░░░░░╔═══════╗░░░░░░│   ← vista anterior atenuada al 60%
    │░░░░░░║ vista ║░░░░░░│
    │░░ ╭──║─prev──║──╮ ░░│   ← ring exterior 220px diámetro, grosor 6px
    │░░ │  ╚═══════╝  │ ░░│     se llena 0→360° según progress 0xFB
    │░░ │    ● ●●●    │ ░░│   ← dot central teal+, respira 12→18px
    │░░ │   ANALYZING │ ░░│   ← micro label fade-in en t=1s
    │░░ ╰─────────────╯ ░░│
    │░░░░░░░░░░░░░░░░░░░░░│
    ╰─────────────────────╯
```

| Elemento | Descripción | Widget | Animación |
|----------|-------------|--------|-----------|
| Vista anterior atenuada | lv_obj_set_style_opa al 60%, transición 100ms | parent screen | fade 0→60% en 100ms |
| Ring exterior | lv_arc, diam 220px, grosor 6px, GF_COLOR_TEAL_PLUS | lv_arc | fill lineal 0→360° según 0xFB value |
| Ring fondo | lv_arc background, GF_COLOR_TEAL_DIM | lv_arc | estático |
| Dot central | lv_obj círculo, GF_COLOR_TEAL_PLUS | lv_obj | size 12→18px, período variable |
| "ANALYZING…" label | CONTEXT 12px, GF_COLOR_GRAY | lv_label | fade-in en t=1s, duración 200ms |
| Flash de entrada | rect LV_OPA_COVER blanco → LV_OPA_TRANSP | lv_obj fill | 100ms linear en t=4000ms |

La velocidad de respiración del dot escala linealmente con el progress del ring:
- t=200ms (progress=0.0): período 800ms (1.25 Hz)
- t=4000ms (progress=1.0): período 300ms (3.3 Hz)
- Período en cualquier punto: `period_ms = 800 - (progress × 500)`

La implementación usa `lv_anim_set_repeat_count(LV_ANIM_REPEAT_INFINITE)` con
`lv_anim_set_time()` actualizado cada vez que llega un nuevo valor de 0xFB. LVGL permite
modificar la duración de una animación en curso; si la API no lo soporta directamente en la
versión en uso, la solución alternativa es detener la animación y reiniciarla con el nuevo
período en cada update de 0xFB (los updates llegan con frecuencia de ~100ms, invisible).

Si el usuario suelta ENC NAV antes de t=4000ms: el ring hace fade-out en 200ms, el dot para,
"ANALYZING…" hace fade-out en 200ms, y la vista anterior recupera opacidad 100% en 200ms.
El carousel no hace ninguna transición de pantalla.

Si el usuario completa hasta t=4000ms: flash blanco 100ms (lv_obj fill blanco sobre todo el
canvas, LV_OPA_COVER → LV_OPA_TRANSP), seguido de transición del carousel a view_10 completa.

#### 3.6.4 BYPASS overlay en FX MAIN

Cuando ENC NAV push toggle activa BYPASS (estado activo):

1. El widget gf_arc_hero (arco + número wet) se atenúa al 30% con `lv_obj_set_style_opa`.
2. Un lv_label "BYPASS" en HERO_XL size (64px) y color GF_COLOR_BYPASS_ORANGE (#FF6A35)
   aparece centrado sobre el área del hero, alineado CENTER con el mismo ofs_y del hero.
3. El dot de la esquina superior derecha del arc title (ya listado en la tabla de §3.6.3a)
   activa su animación de parpadeo: opacity 100→255 en 800ms, `lv_anim` loop infinito,
   easing sine_in_out.

La superposición de "BYPASS" en naranja sobre el hero atenuado al 30% crea una señal visual
inequívoca que no depende de leer un indicador pequeño. El naranja (#FF6A35) es el color de
estado de advertencia del sistema de design de GrooveForge; no coincide con ningún color de
parámetro normal (teal y purple), lo que hace el estado BYPASS reconocible perceptualmente
sin decodificación cognitiva.

Al desactivar BYPASS: lv_label "BYPASS" desaparece, gf_arc_hero vuelve a opacidad 255, dot
para su animación y desaparece, todo en 100ms de transición.

#### 3.6.5 Cambios al theme (ui_theme.h)

Las constantes existentes del theme no se modifican. Se agregan las siguientes:

| Constante | Valor LVGL | Razón |
|-----------|-----------|-------|
| GF_FONT_HERO_XL | Montserrat 64px Bold | Wet hero en FX MAIN; valor SYNTH en edición activa |
| GF_FONT_MAJOR | Montserrat 32px Medium | Sub-params FX (TIME, FEEDBACK, etc.) |
| GF_COLOR_HERO_WHITE | `lv_color_hex(0xF2F2F2)` | Valores hero; #FFF puro evitado por fatiga visual |
| GF_COLOR_BYPASS_ORANGE | `lv_color_hex(0xFF6A35)` | Overlay de estado BYPASS en FX MAIN |
| GF_COLOR_MONITOR_GRAY | `lv_color_hex(0x5A5A6E)` | Monitor de cutoff permanente en SYNTH |

Nota crítica sobre memoria flash del ESP32-S3: Montserrat 64px Bold en el rango completo
Unicode ocupa aproximadamente 180KB por fuente, lo que puede exceder el budget de flash.
La solución recomendada es usar un subset de glifos al generar la fuente con LVGL Font Converter
(https://lvgl.io/tools/fontconverter): incluir solo los caracteres `0123456789.,%-+kHz ms s%`
para GF_FONT_HERO_XL, reduciendo a ~12KB. Para GF_FONT_MAJOR, incluir el alfabeto latino básico
+ los mismos numerales. Si el subset es insuficiente para algún parámetro con unidades especiales,
evaluar Inter o DM Sans como alternativa — ambas tienen métricas similares a Montserrat en el
rango 28–64px y sus licencias OFL son compatibles con el proyecto.

#### 3.6.6 Animaciones nuevas

Todas las animaciones usan `lv_anim` de LVGL. La duración y el easing están
especificados para cada una. El firmware engineer debe verificar que las animaciones
no bloquean el loop de LVGL (usar `lv_anim_start()` con callback, no busy-wait).

| Animación | Vista | Trigger | Duración | Easing | Propiedad animada |
|-----------|-------|---------|----------|--------|------------------|
| Wet arc fill | view_07 FX MAIN | wet/dry change via 0xFF | 200ms | ease_out | lv_arc_set_value |
| List slide vertical | view_08 FX SELECT | ENC NAV rotate tick | 200ms | ease_out | lv_obj_set_y de los 5 labels |
| AI ring progress | view_10 overlay | 0xFB value update | continuous/linear | linear | lv_arc_set_value |
| AI dot breathing speed-up | view_10 overlay | 0xFB value update | 800ms→300ms | linear ramp del período | lv_obj_set_size (12→18px) |
| BYPASS dot blink | view_07 FX MAIN | bypass active | 800ms loop | sine_in_out | lv_obj_set_style_opa |
| Boot transition flash | view_10 entry | t=4000ms del AI hold | 100ms | linear | lv_obj_set_style_opa (blanco) |
| Vista anterior fade-in/out | view_10 overlay | AI arm start / cancel | 100ms / 200ms | linear | lv_obj_set_style_opa del parent |

Regla de CPU para animaciones: el display corre en el core 1 del ESP32-S3 (core 0 es el
bridge). Las animaciones LVGL son manejadas por el scheduler interno de lv_anim y no bloquean
la tarea de LVGL. Sin embargo, animar simultáneamente más de 3 propiedades en objetos
diferentes puede introducir frame drops si el render tarda >16ms (60 fps target). En la
práctica, view_07 tiene wet arc + bypass dot (2 animaciones simultáneas máximo en performance
normal) — dentro del budget. El overlay de AI (t=200ms–4000ms) tiene ring + dot breathing (2
animaciones) — también dentro del budget. Las animaciones más costosas (list slide con 5
objetos) duran solo 200ms y no se superponen con otras animaciones pesadas.

#### 3.6.7 Cambios a archivos existentes — resumen para §4

Esta tabla sintetiza los cambios de archivos que la §4 Implementation plan debe detallar,
originados en los specs visuales de §3.6. Los archivos listados en §4 que ya existían tienen
prioridad de implementación sobre nuevos archivos.

| Archivo | Tipo de cambio | Sección fuente |
|---------|---------------|----------------|
| `apps/firmware-esp32/src/display/ui_theme.h` | Agregar 5 constantes de font y color nuevas | §3.6.5 |
| `apps/firmware-esp32/src/display/screens/views/view_07_fx_main.cpp` | Reemplazar build_fx_main() con layout RMX: gf_arc_hero, sub-params MAJOR izq/der, spectrum 24px max, BYPASS overlay | §3.6.3a, §3.6.4 |
| `apps/firmware-esp32/src/display/screens/views/view_02_synth_main.cpp` | Agregar cursor + lista plana (HERO valor, LABEL nombre param, progress bar, monitor cutoff) | §3.6.3b |
| `apps/firmware-esp32/src/display/screens/views/view_08_fx_select.cpp` | Implementar wrap modular en el for de rows; agregar animación slide 200ms; ajustar opacidades por fila | §3.6.3c |
| `apps/firmware-esp32/src/display/screens/views/view_10_ai_processing.cpp` | Agregar overlay mode: atenuación de vista anterior, ring progress, dot breathing, flash de entrada; separar overlay de view_10 full | §3.6.3d |
| `apps/firmware-esp32/src/display/widgets/gf_widgets.h` | Declarar gf_arc_hero(): widget que combina lv_arc (diam ~140px) + lv_label centrado HERO_XL + lv_label LABEL "WET" inferior | §3.6.3a |
| `apps/firmware-esp32/src/display/widgets/gf_widgets.cpp` | Implementar gf_arc_hero(), lv_arc config con bg arc + indicator arc, animación 200ms ease_out en update | §3.6.3a |
| `apps/firmware-esp32/src/display/carousel/carousel.cpp` | Modificar transición a view_10: no destruir vista anterior durante AI hold overlay; restaurarla si se cancela | §3.6.3d |

El archivo `gf_widgets.h/.cpp` es nuevo. Si no existe en el repositorio, crearlo en
`apps/firmware-esp32/src/display/widgets/`. Su propósito es centralizar widgets compuestos
que se reusan entre vistas (gf_arc_hero es el primero; habrá más en sprints futuros).

---

## 4. Implementation plan — archivos a modificar

Este sprint reemplaza la lógica de navegación de sketch 27. No agrega hardware nuevo.
No modifica el frame format del bridge protocol (los param_ids reservados se documentan
en protocol.h como comentarios, no como nuevos CMDs).

### 4.1 Teensy — sketch principal

**`apps/firmware-teensy/src/sketches/27-fx-selector.cpp`**

Este archivo se refactoriza en profundidad. Es el único archivo de sketch que cambia.

Eliminar:
- `TRIPLE_PUSH_MS` — ya no existe triple push
- `DOUBLE_PUSH_MS` como trigger de MODE TOGGLE
- `g_top_mode` actualizado por doble-click (ahora lo actualiza B2 exclusivamente)
- `g_ai_arming`, `g_ai_arm_start`, `g_ai_last_prog` — reemplazados por lógica de hold timer
- Todo el bloque de lógica SHIFT (condicionales `if (buttons.held(0))` para cambiar semántica
  de ENC NAV)
- `g_nav_prev_down` — mantener si lo usa el timer de hold, eliminar si no

Agregar / refactorizar:
- `handle_button_b1_b4()` — función dedicada para HOME / MODE TOGGLE / TAP TEMPO / PANIC. Cada botón es una rama `if (buttons[N].pressed())` sin condiciones de modo.
- `handle_nav_hold()` — máquina de estados para el hold: IDLE → ARMING (t>200ms) → ARMED → TRIGGERED (t>4000ms). Envía 0xFB por bridge. Retorna si el hold está activo para que `handle_encoders()` ignore rotación de NAV.
- `handle_fx_performance()` — mapea ENC L / ENC NAV / ENC R a sub-1 / wet-dry / sub-2 del FX activo usando la tabla de performance picks de §3.5. Push de ENC L/R envía reset al default.
- `handle_synth_editor()` — gestiona cursor de lista plana. ENC NAV rotate mueve cursor o edita valor (según si hay item seleccionado). ENC L rotate es bypass a cutoff. ENC R rotate edita el param bajo cursor.

### 4.2 Teensy — UI drivers

**`apps/firmware-teensy/src/ui/encoders.h/.cpp`**

No cambiar APIs. `sw_nav_is_down()` ya existe y es la función que usa `handle_nav_hold()`.
Las APIs de `readAndReset()` para los tres encoders son genéricas y no necesitan modificación.

**`apps/firmware-teensy/src/ui/buttons.h/.cpp`**

Ya soporta 4 botones físicos (B1–B4) con debounce via Bounce2. No cambiar. El sketch 27
refactorizado llama a `buttons[0..3].pressed()` directamente.

### 4.3 ESP32 — bridge handlers

**`apps/firmware-esp32/src/bridge/bridge_handlers.cpp`**

El handler de `PARAM_CHANGED` con `param_id=0xFD` (mode switch) cambia:
- Antes: recibía valores 0=FX, 1=SYNTH, 2=AI (donde AI era un modo top-level)
- Ahora: recibe solo 0=FX, 1=SYNTH. AI no es un modo persistente — es una pantalla
  transitoria accedida por hold, que regresa al modo previo al cerrarse.

Eliminar del handler 0xFD: lógica de "modo AI" como estado persistente.

El handler de `0xFB` (AI progress) ya está implementado y no cambia. Recibe float:
- -1.0 = cancelado / inactivo
- 0.0–1.0 = progreso del ring (lineal desde t=200ms hasta t=4000ms)
- 1.0 al completar = trigger de transición a view_10

Agregar: función `bridge_get_top_mode()` ya existe y retorna 0 o 1.
Verificar que no retorna 2 como modo AI persistente — si lo hace, limpiar.

Agregar como param_ids documentados en comentario (no nuevos CMDs, usados via PARAM_CHANGED):
- `0xFA` = HOME action (si se decide notificar el display de HOME events)
- `0xF9` = TAP TEMPO (BPM calculado, si el display muestra el BPM)
- `0xF8` = PANIC action (si el display muestra feedback visual de panic)

La implementación de estos param_ids especiales en este sprint es opcional — el display
puede reaccionar solo a los cambios de parámetro reales sin necesitar notificación de HOME/TAP/PANIC.

### 4.4 ESP32 — vistas de display

**`apps/firmware-esp32/src/display/screens/views/view_07_fx_main.cpp`**

Layout nuevo para FX performance RMX-style:

```
┌────────────────────────────────┐
│ [FX NAME]           [BYPASS]  │  ← header, 20px
├─────────┬────────────┬────────┤
│  SUB-1  │  WET/DRY   │ SUB-2  │  ← zona de valores grandes, 80px
│ (ENC L) │ (ENC NAV)  │ (ENC R)│
│  "time" │    0.75    │ "fbk"  │
├─────────┴────────────┴────────┤
│        [spectrum 8 bandas]    │  ← zona baja existente, 60px
└────────────────────────────────┘
```

Eliminar: sub-modo PARAM_EDIT que existía en la vista anterior. Ya no hay cursor en FX.
El display FX es siempre performance — los tres valores grandes están siempre visibles.

Los nombres de los sub-parámetros (time, feedback, size, etc.) se obtienen de la tabla
de performance picks vía `bridge_get_param_name(fx_idx, 0)` y `bridge_get_param_name(fx_idx, 1)`.
El valor de wet/dry de `bridge_get_wet_dry()`. El estado bypass del FX activo.

**`apps/firmware-esp32/src/display/screens/views/view_02_synth_main.cpp`**

Implementar el layout HÍBRIDO definido en §3.8.3b: HERO grande del param actual + tira
contextual de 3 nombres prev/current/next arriba + progress bar lineal + monitor cutoff.
Ver §3.8.3b para el wireframe completo y la tabla de elementos.

Resumen del comportamiento:

- **Tira contextual top** (`ofs_y=-50` desde centro): muestra 3 nombres de param horizontalmente.
  El central (current) en HERO_WHITE 14px UPPERCASE; los laterales (prev/next) en GRAY 12px
  con opacity 100/255. Wrap modular: en cursor=0 el prev muestra el último; en cursor=N-1 el
  next muestra el primero. Dot teal+ 4px bajo el current name.
- **HERO valor** (`ofs_y=+10`): el valor del param actual en 56px (`GF_FONT_HERO`). Strings
  como "SAW" / "SINE" se renderizan tal cual; números se formatean con `printf("%d")` o
  `printf("%.2f")` según corresponda.
- **Progress bar lineal** (`ofs_y=+55`, ancho 120px): `lv_bar` con relleno proporcional
  cursor/N. Texto "4 / 12" en CONTEXT debajo (`ofs_y=+70`).
- **Cutoff monitor** (`BOTTOM_LEFT, ofs=+14,-28`): label permanente en MONITOR_GRAY
  recordando que ENC L → cutoff sin importar dónde esté el cursor.

Animaciones al rotar ENC NAV:

- Slide horizontal 200ms ease-out de los 3 nombres de la tira (el current sale, prev/next
  toman su lugar).
- Crossfade 150ms del HERO valor (fade-out del viejo, fade-in del nuevo). No slide — el
  HERO no se mueve espacialmente.
- Update inmediato del progress bar (sin animación; cambio de cursor es discreto).

Actualizado por `bridge_get_param_cursor()` (cursor position), `bridge_get_param_value()`
(valor del param activo), `bridge_get_param_name(engine_id, cursor)` para el current name,
y `bridge_get_param_name(engine_id, (cursor±1+N)%N)` para prev/next.

**Tabla de abreviaciones cortas** (para la tira contextual, máx 7 chars):

```c
static const char* PARAM_NAMES_SHORT[NUM_ENGINES][MAX_PARAMS] = {
    /* ENGINE_SUB_GENESIS */ { "VCO_OCT", "WAVE", "DETUNE", "ENV_A", "ENV_D", ... },
    /* ENGINE_FM */          { "OP1_RT", "OP2_RT", "FB", ... },
    ...
};
```

El nombre completo (para el LABEL central, si se decide mostrarlo) sigue viniendo de
`bridge_get_param_name()`.

**`apps/firmware-esp32/src/display/screens/views/view_10_ai_processing.cpp`**

El flow Siri-like ya está parcialmente implementado. Completar:

- Atenuación de la vista previa al 60% durante ARMING (t=200ms–4000ms). La vista_10 no
  debe aparecer de golpe — la transición incluye mostrar el ring sobre la vista anterior atenuada.
- Al cancelar (bridge recibe 0xFB=-1.0): animar ring desapareciendo, restaurar vista previa a 100%.
- Al confirmar (bridge recibe 0xFB=1.0 al t=4000ms): flash blanco, mostrar view_10 completa.

### 4.5 Bridge protocol — param_ids reservados

**`apps/bridge-protocol/include/protocol.h`**

Agregar sección de comentario (no nuevas entradas en el enum GF_Cmd — estos son param_ids
usados con GF_CMD_PARAM_CHANGED, no CMDs separados):

```
/* Param IDs especiales usados con GF_CMD_PARAM_CHANGED (no son CMDs separados):
 * 0xFF = wet/dry del FX activo        (float 0.0–1.0)
 * 0xFE = cursor FX SELECT (0-8)       (uint8 0-8)
 * 0xFD = top mode                     (uint8 0=FX, 1=SYNTH)
 * 0xFC = cursor PARAM_EDIT en SYNTH   (uint8 0-11, 0xFF=sin selección)
 * 0xFB = AI arm progress              (float -1.0=cancel, 0.0-1.0=progress, 1.0=activated)
 * 0xFA = HOME action                  (uint8 0=FX HOME, 1=SYNTH HOME) [reservado]
 * 0xF9 = TAP TEMPO BPM                (float en BPM) [reservado]
 * 0xF8 = PANIC action                 (uint8 0) [reservado]
 * 0xF7 = FX assignment sync           (uint16 encode: high byte = fx_id (0-8),
 *                                      bits 7-4 = sub1_param_idx (0-15),
 *                                      bits 3-0 = sub2_param_idx (0-15))
 *                                      Teensy → ESP32 cuando el usuario guarda en
 *                                      view_fx_assign. ESP32 actualiza labels de
 *                                      ENC L / ENC R en view_07.
 * 0xE0-0xE7 = spectrum band 0-7       (float 0.0–1.0)
 */
```

---

### 4.6 ESP32 — vista nueva: view_fx_assign (Edit assignments ENC L/R)

**`apps/firmware-esp32/src/display/screens/views/view_fx_assign.cpp`**

Vista de edición de assignments: permite al usuario elegir qué parámetro del FX activo
controla ENC L (sub-1) y cuál controla ENC R (sub-2). Reemplaza la tabla hardcoded de
performance picks para cada usuario específico.

**Acceso:** desde view_08 FX SELECT, con `ENC R push` sobre el item highlighted (el FX
bajo cursor). En view_08 `ENC R push` está sin función asignada, por lo que no hay conflicto.

**Wireframe:**

```
         ← 240px →
    ╭─────────────────────╮
    │  GHOST ECHO         │  ← engine name (arc title CONTEXT 12px)
    │  ASSIGN CONTROLS    │  ← subtítulo LABEL 16px
    │                     │
    │  ENC L              │  ← LABEL 14px gris
    │  ← TIME →           │  ← MAJOR 28px del param asignado + flechas indicando que ENC L rota
    │                     │
    │  ENC R              │  ← LABEL 14px gris
    │  ← FEEDBACK →       │  ← MAJOR 28px del param asignado + flechas indicando que ENC R rota
    │                     │
    │  NAV push = SAVE    │  ← CONTEXT 12px pie de página
    │  B1     = CANCEL    │
    ╰─────────────────────╯
```

**Controles dentro de view_fx_assign:**

| Control | Función |
|---------|---------|
| `ENC L` rotar | Cicla el param asignado a ENC L (todos los params del FX activo, en loop) |
| `ENC R` rotar | Cicla el param asignado a ENC R (todos los params del FX activo, en loop) |
| `ENC NAV` push | **Guardar** assignment y volver a view_08 |
| `B1` push | Cancelar sin guardar, volver a view_08 |

Sub-1 y sub-2 **pueden apuntar al mismo param** — se muestra sin error (útil si querés
el mismo param en dos encoders para resolución doble).

**Persistencia en el ESP32 (NVS):** los assignments se almacenan en el NVS del ESP32
(`nvs_set_u8` con namespace "fx_assign", key "fx{N}_sub{1|2}"`). Al boot el ESP32 carga
los assignments del NVS antes de registrar los handlers del bridge. Si no existe el NVS
(first boot o reset), se cargan los defaults de la tabla hardcoded original.

**Sincronización ESP32 → Teensy:** al guardar, el ESP32 envía un frame `GF_CMD_PARAM_CHANGED`
con param_id `0xF7` y el encoding de fx_id + sub1_idx + sub2_idx (ver §4.5). El Teensy
procesa este frame y actualiza la tabla de assignments en RAM — a partir del siguiente giro de
ENC L/R en modo FX, el Teensy envía el `bridge_send_param_changed()` para el param_idx correcto.

**Independencia de la sincronización:** el ESP32 puede mostrar el label correcto en view_07
(el nombre del param asignado) aunque el Teensy todavía no haya procesado el 0xF7. La
sincronización es best-effort con el delay de una poll de bridge (~1ms); en la práctica el
usuario no puede girar ENC L tan rápido como para que el Teensy reciba el giro antes del 0xF7.

**Nuevo handler en `bridge_handlers.cpp`:**

```cpp
// Handler 0xF7: ESP32 recibe de sí mismo en el bus local — en realidad
// el 0xF7 va ESP32 → Teensy. El ESP32 lo envía, no lo recibe.
// En el ESP32 sólo necesitamos guardar en NVS y actualizar estado local.

// En bridge_handlers.h:
uint8_t bridge_get_fx_sub1(uint8_t fx_id);   // 0-7 índice del param sub-1
uint8_t bridge_get_fx_sub2(uint8_t fx_id);   // 0-7 índice del param sub-2
void    bridge_set_fx_assignment(uint8_t fx_id, uint8_t sub1, uint8_t sub2);
```

**Índice en s_views de carousel.cpp:** agregar `VIEW_IDX_FX_ASSIGN` justo después de
`VIEW_IDX_FX_SELECT`. El carousel lo puede crear/destruir normalmente; la vista no tiene
timers ni estado persistente — todo el estado es NVS.

---

## 5. Test plan

### 5.1 Secuencia boot → FX performance

```
1. Power on Teensy + ESP32
2. Verificar: view_07 FX MAIN aparece con "Ghost Echo" (FX 0, último FX = default)
3. Girar ENC NAV → display muestra wet/dry cambiando (0.0–1.0)
4. Girar ENC L → display muestra sub-1 "time" cambiando
5. Girar ENC R → display muestra sub-2 "feedback" cambiando
6. Push ENC NAV → "BYPASS" indicator aparece en header
7. Push ENC NAV de nuevo → bypass se desactiva
Pass: todos los pasos sin error. Display siempre responsivo.
```

### 5.2 Selección de FX

```
1. Desde view_07 FX MAIN, presionar B1
2. Verificar: view_08 FX SELECT aparece con cursor en Ghost Echo
3. Girar ENC NAV → cursor cyclea entre los 9 FX
4. Detenerse en "Modal Reverb" (FX 1)
5. Push ENC NAV → vuelve a view_07 con "Modal Reverb" activo
6. Verificar: ENC L label = "size", ENC R label = "damping"
Pass: FX cambiado, performance picks actualizados correctamente.
```

### 5.3 Hold AI con cancelación

```
1. En cualquier modo, presionar y mantener ENC NAV
2. Verificar: entre t=0 y t=200ms NO aparece ningún feedback visual
3. A t=200ms (verificar con observación — no con cronómetro exacto):
   - Vista actual se atenúa perceptiblemente
   - Ring empieza a aparecer en borde del display
   - Core central empieza a pulsar
4. Soltar ENC NAV en ~t=2000ms (antes de 4s)
5. Verificar: ring desaparece, vista recupera 100% opacidad
6. Monitor serial ESP32: debe aparecer "AI arm cancelled (progress=..." con valor <1.0
Pass: sin transición a view_10. Vista FX restaurada.
```

### 5.4 Hold AI completo → view_10

```
1. En cualquier modo, presionar y mantener ENC NAV
2. Mantener hasta que el ring complete (aprox 4s)
3. Verificar: flash blanco en display
4. Verificar: view_10 AI PROCESSING aparece
5. Monitor serial: "AI arm activated" (progress=1.0)
Pass: transición limpia a AI mode.
```

### 5.5 MODE TOGGLE mid-edit

```
1. Cambiar a SYNTH mode (B2)
2. En view_02 SYNTH MAIN, girar ENC NAV hasta cursor en ENV_ATK (param 5)
3. Push ENC NAV → entra a modo edición de ENV_ATK (cursor fijo)
4. Presionar B2 → cambia a FX mode
5. Verificar: view_07 FX MAIN aparece, sin estado PARAM_EDIT residual
6. Presionar B2 de nuevo → vuelve a SYNTH mode
7. Verificar: cursor está en posición de lista (no en modo edición)
Pass: no hay estados colgados al cambiar de modo.
```

### 5.6 PANIC

```
1. Conectar MIDI keyboard al Teensy (USB-A host)
2. Presionar y mantener nota en teclado → sonido audible
3. Presionar B4 (PANIC)
4. Verificar: nota para inmediatamente (all notes off)
5. Con Ghost Echo activo, presionar B4 → tail de delay para (clear buffer)
Pass: silencio inmediato, sin notas colgadas, sin tail de delay residual.
```

### 5.7 Tests nativos (CI)

No aplica en este sprint. La lógica de navegación de UI depende de timers de hardware
(millis()), debounce de botones físicos, y estado de los encoders. No es testeable en
native sin un mock pesado del framework Arduino que introduciría más superficie de error
que el test mismo.

Los tests de puerta de CI relevantes para este sprint son:
- `pio test -e native -f "test_bridge/"` (protocol.h no cambió — tests existentes deben pasar)
- `pio run -e sketch` (build de sketch 27 refactorizado sin errores de compilación)

---

## 6. Out of scope — qué queda para sprints futuros

| Feature | Motivo de exclusión | Sprint tentativo |
|---------|--------------------|--------------------|
| Touch del display GC9A01 | Hardware disponible pero UX no diseñada. No bloquea performance. | Sprint 32 |
| Reasignación de ENC L en SYNTH (cutoff vs LFO) | La decisión cutoff permanente está cerrada para este sprint. | Backlog |
| B1–B4 reasignables por usuario | Función constante es un feature, no una limitación. Reasignación agrega confusión. | Backlog |
| AI mode con resultados de ML reales | view_10 existe pero ML inference (Sprints 22–26) es independiente de la navegación. | Ya implementado en Sprint 26 |
| **Presets FX** (FX + params tuneados completos + assignment, seleccionables sin tocar wet/dry en live) | Requiere estructura de datos de preset, EEPROM/NVS ampliado, bridge CMDs de save/load, y una vista de gestión de presets. Merita sprint propio. | **Sprint 31** |
| **Presets SYNTH** (patch completo estilo Serum/Diva: engine + VCO + ENV + LFO + FILTER) | Igual que presets FX — misma infraestructura de persistencia. Se implementan juntos en Sprint 31. | **Sprint 31** |
| Persistencia de last-used FX en boot (EEPROM) | Este sprint boot siempre en Ghost Echo (FX 0). EEPROM boot va junto con presets. | Sprint 31 |

### Nota sobre Sprint 31 — Presets

El modelo de presets acordado (sesión 2026-05-25) distingue dos tipos:

- **Preset FX** — snapshot de un FX completamente configurado: FX seleccionado + assignments
  ENC L/R + todos los parámetros (time, feedback, size, etc.). En performance el usuario sólo
  mueve wet/dry; todo lo demás ya viene "tuneado" en el preset. Equivale al preset de un
  pedal de efectos (Line 6 HX Stomp, Strymon BigSky).

- **Preset SYNTH** — patch completo del sintetizador: engine seleccionado + todos los
  parámetros de síntesis (VCO waveform, octave, detune, ENV ADSR, LFO rate/depth/target,
  FILTER cutoff/res/env-amount). Sin relación con el FX activo. Equivale al patch de Serum,
  Diva, o Minifreak.

Ambos se almacenan en EEPROM del Teensy (4 KB disponibles). Los presets FX necesitan ~40
bytes cada uno (1B fx_id + 2B assignments + 8×4B params float); presets SYNTH ~50 bytes
(1B engine_id + 12×4B params). Con 4 KB y overhead mínimo: ~30 presets FX + ~30 presets
SYNTH. Suficiente para v1.

---

## 7. References

**Productos:**
- Pioneer RMX-1000 User Manual (Roland Corporation, 2012) — layout de 3 knobs de performance: Scene FX L / Wet-Dry central / Scene FX R. Validación de mercado del paradigma 3-encoder sin cursor.
- Elektron Digitakt Manual v1.50 (Elektron Music Machines, 2023) — §6.3 "Parameter pages": 8 encoders por página, sin cursor dentro de página. Referente de performance UX con múltiples parámetros.
- Native Instruments Maschine MK3 Reference Manual (NI, 2018) — "Performance mode" con encoders fijos a parámetros críticos del instrumento activo.
- Apple Human Interface Guidelines — "Long Press" (https://developer.apple.com/design/human-interface-guidelines/gestures) — threshold, feedback progresivo, cancelación. Referente para hold gesture design.

**Académico:**
- Fitts, P. M. (1954). "The information capacity of the human motor system in controlling the amplitude of movement." *Journal of Experimental Psychology*, 47(6), 381–391. — Modelo de Fitts para tiempo de movimiento motor (MT = a + b·log2(2D/W)). Justificación cuantitativa de encoders separados vs encoder único con navegación.
- Norman, D. A. (1988/2013). *The Design of Everyday Things*. Basic Books. — Affordances, constraints, natural mapping. Referente para por qué los controles físicos deben mapear directamente a funciones sin modos invisibles.
- Tognazzini, B. (1992). *Tog on Interface*. Addison-Wesley. — Distinción modal vs spring controls. Capítulo "Modes". Contexto original del problema de modal blindness.
- Victor, B. (2011). "A Brief Rant on the Future of Interaction Design." http://worrydream.com/ABriefRantOnTheFutureOfInteractionDesign — Crítica a controles modales invisibles en interfaces táctiles. Aplicable directamente al problema de SHIFT modal en sketch 27.

**Specs del proyecto:**
- `apps/docs/01-architecture.md §3.3` — Pin mapping de hardware: GPIO 2-5 para B1-B4, GPIO 10/11/SW para ENC L, GPIO 13/14/SW para ENC R, GPIO 16/17/SW para ENC NAV.
- `apps/docs/02-bridge-protocol.md §1` — Frame format. Param_ids 0xFB-0xFF usados con GF_CMD_PARAM_CHANGED.
- `apps/docs/05-fx-architecture.md` — Lista de los 9 FX signature y sus parámetros disponibles.

---

## 8. Learnings

*Esta sección se completa después de la implementación. Qué salió distinto al plan, qué
sorprendió, qué estimación estuvo incorrecta.*

---

**Status:** SPEC CERRADO — listo para handoff a Firmware Engineer.
El firmware engineer debe implementar §4.1 (sketch 27 refactor) primero, verificar con
§5.1–5.2 (test básico), luego implementar §4.4 (views de display), verificar con §5.3–5.6
(tests completos).
