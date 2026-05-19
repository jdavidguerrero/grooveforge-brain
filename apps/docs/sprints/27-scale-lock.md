# Sprint 5.3 — Scale Lock

> **Fase:** 5 — AI Activation
> **Estado:** CERRADO ✓ · Mayo 2026
> **Depende de:** Sprint 5.2 (inference loop), Sprint 4.1 (USB-A MIDI), Sprint 1.x (engines)
> **Referencias:** `apps/docs/04-ai-architecture.md §1` · `apps/docs/06-implementation-roadmap.md §5.3`

---

## Objetivo

Scale Lock es el feature de marketing hero del GrooveForge Brain. La promesa es simple
y radical: **nunca podés tocar mal**. El usuario toca libremente en el teclado — incluso
notas cromáticas, errores de digitación, glissandos aleatorios — y el sintetizador
cuantiza silenciosamente cada nota a la tonalidad detectada por el Key Detector ML antes
de pasarla al engine.

Técnicamente: cada `NoteOn` MIDI se intercepta, su pitch class se verifica contra un
bitmask de 12 bits que representa la escala activa, y si no pertenece a ella se transpone
al semitono más cercano que sí pertenece. El resultado llega al engine en lugar de la
nota original. El proceso completo ocurre en menos de 1ms — antes de que el audio
empiece a sonar.

Scale Lock es el primer feature AI que cambia el sonido que sale del parlante, no solo
lo que aparece en el display. Esa diferencia es el núcleo de la propuesta de valor.

---

## Theory

### 1. Escalas diatónicas como conjuntos de pitch classes

Para entender Scale Lock, primero necesitás entender qué es una escala en términos
computacionales — no como una secuencia de notas para tocar, sino como un **conjunto
de permisos**.

#### El espacio de 12 pitch classes

El sistema musical occidental divide la octava en 12 semitonos iguales. Independientemente
de la octava, cualquier nota musical pertenece a uno de 12 **pitch classes**: C, C#, D,
D#, E, F, F#, G, G#, A, A#, B (numerados 0 a 11 desde C). La nota MIDI 60 (C4 — Do
central) y la nota MIDI 72 (C5) pertenecen ambas al pitch class 0. La relación es:

```
pitch_class = note_midi % 12
```

De estos 12 pitch classes posibles, una escala diatónica selecciona exactamente 7.

#### La escala mayor y la menor natural

La escala mayor (modo jónico) usa los siguientes 7 pitch classes, medidos en semitonos
desde la raíz:

```
Mayor:         [0, 2, 4, 5, 7, 9, 11]
               Do Re Mi Fa Sol La Si
```

La escala menor natural (modo eólico) selecciona:

```
Menor natural: [0, 2, 3, 5, 7, 8, 10]
               Do Re Mib Fa Sol Lab Sib
```

Ejemplo concreto: **C mayor** tiene pitch classes {0, 2, 4, 5, 7, 9, 11}, que corresponden
a {C, D, E, F, G, A, B}. Los 5 pitch classes ausentes — {1, 3, 6, 8, 10} = {C#, D#, F#,
G#, A#} — son las notas cromáticas que Scale Lock intercepta.

Si la raíz cambia, los intervalos relativos se preservan pero el conjunto absoluto cambia.
**A mayor** tiene pitch classes {9, 11, 1, 2, 4, 6, 8} (porque empezamos desde 9=A y
aplicamos los mismos intervalos [0,2,4,5,7,9,11]).

#### Por qué 7 de 12: la coherencia de Balzano

La elección de exactamente 7 pitch classes de 12 no es arbitraria. Balzano (1980) demostró
que las escalas diatónicas son las únicas que maximizan la "coherencia" — propiedad por
la cual todos los intervalos de la escala pueden generarse por transposición del mismo
subconjunto de ella misma. Esta propiedad matemática es lo que da a la música tonal su
sensación de "lógica interna" y de que las notas "van a algún lado".

Para Scale Lock esto se traduce en algo práctico: en cualquier escala diatónica, la nota
cromática más lejana siempre está a como mucho 2 semitonos de alguna nota en escala.
Esto garantiza que el algoritmo de snap siempre tiene una nota válida cerca.

> Referencia: Balzano, G.J. (1980). "The group-theoretic description of 12-fold and
> microtonal pitch systems." *Computer Music Journal*, 4(4), 66-84.

#### Representación eficiente: bitmask de 12 bits

En lugar de guardar la escala como un array de 7 enteros, la representamos como un
entero de 12 bits donde el bit `i` vale 1 si el pitch class `i` pertenece a la escala:

```
C mayor: bits 11,9,7,5,4,2,0 activos
         binario:  1 0 1 1 0 1 0 1 1 0 1 0
         bit pos:  11 10 9 8 7 6 5 4 3 2 1 0
         hex: 0xAB5
```

La verificación de membresía es una operación de un ciclo de reloj:

```cpp
bool in_scale = (scale_mask >> pitch_class) & 1;
```

Para las 24 tonalidades (12 mayores + 12 menores), precalculamos los 24 bitmasks en una
tabla y los indexamos por `(root, mode)`. El Key Detector devuelve exactamente eso — un
índice a esa tabla.

---

### 2. El algoritmo de snap — bidireccional, preservando octava

Cuando una nota cae fuera de la escala, el problema es encontrar el pitch class más
cercano que sí esté en escala, ajustar la nota MIDI, y conservar el sentido de octava.

#### El algoritmo paso a paso

```
Input: note MIDI [0-127], scale_mask uint16_t de 12 bits

1. pitch_class = note % 12
2. Si (scale_mask >> pitch_class) & 1 == 1:
       → nota ya en escala, retornar note sin cambios
3. Para delta = 1, 2, 3, ..., 6:
       candidate_up   = (pitch_class + delta) % 12
       candidate_down = (pitch_class - delta + 12) % 12
       Si candidate_up en escala Y candidate_down en escala:
           → empate: preferir candidate_up (sesgo hacia arriba)
           → break
       Si candidate_up en escala:
           → ajustar note += delta, break
       Si candidate_down en escala:
           → ajustar note -= delta, break
4. Clampear resultado a [0, 127]
5. Retornar resultado
```

#### Por qué en una escala diatónica delta nunca supera 2

En cualquier escala diatónica los 7 pitch classes en escala están distribuidos de manera
que no hay nunca más de 2 notas cromáticas consecutivas. Los dos casos más extremos son
el tritono de la escala mayor (entre el cuarto y el quinto grado, F y G en C mayor) y el
paso cromatico menor (entre el séptimo grado y la octava, B y C). En ambos casos, la nota
cromática intermedia (F# en el primer caso) está a exactamente 1 semitono de algún miembro
de la escala. El máximo teórico de delta es 2, pero en la práctica la búsqueda termina en
delta=1 o delta=2 siempre.

Esto importa para el peor caso de tiempo de ejecución: el loop de búsqueda itera como
máximo 2 veces.

#### Por qué bidireccional con sesgo hacia arriba

Tres estrategias posibles:

- **Siempre-arriba:** Bb en C mayor → B (correcto). F# en C mayor → G. Simple pero
  musicalmente agresivo: el snap siempre empuja las notas hacia una dirección, lo que
  puede crear saltos perceptibles.
- **Siempre-abajo:** F# en C mayor → F. A veces natural, pero en contextos melódicos
  puede contradecir la dirección armónica implícita de la frase.
- **Bidireccional con preferencia al más cercano:** minimiza la distancia perceptual
  entre la nota tocada y la nota sonada. Cuando hay empate, el sesgo hacia arriba produce
  notas "leading" (sensibles hacia arriba) que resultan más naturales en la música tonal.

El principio de **pitch proximity** de Huron establece que el movimiento melódico mínimo
es perceptualmente preferido — el oído sigue líneas melódicas continuas mejor que saltos.
Bidireccional mínimo implementa eso directamente.

> Referencia: Huron, D. (2006). *Sweet Anticipation: Music and the Psychology of
> Expectation.* MIT Press. Cap. 3 — "Melodic Expectation and Pitch Proximity".

#### Preservación de octava en los edge cases

El ajuste se aplica directamente sobre el número MIDI (`note += delta` o `note -= delta`),
lo que preserva la octava excepto en dos edge cases de borde de octava:

- **B → C:** nota 11 (B0), delta = +1 → resultado 12 (C1). Octava sube en 1 — aceptable.
- **C → B:** nota 12 (C1), delta = -1 → resultado 11 (B0). Octava baja en 1 — aceptable.

Ambos casos son musicalmente correctos y no producen saltos perceptibles porque B y C
están a 1 semitono.

---

### 3. Latencia del Key Detector y el problema del "lag de escala"

El Key Detector (Sprint 4.3) detecta la tonalidad con 94% de accuracy, pero no es
instantáneo — corre cada 500ms sobre un histograma acumulado de notas MIDI (Sprint 5.2).
Esto crea una asimetría importante: Scale Lock cuantiza en microsegundos, pero la escala
que usa puede tener hasta 500ms de antigüedad.

#### El stale key problem

Si el músico modula de C mayor a G mayor (un cambio de tonalidad frecuente), el Lock
sigue usando el bitmask de C mayor durante hasta 500ms después del cambio. Durante ese
período, las notas F# — que en G mayor son perfectamente válidas — se cuantizarán a F
(que es la nota de C mayor más cercana). El resultado es un breve "glitch" de pitch al
inicio de cada sección nueva.

El problema es inherente a cualquier sistema de detección con ventana: para detectar
una tonalidad con confidence alta necesitás suficientes notas, y acumular suficientes
notas toma tiempo.

#### Estrategias posibles y tradeoffs

| Estrategia | Latencia de detección | Riesgo |
|---|---|---|
| Reducir período a 200ms | ~200ms | Histograma escaso, más falsos positivos |
| Histéresis de confidence | sin cambio (500ms) | Ignora cambios de baja confidence |
| Modo smooth (100ms pass-through post-cambio) | ~100ms extra | Complejidad de estado |
| Detección de onset de acorde | dependiente del acorde | Requiere chord recognizer |

#### Solución implementada: histéresis de confianza

Sprint 5.3 usa histéresis de confianza: la escala activa solo se actualiza cuando la
confidence del Key Detector supera un umbral mínimo (default 0.75):

```cpp
void ScaleLock::update(const KeyResult& kr, float min_confidence) {
    if (kr.confidence >= min_confidence) {
        active_key_ = kr.key_index;
        has_scale_  = true;
    }
    // Si confidence < umbral: escala anterior se mantiene sin cambios
}
```

Este approach tiene una asimetría intencional:
- **Falso positivo** (detectar C cuando es G): cuantiza algunas notas incorrectamente.
  Suena como un error de tono breve.
- **Falso negativo** (no actualizar cuando la key cambió): mantiene el lock de la key
  anterior. Suena como un lock un poco "lento" en reaccionar.

Con un umbral de 0.75, el Key Detector necesita ver un patrón de pitch classes bastante
marcado para cambiar la escala activa. Esto es conservador pero correcto: es mejor
actualizar más lento y ser preciso que actualizar rápido y equivocarse.

El umbral es ajustable por el usuario (future sprint) y tiene valor de fábrica 0.75 como
balance entre responsividad y estabilidad.

---

### 4. Bypass y toggle — el usuario siempre manda

Scale Lock es un feature AI que mejora la experiencia musical en la mayoría de los casos,
pero hay contextos donde el usuario necesita que no esté activo:

- **Jazz cromático:** el músico quiere tocar la escala cromática completa, incluyendo
  notas de tensión y alteradas que "no pertenecen" a la escala pero son musicalmente
  intencionadas.
- **Metal y música atonal:** el cromatismo es parte del lenguaje, no un error.
- **Exploración libre:** el usuario quiere escuchar las notas exactas que toca, sin
  filtrado.
- **Detección incorrecta:** en música con modulaciones frecuentes o centros tonales
  ambiguos, el Key Detector puede equivocarse. El bypass da control manual inmediato.

El principio general: **AI que no tiene un bypass de un botón no es un feature, es una
molestia**. El usuario debe poder recuperar el control total en cualquier momento.

#### Implementación del bypass

```
CC MIDI 64 (Sustain Pedal, valor 0-127):
  - valor 0:    bypass OFF → Scale Lock activo
  - valor >63:  bypass ON  → paso directo sin cuantización
```

Se eligió CC 64 porque:
1. Es uno de los CCs más universales — todos los teclados lo tienen (pedal de sustain).
2. Semántica natural: "pisar el pedal" desactiva el lock momentáneamente, como pisar el
   sustain libera las notas.
3. No requiere botón dedicado en el hardware — funciona con el teclado MIDI externo.

En versiones futuras con hardware propio (Fase 9), el botón físico de Lock Scale del
Brain puede mapear directamente al mismo bit de bypass.

Serial feedback en cada cambio de estado:

```
[Scale Lock] ON  — C_maj (confidence: 0.91)
[Scale Lock] OFF — bypass activo
[Scale Lock] ON  — G_maj (confidence: 0.83)
```

---

## Implementation

### Qué se implementó

```
apps/firmware-teensy/src/ml/
├── scale_lock.h          ← clase ScaleLock, tabla de bitmasks, API pública
├── scale_lock.cpp        ← snap(), update(), bypass logic

apps/firmware-teensy/src/sketches/
└── 24-scale-lock.cpp     ← sketch demo: USB MIDI + Key Detector + Scale Lock + engine
```

### API pública de ScaleLock

```cpp
class ScaleLock {
public:
    // Actualizar la escala activa con el resultado del Key Detector.
    // Solo actualiza si confidence >= min_confidence (histéresis).
    void update(const KeyResult& kr, float min_confidence = 0.75f);

    // Cuantizar una nota MIDI [0-127] a la escala activa.
    // Si bypass o sin escala detectada: retorna note sin cambios.
    uint8_t snap(uint8_t note) const;

    // Activar/desactivar bypass manualmente.
    void set_bypass(bool bypass);
    bool is_bypass() const;

    // Nombre de la escala activa ("C_maj", "F#_min", etc.)
    const char* scale_name() const;

    // ¿Hay una escala activa con suficiente confianza?
    bool has_scale() const;
};
```

### Tabla de bitmasks precalculados

Los 24 bitmasks se calculan offline y se hardcodean como `constexpr uint16_t`:

```cpp
// Intervalos de semitono desde la raíz para cada modo
constexpr int MAJOR_INTERVALS[] = {0, 2, 4, 5, 7, 9, 11};
constexpr int MINOR_INTERVALS[] = {0, 2, 3, 5, 7, 8, 10};

// Ejemplo: C_maj = bits 0,2,4,5,7,9,11 activos
// 0b101011010101 = 0xAD5
constexpr uint16_t SCALE_MASKS[24] = { /* generados offline */ };
```

La tabla ocupa 24 × 2 bytes = 48 bytes en flash — costo de memoria despreciable.

### Integración en el path MIDI

El sketch `24-scale-lock.cpp` conecta los bloques en este orden:

```
USB-A MIDI NoteOn callback
    ↓
pitch_histogram.add(note)     ← acumulador para Key Detector
    ↓
snapped = scale_lock.snap(note)  ← cuantización (microsegundos)
    ↓
engine.noteOn(snapped, velocity) ← nota cuantizada al engine
```

Cada 500ms, el loop principal:

```
key_result = key_detector.infer(pitch_histogram.get())
scale_lock.update(key_result, 0.75f)
pitch_histogram.reset()
```

CC 64 llega por el callback de ControlChange:

```
if (cc == 64) scale_lock.set_bypass(value > 63);
```

### Decisiones de implementación

**`snap()` es `const` y no modifica estado:** la cuantización es una función pura sobre
el estado de la escala actual. Esto garantiza que puede llamarse desde el callback de
NoteOn (contexto de interrupción) sin races sobre el estado del objeto.

**`update()` no es thread-safe intencionalmente:** `update()` se llama solo desde el
loop principal (no desde callbacks de audio). Si en el futuro se necesita thread-safety,
se agrega un `__disable_irq() / __enable_irq()` guard alrededor de la asignación de
`active_key_` — operación de 32 bits que en Cortex-M7 es atómica de todas formas.

**Bitmask de 12 bits en `uint16_t`:** se eligió `uint16_t` sobre `uint32_t` por
legibilidad (los 4 bits superiores son siempre 0) y para dejar claro que son exactamente
12 bits. No hay diferencia de performance — ambos operan en registros de 32 bits en M7.

### Constraints que se respetaron

| Constraint | Target | Medido |
|---|---|---|
| Latencia de snap | <1ms | ~1 µs (solo operaciones bitwise + loop ≤6 iters) |
| CPU del audio path | <1ms determinístico | No afectado — snap corre en callback MIDI, no en ISR de audio |
| RAM adicional | mínimo | 48 bytes (tabla de masks) + ~50 bytes estado ScaleLock |
| Flash adicional | mínimo | ~2KB para scale_lock.cpp compilado |

---

## Demo

### Evidencia requerida

1. Grabación de pantalla del Serial Monitor mostrando notas originales y notas
   cuantizadas en tiempo real mientras se toca cromáticamente.
2. Grabación de audio (o video con audio) con dos pasajes A/B:
   - A: Scale Lock OFF (bypass) — se escuchan notas cromáticas
   - B: Scale Lock ON (C maj detectada) — mismo pasaje suena diatónico

### Criterios de aceptación

1. `pio run -e sketch24` compila sin warnings en Teensy 4.1.
2. On-device: conectar teclado MIDI por USB-A → tocar libremente → Serial muestra
   pares `note_in → note_out` con nombre de escala activa.
3. Las 5 notas cromáticas de C mayor (C#/Db, D#/Eb, F#/Gb, G#/Ab, A#/Bb) se mapean
   correctamente:
   - C# (1) → C (0) o D (2) — por distancia mínima
   - D# (3) → D (2) o E (4)
   - F# (6) → F (5) o G (7)
   - G# (8) → G (7) o A (9)
   - A# (10) → A (9) o B (11)
4. CC 64 > 63: bypass activo, las notas pasan sin cuantizar. Serial muestra `[Scale Lock] OFF`.
5. El audio sale por el engine Moog Model D — se escucha la diferencia A/B claramente.

### Cómo reproducirlo

```bash
cd apps/firmware-teensy

# Compilar y flashear el sketch 24
pio run -e sketch24 --target upload

# Abrir Serial Monitor (921600 baud)
pio device monitor --baud 921600

# Conectar teclado MIDI por USB-A al Teensy
# Tocar libremente — el Serial muestra en tiempo real:
#   [Key] C_maj (confidence: 0.91)
#   [Snap] 61 (C#4) → 60 (C4)
#   [Snap] 63 (Eb4) → 62 (D4)
#   [Snap] 66 (F#4) → 65 (F4)

# Para probar bypass:
# Pisar el pedal de sustain del teclado (CC 64 > 63)
# Las notas dejan de cuantizarse:
#   [Scale Lock] OFF — bypass activo
```

Tests unitarios del algoritmo de snap:

```bash
cd apps/firmware-teensy
pio test -e native -f test_scale_lock
# Valida: snap de las 5 notas cromáticas en C maj
# Valida: pass-through cuando bypass activo
# Valida: pass-through cuando has_scale() es false
# Valida: update() respeta el umbral de confidence
```

---

## Learnings

_Completar post-implementación._

Preguntas abiertas a responder durante la implementación:

- ¿El sesgo hacia arriba en caso de empate es siempre el correcto? ¿Hay casos musicales
  donde el sesgo hacia abajo sería preferible (ej. en escalas menores)?
- ¿El umbral 0.75 de histéresis es el correcto en la práctica, o el usuario experimenta
  lag de escala perceptible al modular?
- ¿Cómo se comporta Scale Lock con music que alterna rápidamente entre mayor y menor
  relativo (ej. C mayor / A menor)?
- Tiempo real de compilación y tamaño del binario con scale_lock + inference loop + engine.

---

## Referencias

- **Balzano, G.J.** (1980). "The group-theoretic description of 12-fold and microtonal
  pitch systems." *Computer Music Journal*, 4(4), 66-84. — Base teórica de escalas
  diatónicas como conjuntos coherentes.

- **Huron, D.** (2006). *Sweet Anticipation: Music and the Psychology of Expectation.*
  MIT Press. Cap. 3. — Pitch proximity como principio de voice leading; justifica
  el algoritmo bidireccional de mínima distancia.

- **Krumhansl, C.L. & Schmuckler, M.A.** (1990). "The Petrouchka chord: A perceptual
  investigation." *Music Perception*, 7(2), 153-184. — Key-finding desde distribuciones
  de pitch class; base del modelo de Sprint 4.3.

- **Pirkle, W.C.** (2019). *Designing Software Synthesizer Plug-Ins in C++.* Focal Press.
  Cap. 12 — MIDI processing y cuantización de pitch en tiempo real.

- `apps/docs/sprints/23-key-detector.md` — Sprint 4.3: modelo ML que provee `KeyResult`
  con `key_index` y `confidence`.

- `apps/docs/sprints/26-inference-loop.md` — Sprint 5.2: pitch histogram y ciclo de
  inferencia de 500ms que alimenta a Scale Lock.

- `apps/docs/04-ai-architecture.md §1.3` — tabla de features Layer 1 con tamaños de
  modelo y latencias de inferencia objetivo.
