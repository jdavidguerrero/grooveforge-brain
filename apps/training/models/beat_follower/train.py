"""
train.py — Script de entrenamiento del modelo Beat Follower.

Uso:
    cd apps/training
    uv run python models/beat_follower/train.py [--epochs N] [--seed N]

Este script implementa Sprint 4.5. Hace:
  1. Genera dataset sintético de IOI histogramas (8K train / 1.6K val)
  2. Construye y entrena el modelo (Dense 3-layer)
  3. Convierte a TFLite float32 e int8 (quantización completa)
  4. Valida delta de accuracy < 5% (spec 04-ai-architecture.md §8)
  5. Reporta accuracy exacta Y accuracy ±1 clase
  6. Calcula top confusiones (ambigüedades double-time/half-time esperadas)
  7. Genera C array header para firmware Teensy
  8. Imprime resumen comparativo con los 3 modelos del Brain

Output en models/beat_follower/artifacts/:
  beat_follower_float32.tflite
  beat_follower_int8.tflite
  beat_follower_model.h   <- C array listo para firmware-teensy

Target de métricas (04-ai-architecture.md §8):
  Accuracy exacta val:   >75%  (sobre dataset sintético con jitter)
  Accuracy ±1 clase val: >90%  (estar a ±3-5 BPM es aceptable)
  Delta float→int8:      <5%
  Tamaño int8:           <30KB
  Tensor arena estimada: <20KB

Cross-ref: apps/docs/04-ai-architecture.md §1 (Beat Follower)
           apps/docs/sprints/25-beat-follower.md
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

from models.beat_follower.dataset import (
    BPM_CLASSES,
    NUM_BPM_CLASSES,
    generate_synthetic_dataset,
    label_to_bpm,
)
from models.beat_follower.model import train_beat_follower
from utils.tflite_utils import (
    compare_float_vs_int8,
    convert_to_tflite,
    convert_to_tflite_int8,
    tflite_to_c_array,
)

ARTIFACTS_DIR = Path(__file__).parent / "artifacts"
FLOAT32_PATH = ARTIFACTS_DIR / "beat_follower_float32.tflite"
INT8_PATH = ARTIFACTS_DIR / "beat_follower_int8.tflite"
HEADER_PATH = ARTIFACTS_DIR / "beat_follower_model.h"

FIRMWARE_HEADER_PATH = (
    Path(__file__).parent.parent.parent.parent
    / "firmware-teensy"
    / "src"
    / "ml"
    / "beat_follower_model.h"
)

# Accuracy mínima exacta para el dataset sintético.
# 75% es más conservador que key detector (85%) porque:
#   1. Hay 32 clases con alta correlación inter-clase (120 BPM vs 126 BPM son similares)
#   2. El jitter de 15ms difunde los picos del histograma, creando ambigüedad
#   3. La ambigüedad double-time/half-time es intrínseca al problema
# El target real de accuracy es el ±1 clase (>90%), no la exacta.
MIN_ACCURACY_EXACT = 0.75
MIN_ACCURACY_WITHIN_1 = 0.90


def make_representative_dataset(X_val: np.ndarray):
    """
    Genera la función representative_dataset para calibración INT8.

    Usa las primeras 200 muestras del validation set para calibrar
    los parámetros de quantización (scale, zero_point) de cada tensor.
    200 muestras cubren bien los 32 BPM targets con ~6 muestras por clase.

    Args:
        X_val: Validation data, shape (N, 32), float32.

    Returns:
        Callable que retorna un generator para TFLiteConverter.
    """
    n_rep = min(200, len(X_val))
    samples = X_val[:n_rep].astype(np.float32)

    def _generator():
        for sample in samples:
            yield [sample[np.newaxis, :]]  # shape (1, 32) — batch dim requerido

    return _generator


def compute_accuracy_within_n_classes(
    model_int8_path: Path,
    X_val: np.ndarray,
    y_val: np.ndarray,
    n_classes: int = 1,
) -> float:
    """
    Calcula la accuracy cuando se permite un margen de ±n_classes de error.

    En beat following, estar a 1 clase de distancia (≈3-5 BPM en el rango central)
    es perceptualmente aceptable — el display mostraría "120 BPM" cuando el real
    es 126 BPM, que el usuario siente como "correcto".

    Esta métrica es más representativa del valor práctico del modelo que
    la accuracy exacta.

    Args:
        model_int8_path: Ruta al .tflite INT8.
        X_val:           Validation data, shape (N, 32), float32.
        y_val:           Labels de validación, shape (N,), int32.
        n_classes:       Margen aceptable en número de clases.

    Returns:
        Fracción de muestras correctas dentro del margen (0.0 a 1.0).
    """
    import tensorflow as tf

    interpreter = tf.lite.Interpreter(model_path=str(model_int8_path))
    interpreter.allocate_tensors()

    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    input_scale = input_details[0]["quantization"][0]
    input_zero_point = input_details[0]["quantization"][1]
    input_dtype = input_details[0]["dtype"]

    correct_within_n = 0

    for sample, true_label in zip(X_val, y_val):
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

        if abs(pred_label - true_label_int) <= n_classes:
            correct_within_n += 1

    return correct_within_n / len(y_val)


def compute_top_confusions(
    model_int8_path: Path,
    X_val: np.ndarray,
    y_val: np.ndarray,
    top_n: int = 5,
) -> list[tuple[str, str, float]]:
    """
    Calcula las top-N confusiones del modelo INT8.

    En beat following, las confusiones esperadas son musicalmente predecibles:
      - 120 BPM → 60 BPM  (half-time: el modelo confunde beat con mitad de compás)
      - 120 BPM → 240 BPM (double-time: el modelo confunde corcheas con beats)
      - 60 BPM  → 120 BPM (complemento simétrico del caso anterior)
      - BPMs adyacentes: 120 → 116, 120 → 126 (clases muy similares)

    Ver sprint doc §3 para el análisis de la ambigüedad double-time/half-time.

    Args:
        model_int8_path: Ruta al .tflite INT8.
        X_val:           Validation data.
        y_val:           Labels de validación.
        top_n:           Número de confusiones a reportar.

    Returns:
        Lista de (true_bpm_str, pred_bpm_str, rate_pct) ordenada por rate desc.
    """
    import tensorflow as tf

    interpreter = tf.lite.Interpreter(model_path=str(model_int8_path))
    interpreter.allocate_tensors()

    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    input_scale = input_details[0]["quantization"][0]
    input_zero_point = input_details[0]["quantization"][1]
    input_dtype = input_details[0]["dtype"]

    confusion_counts: dict[tuple[int, int], int] = {}
    total_errors = 0

    for sample, true_label in zip(X_val, y_val):
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

    sorted_confusions = sorted(
        confusion_counts.items(),
        key=lambda x: x[1],
        reverse=True,
    )[:top_n]

    result = []
    for (true_lbl, pred_lbl), count in sorted_confusions:
        rate = count / total_errors * 100
        true_bpm = f"{BPM_CLASSES[true_lbl]} BPM"
        pred_bpm = f"{BPM_CLASSES[pred_lbl]} BPM"
        result.append((true_bpm, pred_bpm, rate))

    return result


def estimate_tensor_arena(int8_path: Path) -> int:
    """
    Estima el tensor arena requerido para este modelo en Teensy.

    Para un modelo Dense INT8 sin BatchNorm:
      - Activaciones: 64 + 32 + 32 bytes = 128 bytes
      - Overhead TFLite Micro (metadata, op tables): ~8KB
      - Buffer temporal MatMul INT8: ~4KB
      - Padding de alineación: ~2KB

    Estimación: max(16KB, model_size * 1.5)
    Para un modelo ~10KB, el arena será ~16KB.
    """
    model_size = int8_path.stat().st_size
    estimated = max(16 * 1024, int(model_size * 1.5))
    return estimated


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Entrena el Beat Follower — Sprint 4.5"
    )
    parser.add_argument(
        "--epochs",
        type=int,
        default=120,
        help="Máximo de epochs (default: 120). EarlyStopping patience=20.",
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
        default=8000,
        help="Muestras de entrenamiento (default: 8000).",
    )
    parser.add_argument(
        "--n-val",
        type=int,
        default=1600,
        help="Muestras de validación (default: 1600).",
    )
    args = parser.parse_args()

    rng = np.random.default_rng(args.seed)
    ARTIFACTS_DIR.mkdir(parents=True, exist_ok=True)

    print("\n=== Beat Follower Training (Sprint 4.5) ===")
    print(f"Clases: {NUM_BPM_CLASSES} BPM targets ({BPM_CLASSES[0]}-{BPM_CLASSES[-1]} BPM)")
    print(f"IOI range: 50-2000ms | 32 bins | bin width: ~{(2000-50)/32:.0f}ms")

    # ── 1. Dataset
    print(f"\nGenerando dataset sintético...")
    t_data = time.time()
    X_train, y_train = generate_synthetic_dataset(
        n_samples=args.n_train,
        timing_jitter_ms=15.0,
        subdivision_prob=0.3,
        tempo_drift_pct=0.02,
        rng=rng,
    )
    X_val, y_val = generate_synthetic_dataset(
        n_samples=args.n_val,
        timing_jitter_ms=15.0,
        subdivision_prob=0.3,
        tempo_drift_pct=0.02,
        rng=rng,
    )
    elapsed_data = time.time() - t_data

    print(
        f"Dataset: {len(X_train)} train / {len(X_val)} val "
        f"(sintético, {NUM_BPM_CLASSES} clases, {elapsed_data:.1f}s)"
    )
    print(f"BPM range: {BPM_CLASSES[0]}-{BPM_CLASSES[-1]} BPM | IOI range: 50-2000ms | 32 bins")
    print(f"X_train shape: {X_train.shape}, dtype: {X_train.dtype}")
    print(f"Labels rango: [{y_train.min()}, {y_train.max()}]")

    # ── 2. Entrenamiento
    print("\n--- Entrenamiento ---")
    t_train = time.time()
    model, history = train_beat_follower(
        X_train, y_train, X_val, y_val, epochs=args.epochs
    )
    elapsed_train = time.time() - t_train
    print(f"\nEntrenamiento: {elapsed_train:.1f}s")

    # ── 3. Accuracy final en validación (Keras)
    import tensorflow as tf

    val_loss, val_acc = model.evaluate(X_val, y_val, verbose=0)
    print(f"Val accuracy exacta (Keras float32): {val_acc:.2%}")

    spec_exact = "OK" if val_acc >= MIN_ACCURACY_EXACT else f"ADVERTENCIA — target es {MIN_ACCURACY_EXACT:.0%}"
    print(f"Target {MIN_ACCURACY_EXACT:.0%}: {spec_exact}")

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
            "Revisar representative_dataset."
        )

    # ── 7. Accuracy ±1 clase (métrica principal para beat following)
    print("\n--- Accuracy ±1 clase (target >90%) ---")
    acc_within_1 = compute_accuracy_within_n_classes(INT8_PATH, X_val, y_val, n_classes=1)
    print(f"Accuracy ±1 clase (INT8): {acc_within_1:.2%}")
    spec_within_1 = "OK" if acc_within_1 >= MIN_ACCURACY_WITHIN_1 else f"ADVERTENCIA — target es {MIN_ACCURACY_WITHIN_1:.0%}"
    print(f"Target {MIN_ACCURACY_WITHIN_1:.0%}: {spec_within_1}")

    # ── 8. Top confusiones (ambigüedades double-time/half-time)
    print("\n--- Top-5 Confusiones del modelo INT8 ---")
    confusions = compute_top_confusions(INT8_PATH, X_val, y_val, top_n=5)

    print("Top confusiones (musicalmente esperadas):")
    if confusions:
        for true_bpm, pred_bpm, rate in confusions:
            print(f"  {true_bpm:12s} → {pred_bpm:12s} ({rate:.1f}% de errores)")
    else:
        print("  Sin errores detectados")

    print()
    print("Interpretación musical:")
    print("  120 BPM → 60 BPM  (double-time ambiguity: el modelo confunde beat con mitad de compas)")
    print("  120 BPM → 240 BPM (half-time ambiguity: corcheas confundidas con beats)")
    print("  → Estas ambiguedades son intrínsecas al problema de tempo, no fallas del modelo")

    # ── 9. Generar C array header
    print("\n--- Generando C array header ---")
    header_content = tflite_to_c_array(INT8_PATH, var_name="g_beat_follower_model")
    HEADER_PATH.write_text(header_content)
    print(f"Header generado: {HEADER_PATH}")

    # ── 10. Copiar header al directorio de firmware si existe
    if FIRMWARE_HEADER_PATH.parent.exists():
        FIRMWARE_HEADER_PATH.write_text(header_content)
        print(f"Header copiado a firmware: {FIRMWARE_HEADER_PATH}")
    else:
        print(
            f"(firmware-teensy/src/ml/ no existe — copiar manualmente: {HEADER_PATH})"
        )

    # ── 11. Resumen final
    float32_acc = metrics["float_accuracy"] or val_acc
    int8_acc = metrics["int8_accuracy"] or 0.0
    delta_pct = metrics["delta_pct"] or 0.0
    size_float = metrics["size_float_kb"]
    size_int8 = metrics["size_int8_kb"]
    arena_est_kb = estimate_tensor_arena(INT8_PATH) / 1024

    print("\n=== Resultados ===")
    print(f"Float32 accuracy:  {float32_acc:.1%}  (exact BPM class match)")
    print(f"Float32 accuracy ±1 clase: (ver arriba)")
    print(f"Int8    accuracy:  {int8_acc:.1%}")
    print(f"Delta:             {delta_pct:+.1f}% ({'OK' if metrics['spec_ok'] else 'FALLA'})")
    print(f"Accuracy ±1 clase (INT8): {acc_within_1:.1%}")
    print(f"Float32 size:      {size_float:.1f} KB")
    print(f"Int8    size:      {size_int8:.1f} KB")
    print(f"Tensor arena est.: ~{arena_est_kb:.0f} KB")
    print()

    # Comparativa con los 3 modelos
    print("Comparativa de los 3 modelos Layer 1 (Sprint 4.3-4.5):")
    print("  Key Detector:      12→24  clases, ~4KB  int8, ~16KB  arena")
    print("  Chord Recognizer:  12→61  clases, ~20KB int8, ~24KB  arena")
    print(f"  Beat Follower:     32→32  clases, {size_int8:.0f}KB int8, ~{arena_est_kb:.0f}KB arena")
    print()
    print("Nota: accuracy en dataset sintético >> accuracy en musica real.")
    print("La diferencia es el 'reality gap': musica real tiene mas ruido,")
    print("poliritmo, cambios de tempo y ambiguedad que el dataset sintetico.")
    print()

    print("Artifacts guardados en models/beat_follower/artifacts/")
    print(f"  {FLOAT32_PATH.name}")
    print(f"  {INT8_PATH.name}")
    print(f"  {HEADER_PATH.name}  <- listo para copiar a firmware-teensy")

    # ── Exit code no-cero si hay fallo de spec
    has_failure = (
        val_acc < MIN_ACCURACY_EXACT
        or acc_within_1 < MIN_ACCURACY_WITHIN_1
        or not metrics["spec_ok"]
    )
    if has_failure:
        if val_acc < MIN_ACCURACY_EXACT:
            print(
                f"\nFALLA: accuracy exacta {val_acc:.2%} < {MIN_ACCURACY_EXACT:.0%}. "
                "Considerar más epochs o más datos."
            )
        if acc_within_1 < MIN_ACCURACY_WITHIN_1:
            print(
                f"\nFALLA: accuracy ±1 clase {acc_within_1:.2%} < {MIN_ACCURACY_WITHIN_1:.0%}. "
                "El modelo no alcanza el target de beat following."
            )
        sys.exit(1)


if __name__ == "__main__":
    main()
