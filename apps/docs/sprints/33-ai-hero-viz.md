# Sprint 33 — AI Hero Visualization + Scale Lock Dedicada

**Estado:** EN PROGRESO
**Refs:** `02-bridge-protocol.md §7 CMD 0x83`, `04-ai-architecture.md`, `Sprint 32 (32-ml-engine-integration.md)`

---

## Contexto

Sprint 32 cerró con view_10 mostrando los resultados de inferencia (key, chord, BPM)
como labels de texto. El usuario validó on-device y propuso elevar la vista a
**hero feature**: que view_10 *visualice* lo que la AI está haciendo en tiempo real,
no solo enumere resultados.

Sprint 33 implementa esa visualización en 8 capas, agrega una vista dedicada
para Scale Lock (redisenando view_16), y resuelve el problema de descubrimiento
del modo AI (que actualmente se entera el usuario solo si descubre el hold de 4s en
ENC NAV).

**Filosofía del rediseño:** la animación debe ser *diagnóstica*, no decorativa.
Cada cosa que se mueve en pantalla corresponde a un dato real que la AI está
analizando. Cuando una nota se toca, el chromagram se ilumina. Cuando un acorde
se detecta, la triada se forma. Cuando scale lock corrige una nota, una flecha
muestra el cambio. El usuario *ve* el algoritmo trabajando.

---

## Theory

### Por qué un comando nuevo (0x83 GROOVE_STATE) y no extender 0x80/0x81/0x82

Los comandos existentes son **event-driven**: se envían cuando la inferencia
produce un resultado nuevo. KEY_DETECTED solo dispara cuando el key changes,
CHORD_DETECTED solo cuando el chord changes, etc. Eficiente, pero deja sin
respuesta dos preguntas:

1. **Actividad continua entre eventos** — el chromagram necesita saber cuánta
   actividad tiene cada pitch class *ahora*, no solo cuándo cambia el key.
2. **Estado efímero por frame** — un evento de snap dura un instante; necesita
   un canal síncrono al display para que la animación de flecha se vea.

Solución: GROOVE_STATE (0x83) es **state-driven**, no event-driven. Se envía a
ritmo fijo de 4Hz mientras el dispositivo está en AI mode, transportando el estado
*muestreado* de los modelos:

- `pitch_activity[12]`: EMA del histograma de notas tocadas, decay tau=2s
- `snap_event`, `snap_from`, `snap_to`: flag rising-edge del último snap
- `beat_phase_256`: fase dentro del bar actual, para animaciones sync al tempo

Esto es la misma distinción que en MIDI: hay messages (note on/off, ev) y hay
clock (state continuo). Sprint 32 implementó los messages; Sprint 33 agrega
el clock.

### Por qué 4Hz (250ms entre frames)

Tradeoffs:

- **30Hz** (cada 33ms): match perfecto al refresh del display, pero satura el
  bus (16B × 30Hz = 480B/s vs 16B × 4Hz = 64B/s). Y el ojo humano no distingue
  cambios en pitch activity más rápido que ~10Hz.
- **10Hz** (cada 100ms): suficiente para fluidez visual, pero 2.5× más overhead
  vs 4Hz sin ganancia perceptual real.
- **4Hz** (cada 250ms): el chromagram interpola en el ESP32 entre frames usando
  `lv_anim` con duration=250ms, dando fluidez de 60fps en pantalla con solo 4
  paquetes/s en el bus. EMA tau=2s al 4Hz tiene 8 samples por tau — más que
  suficiente para una respuesta exponencial limpia.

**Regla:** la cadencia del transport debe ser la mínima compatible con la
perception, no la del refresh del display. La interpolación visual la hace el
consumidor.

### Por qué EMA con tau=2s para pitch_activity

`_pitch_count[12]` es cumulativo (Sprint 32: nunca se resetea). Eso da estabilidad
a la detección de key, pero es inútil para visualización porque no hay "decay" —
el segmento que se tocó hace 1 hora seguiría iluminado.

EMA (Exponential Moving Average) con tau=2s:
```
activity[pc] = activity[pc] * decay + new_velocity * (1 - decay)
decay = exp(-dt / tau)
```

Con dt=250ms y tau=2s: `decay = exp(-0.25/2) ≈ 0.882`. Cada frame el segmento
pierde ~12% de su brillo si no se toca. Después de 2s queda al 37% (1/e). Después
de 4s al 14%. Para los ojos: cada nota tocada "explota" en su segmento y se desvanece
en ~3s. Sensación de pulso vivo, no de log estático.

### Por qué bar phase (no beat phase)

El "bar sweep" animado del chromagram recorre el círculo completo una vez por
**compás (4 beats)**, no por beat. Si fuera por beat:
- A 120 BPM: 0.5s por vuelta → mareante
- A 60 BPM: 1s por vuelta → todavía rápido

Por bar:
- A 120 BPM: 2s por vuelta → fluido, hipnótico
- A 60 BPM: 4s por vuelta → contemplativo

Además, el bar es la unidad musical de orientación humana ("dame 4 compases").
El bar sweep funciona como reloj de orquesta — muestra dónde estás en la frase.

### Por qué redisenamos view_16 en vez de crear view_26

view_16 ya existe en la tabla del carousel (idx 15, "SCALE LOCK"), actualmente
es un mock estático de 36 líneas con "D DORIAN" hardcoded. Redisenarla:

1. **No agrega slot al carousel** — quedan los 25 actuales, demo cycle no se
   alarga 5s extra.
2. **Mantiene la naming consistente** — el carousel ya muestra "SCALE LOCK"
   donde el usuario lo busca.
3. **Aprovecha infraestructura existente** — `view_16_create()/_destroy()` ya
   están declaradas y llamadas.

El precio: el mock estático se reemplaza. No hay regresión visual porque la viz
nueva es estrictamente mejor (dinámica + responde a datos reales).

### Por qué view_10, view_16 y view_25 son tres vistas y no una

Cada vista tiene **un rol único** (regla aprendida en Sprint 32):

| Vista | Rol | Foco |
|---|---|---|
| **view_10 AI PROC** | inference live | "AI está PENSANDO" |
| **view_16 SCALE LOCK** | acción + transparencia | "AI está ACTUANDO" |
| **view_25 AI MODELS** | configuración | "AI tiene N modelos disponibles" |

Si view_10 incluyera todo lo de view_16 sería sobrecarga visual (8 capas + snap
stats + bypass control). Separarlas permite que cada una tenga jerarquía visual
fuerte. El usuario navega entre ellas con ENC NAV mientras está en AI mode
(Sprint 33 Batch 10).

---

## Implementation

### Comando GROOVE_STATE 0x83

```c
GF_CMD_GROOVE_STATE = 0x83
Payload (16 bytes, @ 4Hz desde Teensy mientras g_in_ai_mode):

  uint8_t pitch_activity[12];   // 0-255, EMA tau=2s de NOTE_ON velocity
  uint8_t snap_event;           // 1 = snap ocurrió este frame, sino 0
  uint8_t snap_from;            // MIDI note 0-127 que se tocó
  uint8_t snap_to;              // MIDI note 0-127 al que se cuantizó
  uint8_t beat_phase_256;       // 0-255 dentro del bar actual (4 beats)

  // Total: 12 + 1 + 1 + 1 + 1 = 16 bytes
```

**Reglas de envío (Teensy):**

- Solo en `g_in_ai_mode == true` (fuera de AI no tiene sentido)
- Cadencia 250ms (4Hz), no más
- `snap_event` es rising-edge: vale 1 por **un solo frame** después del snap, luego 0
- `pitch_activity[i]` mapeado de float [0, ∞) a uint8 [0, 255] con clamp + escala
- `beat_phase_256` calculado como `(now_ms - bar_start_ms) * 256 / bar_period_ms`

**Reglas de recepción (ESP32):**

- Validar `f->len == 16` antes de parsear (rechazar con NACK si distinto)
- Almacenar en variables estáticas del módulo bridge
- Marcar timestamp del último 0x83 recibido para detectar pérdida de conexión
- Loggear a Serial con throttle de 1Hz para no inundar el monitor

### Archivos modificados — Batch 1 (spec-first)

```
apps/docs/sprints/33-ai-hero-viz.md            ← ESTE archivo
apps/bridge-protocol/include/protocol.h         ← +GF_CMD_GROOVE_STATE = 0x83
apps/firmware-esp32/src/bridge/bridge_handlers.h ← +4 getters (groove_state group)
```

### Archivos modificados — Batch 2 (sender Teensy)

```
apps/firmware-teensy/src/ml/ml_engine.h/.cpp   ← _pitch_activity[12] float + EMA
apps/firmware-teensy/src/ml/scale_lock.h/.cpp  ← contadores snapped/played + last snap
apps/firmware-teensy/src/sketches/28-synth-navigator.cpp ← bridge_send_groove_state @ 4Hz
```

### Archivos modificados — Batch 3 (handler ESP32)

```
apps/firmware-esp32/src/bridge/bridge_handlers.cpp ← on_groove_state() + state + getters impl
```

### Sub-view navigation (Batch 10, fuera del bloque actual)

Param `0x00F1` extendido a 3 valores:

```
0.0 → view_10 (AI PROC)      — default al entrar AI mode
1.0 → view_25 (AI MODELS)    — toggle de los 4 modelos
2.0 → view_16 (SCALE LOCK)   — vista dedicada (NUEVO)
```

Gesto hardware: rotación de ENC NAV dentro de AI mode cicla `10 → 25 → 16 → 10`.

---

## Demo

### Batch 1 (este sprint doc + headers)

Verificación:
1. `pio run -e esp32s3` desde `apps/firmware-esp32/` → compila limpio con los
   nuevos getters declarados (sin implementación aún, link fallaría — pero el doc
   precede al impl).
2. `pio run -e sketch` desde `apps/firmware-teensy/` → compila limpio (el sketch
   no usa los nuevos símbolos aún).
3. Spec doc revisable; no requiere hardware.

### Batches 2+3 (data layer completa)

1. Flash Teensy + ESP32.
2. Entrar AI mode (hold ENC NAV 4s).
3. Tocar notas en MIDI.
4. ESP32 serial muestra (1Hz):
   ```
   [bridge] GROOVE_STATE — activity[0..11]=[120,0,80,0,255,0,0,90,0,0,0,0] beat_phase=128
   [bridge] GROOVE_STATE — snap_event=1 from=61 to=60   ← cuando scale lock corrige una nota
   ```
5. Sin viz aún (eso es Batches 4-9). El test es solo que los datos lleguen.

### Demo final (sprint completo, batches 1-10)

1. Hold NAV 4s → view_10 con chromagram vivo
2. Tocar C, E, G → triada teal se forma en el centro, key="C major 🔒"
3. Tocar C# → flash rojo + flecha → snap a D
4. Rotar ENC NAV → view_25 (4 modelos) → view_16 (scale lock dedicada) → view_10
5. View_16 muestra: "C major", chromagram con diatonic mask, "12/47 notes snapped"

---

## Learnings

*(Completar post-implementación de Batches 4-13.)*
