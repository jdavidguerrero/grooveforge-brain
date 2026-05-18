# Sprint 3.4 (anticipado) — UI completa en modo carrusel

> **Fase:** 3 — UI + Display
> **Duración estimada:** 4-5 sesiones
> **Estado:** En implementación · Mayo 2026
> **Depende de:** Sprint 3.1 (`17-esp32-display.md` — LVGL + GC9A01 operativo)
> **Referencias:** `apps/docs/ui-mocks/*.html` · `01-architecture.md §3.4` · `06-implementation-roadmap.md §Fase 3`

---

## Objetivo

Implementar **las 23 vistas** del set de mocks (`apps/docs/ui-mocks/`) y mostrarlas
en un **carrusel automático** que avanza una vista cada 5 s, en bucle infinito.

El roadmap define Sprint 3.4 como *"Full UI menu navigation"*, pero los encoders
y botones (Sprint 3.3) aún no están cableados. Para no bloquear el avance visual
de la UI, se adelanta el trabajo de **diseño y render de las 23 vistas** en un modo
demo sin entrada: el carrusel. Cuando el hardware de control exista, el mismo set
de vistas se conecta a navegación real — el carrusel se descarta o se reusa como
screensaver/demo mode.

Esto es un reorden pragmático: `06-implementation-roadmap.md` es un documento
*living*, y adelantar las vistas no toca Sprint 3.2 (Bridge Protocol) ni 3.3
(encoders). No viola ningún phase-gate: el hito de Fase 3 sigue siendo "Brain
operable con encoders" — esto solo prepara el render.

**Criterio de pass:** las 23 vistas renderizan con fidelidad pixel-perfect a los
mocks (paleta, layout, geometría, animaciones), el carrusel cicla cada 5 s con
transición radial-wipe, y el heap LVGL queda plano tras ≥3 bucles completos.

---

## Teoría

### 1. Patrón screen-manager: tabla de descriptores + `lv_timer`

LVGL ofrece widgets contenedores para multi-pantalla (`lv_tileview`, `lv_tabview`)
pero todos mantienen **todas** las pantallas vivas en memoria simultáneamente. Con
23 vistas y un heap de 96 KB eso es inviable.

El patrón elegido es un **screen-manager imperativo**: una tabla ordenada de
descriptores `{create_fn, destroy_fn, name, dwell_ms}`. Solo existe una vista a la
vez. Al avanzar:

```
destroy_fn()  →  gf_anim_kill_all()  →  lv_obj_clean(scr)  →  create_fn(next)
```

`lv_obj_clean()` destruye recursivamente todos los hijos de la pantalla (widgets +
sus estilos asociados) sin destruir la pantalla raíz. Resultado: la memoria pico
es `max(vista_i)`, no `Σ(vista_i)`.

El avance se dispara con un **`lv_timer`** (no con `millis()` en `loop()`). Razón:
`lv_timer` ya lo sirve `lv_task_handler()` en cada iteración del loop, comparte la
base de tiempo de LVGL, y se pausa/reanuda limpiamente durante la transición. Una
comprobación `millis()` paralela duplicaría la lógica de timing y arriesgaría
desincronización con las animaciones.

### 2. La transición radial-wipe y `lv_layer_top()`

El mock 09 ("Mode Switch") especifica una transición de 400 ms: wipe radial
purple→teal expandiéndose desde el centro, 200 ms de salida + 200 ms de entrada.

El problema: `lv_obj_clean(lv_scr_act())` destruye **todos** los hijos de la
pantalla activa — incluido cualquier objeto de transición que viviera ahí. La
solución es el **top layer** de LVGL: `lv_layer_top()` devuelve una capa que se
dibuja por encima de la pantalla activa y **no es hija de `lv_scr_act()`**, así
que sobrevive al `lv_obj_clean`. El objeto circular del wipe vive ahí, se crea una
vez al arrancar el carrusel, y se oculta entre transiciones.

### 3. `lv_canvas` line-art en vez de `lv_chart` / `lv_list`

Los mocks citan `lv_chart` (ENV, LFO) y `lv_list` (engine/FX/preset select). Ambos
se sustituyen deliberadamente:

- **`lv_chart` → `lv_canvas`:** una curva ADSR o una onda LFO es geométricamente
  una polilínea. `lv_chart` arrastra buffers por-serie y maquinaria de ejes para
  algo que `lv_canvas_draw_line()` resuelve directo. Como los glifos de waveform y
  los 12 glifos FX **ya** requieren `lv_canvas`, consolidar en una sola técnica
  evita habilitar un widget pesado.
- **`lv_list` → `lv_obj` + labels:** `lv_list` trae scroll y layout flex. El
  carrusel muestra un **frame estático** (sin entrada, sin scroll real), así que
  las filas se simulan con contenedores `lv_obj` posicionados manualmente. Evita
  habilitar `LV_USE_LIST` + `LV_USE_FLEX`.

Net: solo se habilitan `LV_USE_BAR`, `LV_USE_CANVAS`, `LV_USE_LINE`.

### 4. Pipeline de fuentes: TTF → C-array LVGL

LVGL no lee TTF en runtime (salvo con FreeType, demasiado pesado para el MCU).
Las fuentes se **pre-rasterizan**: una herramienta (`lv-font-conv`) toma el TTF y
genera un `.c` con la fuente como bitmap embebido — un `lv_font_t` que se enlaza
al binario.

Decisiones:
- **bpp 4** (16 niveles de antialiasing). 1-bpp se ve dentado en una UI premium;
  4-bpp es el balance estándar calidad/tamaño.
- **6 tamaños** (8/11/16/22/36/64) en vez de los ~23 que citan los mocks. En un
  display de 240 px la diferencia entre 7 px y 8 px es imperceptible; el "snap" a
  6 tamaños satisface la intención pixel-perfect dentro de la tolerancia de render.
- El glifo de 64 px se genera con **set restringido** (`0-9 # A-G` + espacio) — la
  nota hero del synth solo necesita esos caracteres → ~40 KB en vez de ~210 KB.

### 5. Ciclo de vida de animaciones

Dos tipos de animación coexisten:
- **One-shot / finitas** (`lv_anim_t` con duración): fades del boot, sweep de arcs
  al entrar, scale-in del checkmark, flash de error ×3. LVGL las libera al terminar.
- **Recurrentes / infinitas**: onda LFO scrolleando, órbita de 8 puntos del AI,
  anillo idle rotando 12 s, breathe del core, pulso REC, peak meters, FSM del MIDI
  Learn. Se implementan con `lv_anim` `LV_ANIM_REPEAT_INFINITE` o con `lv_timer`.

**El riesgo:** al destruir una vista, una animación pendiente cuyo `exec_cb`
apunta a un `lv_obj_t*` ya liberado dereferencia memoria liberada → crash. Igual
con `lv_timer`s.

**La red de seguridad — `gf_anim_kill_all()`**, llamado *antes* de `lv_obj_clean`:
- `lv_anim_del(NULL, NULL)` — borra **todas** las animaciones, sin importar target.
- `lv_timer`s: cada vista que crea un timer recurrente lo registra; `gf_anim`
  mantiene un array de timers de scope-carrusel y los borra a todos.

Orden estricto en cada cambio de vista:
`destroy() → gf_anim_kill_all() → lv_obj_clean() → create()`.

---

## Contradicciones de spec detectadas

| # | Contradicción | Resolución |
|---|---|---|
| 1 | Mocks dicen `FIRMWARE TARGET: LVGL 9`; firmware usa `lvgl@^8.3.0` | Se mantiene **LVGL 8.x**. LVGL 9 rompería `lv_port_disp`, `screen_boot`, `screen_main` recién estabilizados. Los mocks describen comportamiento, no API → sin impacto. |
| 2 | Comentario en `main.cpp` dice `LV_TICK_CUSTOM=1`; `lv_conf.h` real tiene `=0` | El código (`=0` + `lv_tick_inc(5)` en loop) es el correcto y validado. El comentario está obsoleto — se corrige. |
| 3 | `lv_port_disp.h` documenta backlight en GPIO 40; código usa GPIO 2 (`TFT_BL=2`) | El código es autoritativo (GPIO 2, validado contra groove_drum). |
| 4 | Mocks asumen entradas ENC L/R/NAV + B1-B4 que no existen aún | Cada vista se renderiza como **frame representativo estático** + sus micro-animaciones. El carrusel reemplaza la navegación. |

> Ninguna de estas se "resuelve" modificando los specs SSoT `00-06.md`. Son notas
> de implementación; los mocks (`ui-mocks/`) no son SSoT inmutable.

---

## Design tokens

### Paleta (10 colores — reemplaza la paleta de Sprint 3.1)

| Token | Hex | Uso |
|---|---|---|
| `GF_COLOR_BG` | `0x0A0A0A` | Fondo antracita |
| `GF_COLOR_TEAL` | `0x1D9E75` | Acento primario, arcs, títulos |
| `GF_COLOR_TEAL_PLUS` | `0x5DCAA5` | Activo / brillante, animaciones |
| `GF_COLOR_TEAL_DIM` | `0x0E5040` | Teal apagado (anillo idle, divisores) |
| `GF_COLOR_PURPLE` | `0x534AB7` | Chrome del modo FX |
| `GF_COLOR_PURPLE_BRIGHT` | `0x7A6FE0` | FX activo, arcs FX |
| `GF_COLOR_GRAY` | `0x9FE1CB` | Labels (gris con tinte menta) |
| `GF_COLOR_RED` | `0xE84A4A` | Error / clip |
| `GF_COLOR_AMBER` | `0xE8B84A` | Warning / "DO NOT POWER OFF" |
| `GF_COLOR_WHITE` | `0xFFFFFF` | Texto hero |

### Tipografía — IBM Plex Mono, 6 tamaños

| Token | px | Cubre mocks | Uso |
|---|---|---|---|
| `GF_FONT_MICRO` | 8 | 6-9 | micro labels, hints, ETA |
| `GF_FONT_LABEL` | 11 | 10-12 | arc titles, params, mode pill |
| `GF_FONT_BODY` | 16 | 13-18 | state text, FX name |
| `GF_FONT_TITLE` | 22 | 20-26 | FX hero, scale name, preset |
| `GF_FONT_HERO` | 36 | 28-42 | BPM, dB, OTA % |
| `GF_FONT_GIANT` | 64 | 48-64 | nota hero del synth (set restringido) |

### Geometría

| Token | Valor | Significado |
|---|---|---|
| `GF_DISP_W/H` | 240 | Resolución física |
| `GF_DISP_CX/CY` | 120 | Centro |
| `GF_SAFE_R` | 110 | Radio zona segura (Ø 220, 10 px de margen) |
| `GF_ARC_TITLE_R` | 90 | Radio baseline del arc-title |
| `GF_ARC_TITLE_DEG` | 30-150 | Span angular del arc-title (top) |
| `GF_MODE_PILL_Y` | 224 | Centro-y del mode pill |
| `GF_PAGE_DOTS_Y` | 198 | y de los page dots |
| `GF_DOT_R_ACTIVE/IDLE` | 3 / 2 | Radio dot activo/idle |
| `GF_OVERLAY_SCRIM_OPA` | 140 | Opacidad scrim de overlays (~0.55) |

---

## Implementation

### Estructura de archivos

```
src/display/
├── ui_theme.h              tokens (paleta/fuentes/geometría)
├── lv_port_disp.cpp        DISP_BUF_LINES 20→40
├── fonts/                  ibm_plex_mono_{8,11,16,22,36,64}.c + gf_fonts.h
├── carousel/
│   ├── carousel.cpp/.h     tabla de 23 vistas, lv_timer, ciclo de avance
│   └── view_transition.cpp/.h   radial-wipe 400ms sobre lv_layer_top()
├── widgets/
│   ├── gf_widgets.cpp/.h   arc title, mode pill, page dots, hero label, bg
│   ├── gf_glyphs.cpp/.h    lv_canvas line-art (waveforms, 12 glifos FX)
│   └── gf_anim.cpp/.h      registro de timers + gf_anim_kill_all()
└── screens/
    ├── screen_boot.cpp     migrado a tokens GF_*
    ├── screen_main.cpp     referencia (cuerpo de view_02)
    └── views/
        └── view_{01..23}_*.cpp/.h
```

**Contrato de vista** (`view_NN_*.h`):
```c
void view_NN_create(lv_obj_t* parent);   /* construye los widgets */
void view_NN_destroy(void);              /* opcional — solo si registra lv_timer */
```

### Cambios en `lv_conf.h`

- Habilitar: `LV_USE_BAR 1`, `LV_USE_CANVAS 1`, `LV_USE_LINE 1`.
- `LV_MEM_SIZE` 64 KB → **96 KB**. Buffers de canvas grandes (ADSR, LFO) van a
  **PSRAM** vía `heap_caps_malloc(MALLOC_CAP_SPIRAM)` para no consumir el pool.
- Fuentes Montserrat → `0`; `LV_FONT_DEFAULT` → `&ibm_plex_mono_16`.
- Dev: `LV_USE_MEM_MONITOR 1` + `LV_USE_PERF_MONITOR 1` (a `0` en build de demo).

### Carrusel — orden de las 23 vistas

`01 Boot · 02 Synth Main · 03 OSC · 04 ENV/ADSR · 05 LFO/MOD · 06 Engine Select ·
07 FX Main · 08 FX Select · 09 Mode Switch · 10 AI Processing · 11 Idle ·
12 Preset Browser · 13 Insert Layer · 14 Send Layer · 15 Master Layer ·
16 Scale Lock · 17 WiFi Setup · 18 AI Suggestion · 19 DAW Connected · 20 OTA ·
21 Error States · 22 Volume Overlay · 23 MIDI Learn`

### Batches de implementación

| Batch | Contenido | Reto técnico clave |
|---|---|---|
| A | Fundación: ui_theme, fuentes, lv_conf, gf_widgets/glyphs/anim, carousel, transition, main.cpp | radial-wipe sobre `lv_layer_top()`; canvas buffer sizing |
| B | 02 Synth, 03 OSC, 01 Boot | fuente 64px en círculo; glifo waveform en canvas |
| C | 04 ENV/ADSR, 05 LFO | onda LFO animada 30 fps con timer + cleanup |
| D | 07 FX Main, 08 FX Select, 06 Engine Select, 09 Mode Switch | 12 glifos FX line-art |
| E | 17 WiFi, 19 DAW, 20 OTA, 16 Scale Lock | cascada de arcs; ring de degree dots |
| F | 10 AI, 11 Idle, 18 AI Suggestion | órbita 8 puntos; anillo rotando 12 s |
| G | 13/14/15 capas FX, 22 Volume, 21 Error | peak meters 30 fps; flash error ×3 |
| H | 12 Preset Browser, 23 MIDI Learn | FSM 3-estados A→B→C dentro del slot de 5 s |

---

## Demo / criterios de aceptación

1. `cd apps/firmware-esp32 && pio run -e esp32s3` compila; el linker muestra
   +~170 KB de flash por las fuentes, bajo el límite de `huge_app.csv`.
2. En el GC9A01 físico: boot anima una vez → el carrusel arranca en la vista 01.
3. Avanza cada ~5 s con transición radial-wipe; un bucle completo (23 vistas,
   ≈2 min) coincide con cada mock en paleta, layout y micro-animación.
4. Tras la vista 23 hace wrap a la 01 sin flicker ni widgets residuales.
5. **No-leak:** con `LV_USE_MEM_MONITOR 1`, tras ≥3 bucles (~6 min) el `used%`
   oscila sobre una baseline estable (subida monótona = leak). Print serial en
   `carousel_tick` con `ESP.getFreeHeap()` — debe quedar plano.

**Evidencia requerida:** video de un bucle completo de 23 vistas + captura del
`MEM_MONITOR` mostrando heap plano.

---

## Learnings

- **`TFT_RST` en pin equivocado = pantalla negra silenciosa.** El bug previo a
  este sprint: `TFT_RST=12` en vez de `14`. El SPI transmitía bien (flush
  callbacks corrían) pero el GC9A01 nunca recibía hardware-reset → controlador
  en estado indefinido. Lección: validar pines contra hardware de referencia
  conocido (groove_drum), no contra discusiones genéricas.
- **Reset por RTS, no DTR, para capturar el serial.** El monitor pyserial solo
  capturó la salida de la app al pulsar EN vía RTS (`setRTS`), no con DTR.
- **IBM Plex Mono no trae glifos geométricos** (● ▶ ✓ ▲). Se dibujan como
  vector line-art en `gf_glyphs` — coherente con la spec de los mocks.
- **`lv-font-conv` necesita `LV_LVGL_H_INCLUDE_SIMPLE`.** Sin ese flag los `.c`
  generados hacen `#include "lvgl/lvgl.h"` y no compilan.
- **El modelo "una vista viva a la vez" funciona perfecto:** heap LVGL plano
  (delta=0) a lo largo de un loop completo de 23 vistas — verificado 3×.
- **Callbacks de transición diferidos** (lv_timer one-shot desde el ready_cb de
  la animación) evitan reentrancia entre `lv_anim_del` y el procesamiento de
  animaciones en curso. Cero crashes.
- **Canvas en PSRAM:** `psramInit()` confirma PSRAM activa; los buffers de
  canvas (glifos, ondas) viven ahí sin tocar el pool de 96KB de LVGL.

### Pendiente / fuera de alcance cumplido

- Arc-title curvo: implementado con glifos posicionados sobre el arco (sin
  rotar) — la curvatura es suave a 11px y legible.
- Navegación real con encoders → Sprint 3.4 cuando exista el hardware; el set
  de 23 vistas ya está listo para conectarse a entrada real.
