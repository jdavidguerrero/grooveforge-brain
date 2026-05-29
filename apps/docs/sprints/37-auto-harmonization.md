# Sprint 37 — Auto-Harmonization

> **Fase:** 6 — Layer 1 AI completo
> **Sprint del roadmap:** 6.2
> **Estimado:** 3-4 sesiones (~10-14h)
> **Status:** 🟡 In Progress (Theory escrita, código pendiente)
> **Refs:** `apps/docs/06-implementation-roadmap.md §6.2` · `apps/docs/04-ai-architecture.md §1.3`

---

## Theory

### Concepto central

Imaginate que estás tocando en un grupo y hay un segundo músico que te sigue en silencio.
No necesita verte ni escucharte en detalle — solo sabe en qué tonalidad están tocando.
Con eso le alcanza para saber exactamente qué nota tocar junto a la tuya, siempre en
escala, siempre armónica. Eso es la armonización diatónica: **una función que, dada una
nota y una tonalidad, devuelve la nota que un segundo músico competente tocaría encima
sin pensar dos veces**.

En términos técnicos: el GrooveForge Brain ya conoce la tonalidad (el KeyDetector,
Sprint 4.3, 94% de accuracy). El único paso adicional es calcular, para cada nota MIDI
que llega, qué nota de la misma escala está a una tercera o sexta diatónica de distancia.
Ese cálculo es una búsqueda lineal en 7 elementos más una suma — no hay inferencia, no
hay tensor arena, no hay modelo. Es **teoría musical pura convertida en código determinístico**.

El resultado audible: tocás una nota → escuchás dos voces sonando juntas, ambas en escala,
siempre consonantes. Para un músico inexperto, suena como "tocar bien" de forma automática.
Para un músico experimentado, es un segundo color armónico instantáneo.

---

### Por qué lo hacemos así

La pregunta de diseño central es: ¿la armonización debería ser un modelo de ML o una
función determinística?

| Approach | Ventaja | Desventaja | Decisión |
|---|---|---|---|
| Tabla de intervalos diatónicos (determinístico) | Sin training, sin arena tensor, <1 µs CPU, 100% predecible musicalmente, fácil de debuggear | No aprende preferencias del usuario | ✅ Elegido para v1 |
| Modelo TFLite que elige el intervalo | Puede aprender qué intervalos suenan mejor en contexto | Requiere dataset de decisiones armónicas, 8-20ms inferencia, arena RAM, result a veces no-obvio | ❌ Descartado para v1 |
| Voice-leading completo (resolver acordes) | Resultado más "orquestal" | Complejidad O(voces²), latencia multi-ms, fuera del scope de un synth en vivo | ❌ Fuera de scope |

**La tabla de intervalos es completamente determinística dado el tipo de escala.** El ML
ya hizo su trabajo en el Sprint 4.3 (detectar la tonalidad). A partir de ahí, la
armonización es matemática: cada grado de la escala tiene un intervalo de tercera y de
sexta perfectamente definido por la teoría musical occidental. No hay ambigüedad, no hay
entrenamiento necesario.

Esto no es una limitación — es la decisión correcta. La armonización diatónica lleva
siglos siendo determinística. Los músicos del Renacimiento ya conocían exactamente las
mismas tablas que estamos implementando. El ML agrega valor cuando el problema es
estadístico o depende del contexto. Aquí el problema es algebraico.

---

### Cómo funciona (más profundo)

#### Las escalas diatónicas tienen 7 grados, cada uno con su tercera fija

Una escala mayor (modo jónico) o menor natural (modo eólico) tiene exactamente 7 notas.
Cada nota ocupa un **grado** en la escala, numerado del 1 al 7. La relación entre un grado
y su tercera diatónica (dos grados arriba) está fijada por la estructura de la escala —
no cambia según el contexto armónico del momento.

Para calcular la nota de armonía, el algoritmo hace tres pasos:

```
1. Encontrar el grado de la nota tocada dentro de la escala.
2. Consultar la tabla: ese grado → cuántos semitonos sube la armonía.
3. Calcular: harmony_note = midi_note + semitones_from_table[degree]
```

El paso 1 es una búsqueda lineal en un array de 7 elementos (los pitch classes de la
escala activa). El paso 2 es un acceso a array en tiempo constante. El paso 3 es una
suma. Todo el proceso corre en menos de 1 µs en el Cortex-M7 del Teensy 4.1.

#### Tabla de intervalos para escala mayor (modo jónico)

Cada fila es un grado de la escala. La columna "semitones" indica cuántos semitonos hay
que sumarle a la nota original para obtener la tercera diatónica.

| Grado | Nota (en C maj) | Tercera diatónica | Tipo de tercera | Semitonos |
|---|---|---|---|---|
| 1 | C | E | Mayor | +4 |
| 2 | D | F | Menor | +3 |
| 3 | E | G | Menor | +3 |
| 4 | F | A | Mayor | +4 |
| 5 | G | B | Mayor | +4 |
| 6 | A | C | Menor | +3 |
| 7 | B | D | Menor | +3 |

Array en C++: `constexpr int8_t MAJOR_THIRDS[] = {4, 3, 3, 4, 4, 3, 3};`

La suma de ese array es 24 — exactamente 2 octavas, lo que confirma que la tabla "cierra"
correctamente al volver al grado 1 una octava más arriba.

Para la **sexta diatónica** (cuatro grados arriba, que es la inversión de la tercera):

| Grado | Nota (en C maj) | Sexta diatónica | Tipo | Semitonos |
|---|---|---|---|---|
| 1 | C | A | Mayor | +9 |
| 2 | D | B | Mayor | +9 |
| 3 | E | C | Menor | +8 |
| 4 | F | D | Mayor | +9 |
| 5 | G | E | Mayor | +9 |
| 6 | A | F | Menor | +8 |
| 7 | B | G | Menor | +8 |

Array: `constexpr int8_t MAJOR_SIXTHS[] = {9, 9, 8, 9, 9, 8, 8};`

#### Tabla de intervalos para escala menor natural (modo eólico)

La escala menor natural tiene una distribución de semitonos distinta: `[0, 2, 3, 5, 7, 8, 10]`.
Esto cambia qué terceras son mayores y cuáles son menores:

| Grado | Nota (en A min) | Tercera diatónica | Tipo | Semitonos |
|---|---|---|---|---|
| 1 | A | C | Menor | +3 |
| 2 | B | D | Menor | +3 |
| 3 | C | E | Mayor | +4 |
| 4 | D | F | Menor | +3 |
| 5 | E | G | Menor | +3 |
| 6 | F | A | Mayor | +4 |
| 7 | G | B | Mayor | +4 |

Array: `constexpr int8_t MINOR_THIRDS[] = {3, 3, 4, 3, 3, 4, 4};`

Para la **sexta diatónica menor natural**:

| Grado | Nota (en A min) | Sexta diatónica | Tipo | Semitonos |
|---|---|---|---|---|
| 1 | A | F | Menor | +8 |
| 2 | B | G | Menor | +8 |
| 3 | C | A | Mayor | +9 |
| 4 | D | B | Menor | +8 |
| 5 | E | C | Menor | +8 |
| 6 | F | D | Mayor | +9 |
| 7 | G | E | Mayor | +9 |

Array: `constexpr int8_t MINOR_SIXTHS[] = {8, 8, 9, 8, 8, 9, 9};`

**Verificación de integridad:** la suma de MINOR_THIRDS es también 24 (2 octavas). La
suma de los sixths, tanto para mayor como para menor, suma a 60 (5 octavas), lo que
también cierra correctamente.

#### Cómo encontrar el grado de una nota

Dado un `midi_note` y un array `scale_notes[7]` de pitch classes (ya provisto por
`ScaleLock::get_scale_notes()`):

```cpp
uint8_t pitch_class = midi_note % 12;
int degree = -1;
for (int i = 0; i < 7; i++) {
    if (scale_notes[i] == pitch_class) {
        degree = i;  // índice 0-6, que corresponde a grado 1-7
        break;
    }
}
// Si degree == -1: la nota no está en la escala → no armonizar, retornar 0xFF
```

La búsqueda es lineal en 7 elementos. En Cortex-M7 a 600 MHz, una búsqueda lineal de
7 comparaciones toma ~10-15 ciclos de reloj = ~25 ns. Completamente despreciable frente
a los 1ms del buffer de audio.

#### Voice leading básico: no saltar más de una octava

Si `harmony_note > midi_note + 12`, significa que la voz armónica saltó una octava
completa hacia arriba — lo que puede sonar "separado" del original. La regla de voice
leading más básica es mantener las voces dentro de una octava de distancia:

```cpp
if (harmony_note > midi_note + 12) {
    harmony_note -= 12;  // bajar la armonía una octava
}
```

Esto garantiza que la segunda voz siempre esté "cerca" de la primera — dentro de los
7-9 semitonos que caracterizan a las terceras y sextas.

Por ahora **no implementamos** evitar quintas y octavas paralelas (parallel fifths/octaves).
Esa restricción del contrapunto clásico es importante en escritura a 4 voces, pero en un
contexto de 2 voces en vivo produce más restricciones que beneficios audibles. Es
complejidad que puede agregarse en v2 si el usuario lo pide.

#### Por qué terceras y no otros intervalos

Las terceras (y sus inversiones, las sextas) son los intervalos de armonía más universales
en la música occidental y popular:

- Son consonantes (índice de disonancia bajo según Helmholtz) pero no "vacías" como las
  quintas o las octavas.
- Definen el "color" del acorde (la tercera mayor vs menor es lo que distingue un acorde
  mayor de uno menor).
- Se usan en vocales en dúo (ver: Simon & Garfunkel, Eagles, cualquier coro gospel) y en
  instrumentos melódicos dobles (violín, flauta).

Las séptimas o novenas diatónicas son posibles extensiones futuras para un "modo jazz"
pero están fuera del scope de v1.

#### El constraint de polifonía: por qué máximo 3 notas simultáneas

Los engines del Brain (MoogModelD, Juno106, Prophet5) tienen 6 voces polifónicas. Con
armonización activa, cada nota del usuario consume 2 voces: la nota original y la voz
armónica. Esto reduce la polifonía efectiva de 6 a 3 notas simultáneas.

```
6 voces disponibles ÷ 2 voces por nota = 3 notas simultáneas máximo
```

Esto es suficiente para acordes de 3 notas (triadas) con armonía, que es el caso de uso
principal. Para quien use acordes de 4+ notas, la armonización simplemente no agrega
voces cuando el motor está saturado — el `noteOn()` sobrante se descarta limpiamente
por el voice allocator existente en cada engine.

El constraint viene de `apps/docs/01-architecture.md §4.1` (engines a 6 voces ~30% CPU).
La armonización no agrega CPU al path DSP — solo despacha más `noteOn()` al mismo engine.

---

### Relevancia para GrooveForge Brain

La armonización automática es la feature que más directamente transforma el Brain de
"instrumento inteligente" a "colaborador musical". Hay tres tipos de usuarios que se
benefician de maneras distintas:

**El músico inexperto** que no conoce teoría musical toca una nota y escucha dos voces
perfectamente consonantes. Nunca suena "mal". El Brain hace el trabajo de saber qué
tercera corresponde — el usuario solo toca.

**El músico intermedio** que sí conoce teoría pero quiere libertad creativa puede usar la
armonía como una segunda voz instantánea mientras improvisa melodías. En lugar de
planificar las notas armónicas, las escucha en tiempo real y puede reaccionar a ellas.

**El músico avanzado** que ya domina las escalas y los acordes obtiene un segundo color
tímbrico: la misma melodía tocada con armonía de terceras suena más "llena" y "orquestal"
sin requerir una segunda pista o un segundo instrumento.

Desde el punto de vista de marketing, la demo más directa es la más poderosa: tocás
cromático en AI mode → todo suena armonizado, siempre en escala. El usuario que nunca
estudió música suena como si supiera. Ese momento de sorpresa es el buy-reason Tier S
del roadmap.

---

### Diagrama del flow de señal

```
[USB-A MIDI IN]
       |
       | NoteOn(note, vel)
       v
[ScaleLock::snap(note)]          ← cuantiza a escala (Sprint 5.3)
       |
       | snapped_note
       v
[AutoHarmonize::get_harmony_note()] ← consulta tabla diatónica (este sprint)
       |                              retorna harmony_note o 0xFF
       +-----------+
       |           |
       v           v (si harmony_note != 0xFF)
[engine.noteOn(   [engine.noteOn(
  snapped_note,     harmony_note,
  velocity)]        velocity)]
       |           |
       v           v
  [g_engine_mix — AudioMixer4 existente]
       |
       v
  [SGTL5000 → salida analógica]
```

El NoteOff sigue el mismo split: la nota original y la nota armónica reciben `noteOff()`
por separado. Esto requiere que `AutoHarmonize` mantenga un mapa `note_played →
harmony_note` para poder hacer el `noteOff` de la voz correcta.

```
[NoteOff(note)]
       |
       v
[AutoHarmonize::get_stored_harmony(note)]  ← lookuptable note→harmony
       |
       +-----------+
       |           |
       v           v
[engine.noteOff( [engine.noteOff(
  snapped_note)]   stored_harmony)]
```

---

### Referencias

- **Aldwell, E. & Schachter, C.** (2003). *Harmony and Voice Leading.* 3rd ed. Schirmer.
  Cap. 6 — "Diatonic Triads in Major and Minor." Fundamento teórico de los intervalos
  diatónicos y la estructura de grados de escala.

- **Aldwell, E. & Schachter, C.** ibid. Cap. 8 — "Voice Leading in Four-Part Chorale
  Style." Justificación del voice leading básico (mantener voces dentro de octava) y
  por qué las quintas/octavas paralelas son secundarias en un contexto de 2 voces.

- **Huron, D.** (2001). "Tone and Voice: A Derivation of the Rules of Voice-Leading from
  Perceptual Principles." *Music Perception*, 19(1), 1-64. — Base perceptual del voice
  leading mínimo: las reglas del contrapunto clásico derivan de la psicoacústica, no de
  dogma histórico.

- `apps/docs/04-ai-architecture.md §1.3` — tabla de features Layer 1 con presupuesto de
  memoria: Auto-Harmonization figura con 80KB de modelo. En esta implementación
  determinística ese presupuesto no se usa — el saving de RAM es un bonus.

- `apps/docs/01-architecture.md §4.1` — constraint de 6 voces polifónicas por engine.
  Justifica el máximo de 3 notas simultáneas con armonización activa.

- `apps/docs/sprints/27-scale-lock.md` — SprintDoc de Scale Lock. `ScaleLock` provee
  `get_scale_notes()`, `get_key_root()` y `get_scale_type()` que usa `AutoHarmonize`.

---

## Wiring (Cableado)

`N/A — sprint solo software.`

Este sprint no requiere hardware nuevo ni cableado adicional. La armonización opera
completamente en el path MIDI → engine, que ya existe desde el Sprint 4.1 (USB-A MIDI
host) y el Sprint 5.3 (Scale Lock). Los engines ya manejan polifonía de 6 voces
internamente; la armonización simplemente despacha un segundo `noteOn()` al mismo engine
con la nota de armonía calculada.

El `g_engine_mix` (AudioMixer4 existente en `28-synth-navigator.cpp`) mezcla las voces
del engine sin modificaciones — desde la perspectiva del mixer, una nota de armonía es
indistinguible de una nota normal del usuario.

---

## Implementation

### Archivos creados / modificados

| Archivo | Descripción |
|---|---|
| `apps/firmware-teensy/src/ml/auto_harmonize.h` | Declaración de `AutoHarmonize`: tablas constexpr, `get_harmony_note()`, mapa note→harmony para NoteOff |
| `apps/firmware-teensy/src/ml/auto_harmonize.cpp` | Implementación: tablas de intervalos diatónicos, búsqueda de grado, lógica de voice leading, mapa de NoteOff |
| `apps/firmware-teensy/src/sketches/28-synth-navigator.cpp` | Wire: MIDI `noteOn` → `AutoHarmonize` → segunda voz al engine; `noteOff` → release de ambas voces; toggle con B4 en AI mode |
| `apps/firmware-teensy/platformio.ini` [env:sketch] | Agregar `+<ml/auto_harmonize.cpp>` a `build_src_filter` |
| `apps/firmware-teensy/test/test_auto_harmonize/test_auto_harmonize.cpp` | Tests nativos: tablas para C major, Am, Em; nota fuera de escala → 0xFF; boundary octave |

### API pública de AutoHarmonize

```cpp
#pragma once
#include <stdint.h>

// Intervalos de armonía soportados.
// THIRD  = tercera diatónica arriba (2 grados, 3-4 semitonos).
// SIXTH  = sexta diatónica arriba   (5 grados, 8-9 semitonos, inversión de la tercera).
enum class HarmonyInterval { THIRD, SIXTH };

class AutoHarmonize {
public:
    // Calcula la nota MIDI de la voz armónica para `midi_note`.
    //
    // Parámetros:
    //   midi_note   — nota MIDI de la voz principal [0-127]
    //   scale_notes — array de 7 pitch classes de la escala activa (de ScaleLock)
    //   key_root    — pitch class de la raíz [0-11]
    //   is_major    — true = escala mayor, false = menor natural
    //   interval    — THIRD (por defecto) o SIXTH
    //
    // Retorno:
    //   nota MIDI de la armonía [0-127], ajustada para no saltar >12 semitonos
    //   0xFF si midi_note no pertenece a la escala (nota cromática fuera de escala)
    //
    // Nota: es función pura (const) — sin estado, safe en callback de MIDI.
    uint8_t get_harmony_note(uint8_t midi_note,
                             const uint8_t* scale_notes,
                             uint8_t key_root,
                             bool is_major,
                             HarmonyInterval interval = HarmonyInterval::THIRD) const;

    // Registra el par nota→armonía para poder hacer NoteOff de la voz correcta.
    // Llamar inmediatamente después de get_harmony_note() cuando result != 0xFF.
    void store_harmony(uint8_t midi_note, uint8_t harmony_note);

    // Recupera la nota de armonía almacenada para un NoteOff.
    // Retorna 0xFF si no hay armonía registrada para esa nota (nota cromática, bypass).
    uint8_t pop_harmony(uint8_t midi_note);

    // Toggle de activación. El usuario lo controla con B4 en AI mode.
    bool enabled = false;

    // Intervalo seleccionado (puede cambiar en runtime).
    HarmonyInterval interval = HarmonyInterval::THIRD;

private:
    // Tablas de semitonos por grado, indexadas [0..6] (grado - 1).
    static constexpr int8_t MAJOR_THIRDS[7] = {4, 3, 3, 4, 4, 3, 3};
    static constexpr int8_t MAJOR_SIXTHS[7] = {9, 9, 8, 9, 9, 8, 8};
    static constexpr int8_t MINOR_THIRDS[7] = {3, 3, 4, 3, 3, 4, 4};
    static constexpr int8_t MINOR_SIXTHS[7] = {8, 8, 9, 8, 8, 9, 9};

    // Mapa note→harmony para NoteOff. Máximo 6 notas simultáneas (voice count).
    // Entry 0xFF en harmony significa "sin armonía para esta nota".
    struct HarmonyEntry { uint8_t note; uint8_t harmony; };
    HarmonyEntry harmony_map_[6] = {};
};
```

### Plan de implementación por batch

El sprint sigue el principio one-thing-at-a-time del CLAUDE.md. Cada batch es un bloque
de trabajo independiente que puede validarse antes de empezar el siguiente.

**Batch A — Theory (este documento)**
Escrito antes de cualquier código. Contiene tablas completas, API, plan de archivos y
criterios de aceptación del demo. El firmware-engineer puede empezar Batch B a partir
de aquí sin ambigüedad.

**Batch B — `auto_harmonize.h` + `auto_harmonize.cpp` (función pura, sin integración)**
Implementar la clase completa con tablas, `get_harmony_note()`, `store_harmony()`,
`pop_harmony()`. Sin tocar el sketch 28 aún — solo la clase aislada.

Criterio de pass del Batch B: el archivo compila con `pio run -e native` (o el env
correspondiente sin hardware). No requiere Teensy conectado.

**Batch C — Tests nativos**
Escribir `test_auto_harmonize.cpp` y verificar que los vectores de test pasan antes
de integrar en el sketch. Los tests son la documentación ejecutable de las tablas.

Criterio de pass del Batch C: `pio test -e native -f test_auto_harmonize` pasa sin
fallos. Ver sección Tests para los vectores exactos.

**Batch D — Wire en sketch 28**
Integrar `AutoHarmonize` en `28-synth-navigator.cpp`. El callback de `noteOn` pasa
por `AutoHarmonize` cuando `enabled == true`. El callback de `noteOff` hace `pop_harmony()`
y despacha el segundo `noteOff`. Agregar `auto_harmonize.cpp` al `build_src_filter` del
`platformio.ini`.

Criterio de pass del Batch D: el sketch compila y en hardware: tocando una nota se
escuchan dos voces. En Serial aparece `[Harmony] note=60 harmony=64`.

**Batch E — Toggle con B4 en AI mode**
Mapear el botón B4: hold >500ms en AI mode = toggle `g_harmonize.enabled`. El display
muestra el estado (HARM ON / HARM OFF). Single-press de B4 mantiene su comportamiento
actual (AI mode panic en SYNTH mode).

Criterio de pass del Batch E: al presionar B4 >500ms, la armonía se activa/desactiva
auditivamente sin reiniciar el sketch ni crear glitches en el audio.

### Decisiones de implementación

**`get_harmony_note()` es `const`:** la función no modifica el estado del objeto. Es safe
llamarla desde el callback de NoteOn (que corre en el contexto de interrupción USB MIDI).
El mapa `harmony_map_` solo se modifica con `store_harmony()` y `pop_harmony()`, que
se llaman inmediatamente después en el mismo callback — siempre en el mismo contexto de
ejecución, sin races.

**El mapa note→harmony tiene tamaño fijo de 6 entradas:** la polifonía máxima del engine
es 6 voces. Con armonización, el usuario puede tener como máximo 3 notas activas
simultáneamente (3 originales + 3 harmonías = 6 voces). Un array de 6 HarmonyEntry es
suficiente y evita heap allocation en el firmware.

**`pop_harmony()` borra la entrada al leerla:** cuando el usuario suelta una nota, la
entrada en el mapa se marca como libre (note = 0, harmony = 0xFF). Si el mismo MIDI note
se repite (re-trigger), `store_harmony()` escribe sobre la entrada existente o encuentra
una entrada libre.

**Tablas como `constexpr static`:** las tablas de 7 valores se almacenan en flash (no en
RAM), son inmutables, y están accesibles en tiempo de compilación. El compilador las puede
usar para verificar bounds si se anotaran como `std::array`, pero para mantener
compatibilidad con el C++17 bare-metal del proyecto se dejan como arrays C estáticos.

**Por qué no una tabla de 24 escalas × 7 grados:** la alternativa sería precalcular una
tabla 2D `harmony_offsets[24][7]` con los semitonos para cada tonalidad posible. Pero
las tablas MAJOR_THIRDS, MINOR_THIRDS, etc. son las mismas para cualquier raíz — solo
el índice de búsqueda del grado cambia. Parametrizar por raíz es innecesario.

### Constraints respetados

| Constraint | Target | Estimado | Fuente |
|---|---|---|---|
| CPU auto-harmonize | <1 µs | ~25 ns (búsqueda 7 elem + suma) | `01-architecture.md §4.1` |
| Voces simultáneas con harmony | ≤6 total | 3 originales + 3 harmonías | `01-architecture.md §4.1` |
| RAM adicional | mínimo | ~60 bytes (mapa 6 entradas × 2B + tablas en flash) | `04-ai-architecture.md §1.2` |
| Flash adicional | mínimo | ~1KB compilado | `04-ai-architecture.md §1.2` |
| Latencia de harmony noteOn | <1ms | ~1 µs (función pura) | `01-architecture.md §5.2` |
| Audio path CPU | ≤60% total | Sin cambio — no toca el path DSP | `01-architecture.md §5.2` |

---

## Demo

### Qué valida este demo

Que el Brain convierte una sola voz en dos voces armónicas simultáneas, siempre consonantes
con la tonalidad detectada, sin latencia perceptible, con posibilidad de toggle on/off en
vivo. La validación es **auditiva** — el output analógico del SGTL5000 debe sonar a dos voces.

### Cómo reproducirlo

```bash
# 1. Compilar y flashear el sketch 28 con auto-harmonize integrado
cd apps/firmware-teensy
pio run -e sketch -t upload

# 2. Abrir Serial Monitor para diagnóstico
pio device monitor --baud 921600

# 3. Hardware necesario:
#    - Teclado MIDI conectado por USB-A al Teensy
#    - Auriculares o parlante en el jack SGTL5000

# 4. Activar AI mode: hold ENC NAV 4 segundos
#    → Serial muestra: [AI Mode] ON

# 5. Tocar algunas notas en la tonalidad principal (ej: C D E F G en C major)
#    → El Key Detector acumula el histograma
#    → Serial muestra: [Key] C_maj (confidence: 0.88)

# 6. Activar armonía: hold B4 más de 500ms
#    → Serial muestra: [Harmony] ON — THIRD interval
#    → El LED del B4 confirma el estado (si disponible)

# 7. Tocar notas individuales → escuchar dos voces simultáneas

# 8. Demo A/B:
#    - Tocar pasaje con harmony ON → grabar
#    - Hold B4 >500ms → harmony OFF
#    - Tocar el mismo pasaje → grabar
#    → La diferencia debe ser claramente audible

# Diagnóstico en Serial (ejemplo):
#   [Harmony] note=60 (C4) → harmony=64 (E4) — MAJOR 3rd
#   [Harmony] note=62 (D4) → harmony=65 (F4) — minor 3rd
#   [Harmony] note=69 (A4) → 0xFF (nota fuera de escala — no armonizar)
```

### Evidencia a capturar

- [ ] Grabación de audio: pasaje A (harmony ON) + pasaje B (harmony OFF) — archivo en
  `apps/docs/sprints/demos/37-harmony-demo.wav`
- [ ] Screenshot del Serial Monitor mostrando pares `note → harmony` en tiempo real
  para al menos 5 notas distintas
- [ ] Demostración del toggle: Serial muestra `[Harmony] ON` y `[Harmony] OFF` con B4

---

## Tests

### Vectores de test

Los tests son la especificación ejecutable de las tablas. Deben pasar antes de integrar
en el sketch (Batch C antes que Batch D).

```cpp
// apps/firmware-teensy/test/test_auto_harmonize/test_auto_harmonize.cpp
//
// Nota: usa Unity (PlatformIO native test framework).
// Correr con: pio test -e native -f test_auto_harmonize

#include <unity.h>
#include "ml/auto_harmonize.h"

static AutoHarmonize harmonize;

// Escala C mayor: pitch classes {0, 2, 4, 5, 7, 9, 11}
static const uint8_t C_MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};

// Escala A menor natural: pitch classes {9, 11, 0, 2, 4, 5, 7}
static const uint8_t A_MINOR_SCALE[7] = {9, 11, 0, 2, 4, 5, 7};

// Escala E menor natural: pitch classes {4, 6, 7, 9, 11, 0, 2}
static const uint8_t E_MINOR_SCALE[7] = {4, 6, 7, 9, 11, 0, 2};

// --- C Mayor: terceras diatónicas ---

void test_c_major_C4_third() {
    // C4 (60) → E4 (64), 3a mayor, grado 1 (+4st)
    uint8_t h = harmonize.get_harmony_note(60, C_MAJOR_SCALE, 0, true, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(64, h);
}

void test_c_major_D4_third() {
    // D4 (62) → F4 (65), 3a menor, grado 2 (+3st)
    uint8_t h = harmonize.get_harmony_note(62, C_MAJOR_SCALE, 0, true, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(65, h);
}

void test_c_major_E4_third() {
    // E4 (64) → G4 (67), 3a menor, grado 3 (+3st)
    uint8_t h = harmonize.get_harmony_note(64, C_MAJOR_SCALE, 0, true, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(67, h);
}

void test_c_major_G4_third() {
    // G4 (67) → B4 (71), 3a mayor, grado 5 (+4st)
    uint8_t h = harmonize.get_harmony_note(67, C_MAJOR_SCALE, 0, true, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(71, h);
}

// --- C Mayor: sextas diatónicas ---

void test_c_major_C4_sixth() {
    // C4 (60) → A4 (69), 6a mayor, grado 1 (+9st)
    uint8_t h = harmonize.get_harmony_note(60, C_MAJOR_SCALE, 0, true, HarmonyInterval::SIXTH);
    TEST_ASSERT_EQUAL(69, h);
}

void test_c_major_E4_sixth() {
    // E4 (64) → C5 (72), 6a menor, grado 3 (+8st)
    uint8_t h = harmonize.get_harmony_note(64, C_MAJOR_SCALE, 0, true, HarmonyInterval::SIXTH);
    TEST_ASSERT_EQUAL(72, h);
}

// --- A Menor natural: terceras diatónicas ---

void test_a_minor_A4_third() {
    // A4 (69) → C5 (72), 3a menor, grado 1 (+3st)
    uint8_t h = harmonize.get_harmony_note(69, A_MINOR_SCALE, 9, false, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(72, h);
}

void test_a_minor_C5_third() {
    // C5 (72) → E5 (76), 3a mayor, grado 3 (+4st)
    uint8_t h = harmonize.get_harmony_note(72, A_MINOR_SCALE, 9, false, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(76, h);
}

// --- Notas fuera de escala → 0xFF ---

void test_c_major_Csharp_returns_invalid() {
    // C#4 (61) no está en C mayor → 0xFF
    uint8_t h = harmonize.get_harmony_note(61, C_MAJOR_SCALE, 0, true, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(0xFF, h);
}

void test_c_major_Fsharp_returns_invalid() {
    // F#4 (66) no está en C mayor → 0xFF
    uint8_t h = harmonize.get_harmony_note(66, C_MAJOR_SCALE, 0, true, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(0xFF, h);
}

// --- Voice leading: sin saltos de más de una octava ---

void test_no_octave_jump_high_register() {
    // B5 (83) → D6 (86), grado 7, +3st. harmony = 86, que es 83+3 = no jump.
    // Verificar que harmony <= note + 12 siempre.
    uint8_t h = harmonize.get_harmony_note(83, C_MAJOR_SCALE, 0, true, HarmonyInterval::THIRD);
    TEST_ASSERT_LESS_OR_EQUAL(83 + 12, h);
    TEST_ASSERT_NOT_EQUAL(0xFF, h);
}

// --- NoteOff roundtrip ---

void test_store_and_pop_harmony() {
    harmonize.store_harmony(60, 64);
    uint8_t stored = harmonize.pop_harmony(60);
    TEST_ASSERT_EQUAL(64, stored);
    // Segunda llamada → ya no hay entrada
    uint8_t popped_again = harmonize.pop_harmony(60);
    TEST_ASSERT_EQUAL(0xFF, popped_again);
}

void test_pop_nonexistent_returns_invalid() {
    uint8_t h = harmonize.pop_harmony(99);
    TEST_ASSERT_EQUAL(0xFF, h);
}

// --- E Menor: spot check ---

void test_e_minor_E4_third() {
    // E4 (64) → G4 (67), 3a menor, grado 1 de Em (+3st)
    uint8_t h = harmonize.get_harmony_note(64, E_MINOR_SCALE, 4, false, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(67, h);
}

void test_e_minor_G4_third() {
    // G4 (67) → B4 (71), 3a mayor, grado 3 de Em (+4st)
    uint8_t h = harmonize.get_harmony_note(67, E_MINOR_SCALE, 4, false, HarmonyInterval::THIRD);
    TEST_ASSERT_EQUAL(71, h);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_c_major_C4_third);
    RUN_TEST(test_c_major_D4_third);
    RUN_TEST(test_c_major_E4_third);
    RUN_TEST(test_c_major_G4_third);
    RUN_TEST(test_c_major_C4_sixth);
    RUN_TEST(test_c_major_E4_sixth);
    RUN_TEST(test_a_minor_A4_third);
    RUN_TEST(test_a_minor_C5_third);
    RUN_TEST(test_c_major_Csharp_returns_invalid);
    RUN_TEST(test_c_major_Fsharp_returns_invalid);
    RUN_TEST(test_no_octave_jump_high_register);
    RUN_TEST(test_store_and_pop_harmony);
    RUN_TEST(test_pop_nonexistent_returns_invalid);
    RUN_TEST(test_e_minor_E4_third);
    RUN_TEST(test_e_minor_G4_third);
    return UNITY_END();
}
```

```bash
# Correr tests
cd apps/firmware-teensy
pio test -e native -f test_auto_harmonize
```

### Tabla de tests

| Test | Qué verifica | Tipo | Status |
|---|---|---|---|
| `test_c_major_C4_third` | Grado 1 mayor → +4st | native | Pendiente |
| `test_c_major_D4_third` | Grado 2 mayor → +3st | native | Pendiente |
| `test_c_major_E4_third` | Grado 3 mayor → +3st | native | Pendiente |
| `test_c_major_G4_third` | Grado 5 mayor → +4st | native | Pendiente |
| `test_c_major_C4_sixth` | Grado 1 mayor → +9st (sixth) | native | Pendiente |
| `test_c_major_E4_sixth` | Grado 3 mayor → +8st (sixth) | native | Pendiente |
| `test_a_minor_A4_third` | Grado 1 menor → +3st | native | Pendiente |
| `test_a_minor_C5_third` | Grado 3 menor → +4st | native | Pendiente |
| `test_c_major_Csharp_returns_invalid` | Nota cromática → 0xFF | native | Pendiente |
| `test_c_major_Fsharp_returns_invalid` | Nota cromática (tritono) → 0xFF | native | Pendiente |
| `test_no_octave_jump_high_register` | Voice leading: harmony ≤ note+12 | native | Pendiente |
| `test_store_and_pop_harmony` | Roundtrip NoteOff correcto | native | Pendiente |
| `test_pop_nonexistent_returns_invalid` | NoteOff sin NoteOn previo → 0xFF | native | Pendiente |
| `test_e_minor_E4_third` | Em grado 1 → +3st | native | Pendiente |
| `test_e_minor_G4_third` | Em grado 3 → +4st | native | Pendiente |

---

## Learnings

> Esta sección se completa DESPUÉS de la implementación.
> Vacía hasta que el sprint esté Done.

### Qué salió diferente al plan

_Completar post-implementación._

### Qué tomaría diferente

_Completar post-implementación._

### Dependencias para el siguiente sprint

- Sprint 6.3 (Smart Arpeggiator) usa el mismo `ScaleLock::get_scale_notes()` y el mismo
  principio de tablas diatónicas. El pattern de `AutoHarmonize` (tabla por modo + búsqueda
  de grado + función pura) puede reusarse como referencia de arquitectura para el Arp.
- El toggle de B4 establecido en este sprint define la convención de botones para features
  AI subsiguientes. Documentar cualquier decisión de UX que afecte ese mapeo.

### Tiempo real vs estimado

- Estimado: 3-4 sesiones
- Real: _completar_
- Delta: _completar_

---

*Sprint 37 — Theory escrita: 2026-05-28*
*Siguiente sprint: 38 — Smart Arpeggiator (Sprint 6.3)*
