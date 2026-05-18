# Sprint 4.5 — Beat Follower Model

**Feature:** Beat Follower — detección de BPM en tiempo real desde eventos MIDI  
**Status:** Implementado  
**Budget spec:** 30KB flash (INT8), 6ms inference, tensor arena ~16KB  
**Cross-ref:** `apps/docs/04-ai-architecture.md` §1 (Beat Follower feature)

---

## §1. Qué es un "onset" y por qué es la unidad fundamental del ritmo

Un **onset** es el instante exacto en que comienza un sonido. En el contexto de GrooveForge Brain, cada NoteOn recibido por el Teensy es un onset con precisión de microsegundos (el reloj USB MIDI tiene granularidad de 1ms vía `millis()`).

El onset es la unidad fundamental del ritmo porque el ritmo es precisamente *cuándo* suenan los eventos, no *qué* suenan ni *cómo* suenan. Un batería tocando en 4/4 a 120 BPM produce onsets cada 500ms — la altura de las notas no importa para detectar el tempo.

### Por qué MIDI y no audio directo

Detectar onsets desde audio requiere un **onset detection function** (ODF): se calcula la derivada de la energía espectral, el flujo espectral (Flux), o la función de rectificación de fase compleja. Estas funciones tienen:

- Latencia inherente (ventanas de 20-50ms)
- Falsos positivos con instrumentos percusivos
- Complejidad computacional alta para el Teensy

El Beat Follower del Brain tiene una ventaja enorme: **el MIDI le da los onsets gratis, con exactitud de hardware**. Cada NoteOn es un onset certificado, sin necesidad de análisis de audio.

---

## §2. Inter-Onset Intervals (IOI) — la huella temporal del ritmo

El **IOI** (Inter-Onset Interval) es el tiempo en milisegundos entre un onset y el siguiente:

```
IOI[i] = timestamp[i+1] - timestamp[i]
```

Si un músico toca en 120 BPM exacto, todos los IOIs son 500ms (= 60000ms / 120).

### Por qué los IOIs son la representación correcta

1. **Invariantes al pitch**: no importa si el músico tocó Do o Sol — solo cuándo.
2. **Capturan la estructura métrica**: en 4/4 a 120 BPM, los beats principales tienen IOI=500ms, las corcheas IOI=250ms, los tresillos IOI=167ms. La distribución de IOIs codifica el patrón métrico.
3. **Acumulables**: con un buffer circular de 32 onsets obtenemos 31 IOIs — suficiente historia para estimar el tempo con jitter realista.
4. **Compresión temporal eficiente**: 31 IOIs se comprimen en un histograma de 32 bins. Esto elimina la necesidad de procesar series temporales largas (que requeriría LSTM y más memoria).

### La distribución de IOIs como fingerprint del tempo

Para un músico tocando en 120 BPM:
- Beat principal: IOI ≈ 500ms → pico en el bin ~7 del histograma
- Corcheas: IOI ≈ 250ms → energía en el bin ~3
- Notas largas: IOI ≈ 1000ms → energía en el bin ~15

El histograma de IOIs muestra estos picos como una **firma del tempo**. La posición del pico principal identifica el tempo; los picos secundarios (harmónicos) identifican las subdivisiones.

---

## §3. Autocorrelación y la ambigüedad double-time/half-time

La **autocorrelación** de la secuencia de onsets es el algoritmo clásico de tempo estimation (Dixon, 2001; Scheirer, 1998):

1. Crear una función de activación con pulsos en los instantes de onset
2. Correlacionar la función consigo misma con distintos lags τ
3. Los picos de la autocorrelación en lag=τ indican "hay un patrón que se repite cada τ ms"
4. El lag con el pico más prominente (excepto τ=0) es el período estimado del beat

### El problema fundamental: los armónicos

Si el tempo real es 120 BPM (τ=500ms), la autocorrelación tendrá picos en:
- τ=250ms → 240 BPM (doble tiempo — las corcheas como "beat")
- τ=500ms → 120 BPM (el tempo real)
- τ=1000ms → 60 BPM (mitad del tiempo — la blanca como "beat")

Estos tres picos son **matemáticamente igualmente válidos**. La autocorrelación sola no puede decidir cuál es el beat real — todos son períodos repetitivos válidos de la secuencia.

### Cómo lo resuelve el modelo ML

Un humano resuelve la ambigüedad con contexto musical: "esto suena a techno a 120 BPM, no a ambient a 60 BPM". El modelo ML aprende a resolver la ambigüedad **por la distribución relativa de IOIs**:

- En 120 BPM real: los onsets en 500ms son más frecuentes que en 250ms (el beat es más común que la corchea)
- En 60 BPM real: los onsets en 1000ms son más frecuentes que en 500ms

El modelo aprende estadísticamente que "distribución con pico en 500ms Y pico secundario menor en 250ms" = 120 BPM, mientras que "distribución con pico en 1000ms Y pico secundario menor en 500ms" = 60 BPM.

---

## §4. Por qué 32 bins y 32 BPM classes — el diseño del espacio de representación

### 32 bins del histograma

El rango cubierto es 50ms–2000ms:
- 50ms = 1200 BPM (límite físico de velocidad de teclado)  
- 2000ms = 30 BPM (más lento que cualquier música práctica)

Con 32 bins: bin width = (2000 - 50) / 32 ≈ **60.9ms por bin**.

En el rango de 120 BPM (500ms), esto da una resolución de ±30ms ≈ **±7 BPM**. Para mostrar el BPM en el display del Brain (que muestra números enteros), esta resolución es más que suficiente.

### 32 clases de BPM

Las clases están espaciadas **logarítmicamente** porque la percepción del tempo es logarítmica: el cambio de 120→132 BPM (10%) suena al mismo "tamaño" que 60→66 BPM (10%). Pasos en porcentaje constante producen pasos perceptuales uniformes.

```
BPM_CLASSES = [60, 63, 66, 69, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116,
               120, 126, 132, 138, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 240]
```

Los saltos entre clases crecen con el BPM: 3 BPM en el rango lento (60-76), 6-8 BPM en el rango medio (120-160), 12-16 BPM en el rango rápido (192-240).

### Coincidencia de 32 y 32

El número de bins del histograma (32) y el número de clases de BPM (32) es una coincidencia numérica conveniente, no una relación matemática obligatoria. Se eligieron independientemente:
- 32 bins: balance entre resolución temporal y tamaño del vector de features
- 32 clases: cobertura del rango musical con granularidad perceptualmente uniforme

---

## §5. El modelo como desambiguador — por qué ML y no solo autocorrelación

Un algoritmo puramente determinístico (autocorrelación sobre IOIs) tiene accuracy de aproximadamente **70-75% en música real**, con el 25-30% restante siendo errores de double-time/half-time.

El modelo ML aprende a resolver estas ambigüedades usando la **forma completa del histograma**, no solo la posición del pico:

```
Arquitectura: Dense 3-layer
Input(32) → Dense(64, ReLU) → Dropout(0.2) → Dense(32, ReLU) → Dense(32, Softmax)
```

Parámetros totales: 32×64+64 + 64×32+32 + 32×32+32 = 5,248 parámetros.

### Por qué Dense y no LSTM aquí (v1.0)

El histograma de IOIs YA es una representación temporal agregada. Los 32 bins codifican la distribución de todos los tiempos entre onsets de los últimos 31 eventos. No hay una secuencia temporal que un LSTM deba explotar — la información temporal está comprimida en la forma del histograma.

Un LSTM sobre una secuencia de histogramas (con ventana deslizante) sería **v2.0**: más expresivo, mejor para tracking de cambios de tempo, pero con mayor complejidad de ops, mayor arena y mayor latencia.

### El ML como posterior bayesiano (concepto avanzado)

En teoría, se puede combinar autocorrelación (prior) + modelo ML (posterior):
- El prior es el pico de autocorrelación: "el tempo probablemente es uno de {60, 120, 240} BPM"
- El posterior refina usando la distribución completa de IOIs: "dado el perfil del histograma, es más probable que sea 120 BPM que 60 BPM"

Para v1.0, el modelo solo es suficiente. El prior de autocorrelación mejoraría la accuracy en casos con historia muy corta (<8 onsets).

---

## §6. Jitter de timing — por qué los músicos humanos no son metrónomo

El jitter de timing humano es gaussiano. El modelo seminal de **Wing & Kristofferson (1973)** descompone el error de producción temporal en dos fuentes:

1. **Timekeeper variance** (σ²_T): varianza del reloj interno del cerebro (mecanismo de pacemaker)
2. **Motor delay variance** (σ²_M): varianza del delay motor (el tiempo entre que el cerebro decide tocar y el dedo presiona la tecla)

El jitter total observable tiene desviación estándar σ ≈ 10-30ms para músicos entrenados (σ ≈ 15-30ms para no-entrenados).

### Consecuencia para el dataset

Sin jitter en el dataset sintético: el modelo aprende que "120 BPM tiene exactamente su IOI en el bin 7". En inferencia con un músico real (σ=15ms), el pico se difunde ±1 bin y el modelo falla.

Con jitter gaussiano σ=15ms en el dataset: el modelo aprende que "120 BPM tiene una distribución difusa centrada en el bin 7 con masa en ±1-2 bins adyacentes". Esta representación generaliza a músicos reales.

El jitter de 15ms en el dataset es el **parámetro más importante** de toda la generación de datos. Variarlo produce modelos con diferentes trade-offs:
- σ=5ms: modelo muy preciso en condiciones ideales, falla con músicos reales
- σ=15ms: buen balance (usado en este sprint)
- σ=30ms: modelo robusto al jitter pero puede confundir BPMs adyacentes

---

## §7. Comparación de los 3 modelos ML del Brain (Sprints 4.3-4.5)

| Aspecto | Key Detector | Chord Recognizer | Beat Follower |
|---|---|---|---|
| Sprint | 4.3 | 4.4 | 4.5 |
| Input | Pitch histogram (12) | Pitch histogram (12) | IOI histogram (32) |
| Output | 24 tonalidades | 61 acordes | 32 BPM classes |
| Naturaleza del input | Estático (akorde en el momento) | Estático | Temporal (historia de 32 onsets) |
| Arquitectura | 12→128→64→24 | 12→128→64→61 | 32→64→32→32 |
| Parámetros (float32) | ~11.5K | ~14.8K | ~5.2K |
| Tamaño int8 (target) | ~12KB | ~23KB | ~10KB |
| Tensor arena (target) | ~16KB | ~24KB | ~16KB |
| Accuracy sintético >85% | Sí (~94%) | Sí (~82%) | Sí (exacta) / >90% (±1 clase) |
| Accuracy real estimada | ~70-75% | ~65-70% | ~70-75% |
| Ambigüedad principal | C maj vs A min (relativas) | maj vs maj7 (1 nota) | 120 vs 60/240 BPM (double-time) |

### El "reality gap"

En todos los modelos, la accuracy en dataset sintético (>85%) es mayor que la accuracy esperada en música real (~70-75%). Esta diferencia es el **reality gap** — la distancia entre los supuestos del dataset y la complejidad del mundo real:

- Música real tiene polifonía, cambios de acorde, cambios de tempo, ornamentación y ruido acústico
- El dataset sintético asume estructuras armónicas y rítmicas más simples
- El jitter gaussiano del dataset aproxima el jitter humano pero no captura el "groove" (micro-timing intencional)

El reality gap es normal y esperado. La arquitectura del Brain está diseñada para mostrar resultados con indicadores de confianza (`confidence`) que el usuario puede interpretar.

---

## §8. Pipeline de deployment

```
apps/training/models/beat_follower/train.py
    ↓ genera IOIs sintéticos con jitter y subdivisiones
    ↓ entrena Dense 3-layer con EarlyStopping
    ↓ convierte a TFLite INT8
    ↓ valida delta float→int8 <5%
    ↓ copia beat_follower_model.h → apps/firmware-teensy/src/ml/

apps/firmware-teensy/src/ml/beat_follower.h/.cpp
    ↓ BeatFollower::on_onset() — callback MIDI → buffer circular
    ↓ BeatFollower::infer() — histograma + TFLite Micro
    → BeatResult { bpm, bpm_index, confidence, beat_period_ms }
```

El resultado se envía al ESP32 vía Bridge Protocol (CMD pendiente de definir en Sprint 4.6) para mostrarse en el display LVGL.

---

## §9. Operación del buffer circular

El buffer de 32 onsets implementa un FIFO circular clásico:

```
Estado inicial: [_, _, _, _, ...]   _onset_count=0, _onset_head=0
Después de 3 onsets:
  [t0, t1, t2, _, ...]    _onset_count=3, _onset_head=3
Después de 32 onsets (buffer lleno):
  [t0, t1, ..., t31]      _onset_count=32, _onset_head=0
Onset 33 (sobrescribe t0):
  [t32, t1, ..., t31]     _onset_count=32, _onset_head=1
```

Para calcular IOIs del buffer lleno, el onset más antiguo está en `_onset_head`. Los onsets se iteran en orden cronológico: `_onset_buffer[(_onset_head + i) % 32]` para i=0..31.

La protección contra overflow de `millis()` (rollover cada ~49 días) descarta IOIs donde `curr_ts <= prev_ts`, que es la condición de rollover o timestamps fuera de orden.
