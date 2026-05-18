"""
model.py — Arquitectura del modelo Beat Follower.

Modelo: Dense 3-layer classifier
Input:  Histograma de IOIs de 32 bins (float32, suma ≈ 1.0)
Output: 32 clases de BPM (60, 63, 66, ..., 240 BPM)
Target: <30KB en INT8, <6ms inference en Teensy 4.1 @ 600MHz

Por qué Dense y no LSTM/CNN aquí:
  El histograma de IOIs YA es una representación temporal agregada.
  Los 32 bins acumulan la distribución de tiempos entre onsets de los
  últimos 31 eventos — la información temporal está comprimida en la forma
  del histograma, no en una secuencia que un LSTM deba procesar.

  Un LSTM sería el paso v2.0: alimentar una secuencia de histogramas
  con ventanas deslizantes. Para v1.0, Dense sobre el histograma es suficiente
  y cabe en el budget de flash (target 30KB INT8).

  Una CNN 1D podría explotar la estructura espacial del histograma (bins
  adyacentes están relacionados), pero añade complejidad de ops (Conv1D)
  y no está soportada tan eficientemente en TFLite Micro como FullyConnected.

Conteo de parámetros:
  Dense(64):  32×64 + 64 = 2112 params
  Dense(32):  64×32 + 32 = 2080 params
  Dense(32):  32×32 + 32 = 1056 params
  TOTAL:      5248 parámetros (≈5.1KB float32, ≈5.1KB int8 ≈ overhead mínimo)

  Con overhead de metadata TFLite: ~10-15KB en INT8.
  Bien dentro del target de 30KB para Beat Follower.

Por qué relu (no sigmoid/tanh):
  ReLU es el op de activación más eficiente en TFLite Micro + CMSIS-NN.
  En Cortex-M7, las instrucciones DSP aceleran FullyConnected + ReLU INT8
  a velocidades que hacen el target <6ms alcanzable con holgura.

Cross-ref: apps/docs/04-ai-architecture.md §1 (Beat Follower feature)
           apps/docs/sprints/25-beat-follower.md §5
"""

from __future__ import annotations

import numpy as np

from models.beat_follower.dataset import N_IOI_BINS, NUM_BPM_CLASSES


def build_beat_follower(
    input_dim: int = N_IOI_BINS,
    num_classes: int = NUM_BPM_CLASSES,
):
    """
    Construye el modelo Keras para beat following (BPM classification).

    Arquitectura:
        Input(32) → Dense(64, relu) → Dropout(0.2) → Dense(32, relu) → Dense(32, softmax)

    La primera capa (64 neuronas) aprende representaciones de patrones de IOI:
    "bins dominantes en 250-500ms" = señal de 120 BPM, "bins en 500-1000ms" = 60 BPM.

    La segunda capa (32 neuronas — mismo número que las clases de salida) actúa
    como un bottleneck que fuerza al modelo a construir una representación compacta
    de la "identidad del BPM" antes de clasificar.

    Dropout(0.2) en la primera capa: regularización suave. El dataset sintético
    con jitter ya provee variance natural, pero el dropout previene memorización
    de patrones de ruido específicos del dataset.

    Args:
        input_dim:   Dimensión de entrada. 32 para histograma de IOIs.
        num_classes: Número de clases de salida. 32 BPM classes.

    Returns:
        tf.keras.Model compilado, listo para entrenar.
        Loss: SparseCategoricalCrossentropy (labels enteros, no one-hot).
        Optimizer: Adam lr=0.001.
        Metrics: ['accuracy'].
    """
    import tensorflow as tf

    model = tf.keras.Sequential(
        [
            tf.keras.layers.Input(shape=(input_dim,)),
            tf.keras.layers.Dense(64, activation="relu"),
            tf.keras.layers.Dropout(0.2),
            tf.keras.layers.Dense(32, activation="relu"),
            tf.keras.layers.Dense(num_classes, activation="softmax"),
        ],
        name="beat_follower",
    )

    model.compile(
        optimizer=tf.keras.optimizers.Adam(learning_rate=0.001),
        loss=tf.keras.losses.SparseCategoricalCrossentropy(),
        metrics=["accuracy"],
    )

    return model


def train_beat_follower(
    X_train: np.ndarray,
    y_train: np.ndarray,
    X_val: np.ndarray,
    y_val: np.ndarray,
    epochs: int = 120,
    batch_size: int = 256,
):
    """
    Entrena el modelo beat follower.

    EarlyStopping(patience=20) sobre val_accuracy para evitar overfitting.
    ReduceLROnPlateau(patience=10) reduce learning rate cuando el val_accuracy
    se estanca — habitual en clasificación de 32 clases donde el plateau inicial
    suele durar ~30 epochs antes de convergencia final.

    Args:
        X_train:    Histogramas de entrenamiento. Shape (N, 32), float32.
        y_train:    Labels de entrenamiento. Shape (N,), int32, valores [0, 31].
        X_val:      Histogramas de validación. Shape (M, 32), float32.
        y_val:      Labels de validación. Shape (M,), int32.
        epochs:     Máximo de epochs. EarlyStopping puede terminar antes.
        batch_size: Tamaño de batch.

    Returns:
        Tuple (model, history_dict) donde:
          - model es el tf.keras.Model entrenado con los mejores pesos (best val_accuracy)
          - history_dict es el dict de métricas por epoch
    """
    import tensorflow as tf

    model = build_beat_follower()

    callbacks = [
        tf.keras.callbacks.EarlyStopping(
            monitor="val_accuracy",
            patience=20,
            restore_best_weights=True,
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
