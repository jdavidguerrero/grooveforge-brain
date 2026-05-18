# Sprint 4.4 — Chord Recognizer

**Feature:** Chord Recognition (Layer 1 AI)
**Target:** modelo INT8 <80KB, inferencia <8ms en Teensy 4.1, accuracy >80%
**Cross-ref:** `apps/docs/04-ai-architecture.md` §1 (Chord Recognition feature)

---

## §1. Teoría de acordes — por qué estas 5 familias

### Desde la acústica: por qué ciertos intervalos suenan consonantes

Cuando dos notas suenan simultáneamente, sus frecuencias forman una razón. La percepción de consonancia (suena "bien") versus disonancia ("suena tenso") está directamente relacionada con la simplicidad de esa razón:

| Intervalo | Razón de frecuencias | Percepción |
|---|---|---|
| Unísono | 1:1 | Idéntico |
| Octava | 2:1 | Consonante perfecta |
| 5ta justa | 3:2 | Muy consonante |
| 4ta justa | 4:3 | Consonante |
| 3ra mayor | 5:4 | Consonante suave |
| 3ra menor | 6:5 | Consonante suave |
| 7ma menor | 16:9 o 7:4 | Dissonante suave |
| Tritono | √2:1 | Muy disonante |

Un acorde es consonante cuando las razones de sus notas son simples entre sí. La tríada mayor (root + 3ra mayor + 5ta) tiene razones 4:5:6 — tres números pequeños, máxima consonancia.

### Las 5 familias elegidas

```
CHORD_INTERVALS = {
    'maj':  [0, 4, 7],       # root + 3ra mayor + 5ta justa     → razón 4:5:6
    'min':  [0, 3, 7],       # root + 3ra menor + 5ta justa     → razón 10:12:15
    '7':    [0, 4, 7, 10],   # mayor + 7ma menor                → "tensión que resuelve"
    'm7':   [0, 3, 7, 10],   # menor + 7ma menor                → jazz/soul "suave"
    'maj7': [0, 4, 7, 11],   # mayor + 7ma mayor (≠ 7ma menor!) → "sofisticado"
}
```

**Tríada mayor `[0,4,7]`** — el acorde "feliz". Base del pop, rock, folclore universal. C mayor = C + E + G. El E (4 semitonos) es la 3ra mayor que define el "color" brillante.

**Tríada menor `[0,3,7]`** — el acorde "triste". La diferencia con mayor es de un semitono: Eb (3) en lugar de E (4). Ese semitono cambia completamente la sensación emocional. Base del rock pesado, baladas, flamenco.

**Séptima dominante `[0,4,7,10]`** — el acorde de "tensión". Cuando escuchas un C7 (C+E+G+Bb), instintivamente esperas que resuelva a F mayor. Esta resolución V7→I es la base del blues (12-bar blues es básicamente 3 acordes dominantes), jazz, y toda la música tonal occidental. El Bb (10 semitonos) crea la tensión.

**Séptima menor `[0,3,7,10]`** — jazz, soul, R&B. Cm7 = C+Eb+G+Bb. Es la "versión suave" del acorde de séptima: conserva la 7ma menor (Bb) pero cambia E→Eb, dando un color más oscuro y "relajado". Omnipresente en jazz modal (Kind of Blue de Miles Davis usa casi exclusivamente acordes m7).

**Mayor séptima `[0,4,7,11]`** — jazz moderno, bossa nova. Cmaj7 = C+E+G+B. La diferencia con C7 es de un semitono: B (11) en lugar de Bb (10). Esa distinción es la más sutil de las 5 familias y predetermina las confusiones más frecuentes del modelo (ver §4).

### Por qué no más acordes

Existen docenas de tipos: disminuido, aumentado, sus2, sus4, add9, 9, 11, 13, etc. No se incluyen por dos razones:

1. **Cobertura vs complejidad**: estas 5 familias × 12 roots = 60 acordes cubren >90% de la música popular occidental. Añadir más tipos para cubrir el 10% restante duplicaría la dificultad del problema (más clases con mayor ambigüedad) sin beneficio proporcional.

2. **Limitación del histograma**: acordes como sus2 (root+2+7) vs sus4 (root+5+7) son muy similares en el histograma y el modelo los confundiría frecuentemente. Es mejor ser honesto sobre lo que el modelo puede detectar que añadir clases que el modelo no puede distinguir con fiabilidad.

---

## §2. Por qué el pitch class histogram funciona (y dónde falla)

### Por qué funciona

El pitch class histogram colapsa las 88 teclas del piano (o los 127 valores MIDI) a 12 bins, uno por cada semitono de la escala cromática. Esta operación es **invariante de octava**: C4 (262 Hz) y C5 (523 Hz) contribuyen ambos al bin 0.

Para un acorde, esto es perfecto: C mayor en posición fundamental (C-E-G), primera inversión (E-G-C), y segunda inversión (G-C-E) tienen exactamente los mismos 3 pitch classes activos. El histograma es **invariante de inversión**.

```
C4-E4-G4   →  histograma: bin[0]=1/3, bin[4]=1/3, bin[7]=1/3
E4-G4-C5   →  histograma: bin[0]=1/3, bin[4]=1/3, bin[7]=1/3  (igual)
G3-C4-E4   →  histograma: bin[0]=1/3, bin[4]=1/3, bin[7]=1/3  (igual)
```

### Dónde falla

**1. Ambigüedad de subconjunto.** Algunos acordes comparten notas:

```
C_maj  = {C, E, G}
Am     = {A, C, E}
Am7    = {A, C, E, G}  ← contiene todos los bins de C_maj!
```

Con ruido, Am7 y C_maj son difíciles de distinguir. El modelo aprende a usar el acorde que tiene *más* notas activas (Am7 tiene 4, C_maj tiene 3), pero con ruido moderado la distinción es ambigua.

**2. Polifonía compleja.** Si el guitarrista toca C mayor en el registro medio mientras el bajo toca G, el histograma combina ambas fuentes. No hay "un solo acorde" — el histograma mezcla la armonía completa. El modelo intenta predecir el acorde "principal" pero con un input contaminado.

**3. Acordes con bajo extraño (slash chords).** C/E (C mayor con bajo en E) suena diferente a C mayor porque el E en el bajo cambia el centro de gravedad armónico. Pero tienen el mismo histograma. El modelo los predecirá como el mismo acorde.

**Implicación para el producto:** estas limitaciones son aceptables para el GrooveForge Brain porque el input es el propio instrumento (monofónico o con polifonía limitada), no un mix complejo de múltiples instrumentos.

---

## §3. BatchNorm en redes pequeñas — cuándo ayuda y cuándo no

### Qué hace BatchNormalization desde primeros principios

Sin BatchNorm: durante el entrenamiento, la distribución de activaciones de cada capa cambia con cada update de parámetros. La capa siguiente tiene que "adaptarse" constantemente a una distribución moving — esto se llama **Internal Covariate Shift** (Ioffe & Szegedy, 2015). El resultado es convergencia lenta y necesidad de learning rates pequeños.

Con BatchNorm: normalizamos las activaciones de cada capa a media≈0, varianza≈1 *antes* de pasarlas a la siguiente. La siguiente capa siempre ve la misma distribución → convergencia 2-3× más rápida.

La fórmula completa de BatchNorm en el forward pass:

```
μ_batch = (1/m) Σ xᵢ                    # media del mini-batch
σ²_batch = (1/m) Σ (xᵢ - μ_batch)²     # varianza del mini-batch

x̂ᵢ = (xᵢ - μ_batch) / √(σ²_batch + ε) # normalización

yᵢ = γ × x̂ᵢ + β                         # re-escalado aprendido
```

Los parámetros γ (gamma) y β (beta) son **aprendidos** — le permiten al modelo "des-normalizar" si necesario. Epsilon (ε ≈ 1e-3) evita división por cero.

En **inferencia**: no hay mini-batch. BatchNorm usa las medias y varianzas del **dataset completo** (calculadas como media móvil durante training y guardadas como `moving_mean` y `moving_variance`). Por eso BatchNorm tiene comportamiento diferente en `training=True` vs `training=False`.

### Por qué NO se usó en key detector pero SÍ en chord recognizer

**Key detector (24 clases):** las 24 tonalidades tienen perfiles de Krumhansl-Schmuckler bien separados — la distancia entre C mayor y, digamos, F# mayor es grande. La red de 3 capas converge fácilmente sin BatchNorm.

**Chord recognizer (61 clases con alta correlación inter-clase):** la diferencia entre C_7 y C_maj7 es un solo semitono (Bb vs B — bins 10 vs 11). El gradiente de estas dos clases durante backprop interfiere: "aprender C_7" tiende a "desaprender C_maj7" porque comparten 3 de 4 bins. BatchNorm estabiliza los gradientes en estos boundaries.

Adicionalmente: 61 clases desde 12 inputs es un problema muy comprimido (ratio 12/61 ≈ 0.2 — el input es 5× más pequeño que el output). La red necesita aprender representaciones muy eficientes en las capas intermedias. BatchNorm en la primera capa asegura que los 12 features de entrada se procesen en un rango consistente, sin que ningún bin domine arbitrariamente por su escala.

### Costo en Teensy: zero latencia adicional

Esta es la razón más importante por la que BatchNorm es aceptable en TinyML:

El TFLiteConverter INT8 detecta el patrón `Dense → BatchNorm → ReLU` y lo **fusiona** en una sola operación `FullyConnected + Relu`. ¿Cómo?

BatchNorm aplica: `y = γ × (x·W + b - μ) / √(σ² + ε) + β`

Que puede reescribirse como: `y = (γ/√(σ²+ε)) × (x·W) + (γ(b-μ)/√(σ²+ε) + β)`

El TFLiteConverter absorbe los factores de BN directamente en los pesos W y el bias b de la capa Dense:

```
W_fused = W × (γ / √(σ² + ε))    # escalar los pesos
b_fused = (b - μ) × (γ / √(σ² + ε)) + β   # ajustar el bias
```

El resultado es un `FullyConnected` con pesos modificados — sin capa BN separada en el grafo TFLite. El runtime en Teensy ejecuta exactamente las mismas instrucciones CMSIS-NN que sin BatchNorm. **Costo: 0 ciclos adicionales.**

---

## §4. Matriz de confusión como herramienta diagnóstica musical

### Qué es una matriz de confusión

Para N clases, la matriz de confusión es una tabla N×N donde la entrada `[i,j]` representa cuántas veces el modelo predijo la clase j cuando la clase real era i.

```
Diagonal = predicciones correctas
Fuera de diagonal = errores
```

Para 61 clases, la matriz completa es 61×61 = 3721 celdas. En lugar de visualizarla completa, nos centramos en las confusiones más frecuentes (las celdas fuera de diagonal con mayor valor).

### Confusiones musicalmente predecibles

Para el chord recognizer, las confusiones más frecuentes son:

**C_maj ↔ C_maj7** (difieren en 1 nota: el B):
- C_maj  = bins activos: 0 (C), 4 (E), 7 (G)
- C_maj7 = bins activos: 0 (C), 4 (E), 7 (G), **11 (B)**
- Con noise_level=0.05, el bin 11 puede perderse en el ruido → confusión

**C_min ↔ C_m7** (difieren en 1 nota: el Bb):
- C_min = bins activos: 0, 3, 7
- C_m7  = bins activos: 0, 3, 7, **10 (Bb)**
- Misma situación: la 7ma puede estar por debajo del ruido

**C_7 ↔ C_maj7** (difieren en 1 nota: Bb vs B):
- C_7   = bins activos: 0, 4, 7, **10 (Bb)**
- C_maj7 = bins activos: 0, 4, 7, **11 (B)**
- Ambos tienen 4 notas; el único diferenciador es un semitono

Estas confusiones son **musicalmente esperadas y honestamente limitaciones del histograma** — no bugs del modelo. Un músico con audición entrenada puede confundir C7 y Cmaj7 si la nota diferenciadora (Bb vs B) está mal articulada.

### Implicación para el display del GrooveForge Brain

Dado que estas confusiones son inevitables con el histograma, la UI debe comunicar la incertidumbre:

```
confidence > 0.7  →  "C maj7"          (mostrar con certeza)
confidence 0.5–0.7 → "C maj7?"         (mostrar con interrogación)
confidence < 0.5  →  no mostrar acorde  (muy incierto)
```

El umbral de 0.6 está codificado en la clase Teensy como sugerencia de uso. El valor final puede ajustarse después de pruebas con instrumentos reales.

---

## §5. Comparación key detector vs chord recognizer

| Aspecto | Key Detector (Sprint 4.3) | Chord Recognizer (Sprint 4.4) |
|---|---|---|
| Clases | 24 (12 maj + 12 min) | 61 (60 acordes + N) |
| Arquitectura | 12→128→64→24 (Dense) | 12→128→64→61 (Dense+BN) |
| Parámetros | ~11.5K | ~14.8K |
| Tamaño int8 estimado | ~4 KB | ~15-20 KB |
| Accuracy objetivo | >85% | >80% |
| Tensor arena | ~16 KB | ~24 KB |
| BatchNorm | No | Sí (fusionado en TFLite) |
| Latencia Teensy est. | <4ms | <8ms |
| Dataset sintético | 30K samples | 15K samples |
| Difficulty nivel | Medio | Alto |

**Por qué chord recognizer tiene menor accuracy objetivo:**

No es un modelo peor — es un problema genuinamente más difícil. La razón fundamental: con 24 clases de tonalidad, la separación entre perfiles de Krumhansl-Schmuckler es grande (las tonalidades alejadas en el círculo de quintas tienen perfiles muy distintos). Con 61 clases de acorde, muchas clases comparten 3 de 4 notas y son intrínsecamente ambiguas desde el histograma.

**Por qué chord recognizer necesita menos samples:**

Los acordes tienen una representación exacta (un conjunto de pitch classes fijos), mientras que las tonalidades son distribuciones estadísticas complejas. El dataset sintético de acordes es "más puro" — cada sample es una instancia clara del acorde (con ruido pequeño), no una aproximación estadística. 15K samples balanceados son suficientes porque el modelo tiene menos ambigüedad para aprender.

---

## §6. Integración en el firmware — uso conjunto con KeyDetector

En producción, key detector y chord recognizer comparten el mismo input:

```
Evento MIDI (note on/off)
    ↓
acumular notas activas (ventana deslizante de 1-2 segundos)
    ↓
calcular pitch class histogram (12 floats, suma = 1.0)
    ↓
KeyDetector::inference(histogram, key_result)      → display: "Em"
ChordRecognizer::inference(histogram, chord_result) → display: "Em7"
```

El tensor arena **se reutiliza** entre las dos inferencias (se corren secuencialmente, no en paralelo). En el código:

```cpp
// Un solo arena compartido — el más grande de los dos modelos:
static constexpr int kSharedArenaSize = CHORD_RECOGNIZER_ARENA_SIZE;  // 24KB
static uint8_t shared_arena[kSharedArenaSize] __attribute__((aligned(16)));

// KeyDetector y ChordRecognizer usan el mismo arena secuencialmente.
// En la práctica, cada clase tiene su propio _arena[] — esto es correcto
// mientras no se ejecuten en paralelo (no hay RTOS, todo en el loop principal).
```

El budget total de tensor arena es 200KB (04-ai-architecture.md §1.2). Con estos dos modelos:
- KeyDetector:      ~16KB
- ChordRecognizer:  ~24KB
- Subtotal Sprint 4.3+4.4: ~40KB de los 200KB disponibles

---

## Archivos creados en este sprint

| Archivo | Descripción |
|---|---|
| `apps/training/models/chord_recognizer/dataset.py` | Generación de dataset sintético (61 clases) |
| `apps/training/models/chord_recognizer/model.py` | Arquitectura Dense+BN, función de training |
| `apps/training/models/chord_recognizer/train.py` | Pipeline completo: train → TFLite → C array |
| `apps/firmware-teensy/src/ml/chord_recognizer.h` | Clase `ChordRecognizer` para Teensy |
| `apps/firmware-teensy/src/ml/chord_recognizer.cpp` | Implementación TFLite Micro |
| `apps/training/tests/test_chord_recognizer.py` | Tests pytest — dataset y propiedades musicales |

## Comandos de verificación

```bash
# Tests unitarios (sin TensorFlow, sin hardware)
cd apps/training
uv run pytest tests/test_chord_recognizer.py -v

# Training completo (genera artifacts + header para Teensy)
uv run python models/chord_recognizer/train.py

# Build firmware (verifica que el .h compila)
cd apps/firmware-teensy
pio run -e sketch
```

## Métricas de aceptación

| Métrica | Target | Fuente |
|---|---|---|
| Accuracy float32 val | >80% | Sprint 4.4 (relajado de 85% por 61 clases) |
| Accuracy delta float→int8 | <5% | `04-ai-architecture.md` §8 |
| Tamaño int8 | <80KB | `04-ai-architecture.md` §1 |
| Tensor arena | ≤24KB | Estimación post-training |
| Latencia Teensy | <8ms | `04-ai-architecture.md` §1 |
| Tests pytest | 100% pass | Sprint gate |
| Build Teensy | SUCCESS | Sprint gate |
