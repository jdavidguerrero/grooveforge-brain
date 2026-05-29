# Sprint 45 — AI Mode Consolidation

> **Fase:** 6 — Layer 1 AI completo
> **Status:** 🟢 Done — pendiente verificación on-device
> **Refs:** `apps/docs/06-implementation-roadmap.md §0.1.7` (AI que cambia el sonido, no la pantalla)

## Theory

### Concepto central

El modo AI acumuló 5 sub-vistas, de las cuales 3 son puro display (Camelot,
Karaoke, Feed) y solo 2 controlan algo. Al mismo tiempo, 3 features que SÍ
cambian el sonido (Auto-Harmonize, Smart Arpeggiator, Groove Humanizer) no
tenían control en pantalla.

Este sprint corrige el desbalance: consolida el control de los 6 features
sound-changing en una sola vista "AI RACK", y reduce el ciclo de sub-vistas
de 5 a 3.

### Por qué consolidar y no agregar más vistas

El roadmap (§0.1.7) establece "AI que cambia el sonido, no la pantalla" como
principio, y lista como anti-pattern "AI que solo cambia la pantalla y se
vende como buy-reason". Agregar 3 vistas más (una por feature) iría en esa
dirección. Una control surface única es más densa, más rápida de navegar
(3 clics de ciclo vs 6), y agrupa los buy-reasons donde el usuario los percibe
como una unidad: "tools que modifican lo que toco".

### Las 3 vistas que quedan

| # | Vista | Rol |
|---|---|---|
| 0 | Camelot (view_10) | VER — key/chord/BPM hero viz (firma visual del producto) |
| 1 | AI Rack (view_28) | CONTROLAR — 6 features en filas con ENC L/R |
| 2 | Scale Lock (view_16) | DETALLE — chromagram + override manual (gesto único ENC L/R) |

Karaoke (view_26) y AI Feed (view_27) salen del ciclo principal. Su código
permanece para un futuro modo aprendizaje/debug.

### Layout de la AI Rack

6 filas, ENC NAV selecciona fila, ENC L/R ajustan la fila activa:

```
SCALE LOCK   ON        (ENC L de la sub-vista 0: toggle via 0x00F2)
HARMONIZE    3rd       (ENC L: on/off · ENC R: 3rd/6th)
ARP     SMART · 1/8   (ENC L: mode · ENC R: division)
GROOVE  HUMAN ▓▓▓░    (ENC L: style · ENC R: amount)
BEAT FX      ON        (ENC L: on/off via 0x00F3)
VELOCITY     AUTO      (status estático — auto-calibra, sin control)
```

### Encoding de params Bridge (IDs 0x00E9–0x00EE)

Los IDs 0x00E0–0x00E7 son bandas de spectrum, 0x00E8 es fx bypass, 0x00EF ya
está ocupado (manual scale override). Los IDs 0x00E9–0x00EE son los únicos
libres del rango especial (byte-alto=0, byte-bajo=0xE0–0xEF).

| param_id | Feature | Encoding |
|---|---|---|
| 0x00E9 | AI Rack cursor | 0-5 (fila activa) |
| 0x00EA | Auto-harm enabled | 0.0/1.0 |
| 0x00EB | Auto-harm interval | 0.0=THIRD, 1.0=SIXTH |
| 0x00EC | Arp mode | 0-4 (UP/DOWN/UP_DOWN/RANDOM/SMART) |
| 0x00ED | Arp division | 0-2 (QUARTER/EIGHTH/SIXTEENTH) |
| 0x00EE | Groove packed | (int)=style 0-2, frac=amount 0-0.99 |

Scale Lock (0x00F2) y Beat FX (0x00F3) reutilizan los params existentes.
Auto-Harmonize interval usa 0=THIRD / 1=SIXTH (enum HarmonyInterval), no 3/6/8
semitones — el display convierte el índice al nombre ("3rd"/"6th").

### Por qué el encoding packed para Groove

Groove necesita transmitir 2 valores independientes (style 0-2 y amount 0.0-1.0)
en el mismo slot de 4 bytes float. El packed encoding (parte entera = style,
parte fraccionaria = amount) evita agregar un ID extra en el namespace ya ajustado.
El ESP32 desempaca con `(uint8_t)value` y `value - (float)(uint8_t)value`.

## Wiring

N/A — sprint solo software.

## Implementation

### Archivos modificados / creados

- `apps/docs/sprints/45-ai-mode-consolidation.md` — este doc (NUEVO)
- `apps/firmware-esp32/src/display/carousel/carousel.h` — VIEW_IDX_AI_RACK = 27
- `apps/firmware-esp32/src/display/carousel/carousel.cpp` — view_28 en tabla
- `apps/firmware-esp32/src/display/screens/views/views.h` — declaraciones view_28
- `apps/firmware-esp32/src/display/screens/views/view_28_ai_rack.cpp` — NUEVO
- `apps/firmware-esp32/src/bridge/bridge_handlers.h` — getters 0x00E9-0x00EE
- `apps/firmware-esp32/src/bridge/bridge_handlers.cpp` — variables + parsing
- `apps/firmware-teensy/src/sketches/28-synth-navigator.cpp` — subview 5→3, rack handler, sync inicial

## Demo

En AI mode, ENC NAV cicla Camelot → AI Rack → Scale Lock. En la Rack, ENC NAV
mueve entre 6 filas, ENC L/R ajustan cada feature. Los cambios se escuchan
inmediatamente (Scale Lock cuantiza, Harmonize agrega voz, Arp genera patrón,
Groove humaniza el timing).

## Learnings

### Decisiones de diseño validadas

- **3 sub-vistas en lugar de 5** es la decisión correcta: Karaoke y AI Feed eran telemetría, no control. El roadmap §0.1.7 lo decía explícitamente ("AI que solo cambia la pantalla") — el código existía antes de que el principio se aplicara a la UI.
- **AI Rack como control surface único** para 6 features fue la dirección correcta. Versus la propuesta original de 3 vistas separadas (una por feature), el Rack tiene mejor densidad de información para una pantalla 240×240.
- **Packed encoding para Groove** (`int(value)=style`, `frac(value)=amount`) ahorra un ID en el namespace sin sacrificar legibilidad — el ESP32 desempaca trivialmente.
- **`AutoHarmonize::interval` es `HarmonyInterval` enum** (no semitones raw). El agente de implementación lo detectó y corrigió el encoding del bridge sin requerir el header de AutoHarmonize — evitó un bug silencioso de transmisión de datos.

### Qué necesita verificación on-device

- [ ] Ciclo AI mode 3 sub-vistas (Camelot → Rack → Scale Lock) navega sin artefactos
- [ ] AI Rack: cursor de fila sigue el ENC NAV del Teensy en el display
- [ ] ENC L/R en la Rack efectivamente cambian Auto-Harm / Arp / Groove en audio
- [ ] Sincronización inicial al entrar AI mode (todos los params se envían por bridge)
- [ ] Auto-Harm: botón físico + Rack conviven sin conflicto de estado

### Anti-patterns evitados

- No se borraron Karaoke (view_26) y AI Feed (view_27) — el código queda. Si en el futuro hay un "modo aprendizaje" o "modo debug", se re-habilitan.
- No se agregaron sub-vistas por feature (hubiera pasado de 5 a 8 sub-vistas — 8 clicks para ciclar).

---

*Sprint 45 completado — pendiente verificación on-device*
*Siguiente: Fase 7 — WiFi + Cloud AI (firmware-esp32/src/wifi/ y cloud/ vacíos)*
