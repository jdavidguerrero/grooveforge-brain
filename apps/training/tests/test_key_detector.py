"""
test_key_detector.py — Tests unitarios para el Key Detector (Sprint 4.3).

Qué validan estos tests:
  - Que los perfiles de Krumhansl-Schmuckler tienen la forma correcta
  - Que el dataset sintético genera datos con las propiedades esperadas
  - Que la transposición cíclica es musicalmente correcta
  - Que los histogramas generados son distribuciones de probabilidad válidas

Qué NO cubren (fuera del scope de unit tests sin hardware):
  - Accuracy del modelo entrenado (eso lo valida train.py al final)
  - Inferencia TFLite en el Teensy (eso requiere hardware on-device)
  - Tiempo de inferencia (requiere Teensy conectado)

Correr con:
    cd apps/training
    uv run pytest tests/test_key_detector.py -v
"""

from __future__ import annotations

import numpy as np
import pytest

# Importamos solo las funciones de dataset — sin TensorFlow (no necesario para unit tests)
import sys
from pathlib import Path

_TRAINING_ROOT = Path(__file__).parent.parent
if str(_TRAINING_ROOT) not in sys.path:
    sys.path.insert(0, str(_TRAINING_ROOT))

from models.key_detector.dataset import (
    CLASS_NAMES,
    KEY_NAMES,
    NUM_CLASSES,
    generate_key_profiles,
    generate_synthetic_dataset,
)


# ---------------------------------------------------------------------------
# Tests: generate_key_profiles()
# ---------------------------------------------------------------------------


def test_generate_key_profiles_shape() -> None:
    """
    generate_key_profiles() debe retornar exactamente 24 perfiles,
    cada uno con 12 valores (uno por pitch class).

    24 = 12 tonalidades mayores + 12 menores.
    12 = los 12 pitch classes del sistema cromático occidental.
    """
    profiles = generate_key_profiles()

    assert len(profiles) == NUM_CLASSES, (
        f"Se esperaban {NUM_CLASSES} perfiles, got {len(profiles)}"
    )

    for name, profile in profiles.items():
        assert profile.shape == (12,), (
            f"Perfil '{name}' tiene shape {profile.shape}, esperado (12,)"
        )
        assert profile.dtype in (np.float64, np.float32), (
            f"Perfil '{name}' tiene dtype {profile.dtype}"
        )


def test_generate_key_profiles_keys_exist() -> None:
    """
    Los perfiles deben incluir exactamente los 24 nombres esperados.
    Formato: 'C_maj', 'C#_maj', ..., 'B_maj', 'C_min', ..., 'B_min'.
    """
    profiles = generate_key_profiles()

    for key_name in KEY_NAMES:
        assert f"{key_name}_maj" in profiles, f"Falta perfil '{key_name}_maj'"
        assert f"{key_name}_min" in profiles, f"Falta perfil '{key_name}_min'"


def test_c_major_profile_max_on_c() -> None:
    """
    El perfil de C mayor debe tener su pico en el índice 0 (C).

    Esto es una propiedad fundamental de los perfiles de Krumhansl-Schmuckler:
    la tónica (la nota que da nombre a la tonalidad) siempre tiene el mayor rating.
    C_maj → pico en 0 (C)
    G_maj → pico en 7 (G)
    A_min → pico en 9 (A)
    """
    profiles = generate_key_profiles()

    c_major = profiles["C_maj"]
    assert np.argmax(c_major) == 0, (
        f"El pico de C_maj debe estar en índice 0 (C), está en {np.argmax(c_major)}"
    )


def test_d_major_profile_max_on_d() -> None:
    """
    El perfil de D mayor debe tener su pico en el índice 2 (D).

    D es el semitono 2. Si la rotación cíclica está bien implementada,
    np.roll(C_maj_profile, 2) produce el perfil de D mayor.
    """
    profiles = generate_key_profiles()

    d_major = profiles["D_maj"]
    assert np.argmax(d_major) == 2, (
        f"El pico de D_maj debe estar en índice 2 (D), está en {np.argmax(d_major)}"
    )


def test_c_minor_profile_max_on_c() -> None:
    """
    El perfil de C menor debe tener su pico en el índice 0 (C).

    En menor, la tónica también tiene el mayor rating, aunque el patrón
    relativo de los 12 pitch classes es diferente al de mayor.
    """
    profiles = generate_key_profiles()

    c_minor = profiles["C_min"]
    assert np.argmax(c_minor) == 0, (
        f"El pico de C_min debe estar en índice 0 (C), está en {np.argmax(c_minor)}"
    )


def test_profiles_normalized() -> None:
    """
    Todos los perfiles deben estar normalizados (sumar 1.0).

    La normalización es importante porque los histogramas de entrada
    también son distribuciones que suman 1.0. Usar perfiles no normalizados
    crearía un mismatch entre el dataset sintético y los datos reales.
    """
    profiles = generate_key_profiles()

    for name, profile in profiles.items():
        total = profile.sum()
        np.testing.assert_almost_equal(
            total, 1.0, decimal=10,
            err_msg=f"Perfil '{name}' no está normalizado: suma = {total}"
        )


def test_profiles_nonnegative() -> None:
    """Todos los valores de los perfiles deben ser >= 0 (son ratings, no pueden ser negativos)."""
    profiles = generate_key_profiles()

    for name, profile in profiles.items():
        assert np.all(profile >= 0), (
            f"Perfil '{name}' tiene valores negativos: {profile[profile < 0]}"
        )


def test_transposition_consistency() -> None:
    """
    Transponer el perfil de C mayor 2 semitonos debe producir el perfil de D mayor.

    Esta es la propiedad algebraica central de la transposición cíclica:
    rotar el array de pitch classes 2 posiciones IS transponer 2 semitonos.

    Verificamos que los perfiles generados por generate_key_profiles() (que usa
    np.roll internamente) satisfacen esta identidad.
    """
    profiles = generate_key_profiles()

    c_major = profiles["C_maj"]
    d_major = profiles["D_maj"]

    # Transponer C_maj 2 semitonos a la derecha debe dar D_maj
    c_transposed_to_d = np.roll(c_major, 2)

    np.testing.assert_array_almost_equal(
        c_transposed_to_d,
        d_major,
        decimal=10,
        err_msg="np.roll(C_maj, 2) debe ser igual a D_maj (consistencia de transposición)",
    )


def test_major_minor_differ() -> None:
    """
    El perfil de C mayor debe ser diferente al de C menor.

    Esto verifica que los dos perfiles base (major y minor) no se confunden
    en la rotación — si fueran iguales, el modelo no podría distinguir modos.
    """
    profiles = generate_key_profiles()

    c_major = profiles["C_maj"]
    c_minor = profiles["C_min"]

    # Deben ser arrays diferentes (la distancia L2 debe ser > 0)
    diff = np.linalg.norm(c_major - c_minor)
    assert diff > 0.01, (
        f"C_maj y C_min son demasiado similares (||diff|| = {diff:.4f}). "
        "Verificar que los perfiles base son distintos."
    )


# ---------------------------------------------------------------------------
# Tests: generate_synthetic_dataset()
# ---------------------------------------------------------------------------


def test_generate_synthetic_dataset_shape() -> None:
    """
    generate_synthetic_dataset(n=100) debe retornar:
      X con shape (100, 12)
      y con shape (100,)

    Shape correcta es lo mínimo para que el training no crashee.
    """
    rng = np.random.default_rng(seed=0)
    X, y = generate_synthetic_dataset(n_samples=100, rng=rng)

    assert X.shape == (100, 12), f"X.shape esperado (100, 12), got {X.shape}"
    assert y.shape == (100,), f"y.shape esperado (100,), got {y.shape}"


def test_generate_synthetic_dataset_dtypes() -> None:
    """X debe ser float32 (formato de entrada del modelo). y debe ser int32 (labels)."""
    rng = np.random.default_rng(seed=1)
    X, y = generate_synthetic_dataset(n_samples=50, rng=rng)

    assert X.dtype == np.float32, f"X.dtype esperado float32, got {X.dtype}"
    assert y.dtype == np.int32, f"y.dtype esperado int32, got {y.dtype}"


def test_generate_synthetic_dataset_label_range() -> None:
    """
    Las etiquetas deben estar en [0, 23].

    0-11: tonalidades mayores (C_maj=0, ..., B_maj=11)
    12-23: tonalidades menores (C_min=12, ..., B_min=23)
    Valores fuera de este rango son bugs de indexación.
    """
    rng = np.random.default_rng(seed=2)
    X, y = generate_synthetic_dataset(n_samples=1000, rng=rng)

    assert y.min() >= 0, f"Label mínimo: {y.min()}, debe ser >= 0"
    assert y.max() <= 23, f"Label máximo: {y.max()}, debe ser <= 23"


def test_histogram_sums_to_one() -> None:
    """
    Cada histograma sintético debe sumar aproximadamente 1.0.

    El modelo espera histogramas normalizados. Si los sintéticos no suman 1.0,
    el modelo aprende una distribución que no matchea los datos reales
    (que sí están normalizados por get_pitch_class_histogram).
    """
    rng = np.random.default_rng(seed=3)
    X, _ = generate_synthetic_dataset(n_samples=500, rng=rng)

    row_sums = X.sum(axis=1)
    np.testing.assert_array_almost_equal(
        row_sums,
        np.ones(500),
        decimal=5,
        err_msg="Cada histograma sintético debe sumar 1.0 (distribución de probabilidad)",
    )


def test_histogram_nonnegative() -> None:
    """
    Ningún valor del histograma puede ser negativo.

    El ruido gaussiano puede crear valores negativos, pero los clampamos a 0
    antes de normalizar (una duración negativa de nota no tiene sentido físico).
    Usamos noise_level=0.15 para estresar el clampeo sin perder separabilidad.
    """
    rng = np.random.default_rng(seed=4)
    X, _ = generate_synthetic_dataset(n_samples=500, noise_level=0.15, rng=rng)

    assert np.all(X >= 0), (
        f"Hay valores negativos en los histogramas: min = {X.min():.4f}"
    )


def test_all_classes_represented() -> None:
    """
    Con 2400 muestras (100 por clase esperada), deben aparecer las 24 clases.

    Si alguna clase no aparece, hay un bug en el generador de labels o
    el rango [0, 24) está mal definido en rng.integers.
    """
    rng = np.random.default_rng(seed=5)
    _, y = generate_synthetic_dataset(n_samples=2400, rng=rng)

    classes_present = set(y.tolist())
    missing = set(range(NUM_CLASSES)) - classes_present

    assert len(missing) == 0, (
        f"Clases ausentes en el dataset: {missing}. "
        "El generador debe producir ejemplos de todas las 24 tonalidades."
    )


def test_dataset_reproducible_with_seed() -> None:
    """
    Con la misma semilla, generate_synthetic_dataset debe retornar exactamente los mismos datos.

    Reproducibilidad es crítica para poder comparar experimentos y detectar regresiones.
    """
    rng1 = np.random.default_rng(seed=99)
    rng2 = np.random.default_rng(seed=99)

    X1, y1 = generate_synthetic_dataset(n_samples=100, rng=rng1)
    X2, y2 = generate_synthetic_dataset(n_samples=100, rng=rng2)

    np.testing.assert_array_equal(X1, X2, err_msg="X no es reproducible con la misma semilla")
    np.testing.assert_array_equal(y1, y2, err_msg="y no es reproducible con la misma semilla")


def test_noise_level_increases_variance() -> None:
    """
    Mayor noise_level debe producir histogramas con mayor varianza respecto al perfil teórico.

    Este es un test de sanidad del mecanismo de ruido: si noise_level=0.5 produce
    la misma varianza que noise_level=0.01, el ruido no está funcionando.
    """
    rng_low = np.random.default_rng(seed=10)
    rng_high = np.random.default_rng(seed=10)

    X_low, _ = generate_synthetic_dataset(n_samples=500, noise_level=0.001, rng=rng_low)
    X_high, _ = generate_synthetic_dataset(n_samples=500, noise_level=0.1, rng=rng_high)

    var_low = float(np.var(X_low))
    var_high = float(np.var(X_high))

    assert var_high > var_low, (
        f"noise_level=0.1 debe producir mayor varianza que noise_level=0.001. "
        f"Var low: {var_low:.6f}, Var high: {var_high:.6f}"
    )
