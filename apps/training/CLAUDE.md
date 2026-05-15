# Training (TinyML)

Pipeline Python para entrenar, cuantizar y validar los modelos TFLite Micro
que corren en el Teensy 4.1 (Layer 1 AI). Cross-ref: `apps/docs/04-ai-architecture.md`.

## Specs relevantes

- `apps/docs/04-ai-architecture.md` — Layer 1 spec completo: modelos, budgets, pipeline, métricas

## Stack

- Python 3.11+
- uv para package management
- TensorFlow 2.14+ / TFLite converter
- librosa (audio features), mido (MIDI parsing), music21 (music theory)
- pytest para tests de modelos

## Setup

```bash
cd apps/training
uv sync                          # instalar dependencias
```

## Comandos

```bash
# Training
uv run python models/key_detector/train.py
uv run python models/chord_recognizer/train.py
uv run python models/beat_tracker/train.py

# Quantización y export
uv run python models/key_detector/export.py  # genera .tflite + .h

# Tests
uv run pytest                                 # toda la suite
uv run pytest models/key_detector/            # modelo específico
uv run pytest -v --tb=short                   # verbose
```

## Estructura

```
models/
├── key_detector/          # Scale Lock feature (12KB, 4ms inference)
├── chord_recognizer/      # Chord Recognition (45KB, 8ms)
├── beat_tracker/          # Beat Follower (30KB, 6ms)
├── auto_harmonizer/       # Auto-Harmonization v1.1 (80KB, 12ms)
└── genre_fingerprint/     # Genre Fingerprint v1.2 (100KB, 18ms)
datasets/
├── download_lakh.py       # Lakh MIDI Dataset
├── download_maestro.py    # Maestro Dataset
└── download_mss.py        # Million Song Subset
notebooks/
└── exploration.ipynb      # Data exploration inicial
utils/
├── midi_features.py       # Feature extraction MIDI
├── quantize.py            # TFLite INT8 quantization pipeline
└── validate.py            # Accuracy + memory + latency validation
```

## Budgets (04-ai-architecture.md §1.2 + §8)

| Métrica | Target |
|---|---|
| Total models en flash | ≤500KB |
| Tensor arena RAM | ≤200KB |
| Accuracy por modelo | >85% |
| Accuracy delta (float→int8) | <5% |
| Inference latency | <20ms p99 |

## Agente recomendado

Invocar **ML Engineer** para diseño de modelos, cuantización y validación.
El output de este directorio (`.h` files con C arrays) va a `apps/firmware-teensy/src/ml/`.
