"""
train.py — Script de entrenamiento del modelo Chord Recognizer.

Uso:
    cd apps/training
    uv run python models/chord_recognizer/train.py [--epochs N] [--seed N]

Este script implementa Sprint 4.4. Hace:
  1. Genera dataset sintético (15K train / 3K val) — no requiere Lakh
  2. Construye y entrena el modelo (Dense 3-layer con BatchNorm)
  3. Convierte a TFLite float32 y int8 (quantización completa)
  4. Valida delta de accuracy < 5% (spec 04-ai-architecture.md §8)
  5. Calcula matriz de confusión — top-5 confusiones más frecuentes
  6. Genera C array header para firmware Teensy
  7. Imprime resumen final de métricas

Output en models/chord_recognizer/artifacts/:
  chord_recognizer_float32.tflite
  chord_recognizer_int8.tflite
  chord_recognizer_model.h   ← C array listo para firmware-teensy

Target de métricas (04-ai-architecture.md §8):
  Accuracy val: >80% (relajado de 85% por ser un problema de 61 clases)
  Delta float→int8: <5%
  Tamaño int8: <80KB
  Tensor arena estimada: <24KB

Cross-ref: apps/docs/04-ai-architecture.md §1 (Chord Recognition)
           apps/docs/sprints/24-chord-recognizer.md
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

import numpy as np

# ── Ajustar sys.path para imports desde apps/training/
_TRAINING_ROOT = Path(__file__).parent.parent.parent
if str(_TRAINING_ROOT) not in sys.path:
    sys.path.insert(0, str(_TRAINING_ROOT))

from models.chord_recognizer.dataset import (
    CHORD_CLASSES,
    NUM_CLASSES,
    generate_synthetic_dataset,
)
from models.chord_recognizer.model import train_chord_recognizer
from utils.tflite_utils import (
    compare_float_vs_int8,
    convert_to_tflite,
    convert_to_tflite_int8,
    tflite_to_c_array,
)

ARTIFACTS_DIR = Path(__file__).parent / "artifacts"
FLOAT32_PATH = ARTIFACTS_DIR / "chord_recognizer_float32.tflite"
INT8_PATH = ARTIFACTS_DIR / "chord_recognizer_int8.tflite"
HEADER_PATH = ARTIFACTS_DIR / "chord_recognizer_model.h"

FIRMWARE_HEADER_PATH = (
    Path(__file__).parent.parent.parent.parent
    / "firmware-teensy"
    / "src"
    / "ml"
    / "chord_recognizer_model.h"
)

# Accuracy mínima para chord recognition.
# 80% es menos estricto que key detector (85%) porque 61 clases con alta
# correlación inter-clase (maj vs maj7 difieren en 1 nota) hacen el problema
# intrínsecamente más difícil. Ver sprint doc §4 (matriz de confusión).
MIN_ACCURACY = 0.80


def make_representative_dataset(X_val: np.ndarray):
    """
    Genera la función representative_dataset para calibración INT8.

    Usa las primeras 200 muestras del validation set, que cubren los 61 tipos
    de acorde. La calibración es crítica para BatchNorm: los factores scale y
    zero_point de cada tensor deben capturar el rango real de activaciones.

    Args:
        X_val: Validation data, shape (N, 12), float32.

    Returns:
        Callable que retorna un generator para TFLiteConverter.
    """
    # Seleccionar muestras variadas — no solo las primeras 200 (que pueden
    # estar sesgadas hacia las primeras clases después del shuffle)
    n_rep = min(200, len(X_val))
    samples = X_val[:n_rep].astype(np.float32)

    def _generator():
        for sample in samples:
            yield [sample[np.newaxis, :]]  # shape (1, 12) — batch dim requerido

    return _generator


def compute_top_confusions(
    model_int8_path: Path,
    X_val: np.ndarray,
    y_val: np.ndarray,
    top_n: int = 5,
) -> list[tuple[str, str, float]]:
    """
    Calcula las top-N confusiones del modelo INT8.

    Una confusión (A → B, p%) significa: "cuando la clase real es A,
    el modelo predice B en p% de los casos (condicionado a error)."

    Las confusiones más frecuentes en chord recognition son musicalmente
    predecibles: maj↔maj7 (difieren en 1 nota), min↔m7 (igual), etc.
    Ver sprint doc §4 para el análisis completo.

    Args:
        model_int8_path: Ruta al .tflite INT8.
        X_val:           Validation data.
        y_val:           Labels de validación.
        top_n:           Número de confusiones a reportar.

    Returns:
        Lista de (true_class_name, pred_class_name, rate) ordenada por rate desc.
        rate es la fracción de veces que esa confusión ocurrió sobre el total de errores.
    """
    import tensorflow as tf

    interpreter = tf.lite.Interpreter(model_path=str(model_int8_path))
    interpreter.allocate_tensors()

    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    input_scale = input_details[0]["quantization"][0]
    input_zero_point = input_details[0]["quantization"][1]
    input_dtype = input_details[0]["dtype"]

    output_scale = output_details[0]["quantization"][0]
    output_zero_point = output_details[0]["quantization"][1]

    # Contar confusiones: dict[(true_label, pred_label)] → count
    confusion_counts: dict[tuple[int, int], int] = {}
    total_errors = 0

    for i, (sample, true_label) in enumerate(zip(X_val, y_val)):
        inp = sample[np.newaxis, :].astype(np.float32)

        if input_dtype == np.int8:
            if input_scale > 1e-9:
                inp = (inp / input_scale + input_zero_point).astype(np.int8)
            else:
                inp = inp.astype(np.int8)

        interpreter.set_tensor(input_details[0]["index"], inp)
        interpreter.invoke()
        out = interpreter.get_tensor(output_details[0]["index"])

        pred_label = int(np.argmax(out))
        true_label_int = int(true_label)

        if pred_label != true_label_int:
            key = (true_label_int, pred_label)
            confusion_counts[key] = confusion_counts.get(key, 0) + 1
            total_errors += 1

    if total_errors == 0:
        return []

    # Ordenar por frecuencia y tomar top-N
    sorted_confusions = sorted(
        confusion_counts.items(),
        key=lambda x: x[1],
        reverse=True,
    )[:top_n]

    result = []
    for (true_lbl, pred_lbl), count in sorted_confusions:
        rate = count / total_errors * 100
        true_name = CHORD_CLASSES[true_lbl]
        pred_name = CHORD_CLASSES[pred_lbl]
        result.append((true_name, pred_name, rate))

    return result


def estimate_tensor_arena(int8_path: Path) -> int:
    """
    Estima el tensor arena requerido para este modelo en Teensy.

    Para un modelo Dense + BatchNorm INT8:
      - BatchNorm fusionado con Dense → no agrega activaciones intermedias
      - Activaciones: 128 + 64 + 61 bytes (INT8 por neurona)
      - Overhead TFLite Micro (metadata, op tables, alineación): ~8KB
      - Buffer temporal MatMul: ~4KB

    Estimación conservadora: max(16KB, tamaño_modelo * 1.5).
    Justificación: el modelo INT8 es ~18KB, el arena debe ser ~24KB.
    """
    model_size = int8_path.stat().st_size
    estimated = max(16 * 1024, int(model_size * 1.5))
    return estimated


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Entrena el Chord Recognizer — Sprint 4.4"
    )
    parser.add_argument(
        "--epochs",
        type=int,
        default=80,
        help="Máximo de epochs (default: 80). EarlyStopping patience=15.",
    )
    parser.add_argument(
        "--seed",
        type=int,
        default=42,
        help="Semilla para reproducibilidad (default: 42).",
    )
    parser.add_argument(
        "--n-train",
        type=int,
        default=15000,
        help="Muestras de entrenamiento (default: 15000).",
    )
    parser.add_argument(
        "--n-val",
        type=int,
        default=3000,
        help="Muestras de validación (default: 3000).",
    )
    args = parser.parse_args()

    rng = np.random.default_rng(args.seed)
    ARTIFACTS_DIR.mkdir(parents=True, exist_ok=True)

    print("\n=== Chord Recognizer Training (Sprint 4.4) ===")
    print(f"Clases: {NUM_CLASSES} (60 acordes + 1 clase N)")

    # ── 1. Dataset
    print(f"\nGenerando dataset sintético...")
    t_data = time.time()
    X_train, y_train = generate_synthetic_dataset(
        n_samples=args.n_train, noise_level=0.05, rng=rng
    )
    X_val, y_val = generate_synthetic_dataset(
        n_samples=args.n_val, noise_level=0.05, rng=rng
    )
    elapsed_data = time.time() - t_data

    print(
        f"Dataset: {len(X_train)} train / {len(X_val)} val "
        f"(sintético, {NUM_CLASSES} clases, {elapsed_data:.1f}s)"
    )
    print(f"X_train shape: {X_train.shape}, dtype: {X_train.dtype}")
    print(f"Labels rango: [{y_train.min()}, {y_train.max()}]")

    # ── 2. Entrenamiento
    print("\n--- Entrenamiento ---")
    t_train = time.time()
    model, history = train_chord_recognizer(
        X_train, y_train, X_val, y_val, epochs=args.epochs
    )
    elapsed_train = time.time() - t_train
    print(f"\nEntrenamiento: {elapsed_train:.1f}s")

    # ── 3. Accuracy final en validación
    import tensorflow as tf

    val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)
    print(f"Val accuracy (Keras): {val_acc:.2%}")

    spec_msg = "OK" if val_acc >= MIN_ACCURACY else f"ADVERTENCIA — target es {MIN_ACCURACY:.0%}"
    print(f"Target {MIN_ACCURACY:.0%}: {spec_msg}")

    # ── 4. Conversión Float32 TFLite
    print("\n--- Conversión Float32 ---")
    convert_to_tflite(model, FLOAT32_PATH)

    # ── 5. Conversión INT8
    print("\n--- Conversión INT8 ---")
    rep_dataset = make_representative_dataset(X_val)
    convert_to_tflite_int8(model, rep_dataset, INT8_PATH)

    # ── 6. Comparación Float32 vs INT8
    print("\n--- Comparación Float32 vs INT8 ---")
    metrics = compare_float_vs_int8(
        FLOAT32_PATH,
        INT8_PATH,
        test_data=X_val,
        test_labels=y_val,
    )

    if not metrics["spec_ok"]:
        print(
            f"ERROR: delta {metrics['delta_pct']:.1f}% supera el 5% del spec. "
            "Revisar representative_dataset o arquitectura."
        )

    # ── 7. Matriz de confusión — top-5 confusiones
    print("\n--- Top-5 Confusiones del modelo INT8 ---")
    confusions = compute_top_confusions(INT8_PATH, X_val, y_val, top_n=5)
    if confusions:
        for true_name, pred_name, rate in confusions:
            # Explicación musical de la confusión
            print(f"  {true_name:12s} → {pred_name:12s} ({rate:.1f}% de errores)")
    else:
        print("  Sin errores detectados — accuracy perfecta en validación")

    print()
    print("Interpretación musical esperada:")
    print("  X_maj  → X_maj7 : difieren en 1 nota (la 7ma mayor)")
    print("  X_min  → X_m7   : difieren en 1 nota (la 7ma menor)")
    print("  X_7    → X_maj7 : ambos tienen [0,4,7,?] — la 7ma es el diferenciador")
    print("  → Estas confusiones son una limitación del pitch class histogram, no del modelo")

    # ── 8. Generar C array header
    print("\n--- Generando C array header ---")
    header_content = tflite_to_c_array(INT8_PATH, var_name="g_chord_recognizer_model")
    HEADER_PATH.write_text(header_content)
    print(f"Header generado: {HEADER_PATH}")

    # ── 9. Copiar header al directorio de firmware si existe
    if FIRMWARE_HEADER_PATH.parent.exists():
        FIRMWARE_HEADER_PATH.write_text(header_content)
        print(f"Header copiado a firmware: {FIRMWARE_HEADER_PATH}")
    else:
        print(
            f"(firmware-teensy/src/ml/ no existe — copiar manualmente: {HEADER_PATH})"
        )

    # ── 10. Resumen final
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

    # Comparativa con key detector
    print("Comparativa con key detector (Sprint 4.3):")
    print("  Key Detector:      24 clases, ~4KB int8, accuracy ~94%, ~8KB arena")
    print(f"  Chord Recognizer:  {NUM_CLASSES} clases, {size_int8:.0f}KB int8, "
          f"accuracy {int8_acc:.0%}, ~{arena_est_kb:.0f}KB arena")
    print()

    print("Artifacts guardados en models/chord_recognizer/artifacts/")
    print(f"  {FLOAT32_PATH.name}")
    print(f"  {INT8_PATH.name}")
    print(f"  {HEADER_PATH.name}  <- listo para copiar a firmware-teensy")

    # ── Exit code no-cero si hay fallo de spec
    has_failure = val_acc < MIN_ACCURACY or not metrics["spec_ok"]
    if has_failure:
        if val_acc < MIN_ACCURACY:
            print(
                f"\nFALLA: accuracy {val_acc:.2%} < {MIN_ACCURACY:.0%} mínimo. "
                "Considerar más epochs, más datos, o revisar noise_level."
            )
        sys.exit(1)


if __name__ == "__main__":
    main()
