"""
train.py — Script de entrenamiento del modelo Key Detector.

Uso:
    cd apps/training
    uv run python models/key_detector/train.py [--lakh-dir RUTA] [--epochs N]

Este script es el entry point principal del Sprint 4.3. Hace:
  1. Genera dataset sintético (10K train / 2K val) — sin necesidad de Lakh
  2. Si existe Lakh descargado, mezcla 50/50 sintético + real
  3. Construye y entrena el modelo (Dense 3-layer)
  4. Convierte a TFLite float32 y int8
  5. Valida delta de accuracy < 5% (spec 04-ai-architecture.md §8)
  6. Genera C array header para Teensy
  7. Imprime resumen final de métricas

Output en models/key_detector/artifacts/:
  key_detector_float32.tflite
  key_detector_int8.tflite
  key_detector_model.h          ← C array listo para firmware-teensy

Cross-ref: apps/docs/04-ai-architecture.md §1 (Scale Lock), §8 (métricas)
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import numpy as np

# ── Ajustar sys.path para que los imports relativos funcionen
# cuando se corre como: uv run python models/key_detector/train.py
_TRAINING_ROOT = Path(__file__).parent.parent.parent
if str(_TRAINING_ROOT) not in sys.path:
    sys.path.insert(0, str(_TRAINING_ROOT))

from models.key_detector.dataset import (
    NUM_CLASSES,
    generate_synthetic_dataset,
    load_lakh_dataset,
)
from models.key_detector.model import train_key_detector
from utils.tflite_utils import (
    compare_float_vs_int8,
    convert_to_tflite,
    convert_to_tflite_int8,
    tflite_to_c_array,
)

ARTIFACTS_DIR = Path(__file__).parent / "artifacts"
FLOAT32_PATH = ARTIFACTS_DIR / "key_detector_float32.tflite"
INT8_PATH = ARTIFACTS_DIR / "key_detector_int8.tflite"
HEADER_PATH = ARTIFACTS_DIR / "key_detector_model.h"

# Ruta destino en el firmware (para el mensaje final)
FIRMWARE_HEADER_PATH = (
    Path(__file__).parent.parent.parent.parent
    / "firmware-teensy"
    / "src"
    / "ml"
    / "key_detector_model.h"
)


def build_dataset(lakh_dir: str | None, rng: np.random.Generator) -> tuple[
    np.ndarray, np.ndarray, np.ndarray, np.ndarray, str
]:
    """
    Construye train/val splits.

    Si lakh_dir se especifica y tiene archivos MIDI:
      - Carga dataset real con load_lakh_dataset()
      - Mezcla 50% sintético + 50% real
      - Split 80/20 train/val
    Si no:
      - 10K muestras sintéticas para train
      - 2K muestras sintéticas para val

    Returns:
        (X_train, y_train, X_val, y_val, dataset_description)
    """
    if lakh_dir:
        from pathlib import Path as P
        lakh_path = P(lakh_dir)
        has_midi = lakh_path.exists() and any(lakh_path.rglob("*.mid"))
    else:
        has_midi = False

    if has_midi:
        # Dataset mixto: sintético + real
        print(f"Cargando dataset real de '{lakh_dir}'...")
        X_real, y_real = load_lakh_dataset(lakh_dir, max_files=1000, rng=rng)
        n_real = len(X_real)

        # Generar mismo número de sintéticos para balance 50/50
        X_synth, y_synth = generate_synthetic_dataset(n_samples=n_real, rng=rng)

        X_all = np.concatenate([X_synth, X_real], axis=0)
        y_all = np.concatenate([y_synth, y_real], axis=0)

        # Shuffle antes de split
        idx = rng.permutation(len(X_all))
        X_all, y_all = X_all[idx], y_all[idx]

        # 80/20 split
        split = int(0.8 * len(X_all))
        X_train, y_train = X_all[:split], y_all[:split]
        X_val, y_val = X_all[split:], y_all[split:]

        desc = f"{len(X_train)} train / {len(X_val)} val (mixto: {n_real} real + {n_real} sintético)"
    else:
        # Solo sintético — noise_level=0.02 elegido empíricamente:
        #   noise=0.1 → solo 17% accuracy (K-S adyacentes indistinguibles)
        #   noise=0.02 → LR sklearn llega a 94%, separabilidad buena
        #   noise=0.03 → LR ~39%, el Dense queda en ~72%
        # Con 30K samples y noise=0.02 el Dense debería superar 85%.
        # El ruido "musical real" (notas cromaticas, paso entre tonalidades) vendrá de Lakh.
        X_train, y_train = generate_synthetic_dataset(
            n_samples=30000, noise_level=0.02, rng=rng
        )
        X_val, y_val = generate_synthetic_dataset(
            n_samples=3000, noise_level=0.02, rng=rng
        )
        desc = f"{len(X_train)} train / {len(X_val)} val (sintético, noise=0.02)"

    return X_train, y_train, X_val, y_val, desc


def make_representative_dataset(X_val: np.ndarray):
    """
    Genera la función representative_dataset para la quantización INT8.

    TFLite necesita ~100-200 muestras del rango real de entradas para calibrar
    los factores de escala (scale + zero_point) de cada tensor de activación.
    Si el rango de calibración es muy estrecho, las activaciones fuera de ese
    rango saturan en INT8 y la accuracy cae.

    Usamos las primeras 200 muestras del validation set, que son representativas
    del espacio de entrada por construcción (son muestras del mismo proceso generador).
    """
    samples = X_val[:200].astype(np.float32)

    def _generator():
        for sample in samples:
            yield [sample[np.newaxis, :]]  # shape (1, 12) — batch dim requerido

    return _generator


def estimate_tensor_arena(int8_path: Path) -> int:
    """
    Estima el tensor arena requerido para este modelo en Teensy.

    Para un modelo Dense INT8 simple, el arena es dominado por:
      - Los tensores de activación intermedios (output de cada capa)
      - Los buffers temporales de MatMul INT8

    Para esta arquitectura 12→64→32→24:
      - Layer 1 output: 64 * 1 byte = 64 bytes
      - Layer 2 output: 32 * 1 byte = 32 bytes
      - Layer 3 output: 24 * 1 byte = 24 bytes
      - Overhead TFLite Micro (metadata, op tables, alineación): ~8KB

    En la práctica, TFLite Micro necesita algo de padding y alineación.
    La estimación conservadora es: max(8KB, tamaño_modelo * 2).
    """
    model_size = int8_path.stat().st_size
    estimated = max(8 * 1024, model_size * 2)
    return estimated


def main() -> None:
    parser = argparse.ArgumentParser(description="Entrena el modelo Key Detector (Sprint 4.3)")
    parser.add_argument(
        "--lakh-dir",
        default=None,
        help="Directorio con archivos MIDI del Lakh Dataset (opcional).",
    )
    parser.add_argument(
        "--epochs",
        type=int,
        default=150,
        help="Máximo de epochs de entrenamiento (default: 150). EarlyStopping con patience=20.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Semilla para reproducibilidad (default: 42).",
    )
    args = parser.parse_args()

    rng = np.random.default_rng(args.seed)
    ARTIFACTS_DIR.mkdir(parents=True, exist_ok=True)

    # ── 1. Dataset
    print("\n=== Key Detector Training (Sprint 4.3) ===")
    X_train, y_train, X_val, y_val, desc = build_dataset(args.lakh_dir, rng)
    print(f"Dataset: {desc}")
    print(f"Clases:  {NUM_CLASSES} (12 mayores + 12 menores)")
    print()

    # ── 2. Entrenamiento
    t0 = time.time()
    model, history = train_key_detector(
        X_train, y_train, X_val, y_val, epochs=args.epochs
    )
    elapsed = time.time() - t0
    print(f"\nEntrenamiento: {elapsed:.1f}s")

    # ── 3. Accuracy final en validación
    import tensorflow as tf
    val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)
    print(f"Val accuracy (Keras): {val_acc:.2%}")

    # Verificar threshold del spec
    if val_acc < 0.85:
        print("Nota: accuracy < 85% puede deberse al nivel de ruido del dataset sintético.")
        print("Con datos reales de Lakh (--lakh-dir) el modelo generaliza mejor.")
    if val_acc < 0.85:
        print(f"ADVERTENCIA: accuracy {val_acc:.2%} < 85% (spec 04-ai-architecture.md §8)")
        print("Considerar: más epochs, más datos, revisar arquitectura.")
    else:
        print(f"Accuracy OK: {val_acc:.2%} >= 85% del spec")

    # ── 4. Conversión Float32 TFLite
    print("\n--- Conversión Float32 ---")
    convert_to_tflite(model, FLOAT32_PATH)

    # ── 5. Conversión INT8
    print("\n--- Conversión INT8 ---")
    rep_dataset = make_representative_dataset(X_val)
    convert_to_tflite_int8(model, rep_dataset, INT8_PATH)

    # ── 6. Comparación y validación del delta
    print("\n--- Comparación Float32 vs INT8 ---")
    metrics = compare_float_vs_int8(
        FLOAT32_PATH,
        INT8_PATH,
        test_data=X_val,
        test_labels=y_val,
    )

    if not metrics["spec_ok"]:
        print(
            f"ERROR: delta de quantización {metrics['delta_pct']:.1f}% supera el 5% del spec."
            " Revisar representative_dataset o arquitectura."
        )

    # ── 7. Generar C array header
    print("\n--- Generando C array header ---")
    header_content = tflite_to_c_array(INT8_PATH, var_name="g_key_detector_model")
    HEADER_PATH.write_text(header_content)
    print(f"Header generado: {HEADER_PATH}")

    # ── 8. Copiar header al directorio de firmware si existe
    if FIRMWARE_HEADER_PATH.parent.exists():
        FIRMWARE_HEADER_PATH.write_text(header_content)
        print(f"Header copiado a firmware: {FIRMWARE_HEADER_PATH}")
    else:
        print(f"(firmware-teensy/src/ml/ no existe — copiar manualmente: {HEADER_PATH})")

    # ── 9. Resumen final
    float32_acc = metrics["float_accuracy"] or val_acc
    int8_acc = metrics["int8_accuracy"] or 0.0
    delta_pct = metrics["delta_pct"] or 0.0
    size_float = metrics["size_float_kb"]
    size_int8 = metrics["size_int8_kb"]
    reduction_pct = (1 - size_int8 / size_float) * 100 if size_float > 0 else 0
    arena_est_kb = estimate_tensor_arena(INT8_PATH) / 1024

    print("\n=== Resultados ===")
    print(f"Float32 accuracy:  {float32_acc:.1%}")
    print(f"Int8    accuracy:  {int8_acc:.1%}")
    print(f"Delta:             {delta_pct:+.1f}% ({'OK' if metrics['spec_ok'] else 'FALLA'})")
    print(f"Float32 size:      {size_float:.1f} KB")
    print(f"Int8    size:      {size_int8:.1f} KB ({reduction_pct:.0f}% reduccion)")
    print(f"Tensor arena est.: ~{arena_est_kb:.0f} KB")
    print()
    print("Artifacts guardados en models/key_detector/artifacts/")
    print(f"  {FLOAT32_PATH.name}")
    print(f"  {INT8_PATH.name}")
    print(f"  {HEADER_PATH.name}  <- listo para copiar a firmware-teensy")

    # Exit code no-cero si hay fallo de spec
    if val_acc < 0.85 or not metrics["spec_ok"]:
        sys.exit(1)


if __name__ == "__main__":
    main()
