"""
midi_utils.py — Funciones utilitarias para preprocesamiento de archivos MIDI.

Compartidas entre todos los modelos Layer 1 del pipeline TinyML de GrooveForge Brain.

Conceptos clave:
  - Pitch class: nota mod 12. C=0, C#=1, ..., B=11. Invariante de octava.
  - Pitch class histogram: vector (12,) que dice qué fracción del tiempo
    estuvo activa cada clase de nota. Es la feature principal para key detection.
  - Transposición: augmentación de datos que traslada todas las notas N semitonos.
    Genera 12x más datos de entrenamiento a costo casi cero.

Referencias:
  - Krumhansl & Schmuckler (1990) — Key-finding algorithm basado en perfiles tonales.
    El paper original que motivó el uso de pitch class histogram para key detection.
  - Raffel et al. (2016) — Lakh MIDI Dataset. https://colinraffel.com/projects/lmd/
  - pretty_midi docs: https://pretty-midi.readthedocs.io/

Cross-ref: apps/docs/04-ai-architecture.md §1.5
"""

from __future__ import annotations

import copy
from pathlib import Path
from typing import Any

import numpy as np
import pretty_midi


# ---------------------------------------------------------------------------
# Carga
# ---------------------------------------------------------------------------


def load_midi(path: str | Path) -> pretty_midi.PrettyMIDI:
    """
    Carga un archivo MIDI desde disco y retorna un objeto PrettyMIDI.

    Args:
        path: Ruta al archivo .mid o .midi.

    Returns:
        Objeto PrettyMIDI parseado.

    Raises:
        FileNotFoundError: Si el archivo no existe.
        ValueError: Si el archivo no es un MIDI válido.
        Exception: Propaga errores de pretty_midi con mensaje claro.

    Nota: pretty_midi lanza Exception (no un subtipo específico) para archivos
    corruptos. Aquí lo envolvemos en ValueError con contexto.
    """
    path = Path(path)
    if not path.exists():
        raise FileNotFoundError(f"Archivo MIDI no encontrado: {path}")
    if path.suffix.lower() not in {".mid", ".midi"}:
        raise ValueError(
            f"Extension no reconocida: '{path.suffix}'. Se esperaba .mid o .midi."
        )

    try:
        return pretty_midi.PrettyMIDI(str(path))
    except Exception as exc:
        raise ValueError(f"No se pudo parsear el MIDI '{path}': {exc}") from exc


# ---------------------------------------------------------------------------
# Feature extraction
# ---------------------------------------------------------------------------


def get_pitch_class_histogram(
    midi: pretty_midi.PrettyMIDI,
    normalize: bool = True,
    use_duration: bool = True,
) -> np.ndarray:
    """
    Calcula el histograma de pitch class de un archivo MIDI.

    El histograma de pitch class es el feature de entrada principal para
    key detection (Scale Lock). Representa qué notas aparecen más en la pieza,
    independientemente de la octava.

    Pitch class mapping:
        0=C, 1=C#/Db, 2=D, 3=D#/Eb, 4=E, 5=F,
        6=F#/Gb, 7=G, 8=G#/Ab, 9=A, 10=A#/Bb, 11=B

    Args:
        midi: Objeto PrettyMIDI ya cargado.
        normalize: Si True, el resultado suma 1.0 (distribución de probabilidad).
                   Si False, retorna duraciones brutas en segundos.
        use_duration: Si True, pondera cada nota por su duración en segundos
                      (más preciso musicalmente). Si False, cuenta notas.

    Returns:
        np.ndarray de shape (12,), dtype float64.
        Si normalize=True: suma exactamente 1.0 (excepto si no hay notas → zeros).

    Ejemplo:
        Un archivo en C mayor tendrá picos en indices 0 (C), 4 (E), 7 (G).
        Un archivo en A menor tendrá picos en 9 (A), 0 (C), 4 (E).
    """
    histogram = np.zeros(12, dtype=np.float64)

    for instrument in midi.instruments:
        if instrument.is_drum:
            # Los drums no tienen pitch tonal — ignorar para key detection.
            continue
        for note in instrument.notes:
            pitch_class = note.pitch % 12
            weight = (note.end - note.start) if use_duration else 1.0
            histogram[pitch_class] += weight

    total = histogram.sum()
    if normalize and total > 0:
        histogram /= total

    return histogram


def midi_to_note_sequence(
    midi: pretty_midi.PrettyMIDI,
    include_drums: bool = False,
) -> list[dict[str, Any]]:
    """
    Convierte un PrettyMIDI a una lista de eventos de nota ordenados por tiempo.

    Formato de cada evento:
        {
            'pitch':    int,    # 0-127 (MIDI note number)
            'pitch_class': int, # 0-11 (pitch mod 12)
            'octave':   int,    # 0-9
            'start':    float,  # segundos desde el inicio
            'end':      float,  # segundos
            'duration': float,  # end - start, en segundos
            'velocity': int,    # 0-127
            'instrument': str,  # nombre del instrumento
            'is_drum':  bool,
        }

    Args:
        midi: Objeto PrettyMIDI.
        include_drums: Si False (default), omite pistas de drums.

    Returns:
        Lista de dicts ordenada por 'start'.
    """
    notes: list[dict[str, Any]] = []

    for instrument in midi.instruments:
        if instrument.is_drum and not include_drums:
            continue
        for note in instrument.notes:
            notes.append(
                {
                    "pitch": note.pitch,
                    "pitch_class": note.pitch % 12,
                    "octave": note.pitch // 12,
                    "start": note.start,
                    "end": note.end,
                    "duration": note.end - note.start,
                    "velocity": note.velocity,
                    "instrument": instrument.name,
                    "is_drum": instrument.is_drum,
                }
            )

    return sorted(notes, key=lambda n: n["start"])


# ---------------------------------------------------------------------------
# Data augmentation
# ---------------------------------------------------------------------------


def transpose_midi(
    midi: pretty_midi.PrettyMIDI,
    semitones: int,
) -> pretty_midi.PrettyMIDI:
    """
    Transpone todas las notas del MIDI N semitonos hacia arriba o abajo.

    Usado para data augmentation: un archivo MIDI se puede transponer a las
    12 tonalidades posibles, multiplicando el dataset por 12x sin nueva grabación.

    Los eventos de control (tempo, time signature, program changes) se preservan.
    Las notas fuera del rango MIDI válido (0-127) después de la transposición
    se descartan silenciosamente.

    Args:
        midi: Objeto PrettyMIDI original. NO se modifica — se retorna una copia.
        semitones: Número de semitonos. Positivo = hacia arriba, negativo = abajo.
                   Rango típico: -6 a +5 (para las 12 tonalidades sin duplicar).

    Returns:
        Nuevo PrettyMIDI con las notas transpuestas.

    Ejemplo:
        # Generar las 12 tonalidades de un archivo
        for i in range(12):
            transposed = transpose_midi(midi, i)
    """
    # Deep copy para no mutar el objeto original
    transposed = copy.deepcopy(midi)

    for instrument in transposed.instruments:
        if instrument.is_drum:
            # Drums no tienen pitch tonal — no transponer.
            continue
        kept_notes = []
        for note in instrument.notes:
            new_pitch = note.pitch + semitones
            if 0 <= new_pitch <= 127:
                note.pitch = new_pitch
                kept_notes.append(note)
            # Notas fuera de rango: se descartan
        instrument.notes = kept_notes

    return transposed
