"""
test_midi_utils.py — Tests unitarios para utils/midi_utils.py

Principios de testing aplicados aquí:
  - Sin dependencia de red ni de archivos del dataset real.
  - Los fixtures se generan en memoria con pretty_midi.
  - Validamos comportamiento observable (sumas, shapes, duraciones),
    no la implementación interna.

Correr con:
    uv run pytest tests/ -v
    uv run pytest tests/test_midi_utils.py -v
"""

from __future__ import annotations

import struct
from pathlib import Path

import numpy as np
import pretty_midi
import pytest

# Importar las funciones bajo test
from utils.midi_utils import (
    get_pitch_class_histogram,
    load_midi,
    midi_to_note_sequence,
    transpose_midi,
)


# ---------------------------------------------------------------------------
# Fixtures — archivos MIDI generados en memoria (sin red, sin disco)
# ---------------------------------------------------------------------------


def _minimal_midi_bytes() -> bytes:
    """
    Genera un MIDI Type 0 mínimo con dos notas: C4 (1 beat) y E4 (1 beat).
    Tempo: 120 BPM. Resolution: 480 ticks/beat.

    Este es el mismo helper que download_lakh.py para consistencia.
    """
    def var_len(n: int) -> bytes:
        data = [n & 0x7F]
        n >>= 7
        while n:
            data.append((n & 0x7F) | 0x80)
            n >>= 7
        return bytes(reversed(data))

    header = b"MThd" + struct.pack(">IHHH", 6, 0, 1, 480)
    events = (
        var_len(0) + b"\xff\x51\x03\x07\xa1\x20"   # tempo: 500000 us = 120 BPM
        + var_len(0) + b"\x90\x3c\x64"              # note_on C4 (pitch=60) vel=100
        + var_len(480) + b"\x80\x3c\x00"            # note_off C4 (1 beat = 0.5s @ 120 BPM)
        + var_len(0) + b"\x90\x40\x64"              # note_on E4 (pitch=64) vel=100
        + var_len(480) + b"\x80\x40\x00"            # note_off E4 (1 beat)
        + var_len(0) + b"\xff\x2f\x00"              # end of track
    )
    track = b"MTrk" + struct.pack(">I", len(events)) + events
    return header + track


def _make_pretty_midi_fixture(notes: list[tuple[int, float, float]]) -> pretty_midi.PrettyMIDI:
    """
    Crea un PrettyMIDI con un instrumento piano y las notas especificadas.

    Args:
        notes: Lista de (pitch, start_sec, end_sec).

    Returns:
        PrettyMIDI con las notas dadas.
    """
    midi = pretty_midi.PrettyMIDI(initial_tempo=120.0)
    piano = pretty_midi.Instrument(program=0, name="Piano")
    for pitch, start, end in notes:
        piano.notes.append(
            pretty_midi.Note(velocity=80, pitch=pitch, start=start, end=end)
        )
    midi.instruments.append(piano)
    return midi


@pytest.fixture
def midi_c_major_triad() -> pretty_midi.PrettyMIDI:
    """
    MIDI con acorde C mayor: C4 (pitch=60), E4 (pitch=64), G4 (pitch=67).
    Cada nota dura 1.0 segundo, idéntica duración — histograma debe ser 1/3 cada una.
    """
    return _make_pretty_midi_fixture([
        (60, 0.0, 1.0),  # C4 → pitch_class 0
        (64, 0.0, 1.0),  # E4 → pitch_class 4
        (67, 0.0, 1.0),  # G4 → pitch_class 7
    ])


@pytest.fixture
def midi_single_note_repeated() -> pretty_midi.PrettyMIDI:
    """
    MIDI con solo notas C en diferentes octavas (C3, C4, C5).
    El histograma debe tener todo el peso en pitch_class 0.
    """
    return _make_pretty_midi_fixture([
        (48, 0.0, 1.0),   # C3 → pitch_class 0
        (60, 1.0, 2.0),   # C4 → pitch_class 0
        (72, 2.0, 3.0),   # C5 → pitch_class 0
    ])


@pytest.fixture
def tmp_midi_file(tmp_path: Path) -> Path:
    """
    Escribe el MIDI mínimo en un archivo temporal y retorna su ruta.
    Usado para tests de load_midi().
    """
    midi_path = tmp_path / "test.mid"
    midi_path.write_bytes(_minimal_midi_bytes())
    return midi_path


# ---------------------------------------------------------------------------
# Tests: get_pitch_class_histogram
# ---------------------------------------------------------------------------


def test_pitch_class_histogram_sums_to_one(midi_c_major_triad: pretty_midi.PrettyMIDI) -> None:
    """
    El histograma normalizado debe sumar exactamente 1.0.

    Esto es la propiedad más fundamental: el histograma es una distribución
    de probabilidad sobre los 12 pitch classes. Si no suma 1.0, hay un bug
    de normalización que contaminaría el entrenamiento del modelo.
    """
    hist = get_pitch_class_histogram(midi_c_major_triad, normalize=True)

    assert hist.shape == (12,), f"Shape esperado (12,), got {hist.shape}"
    assert hist.dtype == np.float64, f"Dtype esperado float64, got {hist.dtype}"
    assert np.all(hist >= 0), "El histograma no puede tener valores negativos"
    np.testing.assert_almost_equal(
        hist.sum(),
        1.0,
        decimal=10,
        err_msg="El histograma normalizado debe sumar exactamente 1.0",
    )


def test_pitch_class_histogram_correct_peaks(midi_c_major_triad: pretty_midi.PrettyMIDI) -> None:
    """
    Con C mayor (C, E, G), los picos deben estar en indices 0, 4 y 7.
    Cada nota dura lo mismo, entonces cada pitch class vale exactamente 1/3.
    """
    hist = get_pitch_class_histogram(midi_c_major_triad, normalize=True)

    expected_nonzero = {0, 4, 7}
    actual_nonzero = set(np.where(hist > 0)[0].tolist())

    assert actual_nonzero == expected_nonzero, (
        f"Pitch classes activos: {actual_nonzero}, esperados: {expected_nonzero}"
    )

    # Cada nota dura 1 segundo, tres notas iguales → cada una es 1/3
    np.testing.assert_almost_equal(hist[0], 1 / 3, decimal=5)
    np.testing.assert_almost_equal(hist[4], 1 / 3, decimal=5)
    np.testing.assert_almost_equal(hist[7], 1 / 3, decimal=5)


def test_pitch_class_histogram_octave_invariant(
    midi_single_note_repeated: pretty_midi.PrettyMIDI,
) -> None:
    """
    C en diferentes octavas (C3, C4, C5) debe colapsarse al mismo pitch_class=0.
    Todo el peso debe estar en el índice 0.
    """
    hist = get_pitch_class_histogram(midi_single_note_repeated, normalize=True)

    np.testing.assert_almost_equal(hist[0], 1.0, decimal=5)
    assert hist[1:].sum() < 1e-10, "No debería haber actividad en otros pitch classes"


def test_pitch_class_histogram_unnormalized(midi_c_major_triad: pretty_midi.PrettyMIDI) -> None:
    """
    Con normalize=False, el histograma retorna duraciones brutas en segundos.
    Cada nota dura 1.0s, así que cada pitch class activo debe valer ~1.0s.
    """
    hist = get_pitch_class_histogram(midi_c_major_triad, normalize=False)
    np.testing.assert_almost_equal(hist[0], 1.0, decimal=3)
    np.testing.assert_almost_equal(hist[4], 1.0, decimal=3)
    np.testing.assert_almost_equal(hist[7], 1.0, decimal=3)


def test_pitch_class_histogram_empty_midi() -> None:
    """
    Un MIDI sin notas debe retornar un vector de ceros, no crashear.
    """
    empty_midi = pretty_midi.PrettyMIDI(initial_tempo=120.0)
    hist = get_pitch_class_histogram(empty_midi, normalize=True)
    assert hist.shape == (12,)
    np.testing.assert_array_equal(hist, np.zeros(12))


# ---------------------------------------------------------------------------
# Tests: transpose_midi
# ---------------------------------------------------------------------------


def test_transpose_preserves_duration(midi_c_major_triad: pretty_midi.PrettyMIDI) -> None:
    """
    Transponer no debe cambiar las duraciones de las notas.

    La transposición solo afecta el pitch, no el timing. Si las duraciones
    cambian, hay un bug en el deep copy o en la lógica de transposición.
    """
    original_notes = [
        (n.pitch, n.start, n.end)
        for inst in midi_c_major_triad.instruments
        for n in inst.notes
    ]
    original_durations = [(end - start) for _, start, end in original_notes]

    transposed = transpose_midi(midi_c_major_triad, semitones=5)

    transposed_notes = [
        (n.pitch, n.start, n.end)
        for inst in transposed.instruments
        for n in inst.notes
    ]
    transposed_durations = [(end - start) for _, start, end in transposed_notes]

    assert len(transposed_durations) == len(original_durations), (
        "La transposición no debe cambiar el número de notas (dentro de rango)"
    )
    for orig_dur, trans_dur in zip(original_durations, transposed_durations):
        np.testing.assert_almost_equal(
            orig_dur, trans_dur, decimal=6,
            err_msg="La duración no debe cambiar con la transposición"
        )


def test_transpose_correct_pitch_shift(midi_c_major_triad: pretty_midi.PrettyMIDI) -> None:
    """
    Transposición de +7 semitonos debe mover cada nota exactamente 7 semitonos.
    C4 (60) → G4 (67), E4 (64) → B4 (71), G4 (67) → D5 (74).
    """
    transposed = transpose_midi(midi_c_major_triad, semitones=7)

    original_pitches = sorted([
        n.pitch for inst in midi_c_major_triad.instruments for n in inst.notes
    ])
    transposed_pitches = sorted([
        n.pitch for inst in transposed.instruments for n in inst.notes
    ])

    expected_pitches = sorted([p + 7 for p in original_pitches])
    assert transposed_pitches == expected_pitches, (
        f"Pitches esperados: {expected_pitches}, got: {transposed_pitches}"
    )


def test_transpose_does_not_mutate_original(midi_c_major_triad: pretty_midi.PrettyMIDI) -> None:
    """
    transpose_midi() NO debe modificar el objeto original (usa deep copy).
    """
    original_pitches = [
        n.pitch for inst in midi_c_major_triad.instruments for n in inst.notes
    ]
    _ = transpose_midi(midi_c_major_triad, semitones=3)
    after_pitches = [
        n.pitch for inst in midi_c_major_triad.instruments for n in inst.notes
    ]
    assert original_pitches == after_pitches, (
        "transpose_midi() modificó el objeto original — debe retornar una copia"
    )


def test_transpose_drops_out_of_range_notes() -> None:
    """
    Notas que quedarían fuera del rango MIDI (0-127) después de transponer
    deben descartarse silenciosamente, no crashear.
    """
    # Nota en el extremo superior: B9 = pitch 119. Con +20 quedaría en 139 (inválido).
    midi = _make_pretty_midi_fixture([
        (119, 0.0, 1.0),  # B9 — válido
        (60, 0.0, 1.0),   # C4 — válido, quedará en 80 (válido)
    ])
    transposed = transpose_midi(midi, semitones=20)

    transposed_pitches = [
        n.pitch for inst in transposed.instruments for n in inst.notes
    ]
    # Solo C4+20=80 debe sobrevivir. B9+20=139 debe descartarse.
    assert transposed_pitches == [80], (
        f"Solo debe quedar la nota en rango. Got: {transposed_pitches}"
    )


def test_transpose_negative_semitones() -> None:
    """Transposición negativa: bajar 12 semitonos (una octava) debe funcionar."""
    midi = _make_pretty_midi_fixture([(60, 0.0, 1.0)])  # C4
    transposed = transpose_midi(midi, semitones=-12)
    pitches = [n.pitch for inst in transposed.instruments for n in inst.notes]
    assert pitches == [48], f"C4 (60) - 12 semitones = C3 (48). Got: {pitches}"


# ---------------------------------------------------------------------------
# Tests: load_midi
# ---------------------------------------------------------------------------


def test_load_midi_valid_file(tmp_midi_file: Path) -> None:
    """
    Un archivo MIDI válido debe cargarse sin excepción y retornar PrettyMIDI.
    """
    midi = load_midi(tmp_midi_file)
    assert isinstance(midi, pretty_midi.PrettyMIDI)


def test_load_invalid_midi_raises(tmp_path: Path) -> None:
    """
    Un archivo que no es MIDI debe lanzar ValueError con mensaje descriptivo.
    NO debe crashear con una excepción interna de pretty_midi sin contexto.
    """
    bad_file = tmp_path / "not_a_midi.mid"
    bad_file.write_bytes(b"This is not MIDI data at all. Just text.")

    with pytest.raises(ValueError, match="No se pudo parsear"):
        load_midi(bad_file)


def test_load_nonexistent_file_raises() -> None:
    """Un archivo que no existe debe lanzar FileNotFoundError."""
    with pytest.raises(FileNotFoundError):
        load_midi("/tmp/this_file_does_not_exist_grooveforge.mid")


def test_load_wrong_extension_raises(tmp_path: Path) -> None:
    """
    Un archivo con extensión no-.mid/.midi debe lanzar ValueError,
    incluso si el contenido fuera MIDI válido.
    """
    bad_ext = tmp_path / "model.tflite"
    bad_ext.write_bytes(b"irrelevant")
    with pytest.raises(ValueError, match="Extension no reconocida"):
        load_midi(bad_ext)


# ---------------------------------------------------------------------------
# Tests: midi_to_note_sequence
# ---------------------------------------------------------------------------


def test_note_sequence_structure(midi_c_major_triad: pretty_midi.PrettyMIDI) -> None:
    """
    midi_to_note_sequence() debe retornar dicts con las claves esperadas,
    ordenados por tiempo de inicio.
    """
    seq = midi_to_note_sequence(midi_c_major_triad)

    required_keys = {"pitch", "pitch_class", "octave", "start", "end", "duration", "velocity"}
    for event in seq:
        assert required_keys.issubset(event.keys()), (
            f"Faltan claves en el evento: {required_keys - event.keys()}"
        )
        assert event["duration"] > 0, "La duración debe ser positiva"
        assert 0 <= event["pitch"] <= 127
        assert 0 <= event["pitch_class"] <= 11
        assert event["pitch_class"] == event["pitch"] % 12


def test_note_sequence_sorted_by_start(midi_c_major_triad: pretty_midi.PrettyMIDI) -> None:
    """La secuencia debe estar ordenada por tiempo de inicio (start)."""
    seq = midi_to_note_sequence(midi_c_major_triad)
    starts = [e["start"] for e in seq]
    assert starts == sorted(starts), "La secuencia no está ordenada por tiempo de inicio"
