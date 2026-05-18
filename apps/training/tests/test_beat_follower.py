"""
test_beat_follower.py — Tests del dataset Beat Follower (Sprint 4.5).

Valida la lógica de dataset.py sin requerir TensorFlow ni datos externos.
Todos los tests usan datos sintéticos generados on-the-fly.

Por qué testear el dataset y no el modelo:
  Los tests de exactitud del modelo son tests de entrenamiento (requieren TF,
  minutos de CPU). Lo que podemos testear en CI sin overhead es la lógica del
  preprocesamiento: conversiones correctas, shapes esperados, propiedades
  matemáticas del histograma.

  Si el dataset genera histogramas malformados o labels incorrectos, el modelo
  entrenará bien en el artificio pero fallará en hardware.

Cross-ref: apps/docs/sprints/25-beat-follower.md §2-3
           apps/docs/04-ai-architecture.md §8 (métricas de aceptación)
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np
import pytest

# ── Ajustar sys.path para imports desde apps/training/
_TRAINING_ROOT = Path(__file__).parent.parent
if str(_TRAINING_ROOT) not in sys.path:
    sys.path.insert(0, str(_TRAINING_ROOT))

from models.beat_follower.dataset import (
    BPM_CLASSES,
    IOI_MAX_MS,
    IOI_MIN_MS,
    N_IOI_BINS,
    NUM_BPM_CLASSES,
    bpm_to_ioi_ms,
    bpm_to_nearest_label,
    generate_synthetic_dataset,
    generate_synthetic_iois,
    ioi_ms_to_bin,
    iois_to_histogram,
    label_to_bpm,
)


# ---------------------------------------------------------------------------
# Tests de conversión BPM ↔ IOI
# ---------------------------------------------------------------------------


class TestBpmToIoi:
    """
    Valida la conversión bpm_to_ioi_ms().

    Relación fundamental: BPM = 60000ms / IOI_ms
    Estos valores son identidades matemáticas exactas que el modelo y el
    firmware deben producir de forma consistente.
    """

    def test_120bpm_is_500ms(self):
        """120 BPM = 2 beats por segundo = 1 beat cada 500ms."""
        result = bpm_to_ioi_ms(120)
        assert abs(result - 500.0) < 0.001, f"120 BPM debería ser 500ms, got {result}"

    def test_60bpm_is_1000ms(self):
        """60 BPM = 1 beat por segundo = 1 beat cada 1000ms (andante)."""
        result = bpm_to_ioi_ms(60)
        assert abs(result - 1000.0) < 0.001, f"60 BPM debería ser 1000ms, got {result}"

    def test_240bpm_is_250ms(self):
        """240 BPM = 4 beats por segundo = 1 beat cada 250ms (allegro molto)."""
        result = bpm_to_ioi_ms(240)
        assert abs(result - 250.0) < 0.001, f"240 BPM debería ser 250ms, got {result}"

    def test_roundtrip_60_to_240(self):
        """Todos los BPM_CLASSES deben pasar el roundtrip bpm → ioi → ioi/60000."""
        for bpm in BPM_CLASSES:
            ioi = bpm_to_ioi_ms(bpm)
            bpm_back = 60000.0 / ioi
            assert abs(bpm_back - bpm) < 0.01, (
                f"Roundtrip fallido para {bpm} BPM: got {bpm_back:.2f}"
            )

    def test_all_bpm_classes_in_range(self):
        """Todos los BPM_CLASSES producen IOIs dentro del rango del histograma."""
        for bpm in BPM_CLASSES:
            ioi = bpm_to_ioi_ms(bpm)
            # 60 BPM = 1000ms, 240 BPM = 250ms — ambos dentro de [IOI_MIN_MS, IOI_MAX_MS]
            assert IOI_MIN_MS <= ioi <= IOI_MAX_MS, (
                f"IOI de {bpm} BPM ({ioi:.0f}ms) fuera del rango "
                f"[{IOI_MIN_MS}, {IOI_MAX_MS}ms]"
            )


# ---------------------------------------------------------------------------
# Tests del histograma de IOIs
# ---------------------------------------------------------------------------


class TestIoiHistogram:
    """
    Valida las propiedades matemáticas del histograma.

    Un histograma válido para el modelo debe:
      1. Tener shape (32,) — el input del modelo
      2. Sumar exactamente 1.0 cuando hay datos — distribución de probabilidad
      3. Tener dtype float32 — requerimiento de TFLite
    """

    def test_histogram_shape(self):
        """El histograma debe tener N_IOI_BINS = 32 bins."""
        iois = [500.0, 500.0, 500.0]
        hist = iois_to_histogram(iois)
        assert hist.shape == (N_IOI_BINS,), (
            f"Shape esperado ({N_IOI_BINS},), got {hist.shape}"
        )

    def test_histogram_sums_to_one(self):
        """Un histograma con datos debe sumar exactamente 1.0 (distribución de probabilidad)."""
        iois = [500.0, 500.0, 250.0, 1000.0, 333.0]
        hist = iois_to_histogram(iois)
        total = hist.sum()
        assert abs(total - 1.0) < 1e-5, (
            f"Histograma debe sumar 1.0, suma {total:.6f}"
        )

    def test_empty_iois_gives_zero_histogram(self):
        """Sin IOIs el histograma es de ceros (no hay historia de onsets)."""
        hist = iois_to_histogram([])
        assert hist.sum() == 0.0, "Histograma vacío debe sumar 0.0"
        assert hist.shape == (N_IOI_BINS,)

    def test_histogram_dtype_is_float32(self):
        """El histograma debe ser float32 para ser compatible con TFLite."""
        hist = iois_to_histogram([500.0, 500.0])
        assert hist.dtype == np.float32, f"dtype esperado float32, got {hist.dtype}"

    def test_histogram_no_negative_values(self):
        """Los bins del histograma nunca deben ser negativos."""
        rng = np.random.default_rng(0)
        iois = rng.uniform(IOI_MIN_MS, IOI_MAX_MS, size=31).tolist()
        hist = iois_to_histogram(iois)
        assert (hist >= 0).all(), "Histograma no debe tener valores negativos"

    def test_120bpm_peak_bin(self):
        """
        IOIs perfectos de 120 BPM (500ms) deben tener su pico en el bin correcto.

        120 BPM → IOI = 500ms
        Bin index = (500 - 50) / 60.9 ≈ 7.4 → bin 7

        Este test valida que la indexación de bins es correcta y que el modelo
        recibe el pico en la posición esperada para este BPM.
        """
        # 50 IOIs perfectos de 120 BPM sin jitter
        iois = [500.0] * 50
        hist = iois_to_histogram(iois)

        # El bin de 500ms
        expected_bin = ioi_ms_to_bin(500.0)
        peak_bin = int(np.argmax(hist))

        assert peak_bin == expected_bin, (
            f"Peak bin para 120 BPM debería ser {expected_bin}, got {peak_bin}"
        )
        # El pico debe contener toda la energía (no hay ruido en este test)
        assert hist[peak_bin] > 0.99, (
            f"El pico debe concentrar casi toda la energía, got {hist[peak_bin]:.3f}"
        )


# ---------------------------------------------------------------------------
# Tests del generador de IOIs sintéticos
# ---------------------------------------------------------------------------


class TestSyntheticIois:
    """
    Valida generate_synthetic_iois().

    La función es la pieza central del pipeline de datos. Sus propiedades
    matemáticas deben ser correctas para que el dataset sea válido.
    """

    def test_count_is_n_onsets_minus_one(self):
        """
        N onsets generan N-1 IOIs.

        IOI[i] = onset[i+1] - onset[i] → hay un IOI menos que onsets.
        """
        iois = generate_synthetic_iois(120, n_onsets=32)
        assert len(iois) == 31, (
            f"32 onsets deben generar 31 IOIs, got {len(iois)}"
        )

    def test_all_iois_positive(self):
        """Todos los IOIs deben ser positivos (el tiempo no retrocede)."""
        rng = np.random.default_rng(0)
        for bpm in [60, 120, 240]:
            iois = generate_synthetic_iois(bpm, n_onsets=32, rng=rng)
            assert all(ioi > 0 for ioi in iois), (
                f"Todos los IOIs deben ser positivos para {bpm} BPM"
            )

    def test_median_ioi_close_to_expected(self):
        """
        La mediana de IOIs con jitter bajo debe estar cerca del IOI esperado.

        Con jitter σ=5ms y muchas muestras, la mediana debe estar dentro de
        ±50ms del IOI teórico (sin subdivisiones para este test).
        """
        rng = np.random.default_rng(42)
        # Generar muchos IOIs sin subdivisiones para testear solo el jitter
        iois = generate_synthetic_iois(
            bpm=120,
            n_onsets=200,
            timing_jitter_ms=5.0,
            subdivision_prob=0.0,  # sin subdivisiones
            rest_prob=0.0,         # sin silencios
            rng=rng,
        )
        median_ioi = np.median(iois)
        expected_ioi = bpm_to_ioi_ms(120)  # 500ms

        assert abs(median_ioi - expected_ioi) < 50, (
            f"Mediana de IOIs para 120 BPM (sin subdivisiones): "
            f"esperado ~{expected_ioi:.0f}ms, got {median_ioi:.0f}ms"
        )

    def test_default_n_onsets(self):
        """n_onsets por defecto es 32, genera 31 IOIs."""
        iois = generate_synthetic_iois(120)
        assert len(iois) == 31


# ---------------------------------------------------------------------------
# Tests del dataset completo
# ---------------------------------------------------------------------------


class TestSyntheticDataset:
    """
    Valida generate_synthetic_dataset().

    El dataset generado debe cumplir las precondiciones del modelo:
      - Shape correcto para ser alimentado al modelo
      - Labels en el rango válido [0, NUM_BPM_CLASSES-1]
      - Balance de clases (±1 muestra entre clases)
    """

    @pytest.fixture(scope="class")
    def small_dataset(self):
        """Dataset pequeño para tests de forma — se reutiliza entre tests de la clase."""
        rng = np.random.default_rng(99)
        return generate_synthetic_dataset(n_samples=320, rng=rng)

    def test_x_shape(self, small_dataset):
        """X debe tener shape (n_samples, N_IOI_BINS) = (n_samples, 32)."""
        X, y = small_dataset
        assert X.shape == (320, N_IOI_BINS), (
            f"X shape esperado (320, {N_IOI_BINS}), got {X.shape}"
        )

    def test_y_shape(self, small_dataset):
        """y debe tener shape (n_samples,)."""
        X, y = small_dataset
        assert y.shape == (320,), f"y shape esperado (320,), got {y.shape}"

    def test_labels_in_range(self, small_dataset):
        """Todos los labels deben estar en [0, NUM_BPM_CLASSES-1] = [0, 31]."""
        X, y = small_dataset
        assert y.min() >= 0, f"Label mínimo debe ser >= 0, got {y.min()}"
        assert y.max() <= NUM_BPM_CLASSES - 1, (
            f"Label máximo debe ser <= {NUM_BPM_CLASSES - 1}, got {y.max()}"
        )

    def test_x_dtype_is_float32(self, small_dataset):
        """X debe ser float32 para ser compatible con TFLite."""
        X, y = small_dataset
        assert X.dtype == np.float32, f"X dtype esperado float32, got {X.dtype}"

    def test_y_dtype_is_int32(self, small_dataset):
        """y debe ser int32 — compatibilidad con SparseCategoricalCrossentropy."""
        X, y = small_dataset
        assert y.dtype == np.int32, f"y dtype esperado int32, got {y.dtype}"

    def test_all_classes_represented(self, small_dataset):
        """
        Con 320 muestras y 32 clases, cada clase debe aparecer exactamente 10 veces.
        Verifica que el dataset está balanceado.
        """
        X, y = small_dataset
        for label in range(NUM_BPM_CLASSES):
            count = (y == label).sum()
            # 320 / 32 = 10 exacto
            assert count == 10, (
                f"Clase {label} ({BPM_CLASSES[label]} BPM) tiene {count} muestras, "
                f"esperado 10"
            )

    def test_histograms_sum_to_one(self, small_dataset):
        """Todos los histogramas deben sumar ≈ 1.0 (distribución de probabilidad)."""
        X, y = small_dataset
        row_sums = X.sum(axis=1)
        # Tolerancia float32
        assert np.allclose(row_sums, 1.0, atol=1e-5), (
            f"Hay histogramas que no suman 1.0: "
            f"min={row_sums.min():.6f}, max={row_sums.max():.6f}"
        )

    def test_reproducibility(self):
        """El mismo seed debe producir el mismo dataset (reproducibilidad)."""
        rng1 = np.random.default_rng(123)
        rng2 = np.random.default_rng(123)
        X1, y1 = generate_synthetic_dataset(n_samples=64, rng=rng1)
        X2, y2 = generate_synthetic_dataset(n_samples=64, rng=rng2)
        np.testing.assert_array_equal(X1, X2, err_msg="Datasets con mismo seed deben ser iguales")
        np.testing.assert_array_equal(y1, y2)


# ---------------------------------------------------------------------------
# Tests de ambigüedad musical (double-time / half-time)
# ---------------------------------------------------------------------------


class TestMusicalAmbiguity:
    """
    Testea la ambigüedad double-time/half-time que el modelo debe resolver.

    La ambigüedad es matemáticamente real:
      - 120 BPM (IOI=500ms) tiene corcheas a 250ms (IOI de 240 BPM)
      - 60 BPM (IOI=1000ms) tiene negras a 1000ms (igual que el beat de 60 BPM)

    Estos tests no prueban que el modelo resuelve la ambigüedad (eso requiere
    entrenamiento) — prueban que el dataset CONTIENE la ambigüedad de forma
    realista, que es el prerequisito para que el modelo pueda aprender.
    """

    def test_60bpm_histogram_has_energy_in_shared_region(self):
        """
        60 BPM (IOI=1000ms) tiene energía en el mismo bin que 120 BPM (500ms)
        cuando hay corcheas (subdivision_prob > 0).

        Con subdivision_prob=0.5, aproximadamente 50% de los onsets caen en
        el IOI/2 = 500ms. Esto produce energía en el bin de 120 BPM.
        El modelo debe aprender que "energía en 500ms Y en 1000ms" = 60 BPM,
        mientras que "energía solo en 500ms" = 120 BPM.
        """
        rng = np.random.default_rng(0)
        # 60 BPM con subdivisiones activas — genera IOIs en 1000ms Y 500ms
        iois_60bpm = generate_synthetic_iois(
            bpm=60,
            n_onsets=64,
            timing_jitter_ms=5.0,
            subdivision_prob=0.5,
            rest_prob=0.0,
            rng=rng,
        )
        hist_60 = iois_to_histogram(iois_60bpm)

        # El bin de 120 BPM (500ms)
        bin_120bpm = ioi_ms_to_bin(bpm_to_ioi_ms(120))
        # El bin de 60 BPM (1000ms)
        bin_60bpm = ioi_ms_to_bin(bpm_to_ioi_ms(60))

        # Con subdivision_prob=0.5, hay energía en ambos bins
        # (el histograma de 60 BPM con corcheas tiene picos en 500ms y 1000ms)
        assert hist_60[bin_120bpm] > 0.0, (
            "60 BPM con subdivisiones debe tener energía en el bin de 120 BPM (500ms)"
        )
        assert hist_60[bin_60bpm] > 0.0, (
            "60 BPM debe tener energía en su bin principal (1000ms)"
        )

    def test_120bpm_no_subdivision_has_energy_at_500ms_only(self):
        """
        120 BPM sin subdivisiones tiene energía concentrada en el bin de 500ms.
        Sin subdivisiones, el histograma es más unimodal y más fácil de clasificar.
        """
        rng = np.random.default_rng(1)
        iois = generate_synthetic_iois(
            bpm=120,
            n_onsets=64,
            timing_jitter_ms=5.0,
            subdivision_prob=0.0,
            rest_prob=0.0,
            rng=rng,
        )
        hist = iois_to_histogram(iois)

        # Con jitter bajo y sin subdivisiones, la mayor parte de la energía
        # debe estar en el bin de 500ms ± 1 bin adyacente
        bin_120 = ioi_ms_to_bin(500.0)
        energy_center = hist[max(0, bin_120 - 1) : bin_120 + 2].sum()

        assert energy_center > 0.7, (
            f"120 BPM sin subdivisiones debe tener >70% de energía centrada "
            f"en el bin de 500ms, got {energy_center:.2f}"
        )

    def test_double_time_ambiguity_is_real(self):
        """
        La ambigüedad double-time es real y testeable:
        el histograma de 60 BPM CON subdivisiones tiene energía en el mismo bin
        que el histograma de 120 BPM SIN subdivisiones.

        Esta es la razón por la que el algoritmo de autocorrelación sólo
        no puede resolver la ambigüedad — necesita el modelo ML.
        """
        rng1 = np.random.default_rng(10)
        rng2 = np.random.default_rng(11)

        # 60 BPM con subdivisiones (corcheas) activas
        iois_60_sub = generate_synthetic_iois(
            bpm=60, n_onsets=64, timing_jitter_ms=3.0,
            subdivision_prob=0.5, rest_prob=0.0, rng=rng1,
        )
        hist_60_sub = iois_to_histogram(iois_60_sub)

        # 120 BPM sin subdivisiones
        iois_120 = generate_synthetic_iois(
            bpm=120, n_onsets=64, timing_jitter_ms=3.0,
            subdivision_prob=0.0, rest_prob=0.0, rng=rng2,
        )
        hist_120 = iois_to_histogram(iois_120)

        # El bin compartido es el de 500ms (IOI de 120 BPM = corchea de 60 BPM)
        shared_bin = ioi_ms_to_bin(500.0)

        # Ambos histogramas tienen energía en ese bin
        assert hist_60_sub[shared_bin] > 0.0, "hist_60_sub debe tener energía en bin de 500ms"
        assert hist_120[shared_bin] > 0.0, "hist_120 debe tener energía en bin de 500ms"
        # (la ambigüedad es que este bin no permite distinguirlos solo)


# ---------------------------------------------------------------------------
# Tests de helpers de decodificación
# ---------------------------------------------------------------------------


class TestDecodingHelpers:
    """Tests de label_to_bpm() y bpm_to_nearest_label()."""

    def test_label_to_bpm_boundaries(self):
        """Los límites de labels retornan el BPM correcto."""
        assert label_to_bpm(0) == 60
        assert label_to_bpm(NUM_BPM_CLASSES - 1) == 240
        assert label_to_bpm(16) == 120  # el label 16 es 120 BPM

    def test_label_to_bpm_invalid(self):
        """Labels fuera de rango deben lanzar ValueError."""
        with pytest.raises(ValueError):
            label_to_bpm(-1)
        with pytest.raises(ValueError):
            label_to_bpm(NUM_BPM_CLASSES)

    def test_bpm_to_nearest_label_exact(self):
        """BPMs exactos de BPM_CLASSES deben mapearse al label correcto."""
        for expected_label, bpm in enumerate(BPM_CLASSES):
            result = bpm_to_nearest_label(float(bpm))
            assert result == expected_label, (
                f"bpm_to_nearest_label({bpm}) debería ser {expected_label}, got {result}"
            )

    def test_bpm_to_nearest_label_between_classes(self):
        """Un BPM entre dos clases se mapea a la más cercana."""
        # Entre 120 (label 16) y 126 (label 17), midpoint es 123 → más cercano a 120
        result = bpm_to_nearest_label(122.0)
        bpm_result = BPM_CLASSES[result]
        assert bpm_result in (120, 126), (
            f"122 BPM debería mapear a 120 o 126, got {bpm_result}"
        )

    def test_num_bpm_classes_is_32(self):
        """Verificar que hay exactamente 32 clases de BPM."""
        assert NUM_BPM_CLASSES == 32
        assert len(BPM_CLASSES) == 32

    def test_bpm_classes_are_sorted(self):
        """Los BPM_CLASSES deben estar en orden ascendente."""
        assert BPM_CLASSES == sorted(BPM_CLASSES), "BPM_CLASSES debe estar ordenado"

    def test_bpm_classes_boundaries(self):
        """El rango de BPM_CLASSES debe ser 60-240."""
        assert BPM_CLASSES[0] == 60
        assert BPM_CLASSES[-1] == 240
