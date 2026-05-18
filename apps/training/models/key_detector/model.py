"""
model.py — Arquitectura y entrenamiento del modelo Key Detector.

Modelo: Dense 3-layer classifier
Input:  pitch class histogram de 12 valores (float32, suma ≈ 1.0)
Output: 24 clases (12 tonalidades mayores + 12 menores)
Target: <4KB en INT8, <5ms inference en Teensy 4.1 @ 600MHz

Por qué Dense y no CNN/LSTM:
  - El pitch class histogram ya es una representación agregada (no temporal).
  - No hay dimensión espacial que una CNN deba explotar.
  - Dense con 3 capas captura las correlaciones no-lineales entre pitch classes
    que distinguen una tonalidad de otra, con <4K parámetros totales.
  - Un LSTM necesitaría una secuencia de histogramas — el Teensy enviará
    histogramas de ventanas de 2-4 segundos, no streams continuos.

Cross-ref: apps/docs/04-ai-architecture.md §1 (Scale Lock, Layer 1)
"""

from __future__ import annotations

import numpy as np


def build_key_detector(input_dim: int = 12, num_classes: int = 24):
    """
    Construye el modelo Keras para key detection.

    Arquitectura:
        Input(12) → Dense(128, relu) → Dropout(0.3) → Dense(64, relu) → Dropout(0.2) → Dense(24, softmax)

    Conteo de parámetros:
        Dense(128):  12×128 + 128   = 1664 params
        Dense(64):   128×64 + 64    = 8256 params
        Dense(24):   64×24 + 24     = 1560 params
        TOTAL:       11480 parámetros (≈11.2KB en float32, ≈2.8KB en int8)

    Por qué 128 neuronas en la primera capa (vs 64 en el spec original):
        El problema de distinguir 24 tonalidades desde 12 inputs es más difícil
        de lo que parece — los perfiles de tonalidades relacionadas (ej. C mayor y
        G mayor, o C mayor y A menor) comparten muchos pitch classes. La primera
        capa necesita aprender representaciones no-lineales suficientemente ricas
        como para separar estos patrones similares. 128 neuronas = 128 detectores
        de patrones de co-ocurrencia tonal, lo que da más margen para aprender
        estas sutilezas sin aumentar demasiado el footprint en flash.

        En int8: 11480 bytes ≈ 11.2KB de pesos. El modelo completo con overhead
        TFLite será ~15-20KB, bien dentro del budget de 500KB total de flash.

    Por qué dos Dropouts:
        Dropout(0.3) en la primera capa: la mayor fuente de varianza, necesita
        más regularización porque es la capa con más parámetros.
        Dropout(0.2) en la segunda capa: regularización más suave para no perder
        la representación que ya aprendió la primera capa.

    Por qué relu (no sigmoid/tanh):
        ReLU es el único op de activación que TFLite Micro soporta eficientemente
        vía CMSIS-NN en Cortex-M7. sigmoid y tanh requieren aproximaciones en int8
        que consumen más ciclos y tienen menor precisión post-quantización.

    Args:
        input_dim:   Dimensión de entrada. 12 para pitch class histogram.
        num_classes: Número de clases de salida. 24 (12 maj + 12 min).

    Returns:
        tf.keras.Model compilado, listo para entrenar.
        Loss: SparseCategoricalCrossentropy (labels enteras, no one-hot).
        Optimizer: Adam lr=0.001.
        Metrics: ['accuracy'].
    """
    import tensorflow as tf

    model = tf.keras.Sequential(
        [
            tf.keras.layers.Input(shape=(input_dim,)),
            tf.keras.layers.Dense(128, activation="relu"),
            tf.keras.layers.Dropout(0.3),
            tf.keras.layers.Dense(64, activation="relu"),
            tf.keras.layers.Dropout(0.2),
            tf.keras.layers.Dense(num_classes, activation="softmax"),
        ],
        name="key_detector",
    )

    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss=tf.keras.losses.SparseCategoricalCrossentropy(),
        metrics=["accuracy"],
    )

    return model


def train_key_detector(
    X_train: np.ndarray,
    y_train: np.ndarray,
    X_val: np.ndarray,
    y_val: np.ndarray,
    epochs: int = 150,
    batch_size: int = 512,
) -> tuple:
    """
    Entrena el modelo key detector.

    Usa EarlyStopping(patience=10) sobre val_accuracy para evitar overfitting.
    ReduceLROnPlateau(patience=5) reduce el learning rate si el val_accuracy
    se estanca — ayuda a convergencia final después del plateau inicial.

    Args:
        X_train:    Histogramas de entrenamiento. Shape (N, 12), float32.
        y_train:    Labels de entrenamiento. Shape (N,), int32, valores [0, 23].
        X_val:      Histogramas de validación. Shape (M, 12), float32.
        y_val:      Labels de validación. Shape (M,), int32.
        epochs:     Máximo de epochs. EarlyStopping puede terminar antes.
        batch_size: Tamaño de batch. 256 es un buen balance para dataset de 10K.

    Returns:
        Tuple (model, history_dict) donde:
          - model es el tf.keras.Model entrenado con los mejores pesos (best val_accuracy)
          - history_dict es el dict de métricas por epoch (keys: 'loss', 'accuracy',
            'val_loss', 'val_accuracy')
    """
    import tensorflow as tf

    model = build_key_detector()

    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor="val_accuracy",
            patience=20,
            restore_best_weights=True,  # retorna pesos del mejor epoch, no del último
            verbose=1,
        ),
        tf.keras.callbacks.ReduceLROnPlateau(
            monitor="val_accuracy",
            factor=0.5,
            patience=10,
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
