# Sprint 32 — ML Engine Integration: Display AI Results

**Estado:** COMPLETADO  
**Refs:** `04-ai-architecture.md`, `02-bridge-protocol.md §7 CMD 0x80-0x83`

---

## Theory

### Por qué comandos dedicados y no PARAM_CHANGED

El protocolo Bridge tiene dos mecanismos para enviar datos numéricos del Teensy al ESP32:

- **PARAM_CHANGED (0x14)**: pensado para valores continuos de parámetros de síntesis — cutoff,
  resonancia, wet/dry. Payload = `[param_id: 2B, value: 4B float]`. Un float de 32 bits es la
  representación natural de un knob que va de 0.0 a 1.0.

- **ML inference CMDs (0x80-0x83)**: pensados exactamente para resultados de inferencia — KEY,
  CHORD, BEAT, GENRE. Sus payloads son discretos y semánticos, no continuos.

Usar PARAM_CHANGED para enviar un key index requiere codificar un entero en float
(key_idx=5 → 5.0f) y acordar un param_id especial (ej. 0x00E8) que no aparece en el spec.
Esto es abuso de abstracción: mezcla dos concerns distintos en el mismo canal.

Los comandos 0x80-0x83 ya existen en `protocol.h`. Su payload puede incluir confidence,
root note, chord quality — información que PARAM_CHANGED no puede portar limpiamente sin
inventar aún más param_ids especiales.

**Regla:** resultados ML → comandos 0x80-0x83. Parámetros de síntesis → PARAM_CHANGED.

### Frame payload de los comandos ML

```
GF_CMD_KEY_DETECTED (0x80):
  payload[0] = key_idx   uint8: 0-11 = C-B mayor, 12-23 = C-B menor
  payload[1] = confidence uint8: 0-100 (%)

GF_CMD_CHORD_DETECTED (0x81):
  payload[0] = root  uint8: 0-11 (C=0, C#=1, ..., B=11)
  payload[1] = quality uint8: encoding nativo de ChordRecognizer:
               0=MAJ, 1=MIN, 2=7, 3=m7, 4=MAJ7, 5=N (no chord detectado)
  payload[2] = confidence uint8: 0-100

GF_CMD_BEAT_DETECTED (0x82):
  payload[0:1] = bpm_x10 uint16 little-endian: BPM × 10
                          ej. 1205 = 120.5 BPM; 0 = desconocido
```

El confidence permite al display hacer dimming del texto cuando la inferencia
es insegura (confidence < 50 → opacidad 40%). No implementado en Sprint 32 pero
el campo ya está en el payload para compatibilidad futura.

### Por qué el display no redibuja al recibir el frame

Los frames ML llegan a ~10fps (el inference loop del Teensy corre a 30fps pero
el beat follower actualiza BPM solo cuando detecta cambio). Crear/destruir labels
en cada frame sería costoso en LVGL8 (heap + invalidation del display).

Solución: los labels se crean una vez en `view_10_create()`. Un timer de 50ms
llama a `bridge_get_ai_key_name()` etc. y actualiza el texto con `lv_label_set_text()`.
LVGL8 detecta si el texto cambió y solo invalida si hay diferencia (string compare interno).

Este patrón es idéntico al que usan view_02 (osciloscopio) y view_07 (spectrum): estado
almacenado en bridge_handlers, consumido por un timer de la vista a 20-30fps.

### Z-ordering en LVGL8: labels sobre orbit dots

LVGL8 pinta los widgets en orden de creación dentro del mismo padre: los creados
más tarde aparecen encima. En `view_10_create()` la secuencia es:

1. Arco exterior (background)
2. Glow dot (center, primer hijo)
3. 8 orbit dots (hijos 2-9)
4. Core breathing dot (hijo 10)
5. **Labels ML** (hijos 11+) → siempre encima de los orbit dots

Los orbit dots a r=50px del centro se superponen ligeramente con los labels a dy=46-70.
Pero como los labels son hijos más recientes, se pintan encima. El resultado visual:
texto legible, orbit dots parcialmente cubiertos — similar al efecto que hace iOS con
sus animaciones de Siri.

---

## Implementation

### Archivos modificados (Sprint 32 — Batch A: ESP32)

```
apps/firmware-esp32/src/bridge/bridge_handlers.h   ← +3 getters ML + bypass getters
apps/firmware-esp32/src/bridge/bridge_handlers.cpp ← handlers 0x80/0x81/0x82 + param 0x00F1
apps/firmware-esp32/src/display/screens/views/view_10_ai_processing.cpp ← labels dinámicos, pills eliminados
apps/firmware-esp32/src/display/screens/views/view_25_ai_models.cpp  ← NUEVO: toggle/status view
apps/firmware-esp32/src/display/screens/views/views.h                ← +view_25 declarations
apps/firmware-esp32/src/display/carousel/carousel.cpp                ← +view_25 en tabla s_views[24]
apps/firmware-esp32/src/display/carousel/carousel.h                  ← +VIEW_IDX_AI_MODELS=24
```

### Archivos modificados (Sprint 32 — Batch B: Teensy)

```
apps/firmware-teensy/src/ml/ml_engine.h/.cpp       ← orchestrador: KeyDetector+ChordRecognizer+BeatFollower, 500ms
apps/firmware-teensy/src/sketches/28-synth-navigator.cpp ← ML wiring, ScaleLock snap, bridge_send_ml_*
apps/firmware-teensy/platformio.ini                 ← [env:sketch] include 5 ML source files
```

El Teensy envía:
```cpp
// En ml_engine.tick(), después de cada inferencia:
GF_Frame f;
uint8_t kp[2] = { key_idx, confidence };
gf_frame_build(&f, GF_CMD_KEY_DETECTED, seq++, kp, 2);
Serial1.write(gf_frame_serialize_buf, gf_frame_serialize(&f, buf, sizeof(buf)));
```

### Vista view_10 — layout post-Sprint 32

```
╔══════════════════════════════╗  ← 240×240 round GC9A01
║   · · · GROOVEFORGE AI · · ·║  ← gf_arc_title (curved, y≈18)
║   ┌─────────────────────┐   ║  ← arco exterior 220px (progress=1.0 → teal full)
║   │  · · · · · · · ·   │   ║  ← 8 orbit dots r=50, chase animation
║   │       [glow]        │   ║
║   │     [core ✦]        │   ║  ← core breathing 10-18px
║   │         C           │   ║  ← s_key_lbl    dy=46 GF_FONT_LABEL  TEAL_PLUS
║   │      Am · 120       │   ║  ← s_result_lbl dy=63 GF_FONT_MICRO  WHITE
║   └─────────────────────┘   ║
║  ┌──── AI PROC ─────────┐   ║  ← mode pill (bottom)
╚══════════════════════════════╝
```

Estado inicial (antes de recibir resultados): key="---"  resultado="---"
Estado activo: key="C"  resultado="Am · 120"

**Nota de diseño:** los bypass pills (SCALE LOCK / BEAT FOLLOW) se eliminaron de
view_10 y se movieron a view_25 (AI MODELS, carousel idx 24). view_10 es ahora
una vista de solo lectura — visualiza resultados ML, no permite toggles. Los toggles
viven exclusivamente en view_25.

### Vista view_25 — layout (NUEVA)

```
╔══════════════════════════════╗
║  · · ·  AI MODELS  · · ·   ║  ← gf_arc_title teal, y≈18
║                             ║
║  ● KEY DETECT    ACTIVE     ║  ← fila 0, dy=-52
║  ● CHORD DETECT  IDLE       ║  ← fila 1, dy=-26
║  ● BEAT FOLLOW   ON         ║  ← fila 2, dy=0   (toggleable vía param 0x00F3)
║  ● SCALE LOCK    OFF        ║  ← fila 3, dy=+26  (toggleable vía param 0x00F2)
║                             ║
║  ┌──── MODELS ───────────┐  ║  ← mode pill
╚══════════════════════════════╝
```

Opacidad de cada fila: LV_OPA_COVER (activo/ON) vs 90/255 (idle/OFF).
Timer 50ms actualiza texto y opacidad desde estado bridge sin rebuild.

---

## Demo

### Sin Teensy (script Python)

```bash
cd tools/bridge-test
# Demo canónico: C key + Am chord + 120 BPM → view_10
python3 send_ml_results.py --port /dev/cu.usbmodem*

# Loop interactivo (cicla 5 escenarios cada 3s)
python3 send_ml_results.py --port /dev/cu.usbmodem* --loop

# Navegar a view_25 AI MODELS
python3 send_ml_results.py --port /dev/cu.usbmodem* --view models

# Toggle bypass BEAT FOLLOW desde CLI
python3 send_ml_results.py --port /dev/cu.usbmodem* --bypass beat off
```

Resultado esperado en monitor serial:
```
[bridge] HEARTBEAT received
[bridge] PARAM_CHANGED — id=0x00F1 val=0.00  (→ carousel_goto view_10)
[bridge] KEY_DETECTED — C
[bridge] CHORD_DETECTED — Am
[bridge] BEAT_DETECTED — 120 BPM
```

Verificar en display GC9A01: view_10 muestra `C` (TEAL_PLUS) y `Am · 120` (WHITE).

### Con Teensy físico

1. Flash ESP32: `pio run -e esp32s3 -t upload` desde `apps/firmware-esp32/`
2. Flash Teensy: `pio run -e sketch -t upload` desde `apps/firmware-teensy/`
3. Tocar escala de Do mayor en MIDI → display muestra resultados reales en ~500ms.
4. Confirmar: `handle_ai_inference()` llama `bridge_send_ml_key/chord/beat()`.

---

## Learnings

### 1. El encoding de ChordRecognizer no es el del spec de wire

El spec de wire (`02-bridge-protocol.md`) no precisaba el encoding de `quality`.
`ChordRecognizer` usa internamente `{0=maj, 1=min, 2=7, 3=m7, 4=maj7, 5=N}` — un
orden distinto al que se asumió inicialmente en `on_chord_detected()` (que tenía 8
tipos con DIM/SUS/AUG). La corrección requirió cambiar `QUAL[8]` a `QUAL[5]` y
agregar el caso especial `qual == 5 → "---"`. **Regla aprendida:** cuando el payload
de un comando ML lleva un enum, el spec de wire debe citar el encoding del modelo
de ML, no uno nuevo. Actualizar el spec si hay divergencia.

### 2. Separar "visualización" de "control" simplifica mucho las vistas

Originalmente view_10 tenía pills de bypass toggle. Moverlos a view_25 dejó view_10
como vista de solo lectura pura. El código es más simple: un timer que llama
`lv_label_set_text()` y nada más. Sin event handlers, sin state de botones.
**Patrón:** una vista = un rol. Si una vista necesita mostrar Y controlar, es señal
de que necesita dividirse.

### 3. La acumulación sin reset del pitch histogram es intencional

`_pitch_count[12]` nunca se resetea durante la sesión de AI mode. Esto hace que
la detección de tonalidad sea más estable (no salta con 2-3 notas) pero más
lenta en responder a cambios reales de key. Es el tradeoff correcto para un sintetizador:
preferimos estabilidad a reactividad. Si se quisiera reactividad, se podría
resetear el histograma cada N segundos — pero eso sería un cambio deliberado, no accidental.

### 4. `gf_anim_kill_all()` es crítico antes de `lv_obj_clean()`

Si `carousel_goto()` llama `lv_obj_clean()` sin antes matar animaciones, LVGL8
puede disparar callbacks de animación sobre punteros ya liberados → undefined behavior.
El orden correcto: `destroy()` → `gf_anim_kill_all()` → `lv_obj_clean()` → `build_current()`.
Este patrón ya existía en el carousel pero es importante documentarlo para nuevas vistas
con animaciones propias (como view_10 con los orbit dots).

### 5. Los frames ML deben enviarse solo cuando el valor cambia

`handle_ai_inference()` compara el resultado actual con `_prev_key_idx` / `_prev_chord_idx` /
`_prev_bpm_int` antes de enviar. Sin este filtro, se enviarían ~2 frames/segundo de
KEY_DETECTED idénticos — saturando el bus y causando micro-rebuilds innecesarios
en el timer del display. El bitmask de retorno de `tick()` es elegante: 0 = nada cambió,
bits individuales indican qué modelo actualizó.
