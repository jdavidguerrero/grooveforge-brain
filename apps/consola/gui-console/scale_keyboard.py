"""Pure in-key layout. Row zero is the physical top row of the M4."""
from dataclasses import dataclass

ROOTS = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')
SCALES = {'Minor': (0, 2, 3, 5, 7, 8, 10), 'Major': (0, 2, 4, 5, 7, 9, 11),
          'Dorian': (0, 2, 3, 5, 7, 9, 10), 'Pentatonic': (0, 3, 5, 7, 10),
          'Chromatic': tuple(range(12))}


@dataclass(frozen=True)
class ScaleKeyboard:
    root: int = 9
    scale: str = 'Minor'
    octave: int = 3

    def __post_init__(self):
        if not 0 <= self.root < 12 or self.scale not in SCALES or not 0 <= self.octave <= 7:
            raise ValueError('Invalid root, scale or octave')

    def degree(self, index):
        if not 0 <= index < 32:
            raise ValueError('Pad index must be 0..31')
        row, column = divmod(index, 8)
        return column + (3-row)*3

    def note(self, index):
        degrees = SCALES[self.scale]
        octave, degree = divmod(self.degree(index), len(degrees))
        note = 12*(self.octave+1) + self.root + 12*octave + degrees[degree]
        return note if note <= 127 else None

    def is_root(self, index):
        return self.degree(index) % len(SCALES[self.scale]) == 0
