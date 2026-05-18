"""
test_chord_recognizer.py — Tests unitarios para el Chord Recognizer (Sprint 4.4).

Qué validan estos tests:
  - Estructura correcta del dataset (61 clases, shapes, dtypes)
  - Propiedades musicales de los histogramas generados (picos en bins correctos)
  - Balance del dataset (todas las clases representadas equitativamente)
  - Invarianza de transposición (C_maj transpuesto 2 st = D_maj)
  - Propiedad de la clase N (histograma aproximadamente uniforme)

Qué NO cubren (requieren TensorFlow o hardware):
  - Accuracy del modelo entrenado (train.py lo valida al final)
  - Inferencia TFLite en Teensy (tests on-device)
  - Tiempo de inferencia en hardware real

Correr con:
    cd apps/training
    uv run pytest tests/test_chord_recognizer.py -v

Cross-ref: apps/docs/sprints/24-chord-recognizer.md
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

_TRAINING_ROOT = Path(__file__).parent.parent
if str(_TRAINING_ROOT) not in sys.path:
    sys.path.insert(0, str(_TRAINING_ROOT))

from models.chord_recognizer.dataset import (
    CHORD_CLASSES,
    CHORD_INTERVALS,
    CHORD_TYPES,
    NOTE_NAMES,
    NUM_CLASSES,
    NUM_ROOTS,
    chord_to_histogram,
    generate_synthetic_dataset,
    label_to_chord_info,
)


# ---------------------------------------------------------------------------
# Tests: constantes y estructura de clases
# ---------------------------------------------------------------------------


def test_chord_classes_count() -> None:
    """
    CHORD_CLASSES debe tener exactamente 61 elementos.

    61 = 5 tipos × 12 roots + 1 clase N
       = maj(12) + min(12) + 7(12) + m7(12) + maj7(12) + N(1)

    Si este count falla, el modelo tiene el número de clases incorrecto
    y todos los labels del dataset son incorrectos.
    """
    assert len(CHORD_CLASSES) == 61, (
        f"CHORD_CLASSES debe tener 61 elementos, tiene {len(CHORD_CLASSES)}. "
        "Verificar que CHORD_TYPES tiene 5 elementos y NOTE_NAMES tiene 12."
    )
    assert NUM_CLASSES == 61, (
        f"NUM_CLASSES debe ser 61, es {NUM_CLASSES}."
    )


def test_chord_classes_last_is_n() -> None:
    """El último elemento de CHORD_CLASSES debe ser 'N' (label 60)."""
    assert CHORD_CLASSES[-1] == "N", (
        f"El último elemento (label 60) debe ser 'N', es '{CHORD_CLASSES[-1]}'"
    )


def test_chord_classes_first_is_c_maj() -> None:
    """El primer elemento de CHORD_CLASSES debe ser 'C_maj' (label 0)."""
    assert CHORD_CLASSES[0] == "C_maj", (
        f"El primer elemento (label 0) debe ser 'C_maj', es '{CHORD_CLASSES[0]}'"
    )


def test_chord_classes_label_12_is_c_min() -> None:
    """
    Label 12 debe ser 'C_min' — primer acorde menor.

    Esquema: 0-11 = maj, 12-23 = min, 24-35 = 7, etc.
    """
    assert CHORD_CLASSES[12] == "C_min", (
        f"Label 12 debe ser 'C_min', es '{CHORD_CLASSES[12]}'"
    )


def test_chord_classes_label_60_is_n() -> None:
    """Label 60 debe ser exactamente 'N'."""
    assert CHORD_CLASSES[60] == "N", (
        f"Label 60 debe ser 'N', es '{CHORD_CLASSES[60]}'"
    )


def test_chord_intervals_have_five_types() -> None:
    """CHORD_INTERVALS debe tener exactamente 5 tipos de acorde."""
    assert len(CHORD_INTERVALS) == 5, (
        f"CHORD_INTERVALS debe tener 5 tipos, tiene {len(CHORD_INTERVALS)}"
    )
    for expected in ["maj", "min", "7", "m7", "maj7"]:
        assert expected in CHORD_INTERVALS, (
            f"Falta el tipo de acorde '{expected}' en CHORD_INTERVALS"
        )


# ---------------------------------------------------------------------------
# Tests: chord_to_histogram() — propiedades musicales
# ---------------------------------------------------------------------------


def test_c_major_histogram_peaks() -> None:
    """
    C_maj (root=0, tipo='maj') debe tener activaciones principales en bins 0, 4, 7.

    Intervalos de C mayor: [0, 4, 7] = C (0), E (4), G (7).
    Los tres bins del acorde deben sumar más que el resto del histograma
    (el ruido y notas de paso son pequeños).

    Por qué no verificar exact=1.0/3: el histograma incluye ruido gaussiano
    y potenciales notas de paso (noise_level=0.05). Verificamos que los bins
    del acorde son los más prominentes, no que tienen exactamente 1/3.
    """
    rng = np.random.default_rng(seed=0)
    # Múltiples samples para promediar y reducir efecto del ruido
    hists = [
        chord_to_histogram(
            root=0, chord_type="maj",
            noise_level=0.0,    # sin ruido para test determinista
            add_duplications=False,
            add_passing_tones=False,
            rng=rng,
        )
        for _ in range(10)
    ]
    hist_mean = np.mean(hists, axis=0)

    chord_bins = [0, 4, 7]  # C, E, G
    non_chord_bins = [i for i in range(12) if i not in chord_bins]

    min_chord = hist_mean[chord_bins].min()
    max_non_chord = hist_mean[non_chord_bins].max()

    assert min_chord > max_non_chord, (
        f"Los bins del acorde C_maj (min={min_chord:.3f}) deben dominar "
        f"sobre los no-acorde (max={max_non_chord:.3f}). "
        "Verificar CHORD_INTERVALS['maj'] = [0, 4, 7]."
    )


def test_c_minor_histogram_peaks() -> None:
    """
    C_min (root=0, tipo='min') debe tener activaciones en bins 0, 3, 7.

    C menor: C (0), Eb (3), G (7). La diferencia con C mayor es el bin 4→3
    (3ra mayor → 3ra menor). Sin ruido, estos son los únicos bins activos.
    """
    rng = np.random.default_rng(seed=1)
    hist = chord_to_histogram(
        root=0, chord_type="min",
        noise_level=0.0,
        add_duplications=False,
        add_passing_tones=False,
        rng=rng,
    )

    chord_bins = [0, 3, 7]  # C, Eb, G
    non_chord_bins = [i for i in range(12) if i not in chord_bins]

    assert hist[chord_bins].min() > hist[non_chord_bins].max(), (
        f"Los bins de C_min (0,3,7) deben ser mayores que los no-acorde.\n"
        f"hist = {hist}"
    )


def test_c_dominant7_histogram_peaks() -> None:
    """
    C_7 (root=0, tipo='7') debe tener activaciones en bins 0, 4, 7, 10.

    Séptima dominante: C (0), E (4), G (7), Bb (10).
    La 7ma menor (10) es lo que distingue C7 de C_maj.
    """
    rng = np.random.default_rng(seed=2)
    hist = chord_to_histogram(
        root=0, chord_type="7",
        noise_level=0.0,
        add_duplications=False,
        add_passing_tones=False,
        rng=rng,
    )

    chord_bins = [0, 4, 7, 10]  # C, E, G, Bb
    non_chord_bins = [i for i in range(12) if i not in chord_bins]

    assert hist[chord_bins].min() > hist[non_chord_bins].max(), (
        f"Los bins de C_7 (0,4,7,10) deben dominar.\nhist = {hist}"
    )


def test_c_maj7_histogram_peaks() -> None:
    """
    C_maj7 (root=0, tipo='maj7') debe tener activaciones en bins 0, 4, 7, 11.

    Mayor séptima: C (0), E (4), G (7), B (11).
    El bin 11 (B) vs bin 10 (Bb) es la distinción entre maj7 y dom7.
    """
    rng = np.random.default_rng(seed=3)
    hist = chord_to_histogram(
        root=0, chord_type="maj7",
        noise_level=0.0,
        add_duplications=False,
        add_passing_tones=False,
        rng=rng,
    )

    chord_bins = [0, 4, 7, 11]  # C, E, G, B
    non_chord_bins = [i for i in range(12) if i not in chord_bins]

    assert hist[chord_bins].min() > hist[non_chord_bins].max(), (
        f"Los bins de C_maj7 (0,4,7,11) deben dominar.\nhist = {hist}"
    )


def test_histogram_sums_to_one() -> None:
    """
    chord_to_histogram debe retornar un histograma normalizado que suma 1.0.

    La normalización es un contrato explícito: el modelo espera distribuciones
    de probabilidad. Violarlo causaría mismatch entre training y deployment.
    """
    rng = np.random.default_rng(seed=4)
    for root in range(12):
        for ctype in CHORD_TYPES:
            hist = chord_to_histogram(root, ctype, rng=rng)
            total = hist.sum()
            np.testing.assert_almost_equal(
                total, 1.0, decimal=5,
                err_msg=f"chord_to_histogram({root}, {ctype}) suma {total:.6f}, no 1.0"
            )


def test_histogram_nonnegative() -> None:
    """
    Todos los valores del histograma deben ser >= 0.

    El ruido gaussiano puede crear valores negativos; el clampeo los elimina.
    Usamos noise_level=0.2 para estresar el clampeo.
    """
    rng = np.random.default_rng(seed=5)
    for _ in range(50):
        root = rng.integers(0, 12)
        ctype = CHORD_TYPES[rng.integers(0, len(CHORD_TYPES))]
        hist = chord_to_histogram(
            int(root), ctype, noise_level=0.2,
            add_duplications=True, add_passing_tones=True,
            rng=rng
        )
        assert np.all(hist >= 0), (
            f"Valores negativos en histograma de {NOTE_NAMES[root]}_{ctype}: "
            f"min={hist.min():.4f}"
        )


def test_no_chord_histogram_flat() -> None:
    """
    La clase N (no chord) debe generar histogramas aproximadamente uniformes.

    "Aproximadamente uniforme" significa que ningún pitch class domina claramente.
    Verificamos que la varianza del histograma N es menor que la de un C mayor
    (que tiene picos claros en 0, 4, 7).

    Importa porque la clase N debe ser distinguible de acordes reales —
    pero su histograma no debe ser idéntico a ningún acorde.
    """
    from models.chord_recognizer.dataset import _generate_no_chord_histogram

    rng = np.random.default_rng(seed=6)

    # Generar múltiples histogramas N y promediar
    n_hists = [_generate_no_chord_histogram(rng) for _ in range(100)]
    n_mean = np.mean(n_hists, axis=0)

    # Comparar varianza con la de C mayor (que tiene picos claros)
    c_maj_hists = [
        chord_to_histogram(0, "maj", noise_level=0.05, rng=rng)
        for _ in range(100)
    ]
    c_maj_mean = np.mean(c_maj_hists, axis=0)

    var_n = float(np.var(n_mean))
    var_c_maj = float(np.var(c_maj_mean))

    # El histograma N debe ser menos variable (más uniforme) que C mayor
    assert var_n < var_c_maj, (
        f"La clase N debe ser más uniforme que C_maj. "
        f"Var(N)={var_n:.6f}, Var(C_maj)={var_c_maj:.6f}. "
        "Verificar _generate_no_chord_histogram."
    )


# ---------------------------------------------------------------------------
# Tests: generate_synthetic_dataset()
# ---------------------------------------------------------------------------


def test_dataset_shape() -> None:
    """
    generate_synthetic_dataset(n_samples) debe retornar:
      X con shape (n_samples, 12)
      y con shape (n_samples,)

    Shape correcta es prerequisito para que el modelo no crashee en training.
    """
    rng = np.random.default_rng(seed=10)
    X, y = generate_synthetic_dataset(n_samples=122, rng=rng)

    assert X.shape == (122, 12), (
        f"X.shape esperado (122, 12), got {X.shape}"
    )
    assert y.shape == (122,), (
        f"y.shape esperado (122,), got {y.shape}"
    )


def test_dataset_dtypes() -> None:
    """X debe ser float32 (formato del modelo). y debe ser int32 (labels)."""
    rng = np.random.default_rng(seed=11)
    X, y = generate_synthetic_dataset(n_samples=61, rng=rng)

    assert X.dtype == np.float32, f"X.dtype esperado float32, got {X.dtype}"
    assert y.dtype == np.int32, f"y.dtype esperado int32, got {y.dtype}"


def test_dataset_label_range() -> None:
    """
    Los labels deben estar en [0, 60].

    0-59: acordes, 60: N (no chord).
    Valores fuera de este rango son bugs de indexación en generate_synthetic_dataset.
    """
    rng = np.random.default_rng(seed=12)
    X, y = generate_synthetic_dataset(n_samples=6100, rng=rng)

    assert int(y.min()) >= 0, f"Label mínimo: {y.min()}, debe ser >= 0"
    assert int(y.max()) <= 60, f"Label máximo: {y.max()}, debe ser <= 60"


def test_dataset_balanced() -> None:
    """
    Cada clase debe tener al menos n_samples // 61 * 0.8 ejemplos.

    El factor 0.8 da margen para la varianza del shuffle. El dataset debe estar
    "suficientemente balanceado" — desbalanceo severo causa que el modelo ignore
    clases minoritarias.

    Con n_samples=6100: esperamos ~100 por clase. El mínimo aceptable es 80.
    """
    rng = np.random.default_rng(seed=13)
    n = 6100
    X, y = generate_synthetic_dataset(n_samples=n, rng=rng)

    expected_per_class = n // NUM_CLASSES
    min_acceptable = int(expected_per_class * 0.8)

    for class_idx in range(NUM_CLASSES):
        count = int(np.sum(y == class_idx))
        assert count >= min_acceptable, (
            f"Clase {class_idx} ({CHORD_CLASSES[class_idx]}) tiene {count} muestras, "
            f"mínimo aceptable: {min_acceptable} "
            f"(n={n}, expected_per_class={expected_per_class})"
        )


def test_dataset_all_classes_present() -> None:
    """
    Con 6100 muestras (~100 por clase), deben aparecer las 61 clases.

    Si alguna clase falta, hay un bug en generate_synthetic_dataset.
    """
    rng = np.random.default_rng(seed=14)
    _, y = generate_synthetic_dataset(n_samples=6100, rng=rng)

    classes_present = set(y.tolist())
    missing = set(range(NUM_CLASSES)) - classes_present

    assert len(missing) == 0, (
        f"Clases ausentes en el dataset: {[CHORD_CLASSES[i] for i in missing]}. "
        "Verificar el loop de generación en generate_synthetic_dataset."
    )


def test_transposition_c_to_d() -> None:
    """
    El histograma de C_maj transpuesto 2 semitonos debe ser similar al de D_maj.

    Esta es la propiedad de transposición cíclica: np.roll(C_maj_hist, 2) IS D_maj_hist.
    Es exacta en el espacio de pitch classes (sin ruido).

    Sin ruido: C_maj tiene activaciones exactas en [0,4,7].
    np.roll([...], 2) mueve todo 2 posiciones → activaciones en [2,6,9] = D, F#, A = D_maj.

    Verificamos que la distancia L2 entre el histograma transpuesto y el D_maj
    generado directamente es pequeña (< 0.1 — el ruido es 0 en ambos).
    """
    rng_c = np.random.default_rng(seed=20)
    rng_d = np.random.default_rng(seed=20)  # misma semilla para mismas decisiones de ruido

    c_maj = chord_to_histogram(
        root=0, chord_type="maj",
        noise_level=0.0,
        add_duplications=False,
        add_passing_tones=False,
        rng=rng_c,
    )

    d_maj = chord_to_histogram(
        root=2, chord_type="maj",
        noise_level=0.0,
        add_duplications=False,
        add_passing_tones=False,
        rng=rng_d,
    )

    # Transponer C_maj 2 semitonos con roll
    c_transposed_to_d = np.roll(c_maj, 2)

    # Sin ruido, deben ser idénticos
    np.testing.assert_array_almost_equal(
        c_transposed_to_d, d_maj, decimal=5,
        err_msg=(
            "np.roll(C_maj_hist, 2) debe ser igual a D_maj_hist (sin ruido). "
            "Verificar que chord_to_histogram usa los intervalos de CHORD_INTERVALS correctamente."
        )
    )


def test_different_roots_different_histograms() -> None:
    """
    C_maj y D_maj deben tener histogramas distintos (con ruido=0).

    Propiedad obvia pero que puede fallar si hay un bug en el manejo del root
    (ej: siempre usar root=0 ignorando el argumento).
    """
    rng = np.random.default_rng(seed=21)
    c_maj = chord_to_histogram(
        root=0, chord_type="maj",
        noise_level=0.0, add_duplications=False, add_passing_tones=False,
        rng=rng
    )
    d_maj = chord_to_histogram(
        root=2, chord_type="maj",
        noise_level=0.0, add_duplications=False, add_passing_tones=False,
        rng=rng
    )

    diff = float(np.linalg.norm(c_maj - d_maj))
    assert diff > 0.1, (
        f"C_maj y D_maj son demasiado similares (||diff||={diff:.4f}). "
        "Verificar que el argumento 'root' se usa en chord_to_histogram."
    )


def test_major_vs_minor_differ() -> None:
    """
    C_maj y C_min deben ser distintos (sin ruido).

    La diferencia: C_maj tiene bin 4 (E), C_min tiene bin 3 (Eb).
    Si fueran iguales, el modelo no podría distinguir mayor de menor.
    """
    rng = np.random.default_rng(seed=22)
    c_maj = chord_to_histogram(
        root=0, chord_type="maj",
        noise_level=0.0, add_duplications=False, add_passing_tones=False,
        rng=rng
    )
    c_min = chord_to_histogram(
        root=0, chord_type="min",
        noise_level=0.0, add_duplications=False, add_passing_tones=False,
        rng=rng
    )

    diff = float(np.linalg.norm(c_maj - c_min))
    assert diff > 0.1, (
        f"C_maj y C_min son demasiado similares (||diff||={diff:.4f}). "
        "Verificar CHORD_INTERVALS: maj=[0,4,7], min=[0,3,7]."
    )


# ---------------------------------------------------------------------------
# Tests: label_to_chord_info()
# ---------------------------------------------------------------------------


def test_label_to_chord_info_c_maj() -> None:
    """label_to_chord_info(0) debe retornar C_maj."""
    info = label_to_chord_info(0)
    assert info["name"] == "C_maj"
    assert info["root"] == "C"
    assert info["chord_type"] == "maj"
    assert info["root_idx"] == 0
    assert info["type_idx"] == 0


def test_label_to_chord_info_n() -> None:
    """label_to_chord_info(60) debe retornar la clase N."""
    info = label_to_chord_info(60)
    assert info["name"] == "N"
    assert info["chord_type"] == "N"
    assert info["root_idx"] == -1
    assert info["type_idx"] == -1


def test_label_to_chord_info_invalid() -> None:
    """label_to_chord_info con valores fuera de [0,60] debe lanzar ValueError."""
    with pytest.raises(ValueError):
        label_to_chord_info(61)
    with pytest.raises(ValueError):
        label_to_chord_info(-1)


def test_chord_to_histogram_invalid_type() -> None:
    """chord_to_histogram con chord_type inválido debe lanzar ValueError."""
    rng = np.random.default_rng(seed=99)
    with pytest.raises(ValueError, match="no reconocido"):
        chord_to_histogram(root=0, chord_type="diminished", rng=rng)


def test_chord_to_histogram_invalid_root() -> None:
    """chord_to_histogram con root fuera de [0,11] debe lanzar ValueError."""
    rng = np.random.default_rng(seed=99)
    with pytest.raises(ValueError, match="debe estar en"):
        chord_to_histogram(root=12, chord_type="maj", rng=rng)
