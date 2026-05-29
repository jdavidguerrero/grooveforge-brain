# Sprint 34 — Camelot Hero View + Modes (Karaoke + AI Feed)

> **Fase:** 4 — UX hero / diferenciador
> **Estimado:** 2-3 sesiones
> **Status:** 🟡 In Progress
> **Refs:** Sprint 33 (`33-ai-hero-viz.md`) — pivot del CoF cromagram

---

## Contexto del pivot

Sprint 33 implementó el chromagram Circle of Fifths con 8 capas (chromagram, tonic, diatonic mask, triada, BPM, core pulse, bar sweep, snap arrow). Test on-device reveló:

1. **La info correcta está ahí, pero no se autoexplica** — el usuario no sabía cómo leer la viz.
2. **Stale state visible** — labels persistían tras parar de tocar.
3. **Foco difuso** — 13 capas competían por 240×240 pixels.

Sprint 34 **revisa la decisión de diseño**: en vez de seguir agregando capas al chromagram, **adoptamos vocabulario musical-DJ reconocible** y reestructuramos jerarquía para que "qué hace la AI" sea el héroe.

---

## Theory

### Por qué Camelot Wheel sobre CoF puro

**Camelot Wheel** (de Mixed In Key, 2006) asigna a cada tonalidad un código `<número><letra>`:
- Número 1-12 = posición en el círculo de quintas
- Letra A/B = minor/major
- Cada número tiene un **color asociado** del rainbow

| CoF Pos | Pitch | Camelot Major | Camelot Minor | Color (HSV hue) |
|---|---|---|---|---|
| 0 | C | 8B | 8A (Am) | 187° cyan |
| 1 | G | 9B | 9A (Em) | 207° azul-cyan |
| 2 | D | 10B | 10A (Bm) | 220° azul |
| 3 | A | 11B | 11A (F#m) | 245° indigo |
| 4 | E | 12B | 12A (C#m) | 282° púrpura |
| 5 | B | 1B | 1A (G#m) | 358° rojo |
| 6 | F# | 2B | 2A (D#m) | 14° rojo-naranja |
| 7 | C# | 3B | 3A (A#m) | 30° naranja |
| 8 | G# | 4B | 4A (Fm) | 48° amarillo |
| 9 | D# | 5B | 5A (Cm) | 71° verde-limón |
| 10 | A# | 6B | 6A (Gm) | 110° verde |
| 11 | F | 7B | 7A (Dm) | 153° verde-teal |

### Por qué funciona visceralmente

1. **Color = identidad** — el usuario asocia "estoy en zona naranja" con C# major en 5min de uso. No requiere leer texto.
2. **Adyacencia = compatibilidad** — segmentos vecinos en la rueda son tonos amigos. Una modulación V→I se ve como un **shift sutil de color**.
3. **Vocabulario existente** — todo DJ con Mixed In Key, Rekordbox o Serato reconoce "8B" o "11A". Bridge a la cultura productor/DJ.
4. **Fotografiable** — un ring rainbow con 7 segmentos brillando = imagen de Instagram. Marketing orgánico.

### Por qué el hero ahora es "qué hace la AI"

Feedback del usuario: el diferenciador del dispositivo no es "que muestra notas" — es que **una AI embedded está analizando en vivo**. Esa AI debe ser la protagonista visible:

- **Status line explícita** ("KEY LOCKED · A major", "SNAP G# → A")
- **Flashes visuales** en el chromagram cuando la AI dispara eventos
- **Confidence indicator** (futuro)
- **Camelot code chip** ("11A") que es UI exclusivo de AI mode

### Por qué múltiples modos

Un solo layout no sirve a todos los casos:
- **Camelot** = sesión análisis (productor / improvisador)
- **Karaoke** = aprendizaje / foco en presente (estudiante)
- **AI Feed** = debug / showcase (curioso / demo)

Cada modo es una vista del carousel con ENC NAV rotation cycling. Comparten data layer (todos los getters bridge) pero renderizan distinto.

### Relevancia para GrooveForge Brain

Esto es **el hero feature** del dispositivo. Si alguien filma un Reel de 3s de view_10 mostrando Camelot colors + AI status fire en vivo → eso vende el producto. El resto de los sintetizadores y FX son commodity.

### Referencias

- Mixed In Key documentation (Camelot system) — https://mixedinkey.com/harmonic-mixing-guide/
- Wikipedia: Camelot Wheel / Circle of Fifths
- Endlesss (loop community app) — chromagram circular precedent
- Roland Aerophone / DJ controllers — color-coded key indicators

---

## Wiring

N/A — sprint solo software. La GUI corre en ESP32-S3 con display GC9A01 ya wired desde Sprint 3.1.

---

## Implementation

### Paleta Camelot — `apps/firmware-esp32/src/display/ui_theme.h`

12 colores HSV→RGB que coinciden con la convención Mixed In Key. Ordenados por posición CoF (0=C, 1=G, …, 11=F).

```cpp
// ── Camelot Wheel colors (Sprint 34) ─────────────────────────────────────────
// Indexed by Circle of Fifths position: COF_POS_TO_CAMELOT_COLOR[p]
// Position 0=C is Camelot 8B (cyan).
#define GF_COLOR_CAMELOT_0   lv_color_make(0x1F, 0xB6, 0xBB)  // C  — 8B  cyan
#define GF_COLOR_CAMELOT_1   lv_color_make(0x1D, 0x86, 0xC4)  // G  — 9B  azul-cyan
#define GF_COLOR_CAMELOT_2   lv_color_make(0x33, 0x58, 0xC0)  // D  — 10B azul
#define GF_COLOR_CAMELOT_3   lv_color_make(0x55, 0x40, 0xB8)  // A  — 11B indigo
#define GF_COLOR_CAMELOT_4   lv_color_make(0x8A, 0x2D, 0xB5)  // E  — 12B púrpura
#define GF_COLOR_CAMELOT_5   lv_color_make(0xB6, 0x1F, 0x32)  // B  — 1B  rojo
#define GF_COLOR_CAMELOT_6   lv_color_make(0xDD, 0x4D, 0x28)  // F# — 2B  rojo-naranja
#define GF_COLOR_CAMELOT_7   lv_color_make(0xDD, 0x90, 0x2C)  // C# — 3B  naranja
#define GF_COLOR_CAMELOT_8   lv_color_make(0xDD, 0xB4, 0x2A)  // G# — 4B  amarillo
#define GF_COLOR_CAMELOT_9   lv_color_make(0xB0, 0xCC, 0x30)  // D# — 5B  verde-limón
#define GF_COLOR_CAMELOT_10  lv_color_make(0x74, 0xC4, 0x43)  // A# — 6B  verde
#define GF_COLOR_CAMELOT_11  lv_color_make(0x2E, 0xBB, 0x87)  // F  — 7B  verde-teal
```

Helper inline:
```cpp
static inline lv_color_t gf_camelot_color(uint8_t cof_pos) {
    static const lv_color_t T[12] = {
        GF_COLOR_CAMELOT_0, GF_COLOR_CAMELOT_1, GF_COLOR_CAMELOT_2,
        GF_COLOR_CAMELOT_3, GF_COLOR_CAMELOT_4, GF_COLOR_CAMELOT_5,
        GF_COLOR_CAMELOT_6, GF_COLOR_CAMELOT_7, GF_COLOR_CAMELOT_8,
        GF_COLOR_CAMELOT_9, GF_COLOR_CAMELOT_10, GF_COLOR_CAMELOT_11,
    };
    return T[cof_pos % 12];
}
```

### Mapeo CoF → Camelot code

```cpp
// CoF position → Camelot number (1-12)
static const uint8_t COF_TO_CAMELOT_NUM[12] = { 8, 9, 10, 11, 12, 1, 2, 3, 4, 5, 6, 7 };
// Major key letter = 'B', minor = 'A'
// key_idx 0-11 = major, 12-23 = minor
```

Para key_idx=3 (A major): pos en CoF = 3, num = 11, letter = 'B' → **"11B"**
Para key_idx=15 (A minor): pos = 3, num = 11, letter = 'A' → **"11A"**

### Batches

| # | Batch | Archivos | Output |
|---|---|---|---|
| **A** | Camelot palette + helper + COF_TO_CAMELOT tables en ui_theme.h | `ui_theme.h` | tabla disponible para views |
| **B** | view_10 chromagram refactor: cada seg con su `gf_camelot_color()`, labels INSIDE bien legibles, NO diatonic mask de opacity (los colores diferencian) | `view_10_ai_processing.cpp` | rainbow ring visible |
| **C** | Chord card hero al centro (reemplaza el "A" floating) | `view_10_ai_processing.cpp` | "Am7" en card |
| **D** | AI status line con priority state machine + flashes sincronizados | `view_10_ai_processing.cpp`, `bridge_handlers.cpp` | "KEY LOCKED · A" / "SNAP G#→A" |
| **E** | Camelot code chip ("11A") debajo del chord card | `view_10_ai_processing.cpp` | "11A" / "11B" / "—" |
| **F** | Eliminar triada (los colores Camelot ya muestran chord), bar sweep en peripheral, simplificar | `view_10_ai_processing.cpp` | layout limpio |
| **G** | view_26 KARAOKE — nota actual HUGE + AI commentary line | `view_26_karaoke.cpp`, `views.h`, `carousel.cpp` | vista alternativa |
| **H** | view_27 AI_FEED — timeline scroll de eventos AI | `view_27_ai_feed.cpp`, `views.h`, `carousel.cpp` | vista log |
| **I** | Nav: ENC NAV rotation en AI mode cicla view_10 → view_25 → view_16 → view_26 → view_27 | `bridge_handlers.cpp` param 0x00F1 → 0-4 + sketch 28 | navegación completa |

### Estados de la AI status line (priority)

```
SNAP (2s post-event)    > "▶ SNAP · G# → A"          highest priority
CHORD (last 5s)         > "▶ CHORD · Am7"
KEY LOCKED              > "▶ KEY LOCKED · A major"
KEY DETECTING           > "● DETECTING KEY..."
ACTIVITY but no key     > "● LISTENING..."
IDLE (>5s sin notas)    > "○ Play notes to engage AI"   lowest priority
```

`▶` = active event, `●` = analyzing, `○` = idle. Color teal cuando active, gray cuando idle.

### Flashes en chromagram (visual AI feedback)

| Evento AI | Visual |
|---|---|
| KEY locked nuevo | los 7 segmentos diatónicos brillan brevemente (boost +30% opa 300ms) |
| CHORD detected nuevo | los 3 segmentos del chord (root, 3rd, 5th) flash teal overlay 200ms |
| SNAP fired | segmento `snap_from%12` flash rojo 100ms → arc teal 200ms → segmento `snap_to%12` flash teal 200ms |
| BPM locked | el bar sweep aparece de fade-in 500ms (antes no estaba visible) |

### Modos = sub-vistas en AI mode

Param `0x00F1` ahora con 5 valores:
```
0.0 → view_10 (AI PROC Camelot)    default al entrar
1.0 → view_25 (AI MODELS)
2.0 → view_16 (SCALE LOCK)
3.0 → view_26 (KARAOKE)
4.0 → view_27 (AI FEED)
```

ENC NAV rotation en AI mode: cycle 10 → 25 → 16 → 26 → 27 → 10 (loop).

---

## Demo

### Camelot mode (view_10) sin Teensy

```bash
cd tools/bridge-test

# Forzar key A major (key_idx=3)
python3 send_ml_results.py --port /dev/cu.usbmodem* --cmd KEY --key 3 --conf 90

# Forzar chord Am7
python3 send_ml_results.py --port /dev/cu.usbmodem* --cmd CHORD --root 9 --qual 3 --conf 85

# Forzar BPM 128
python3 send_ml_results.py --port /dev/cu.usbmodem* --cmd BEAT --bpm 128
```

**Esperado visualmente:**
- Rainbow ring con 12 colores Camelot
- Los segmentos D, A, E, B, F#, C#, G# (escala de A major) brillan más
- Centro: "Am7" card
- Debajo: "11A" Camelot code
- Status line: "▶ KEY LOCKED · A major"

### Karaoke mode (view_26)

Navegar con: `python3 send_frame.py --cmd PARAM_CHANGED --param 241 --value 3`

Esperado: nota actual HUGE (e.g. "A4"), AI commentary debajo ("Am7 in A major").

### AI Feed (view_27)

Navegar con: `python3 send_frame.py --cmd PARAM_CHANGED --param 241 --value 4`

Esperado: lista scroll de últimos N eventos AI:
```
[2.1s] CHORD: Am7
[1.5s] KEY: A major locked
[0.8s] SNAP: G# → A
[0.3s] BEAT: 120 BPM
```

---

## Tests

| Test | Tipo | Status |
|---|---|---|
| Build esp32s3 limpio | CI | TBD |
| Camelot color tabla mapping correcto (manual) | unit | TBD |
| Navegación entre 5 sub-vistas vía 0x00F1 | manual | TBD |

---

## Learnings

*(Completar post-implementación)*

---

*Sprint 34 documentado: 2026-05-28*
*Siguiente sprint: 35 — refinamiento + Scale Lock view_16 redesign*

---

## Batch J — Manual scale override (2026-05-28)

Control manual de escala desde ENC L/R en view_16 + redesign de view_16.

### Gestures

- **ENC L rotation** (en AI mode + sub-vista 2 = view_16): cambia root C→C#→…→B, entra MANUAL mode.
- **ENC R rotation** (ídem): toggle major ↔ minor, entra MANUAL mode.
- **ENC L/R push**: siguen siendo bypass toggles (sin cambio).
- **B1 HOME**: al salir de AI mode resetea a AUTO y notifica al ESP32.

### Nuevo param `0x00EF`

Nota: el spec original propuso `0x00F6`, pero ese ID está ocupado para SYNTH group nav
(`group*10 + cursor`). Se migró a `0x00EF` (libre en el rango 0x00E8-0x00EE).

- `value < 0` → AUTO mode (limpiar override)
- `value 0-23` → MANUAL key_idx (0-11 major, 12-23 minor)

### ScaleLock API añadida

`set_manual_key(idx)`: fija key, reconstruye `_mask` inmediatamente, AI no sobrescribe.
`clear_manual()`: libera override; próximo `update()` con confianza ≥ 0.75 restaura AI.
`is_manual()`: getter de estado.
`update()`: guard `if (_manual_mode) return;` al inicio.

### view_16 rediseñada

- Mock estático de 36 líneas → vista dinámica funcional con timer a 50ms (20fps).
- Widgets: mode badge `[AUTO]`/`[MANUAL]`, scale name hero en color Camelot, código Camelot (`8B`/`11A`), 12 dots cromagram (diatónicas bright Camelot, cromáticas dim), labels nota, hint fade 3s+1s, stats `snap N/M`, pill ACTIVE/BYPASS.

### Archivos modificados

- `apps/firmware-teensy/src/ml/scale_lock.h` — API pública: `set_manual_key`, `clear_manual`, `is_manual`; privados: `_manual_mode`, `_manual_key_idx`, `_active_key`
- `apps/firmware-teensy/src/ml/scale_lock.cpp` — impl de los tres métodos + guard en `update()`
- `apps/firmware-teensy/src/sketches/28-synth-navigator.cpp` — globals `g_scale_manual_*`, gesture block en `handle_encoders()`, reset en `handle_buttons()` B1 HOME
- `apps/firmware-esp32/src/bridge/bridge_handlers.h` — `bridge_get_scale_manual_mode()`, `bridge_get_scale_manual_key()`
- `apps/firmware-esp32/src/bridge/bridge_handlers.cpp` — state `s_scale_manual_*`, getters, handler `0x00EF`
- `apps/firmware-esp32/src/display/screens/views/view_16_scale_lock.cpp` — rewrite completo
