"""
dataset.py — Generación de dataset sintético para chord recognition.

Estrategia de datos:
  Dataset sintético exclusivo: cada acorde tiene una definición exacta en
  términos de intervalos (semitonos desde la root). A diferencia del key detector
  —que necesita perfiles estadísticos de Krumhansl-Schmuckler porque las tonalidades
  no tienen una representación binaria exacta— los acordes SÍ tienen una:
  C mayor = {C, E, G} exactamente.

  El problema del dataset real para acordes es el etiquetado: requiere anotación
  humana o un análisis armónico complejo de cada segmento de audio. El sintético
  evita este problema generando histogramas con las propiedades correctas más ruido
  musical realista (duplicaciones de octava, notas de paso, dinámica variable).

Por qué estas 5 familias de acordes y no otras:
  Las 5 familias elegidas (maj, min, 7, m7, maj7) cubren >90% de la música popular
  occidental. Ver apps/docs/sprints/24-chord-recognizer.md §1 para la teoría completa.

Esquema de labels (orden canónico):
  0-11  : C_maj ... B_maj    (tríadas mayores)
  12-23 : C_min ... B_min    (tríadas menores)
  24-35 : C_7   ... B_7      (séptima dominante)
  36-47 : C_m7  ... B_m7     (séptima menor)
  48-59 : C_maj7... B_maj7   (mayor séptima)
  60    : N                  (no chord — silencio / indeterminado)
  TOTAL : 61 clases

Cross-ref: apps/docs/04-ai-architecture.md §1 (Chord Recognition feature)
           apps/docs/sprints/24-chord-recognizer.md §2 (por qué el histograma funciona)
"""

from __future__ import annotations

import numpy as np

# ---------------------------------------------------------------------------
# Constantes de acordes
# ---------------------------------------------------------------------------

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

# Intervalos en semitonos desde la root para cada tipo de acorde.
#
# Teoría rápida:
#   Tríada mayor   [0,4,7]      → root + 3ra mayor (4st) + 5ta justa (7st)
#   Tríada menor   [0,3,7]      → root + 3ra menor (3st) + 5ta justa (7st)
#   Séptima dom.   [0,4,7,10]   → mayor + 7ma menor (10st) → "tensión que resuelve"
#   Séptima menor  [0,3,7,10]   → menor + 7ma menor (10st) → el acorde "suave" del jazz
#   Mayor séptima  [0,4,7,11]   → mayor + 7ma mayor (11st) → el acorde "sofisticado"
#
# La 7ma dominante (10st desde root) vs 7ma mayor (11st desde root) es la distinción
# más sutil: un semitono de diferencia. Esto predetermina que el modelo tendrá
# confusiones entre '7' y 'maj7' en presencia de ruido — ver sprint doc §4.
CHORD_INTERVALS: dict[str, list[int]] = {
    "maj":  [0, 4, 7],
    "min":  [0, 3, 7],
    "7":    [0, 4, 7, 10],
    "m7":   [0, 3, 7, 10],
    "maj7": [0, 4, 7, 11],
}

# Tipos de acorde en orden canónico (mismo orden que en el esquema de labels).
CHORD_TYPES = ["maj", "min", "7", "m7", "maj7"]
NUM_CHORD_TYPES = len(CHORD_TYPES)  # 5
NUM_ROOTS = 12
# 5 tipos × 12 roots + 1 clase "N" = 61
NUM_CLASSES = NUM_CHORD_TYPES * NUM_ROOTS + 1

# Lista canónica de 61 nombres de clases.
# label = chord_type_idx * 12 + root_idx  (para clases 0-59)
# label = 60                               (para "N")
#
# Ejemplo:
#   label=0  → C_maj  (tipo 0, root 0)
#   label=11 → B_maj  (tipo 0, root 11)
#   label=12 → C_min  (tipo 1, root 0)
#   label=60 → N
CHORD_CLASSES: list[str] = []
for _ctype in CHORD_TYPES:
    for _note in NOTE_NAMES:
        CHORD_CLASSES.append(f"{_note}_{_ctype}")
CHORD_CLASSES.append("N")

assert len(CHORD_CLASSES) == NUM_CLASSES, (
    f"CHORD_CLASSES debe tener {NUM_CLASSES} elementos, tiene {len(CHORD_CLASSES)}"
)


# ---------------------------------------------------------------------------
# Funciones de generación de histogramas
# ---------------------------------------------------------------------------


def chord_to_histogram(
    root: int,
    chord_type: str,
    noise_level: float = 0.05,
    add_duplications: bool = True,
    add_passing_tones: bool = True,
    rng: np.random.Generator | None = None,
) -> np.ndarray:
    """
    Genera un histograma de pitch class para un acorde dado.

    Proceso de generación:
      1. Activar los pitch classes del acorde con peso base 1.0
      2. Añadir ruido gaussiano a todos los bins (simula imprecisión tímbrica)
      3. Opcionalmente reforzar bins con "duplicaciones de octava"
         (notas del acorde en otra octava — refuerza el mismo pitch class)
      4. Opcionalmente añadir "notas de paso" a bins adyacentes
         (notas cromáticas de ornamento — peso bajo)
      5. Clampear a no-negativo y normalizar a suma 1.0

    Por qué ruido si los acordes son exactos:
      En música real, un C mayor puede tener: la nota C doblada en 3 octavas
      (pesos desiguales), E como nota más débil porque la guitarra no lo toca
      fuerte, un Bb pasajero porque el guitarrista hace un bend. El modelo debe
      aprender a reconocer "C E G dominan" no "solo C E G activos con peso exacto 1/3".

    Args:
        root:             Nota raíz [0-11]. C=0, C#=1, ..., B=11.
        chord_type:       Tipo de acorde. Debe ser una clave de CHORD_INTERVALS.
        noise_level:      Desviación estándar del ruido gaussiano. 0.05 es realista.
        add_duplications: Si True, refuerza aleatoriamente bins del acorde con
                          factor extra [0.3, 0.7] (simula notas dobladas).
        add_passing_tones: Si True, añade ruido leve [0.05, 0.2] a 1-2 bins
                           adyacentes a las notas del acorde (notas de paso).
        rng:              Random generator. Si None, usa np.random.default_rng().

    Returns:
        np.ndarray de shape (12,), dtype float32. Suma exactamente 1.0.
        Los índices son [C, C#, D, D#, E, F, F#, G, G#, A, A#, B].

    Raises:
        ValueError: Si chord_type no es una clave de CHORD_INTERVALS.
        ValueError: Si root no está en [0, 11].
    """
    if chord_type not in CHORD_INTERVALS:
        raise ValueError(
            f"chord_type '{chord_type}' no reconocido. "
            f"Opciones: {list(CHORD_INTERVALS.keys())}"
        )
    if not (0 <= root <= 11):
        raise ValueError(f"root debe estar en [0, 11], recibido: {root}")

    if rng is None:
        rng = np.random.default_rng()

    hist = np.zeros(12, dtype=np.float64)

    # Paso 1: activar pitch classes del acorde con peso base
    intervals = CHORD_INTERVALS[chord_type]
    chord_pcs = [(root + interval) % 12 for interval in intervals]

    for pc in chord_pcs:
        hist[pc] += 1.0

    # Paso 2: ruido gaussiano en todos los bins
    noise = rng.normal(0.0, noise_level, size=12)
    hist += noise

    # Paso 3: duplicaciones de octava — refuerzo adicional de los bins del acorde
    # Simula que el pianista toca la misma nota en múltiples octavas.
    if add_duplications:
        for pc in chord_pcs:
            if rng.random() < 0.5:  # 50% chance de duplicar cada nota del acorde
                extra_weight = rng.uniform(0.3, 0.7)
                hist[pc] += extra_weight

    # Paso 4: notas de paso — activación leve de 1-2 notas no-del-acorde
    # Simula notas ornamentales, bends, aproximaciones cromáticas.
    if add_passing_tones:
        num_passing = rng.integers(0, 3)  # 0, 1 o 2 notas de paso
        non_chord_pcs = [i for i in range(12) if i not in chord_pcs]
        if non_chord_pcs and num_passing > 0:
            chosen = rng.choice(
                non_chord_pcs,
                size=min(num_passing, len(non_chord_pcs)),
                replace=False,
            )
            for pc in chosen:
                hist[pc] += rng.uniform(0.05, 0.2)

    # Paso 5: clampear y normalizar
    hist = np.maximum(hist, 0.0)
    total = hist.sum()
    if total > 1e-9:
        hist /= total
    else:
        # Caso degenerado: activar notas del acorde con pesos iguales
        hist = np.zeros(12, dtype=np.float64)
        for pc in chord_pcs:
            hist[pc] = 1.0 / len(chord_pcs)

    return hist.astype(np.float32)


def _generate_no_chord_histogram(rng: np.random.Generator) -> np.ndarray:
    """
    Genera un histograma para la clase N (no chord / silencio / ruido).

    Estrategia: ruido uniforme en los 12 bins, con posibles spikes aleatorios
    que no corresponden a ningún acorde conocido.

    La clase N representa:
      - Silencio total (hist ~ zeros → se convierte en distribución uniforme)
      - Ruido armónico (muchas notas simultáneas sin patrón)
      - Transiciones entre acordes (el histograma mezcla dos acordes)

    Returns:
        np.ndarray de shape (12,), dtype float32. Suma 1.0.
    """
    # Ruido uniforme base
    hist = rng.uniform(0.0, 1.0, size=12)

    # Añadir ocasionalmente un spike aleatorio para simular una nota dominante
    # que no pertenece a un acorde reconocible
    if rng.random() < 0.4:
        spike_pc = rng.integers(0, 12)
        hist[spike_pc] += rng.uniform(0.5, 1.5)

    hist = np.maximum(hist, 0.0)
    total = hist.sum()
    if total > 1e-9:
        hist /= total
    else:
        hist = np.ones(12, dtype=np.float64) / 12.0

    return hist.astype(np.float32)


# ---------------------------------------------------------------------------
# API pública: generación del dataset completo
# ---------------------------------------------------------------------------


def generate_synthetic_dataset(
    n_samples: int = 15000,
    noise_level: float = 0.05,
    rng: np.random.Generator | None = None,
) -> tuple[np.ndarray, np.ndarray]:
    """
    Genera un dataset balanceado de 61 clases de acordes.

    Distribución de muestras:
      - Cada una de las 61 clases recibe n_samples // 61 muestras base.
      - El remanente (n_samples % 61) se distribuye ciclicamente en las primeras clases.
      - Esto garantiza que el dataset está perfectamente balanceado o casi-balanceado
        (diferencia máxima de 1 muestra entre clases).

    Data augmentation por transposición cíclica:
      Para cada muestra de acorde C_maj con root=0, además de generar el histograma
      base, con probabilidad 0.5 se transforma a otro root usando np.roll.
      Esto es musicalmente exacto: rotar el histograma de C_maj 2 semitonos
      produce un histograma válido de D_maj.

      Esto NO duplica el dataset — es una técnica de augmentation on-the-fly
      que genera variedad adicional dentro de cada clase.

    Args:
        n_samples:   Número total de muestras (train + val combinados).
                     Recomendado: 15000 para train, 3000 para val.
        noise_level: Nivel de ruido gaussiano. 0.05 es realista para música real.
        rng:         Random generator (para reproducibilidad). Si None, usa default.

    Returns:
        X: np.ndarray de shape (n_samples, 12), dtype float32.
        y: np.ndarray de shape (n_samples,), dtype int32. Valores en [0, 60].

    Ejemplo:
        rng = np.random.default_rng(42)
        X_train, y_train = generate_synthetic_dataset(15000, rng=rng)
        X_val, y_val = generate_synthetic_dataset(3000, rng=rng)
    """
    if rng is None:
        rng = np.random.default_rng()

    samples_per_class = n_samples // NUM_CLASSES
    remainder = n_samples % NUM_CLASSES

    X_list: list[np.ndarray] = []
    y_list: list[int] = []

    for class_idx in range(NUM_CLASSES):
        # Cuántas muestras de esta clase
        count = samples_per_class + (1 if class_idx < remainder else 0)

        for _ in range(count):
            if class_idx == 60:
                # Clase N: histograma de ruido
                hist = _generate_no_chord_histogram(rng)
            else:
                # Clases 0-59: acorde real
                chord_type_idx = class_idx // NUM_ROOTS
                root = class_idx % NUM_ROOTS
                chord_type = CHORD_TYPES[chord_type_idx]

                # Augmentation por transposición: con probabilidad 0.5,
                # usar un root alternativo y rotar el histograma.
                # Esto genera variedad en la distribución de activación de bins.
                if rng.random() < 0.5:
                    # Transponer a otro root equivalente
                    alt_offset = int(rng.integers(1, 12))
                    alt_root = (root + alt_offset) % 12
                    alt_class_idx = chord_type_idx * NUM_ROOTS + alt_root

                    hist = chord_to_histogram(
                        alt_root, chord_type,
                        noise_level=noise_level,
                        add_duplications=True,
                        add_passing_tones=True,
                        rng=rng,
                    )
                    # Rotar de vuelta al root original para que el label sea correcto.
                    # np.roll(hist, -alt_offset) traslada la distribución al root original.
                    # Esto es una identidad matemática exacta en pitch class space.
                    hist = np.roll(hist, -alt_offset).astype(np.float32)

                    # Re-normalizar después del roll (el roll no cambia la suma,
                    # pero garantizamos que los floats están bien)
                    total = hist.sum()
                    if total > 1e-9:
                        hist = (hist / total).astype(np.float32)
                else:
                    hist = chord_to_histogram(
                        root, chord_type,
                        noise_level=noise_level,
                        add_duplications=True,
                        add_passing_tones=True,
                        rng=rng,
                    )

            X_list.append(hist)
            y_list.append(class_idx)

    X = np.array(X_list, dtype=np.float32)
    y = np.array(y_list, dtype=np.int32)

    # Shuffle final para mezclar clases
    idx = rng.permutation(len(X))
    return X[idx], y[idx]


# ---------------------------------------------------------------------------
# Helpers de decodificación (para el firmware y para reporting)
# ---------------------------------------------------------------------------


def label_to_chord_info(label: int) -> dict[str, str | int]:
    """
    Decodifica un label entero [0, 60] en sus componentes.

    Returns:
        dict con claves:
          'name'       — nombre completo (ej: "C#_m7", "N")
          'root'       — nota raíz como string (ej: "C#") o "" si N
          'chord_type' — tipo de acorde (ej: "m7") o "N" si N
          'root_idx'   — índice de la root [0-11] o -1 si N
          'type_idx'   — índice del tipo [0-4] o -1 si N

    Raises:
        ValueError: Si label no está en [0, 60].
    """
    if not (0 <= label <= 60):
        raise ValueError(f"label debe estar en [0, 60], recibido: {label}")

    if label == 60:
        return {
            "name": "N",
            "root": "",
            "chord_type": "N",
            "root_idx": -1,
            "type_idx": -1,
        }

    type_idx = label // NUM_ROOTS
    root_idx = label % NUM_ROOTS
    return {
        "name": CHORD_CLASSES[label],
        "root": NOTE_NAMES[root_idx],
        "chord_type": CHORD_TYPES[type_idx],
        "root_idx": root_idx,
        "type_idx": type_idx,
    }
