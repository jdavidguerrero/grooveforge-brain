"""
dataset.py — Generación de dataset sintético para beat following (BPM detection).

Estrategia de datos:
  Dataset sintético exclusivo basado en Inter-Onset Intervals (IOIs).
  Un IOI es el tiempo en milisegundos entre dos onsets (NoteOn) consecutivos.
  Dado que en el Teensy los onsets vienen de MIDI con precisión de microsegundos,
  no necesitamos onset detection: los onsets son datos duros.

  El dataset simula la realidad musical con dos fuentes de varianza:
    1. Jitter de timing gaussiano (σ=15ms): músicos reales no son metrónomo.
    2. Subdivisiones rítmicas: corcheas (IOI/2), tresillos (IOI/3), blancas (IOI*2).

  El modelo aprende a mapear una distribución de IOIs a un BPM discreto,
  resolviendo la ambigüedad double-time/half-time que la autocorrelación sola
  no puede resolver.

Referencia:
  Wing, A.M. & Kristofferson, A.B. (1973). Response delays and the timing of
  discrete motor responses. Perception & Psychophysics, 14(1), 5-12.
  (El paper fundacional sobre jitter gaussiano en producción temporal humana.)

Cross-ref: apps/docs/04-ai-architecture.md §1 (Beat Follower feature)
           apps/docs/sprints/25-beat-follower.md §2-3
"""

from __future__ import annotations

import numpy as np

# ---------------------------------------------------------------------------
# Constantes del espacio de representación
# ---------------------------------------------------------------------------

# 32 BPM targets discretos cubriendo el rango musical 60-240 BPM.
# Espaciados logarítmicamente porque la percepción del tempo es logarítmica:
# el "salto" de 120→132 BPM suena equivalente al salto de 60→66 BPM.
#
# Distribución de saltos:
#   60-96 BPM:   pasos de 3-4 BPM (territorio lento, más granularidad)
#   96-120 BPM:  pasos de 4-8 BPM (territorio medio, sweet spot musical)
#   120-240 BPM: pasos de 6-16 BPM (territorio rápido, percepción más comprimida)
BPM_CLASSES: list[int] = [
    60, 63, 66, 69, 72, 76, 80, 84, 88, 92, 96, 100, 104, 108, 112, 116,
    120, 126, 132, 138, 144, 152, 160, 168, 176, 184, 192, 200, 208, 216, 224, 240,
]

NUM_BPM_CLASSES = len(BPM_CLASSES)  # 32

# Rango de IOIs cubierto por el histograma.
# IOI_MIN_MS = 50ms → 1200 BPM máximo posible (límite físico de teclado)
# IOI_MAX_MS = 2000ms → 30 BPM mínimo (más lento que la música práctica)
#
# El rango 50-2000ms cubre con holgura los 60-240 BPM del rango de clases:
#   60 BPM → IOI = 1000ms (beat principal en el centro del rango)
#   240 BPM → IOI = 250ms (con margen hacia IOI_MIN)
IOI_MIN_MS: float = 50.0
IOI_MAX_MS: float = 2000.0

# Resolución del histograma: 32 bins sobre 50-2000ms
# Bin width = (2000 - 50) / 32 ≈ 60.9ms por bin
# Para 120 BPM (500ms), la resolución es ±30ms ≈ ±7 BPM — suficiente para display
N_IOI_BINS: int = 32
_BIN_WIDTH: float = (IOI_MAX_MS - IOI_MIN_MS) / N_IOI_BINS  # ≈ 60.9ms


# ---------------------------------------------------------------------------
# Funciones de conversión IOI ↔ BPM ↔ bin
# ---------------------------------------------------------------------------


def bpm_to_ioi_ms(bpm: float) -> float:
    """
    Convierte BPM a Inter-Onset Interval en milisegundos.

    Relación: un beat a 120 BPM ocurre 120 veces por minuto = cada 500ms.
    BPM = 60000ms / IOI_ms  →  IOI_ms = 60000ms / BPM

    Args:
        bpm: Tempo en beats por minuto. Debe ser > 0.

    Returns:
        IOI en milisegundos.

    Ejemplos:
        60 BPM → 1000ms (una nota por segundo)
        120 BPM → 500ms (dos notas por segundo)
        240 BPM → 250ms (cuatro notas por segundo)
    """
    return 60000.0 / bpm


def ioi_ms_to_bin(ioi_ms: float) -> int:
    """
    Mapea un IOI en milisegundos al índice de bin del histograma (0-31).

    El mapeo es lineal sobre el rango [IOI_MIN_MS, IOI_MAX_MS].
    IOIs fuera de rango se clampean al bin más cercano (0 o N_IOI_BINS-1).

    Args:
        ioi_ms: IOI en milisegundos.

    Returns:
        Índice de bin entero en [0, N_IOI_BINS-1].

    Nota sobre la fórmula:
        bin = (ioi_ms - IOI_MIN_MS) / _BIN_WIDTH
        El clampeo maneja IOIs que lleguen de música extremadamente rápida
        (<50ms = >1200 BPM) o muy lenta (>2000ms = <30 BPM).
    """
    raw = (ioi_ms - IOI_MIN_MS) / _BIN_WIDTH
    clamped = int(max(0, min(N_IOI_BINS - 1, raw)))
    return clamped


def iois_to_histogram(iois_ms: list[float]) -> np.ndarray:
    """
    Convierte una lista de IOIs a un histograma normalizado de 32 bins.

    Cada IOI incrementa el bin correspondiente en 1. El histograma se
    normaliza para que la suma sea 1.0 — convirtiéndolo en una distribución
    de probabilidad. Esto hace al histograma invariante al número de onsets
    (8 beats vs 32 beats producen el mismo histograma si el BPM es igual).

    Args:
        iois_ms: Lista de IOIs en milisegundos. Puede estar vacía.

    Returns:
        np.ndarray de shape (N_IOI_BINS,) = (32,), dtype float32.
        Si la lista está vacía o todos los IOIs están fuera de rango,
        retorna histograma de ceros (no normalizado) — el modelo debe
        manejar este caso como "no hay datos suficientes".
    """
    histogram = np.zeros(N_IOI_BINS, dtype=np.float64)

    for ioi in iois_ms:
        bin_idx = ioi_ms_to_bin(ioi)
        histogram[bin_idx] += 1.0

    total = histogram.sum()
    if total > 1e-9:
        histogram /= total

    return histogram.astype(np.float32)


# ---------------------------------------------------------------------------
# Generación de IOIs sintéticos
# ---------------------------------------------------------------------------


def generate_synthetic_iois(
    bpm: float,
    n_onsets: int = 32,
    timing_jitter_ms: float = 15.0,
    subdivision_prob: float = 0.3,
    rest_prob: float = 0.1,
    rng: np.random.Generator | None = None,
) -> list[float]:
    """
    Genera una lista de IOIs sintéticos para un BPM dado.

    Simula la realidad musical con tres fuentes de varianza:

    1. Jitter de timing gaussiano (σ = timing_jitter_ms):
       Los músicos humanos no son metrónomo. Su producción temporal tiene
       jitter gaussiano con σ ≈ 10-30ms (Wing & Kristofferson, 1973).
       Sin este jitter, el modelo aprende a buscar el bin exacto del BPM,
       lo que falla con músicos reales cuyo histograma es más difuso.

    2. Subdivisiones rítmicas (probabilidad subdivision_prob por onset):
       En música real, no todos los onsets caen en el beat principal.
       Con probabilidad subdivision_prob, un onset cae en:
         - Corchea: IOI/2 (50% de la duración del beat)
         - Tresillo: IOI/3 (33%)
       El histograma resultante tiene energía en el beat Y sus subdivisiones.
       Esto es musicalmente correcto: en 120 BPM, las corcheas son 250ms.

    3. Silencios ocasionales (probabilidad rest_prob por beat):
       A veces hay un silencio — no se toca una nota en ese beat.
       Se representa como un IOI doble (IOI*2): el siguiente onset llega
       dos beats después. El histograma tiene energía en el múltiplo x2.

    Args:
        bpm:              Tempo en BPM. Debe estar en el rango musical útil (30-300).
        n_onsets:         Número de onsets a generar. Retorna n_onsets-1 IOIs.
        timing_jitter_ms: Desviación estándar del jitter gaussiano (ms).
        subdivision_prob: Probabilidad de que un onset caiga en subdivisión.
        rest_prob:        Probabilidad de que un beat sea un silencio (IOI*2).
        rng:              Random generator. Si None, usa np.random.default_rng().

    Returns:
        Lista de n_onsets-1 IOIs en milisegundos. Todos son > 0.

    Nota sobre n_onsets vs n_iois:
        N onsets generan N-1 IOIs porque el IOI[i] = onset[i+1] - onset[i].
        Con n_onsets=32 obtenemos exactamente 31 IOIs.
    """
    if rng is None:
        rng = np.random.default_rng()

    beat_period_ms = bpm_to_ioi_ms(bpm)
    iois: list[float] = []

    for _ in range(n_onsets - 1):
        # Decidir el tipo de este intervalo
        p = rng.random()

        if p < rest_prob:
            # Silencio: el siguiente onset llega en 2 beats
            base_ioi = beat_period_ms * 2.0
        elif p < rest_prob + subdivision_prob * 0.5:
            # Corchea: IOI = beat/2
            base_ioi = beat_period_ms / 2.0
        elif p < rest_prob + subdivision_prob:
            # Tresillo: IOI = beat/3
            base_ioi = beat_period_ms / 3.0
        else:
            # Beat principal
            base_ioi = beat_period_ms

        # Jitter gaussiano — simula imprecisión temporal humana
        jitter = rng.normal(0.0, timing_jitter_ms)
        ioi = base_ioi + jitter

        # Garantizar IOI positivo (no se puede ir hacia atrás en el tiempo)
        ioi = max(10.0, ioi)
        iois.append(ioi)

    return iois


# ---------------------------------------------------------------------------
# Generación del dataset completo
# ---------------------------------------------------------------------------


def generate_synthetic_dataset(
    n_samples: int = 8000,
    timing_jitter_ms: float = 15.0,
    subdivision_prob: float = 0.3,
    tempo_drift_pct: float = 0.02,
    rng: np.random.Generator | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    """
    Genera histogramas de IOIs con sus labels de clase BPM.

    Proceso por muestra:
      1. Seleccionar un BPM de BPM_CLASSES (distribución uniforme entre clases)
      2. Aplicar tempo drift: ±tempo_drift_pct del BPM base (simula acelerandos/rallentandos)
      3. Generar 32 onsets sintéticos → 31 IOIs con jitter y subdivisiones
      4. Convertir a histograma normalizado de 32 bins

    El tempo drift (±2%) simula que un músico puede tocar a "120 BPM" y en realidad
    estar entre 117.6 y 122.4 BPM. Esto entrena al modelo a ser robusto a pequeñas
    desviaciones del tempo objetivo.

    Balance de clases:
      Las 8000 muestras se distribuyen uniformemente: 8000/32 = 250 por clase.
      Si n_samples no es divisible por 32, el remanente se distribuye ciclicamente.

    Args:
        n_samples:        Total de muestras a generar.
        timing_jitter_ms: Jitter de timing gaussiano en ms (default 15ms).
        subdivision_prob: Probabilidad de subdivisión rítmica (default 0.3).
        tempo_drift_pct:  Drift máximo de tempo como fracción (default 0.02 = ±2%).
        rng:              Random generator. Si None, usa np.random.default_rng().

    Returns:
        X: np.ndarray de shape (n_samples, N_IOI_BINS) = (n_samples, 32), dtype float32.
           Cada fila es un histograma normalizado de IOIs.
        y: np.ndarray de shape (n_samples,), dtype int32.
           Labels de clase en [0, NUM_BPM_CLASSES-1] = [0, 31].
           label=0 → BPM_CLASSES[0]=60 BPM, label=31 → BPM_CLASSES[31]=240 BPM.
    """
    if rng is None:
        rng = np.random.default_rng()

    samples_per_class = n_samples // NUM_BPM_CLASSES
    remainder = n_samples % NUM_BPM_CLASSES

    X_list: list[np.ndarray] = []
    y_list: list[int] = []

    for class_idx, bpm_target in enumerate(BPM_CLASSES):
        count = samples_per_class + (1 if class_idx < remainder else 0)

        for _ in range(count):
            # Tempo drift: el BPM real puede desviarse ±2% del objetivo
            drift = rng.uniform(-tempo_drift_pct, tempo_drift_pct)
            bpm_actual = bpm_target * (1.0 + drift)

            # Generar IOIs sintéticos para este BPM con jitter y subdivisiones
            iois = generate_synthetic_iois(
                bpm=bpm_actual,
                n_onsets=32,
                timing_jitter_ms=timing_jitter_ms,
                subdivision_prob=subdivision_prob,
                rng=rng,
            )

            # Convertir a histograma
            hist = iois_to_histogram(iois)

            X_list.append(hist)
            y_list.append(class_idx)

    X = np.array(X_list, dtype=np.float32)
    y = np.array(y_list, dtype=np.int32)

    # Shuffle para mezclar clases en el entrenamiento
    idx = rng.permutation(len(X))
    return X[idx], y[idx]


# ---------------------------------------------------------------------------
# Helpers de decodificación (para firmware y reporting)
# ---------------------------------------------------------------------------


def label_to_bpm(label: int) -> int:
    """
    Decodifica un label de clase [0, 31] a su BPM objetivo.

    Args:
        label: Índice de clase. Debe estar en [0, NUM_BPM_CLASSES-1].

    Returns:
        BPM entero de BPM_CLASSES[label].

    Raises:
        ValueError: Si label está fuera de [0, NUM_BPM_CLASSES-1].
    """
    if not (0 <= label < NUM_BPM_CLASSES):
        raise ValueError(f"label debe estar en [0, {NUM_BPM_CLASSES - 1}], recibido: {label}")
    return BPM_CLASSES[label]


def bpm_to_nearest_label(bpm: float) -> int:
    """
    Encuentra el label de clase más cercano a un BPM dado.

    Usado en validación para determinar si una predicción es "correcta"
    incluso cuando el BPM real no está en BPM_CLASSES.

    Args:
        bpm: Tempo en BPM.

    Returns:
        Índice de clase [0, 31] del BPM_CLASSES más cercano a bpm.
    """
    bpm_array = np.array(BPM_CLASSES, dtype=float)
    return int(np.argmin(np.abs(bpm_array - bpm)))
