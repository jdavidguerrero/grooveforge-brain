"""
tflite_utils.py — Conversión, validación y export de modelos TFLite para Teensy.

Pipeline completo:
  1. convert_to_tflite()       — Float32, para validar accuracy base
  2. convert_to_tflite_int8()  — Quantizado INT8, para deployment
  3. compare_float_vs_int8()   — Verifica que el delta de accuracy < 5% (spec §1.5)
  4. tflite_to_c_array()       — Genera el header C para incluir en el firmware

Por qué INT8 en Teensy 4.1:
  - Flash budget: 500KB para TODOS los modelos (04-ai-architecture.md §1.2)
  - INT8 = 4x menos memoria que Float32
  - Cortex-M7 con CMSIS-NN: INT8 multiplicaciones son ~4x más rápidas
  - Target: <20ms p99 de inference (04-ai-architecture.md §8)

Cross-ref: apps/docs/04-ai-architecture.md §1.5, §8
"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Callable, Generator, Iterator

import numpy as np


# ---------------------------------------------------------------------------
# Conversión Float32
# ---------------------------------------------------------------------------


def convert_to_tflite(
    model,  # tf.keras.Model — sin import estático para evitar carga de TF en tests
    output_path: str | Path,
) -> None:
    """
    Convierte un modelo Keras a TFLite Float32.

    El modelo Float32 sirve como baseline para medir el accuracy delta
    antes de quantización. NO usar en producción Teensy — usar INT8.

    Args:
        model:       tf.keras.Model entrenado.
        output_path: Ruta donde guardar el .tflite resultante.

    Output:
        Archivo .tflite en output_path.
    """
    import tensorflow as tf  # import lazy para no romper tests sin TF

    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    converter = tf.lite.TFLiteConverter.from_keras_model(model)
    tflite_model = converter.convert()

    output_path.write_bytes(tflite_model)
    size_kb = len(tflite_model) / 1024
    print(f"TFLite Float32 guardado: {output_path} ({size_kb:.1f} KB)")


# ---------------------------------------------------------------------------
# Conversión INT8 (quantización)
# ---------------------------------------------------------------------------


def convert_to_tflite_int8(
    model,
    representative_dataset: Callable[[], Iterator[list[np.ndarray]]],
    output_path: str | Path,
) -> None:
    """
    Convierte un modelo Keras a TFLite INT8 con quantización completa.

    Quantización INT8 completa significa:
      - Pesos: float32 → int8 (per-channel)
      - Activaciones: float32 → int8 (per-tensor, calibradas con representative_dataset)
      - Inputs/outputs del modelo: también int8

    El representative_dataset DEBE ser representativo del rango real de entradas.
    Un dataset muy pequeño o sesgado produce escala/zero-point incorrectos y
    puede causar accuracy drop > 5% (por encima del límite del spec).

    Args:
        model:                  tf.keras.Model entrenado.
        representative_dataset: Callable que retorna un generator de listas de
                                 np.ndarray. Cada elemento es un batch de entrada.
                                 Recomendado: 100-200 muestras del validation set.

                                 Ejemplo:
                                     def rep_dataset():
                                         for x in val_data[:200]:
                                             yield [x[np.newaxis, :].astype(np.float32)]

        output_path:            Ruta donde guardar el .tflite quantizado.

    Output:
        Archivo .tflite INT8 en output_path.

    Verificar post-conversión:
        Usar compare_float_vs_int8() para confirmar delta < 5%.
    """
    import tensorflow as tf

    output_path = Path(output_path)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    converter = tf.lite.TFLiteConverter.from_keras_model(model)

    # Quantización completa INT8 — patrón estándar para TFLite Micro
    converter.optimizations = [tf.lite.Optimize.DEFAULT]
    converter.representative_dataset = representative_dataset
    converter.target_spec.supported_ops = [tf.lite.OpsSet.TFLITE_BUILTINS_INT8]
    converter.inference_input_type = tf.int8
    converter.inference_output_type = tf.int8

    tflite_model = converter.convert()

    output_path.write_bytes(tflite_model)
    size_kb = len(tflite_model) / 1024
    print(f"TFLite INT8 guardado: {output_path} ({size_kb:.1f} KB)")


# ---------------------------------------------------------------------------
# Comparación Float32 vs INT8
# ---------------------------------------------------------------------------


def compare_float_vs_int8(
    model_float_path: str | Path,
    model_int8_path: str | Path,
    test_data: np.ndarray,
    test_labels: np.ndarray | None = None,
) -> dict[str, float]:
    """
    Compara accuracy y tamaño entre un modelo Float32 y su versión INT8.

    Verifica que el accuracy delta sea < 5% (04-ai-architecture.md §1.5).
    Si el delta supera 5%, el modelo necesita revisión arquitectural —
    generalmente BatchNorm mal ubicado o capas muy sensibles a quantización.

    Args:
        model_float_path: Ruta al .tflite Float32.
        model_int8_path:  Ruta al .tflite INT8.
        test_data:        np.ndarray de shape (N, *input_shape). Float32.
        test_labels:      np.ndarray de shape (N,) con labels enteros.
                          Si None, solo reporta tamaños y outputs sin accuracy.

    Returns:
        dict con las siguientes claves:
            'float_accuracy'   — float, 0.0–1.0 (o None si no hay labels)
            'int8_accuracy'    — float, 0.0–1.0 (o None si no hay labels)
            'delta'            — float, diferencia absoluta de accuracy
            'delta_pct'        — float, delta * 100 (para comparar contra el 5% del spec)
            'size_float_kb'    — float, tamaño del modelo Float32 en KB
            'size_int8_kb'     — float, tamaño del modelo INT8 en KB
            'size_ratio'       — float, cuántas veces más grande es el Float32
            'spec_ok'          — bool, True si delta < 5%
    """
    import tensorflow as tf

    model_float_path = Path(model_float_path)
    model_int8_path = Path(model_int8_path)

    def _run_tflite(tflite_path: Path, data: np.ndarray) -> np.ndarray:
        """Ejecuta inferencia con TFLite Interpreter y retorna outputs."""
        interpreter = tf.lite.Interpreter(model_path=str(tflite_path))
        interpreter.allocate_tensors()

        input_details = interpreter.get_input_details()
        output_details = interpreter.get_output_details()

        input_scale = input_details[0].get("quantization", (1.0, 0))[0]
        input_zero_point = input_details[0].get("quantization", (1.0, 0))[1]
        input_dtype = input_details[0]["dtype"]

        all_outputs = []
        for sample in data:
            inp = sample[np.newaxis, :]  # add batch dim

            # Cuantizar input si el modelo es INT8
            if input_dtype == np.int8:
                inp = (inp / input_scale + input_zero_point).astype(np.int8)
            else:
                inp = inp.astype(np.float32)

            interpreter.set_tensor(input_details[0]["index"], inp)
            interpreter.invoke()
            out = interpreter.get_tensor(output_details[0]["index"])

            # Descuantizar output INT8 si aplica
            if output_details[0]["dtype"] == np.int8:
                out_scale = output_details[0].get("quantization", (1.0, 0))[0]
                out_zp = output_details[0].get("quantization", (1.0, 0))[1]
                out = (out.astype(np.float32) - out_zp) * out_scale

            all_outputs.append(out[0])

        return np.array(all_outputs)

    outputs_float = _run_tflite(model_float_path, test_data)
    outputs_int8 = _run_tflite(model_int8_path, test_data)

    size_float_kb = model_float_path.stat().st_size / 1024
    size_int8_kb = model_int8_path.stat().st_size / 1024
    size_ratio = size_float_kb / size_int8_kb if size_int8_kb > 0 else float("inf")

    result: dict[str, float] = {
        "float_accuracy": None,  # type: ignore[dict-item]
        "int8_accuracy": None,  # type: ignore[dict-item]
        "delta": None,  # type: ignore[dict-item]
        "delta_pct": None,  # type: ignore[dict-item]
        "size_float_kb": round(size_float_kb, 2),
        "size_int8_kb": round(size_int8_kb, 2),
        "size_ratio": round(size_ratio, 2),
        "spec_ok": True,  # type: ignore[dict-item]
    }

    if test_labels is not None:
        preds_float = np.argmax(outputs_float, axis=-1)
        preds_int8 = np.argmax(outputs_int8, axis=-1)

        float_acc = float(np.mean(preds_float == test_labels))
        int8_acc = float(np.mean(preds_int8 == test_labels))
        delta = abs(float_acc - int8_acc)

        result["float_accuracy"] = round(float_acc, 4)
        result["int8_accuracy"] = round(int8_acc, 4)
        result["delta"] = round(delta, 4)
        result["delta_pct"] = round(delta * 100, 2)
        result["spec_ok"] = delta < 0.05  # type: ignore[assignment]

        status = "OK" if result["spec_ok"] else "FAIL — supera el 5% del spec"
        print(f"Float32 accuracy:  {float_acc:.2%}")
        print(f"INT8 accuracy:     {int8_acc:.2%}")
        print(f"Delta:             {delta:.2%}  [{status}]")

    print(f"Tamano Float32:   {size_float_kb:.1f} KB")
    print(f"Tamano INT8:      {size_int8_kb:.1f} KB  (ratio {size_ratio:.1f}x)")

    return result


# ---------------------------------------------------------------------------
# Export a C array para firmware Teensy
# ---------------------------------------------------------------------------


def tflite_to_c_array(tflite_path: str | Path, var_name: str) -> str:
    """
    Genera el contenido de un header C con el modelo TFLite como array de bytes.

    El output se incluye directamente en el firmware Teensy como:
        #include "model_data.h"
        const unsigned char g_model_data[] = { 0x1c, 0x00, ... };

    TFLite Micro necesita el modelo en flash (no RAM). El compilador lo pone
    automáticamente en .rodata si se declara const. Para PROGMEM en AVR usaría
    PROGMEM, pero en Teensy 4.1 (Cortex-M7) const en flash es suficiente.

    Equivalente al comando:
        xxd -i model.tflite > model_data.h

    pero portátil (no requiere xxd instalado) y con formato controlado.

    Args:
        tflite_path: Ruta al .tflite (Float32 o INT8).
        var_name:    Nombre de la variable C. Ej: "g_key_detector_model".

    Returns:
        String con el contenido completo del .h file.

    Ejemplo de uso:
        content = tflite_to_c_array("model_int8.tflite", "g_key_detector_model")
        Path("apps/firmware-teensy/src/ml/key_detector_data.h").write_text(content)

    Ejemplo de output:
        // Auto-generado por tflite_utils.py — NO editar manualmente.
        // Modelo: model_int8.tflite — 11264 bytes (11.0 KB)
        #pragma once
        #include <cstdint>

        alignas(8) const uint8_t g_key_detector_model[] = {
          0x1c, 0x00, 0x00, 0x00, 0x54, 0x46, 0x4c, 0x33, ...
        };
        const uint32_t g_key_detector_model_len = 11264;
    """
    tflite_path = Path(tflite_path)
    data = tflite_path.read_bytes()
    size_kb = len(data) / 1024

    # Formatear bytes en filas de 12, alineadas para legibilidad
    hex_bytes = [f"0x{b:02x}" for b in data]
    rows = [hex_bytes[i : i + 12] for i in range(0, len(hex_bytes), 12)]
    array_body = ",\n  ".join(", ".join(row) for row in rows)

    header = f"""\
// Auto-generado por tflite_utils.py — NO editar manualmente.
// Regenerar con: uv run python utils/tflite_utils.py {tflite_path.name}
// Modelo: {tflite_path.name} — {len(data)} bytes ({size_kb:.1f} KB)
// Budget spec: flash ≤500KB total (04-ai-architecture.md §1.2)
#pragma once
#include <cstdint>

// alignas(8): TFLite Micro requiere alineación de 8 bytes para el flatbuffer.
alignas(8) const uint8_t {var_name}[] = {{
  {array_body}
}};
const uint32_t {var_name}_len = {len(data)};
"""
    return header


# ---------------------------------------------------------------------------
# CLI de utilidad (uso directo)
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(
        description="Convierte un .tflite a C array header para firmware Teensy."
    )
    parser.add_argument("tflite_path", help="Ruta al archivo .tflite")
    parser.add_argument(
        "--var-name",
        default=None,
        help="Nombre de la variable C. Default: derivado del nombre del archivo.",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="Ruta de salida del .h. Default: mismo directorio que el .tflite.",
    )
    args = parser.parse_args()

    tflite_path = Path(args.tflite_path)
    var_name = args.var_name or ("g_" + tflite_path.stem.replace("-", "_"))
    output_path = Path(args.output) if args.output else tflite_path.with_suffix(".h")

    content = tflite_to_c_array(tflite_path, var_name)
    output_path.write_text(content)
    print(f"Header generado: {output_path}")
    print(f"Variable:        {var_name}[]")
    print(f"Longitud:        {tflite_path.stat().st_size} bytes")
