"""
model.py — Arquitectura del modelo Chord Recognizer.

Modelo: Dense 3-layer classifier con BatchNormalization
Input:  pitch class histogram de 12 valores (float32, suma ≈ 1.0)
Output: 61 clases (60 acordes + 1 clase N)
Target: <80KB en INT8, <8ms inference en Teensy 4.1 @ 600MHz

Por qué Dense y no CNN/RNN:
  Mismo razonamiento que key detector: el pitch class histogram es una
  representación agregada (invariante de octava e inversión). No hay dimensión
  espacial o temporal que explotar. Dense captura las correlaciones entre
  pitch classes que distinguen, por ejemplo, C_maj (bins 0,4,7) de C_m7 (bins 0,3,7,10).

Por qué BatchNorm aquí y no en key detector:
  Con 61 clases y alta correlación entre clases similares (C_maj y C_maj7 difieren
  en 1 nota), el gradiente de diferentes clases interfiere durante training.
  BatchNorm estabiliza la distribución de activaciones, logrando convergencia
  2-3x más rápida. Ver apps/docs/sprints/24-chord-recognizer.md §3 para teoría completa.

  Importante: en TFLite INT8, BatchNorm se FUSIONA con la capa Dense precedente
  como operación de scale+bias — no agrega latencia de inferencia en Teensy.

Conteo de parámetros (float32):
  Dense(128):  12×128 + 128   =  1792
  BatchNorm:   128×4           =   512   (gamma, beta, moving_mean, moving_var)
  Dense(64):   128×64 + 64    =  8256
  BatchNorm:   64×4            =   256
  Dense(61):   64×61 + 61     =  3966
  TOTAL:                        14782 parámetros

  En INT8 (solo pesos de Dense, BatchNorm fusionado):
    Dense weights en int8: (12×128 + 128×64 + 64×61) = ~14KB de pesos
    Overhead TFLite + flatbuffer header: ~4KB
    Total estimado: ~18KB — bien dentro del target de 80KB

Cross-ref: apps/docs/04-ai-architecture.md §1 (Chord Recognition feature)
           apps/docs/sprints/24-chord-recognizer.md §3 (BatchNorm), §5 (comparativa)
"""

from __future__ import annotations

import numpy as np

# Número de clases: 60 acordes + 1 clase N
NUM_CLASSES = 61


def build_chord_recognizer(input_dim: int = 12, num_classes: int = NUM_CLASSES):
    """
    Construye el modelo Keras para chord recognition.

    Arquitectura:
        Input(12)
          → Dense(128, relu) → BatchNorm → Dropout(0.3)
          → Dense(64, relu)  → BatchNorm → Dropout(0.2)
          → Dense(61, softmax)

    Por qué 128 en la primera capa:
        Con 61 clases y inputs de 12 dimensiones, necesitamos suficientes
        "detectores de patrón" para separar clases muy similares.
        Ejemplo: C_maj (bins 0,4,7) vs C_maj7 (bins 0,4,7,11) — la primera
        capa necesita un detector específico para el bin 11 (B) que aparece
        solo en maj7. Con 128 neuronas hay margen para aprender este y los
        demás patrones diferenciadores.

    Por qué BatchNorm después de Dense (no antes):
        El orden Dense → BatchNorm → Activation es el estándar de la literatura
        moderna (He et al., 2016 — "Identity Mappings in Deep Residual Networks").
        Pero para TFLite: el converter puede fusionar BN con la Dense precedente
        solo en este orden. Si BN va antes de Dense, no puede fusionarse y agrega
        latencia en Teensy.

        En nuestra arquitectura: Dense(relu) ya tiene la activación fusionada.
        BatchNorm se coloca ANTES de Dropout para que normalice la distribución
        antes de aplicar el dropout — evita que el dropout interactúe con
        distribuciones mal escaladas.

        Implementación: usamos la capa Dense sin activación explícita + BN + ReLU
        para permitir la fusión en TFLite. El converter reconoce el patrón
        Dense → BN → ReLU como fusionable a un solo op.

    Por qué Dropout(0.3) y Dropout(0.2):
        La primera capa (128 neuronas) tiene más capacidad y más riesgo de
        memorizar el ruido del histograma → mayor dropout.
        La segunda capa (64 neuronas) ya aprendió representaciones más abstractas
        → dropout más suave para no perder esas representaciones.

    Args:
        input_dim:   Dimensión de entrada. 12 para pitch class histogram.
        num_classes: Número de clases. 61 (60 acordes + N).

    Returns:
        tf.keras.Model compilado.
        Loss: SparseCategoricalCrossentropy(from_logits=False).
        Optimizer: Adam lr=0.001 con clipnorm=1.0 para estabilidad con BN.
        Metrics: ['accuracy'].
    """
    import tensorflow as tf

    model = tf.keras.Sequential(
        [
            tf.keras.layers.Input(shape=(input_dim,), name="pitch_histogram"),

            # --- Bloque 1: Dense(128) + BN + ReLU + Dropout ---
            # Dense SIN activación: la activación la aplica la capa ReLU explícita
            # después de BatchNorm, para permitir fusión en TFLite.
            tf.keras.layers.Dense(128, activation=None, name="dense_1"),
            tf.keras.layers.BatchNormalization(
                momentum=0.99,  # default 0.99 — bueno para batches de 512
                epsilon=1e-3,   # default de Keras — estabilidad numérica
                name="bn_1",
            ),
            tf.keras.layers.ReLU(name="relu_1"),
            tf.keras.layers.Dropout(0.3, name="dropout_1"),

            # --- Bloque 2: Dense(64) + BN + ReLU + Dropout ---
            tf.keras.layers.Dense(64, activation=None, name="dense_2"),
            tf.keras.layers.BatchNormalization(
                momentum=0.99,
                epsilon=1e-3,
                name="bn_2",
            ),
            tf.keras.layers.ReLU(name="relu_2"),
            tf.keras.layers.Dropout(0.2, name="dropout_2"),

            # --- Capa de salida: Dense(61) + Softmax ---
            # Softmax convierte los 61 logits en probabilidades que suman 1.0.
            tf.keras.layers.Dense(num_classes, activation="softmax", name="output"),
        ],
        name="chord_recognizer",
    )

    # clipnorm=1.0: clip de gradiente para estabilizar el entrenamiento con BN.
    # Sin clip, los gradientes de clases muy similares (C_maj vs C_maj7) pueden
    # generar explosiones de gradiente en los primeros epochs.
    model.compile(
        optimizer=tf.keras.optimizers.Adam(
            learning_rate=0.001,
            clipnorm=1.0,
        ),
        loss=tf.keras.losses.SparseCategoricalCrossentropy(),
        metrics=["accuracy"],
    )

    return model


def train_chord_recognizer(
    X_train: np.ndarray,
    y_train: np.ndarray,
    X_val: np.ndarray,
    y_val: np.ndarray,
    epochs: int = 80,
    batch_size: int = 512,
) -> tuple:
    """
    Entrena el modelo chord recognizer.

    Callbacks:
      EarlyStopping(patience=15): con 61 clases, el modelo necesita más tiempo
      para converger que el key detector (24 clases). patience=15 es más generoso.

      ReduceLROnPlateau(patience=7): reduce LR a la mitad si val_accuracy se
      estanca por 7 epochs. Ayuda a salir de plateaus donde el modelo está
      aprendiendo sutilezas entre clases similares (ej: C_7 vs C_maj7).

    Args:
        X_train:    Shape (N, 12), float32.
        y_train:    Shape (N,), int32, valores [0, 60].
        X_val:      Shape (M, 12), float32.
        y_val:      Shape (M,), int32.
        epochs:     Máximo de epochs. EarlyStopping puede terminar antes.
        batch_size: 512 — adecuado para dataset de 15K muestras. Batch grande
                    ayuda a BatchNorm a estimar estadísticas estables por batch.

    Returns:
        Tuple (model, history_dict):
          - model: tf.keras.Model con los mejores pesos (restore_best_weights=True)
          - history_dict: métricas por epoch
    """
    import tensorflow as tf

    model = build_chord_recognizer()

    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor="val_accuracy",
            patience=15,
            restore_best_weights=True,
            verbose=1,
        ),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_accuracy",
            factor=0.5,
            patience=7,
            min_lr=1e-5,
            verbose=1,
        ),
    ]

    history = model.fit(
        X_train,
        y_train,
        validation_data=(X_val, y_val),
        epochs=epochs,
        batch_size=batch_size,
        callbacks=callbacks,
        verbose=1,
    )

    return model, history.history
