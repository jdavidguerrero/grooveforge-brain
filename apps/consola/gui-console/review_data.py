"""Deterministic snapshots transcribed from the two design exports, never I/O adapters."""
import copy
import json
import re
from pathlib import Path
from PySide6.QtCore import QObject, Property
from models import SnapshotModel

SAMPLES = json.loads((Path(__file__).with_name('review_samples.json')).read_text())
LIST_NAMES = ['melodicPads', 'hybridEngines'] + sorted({k for data in SAMPLES.values() for k, value in data.items() if isinstance(value, list)})


class ReviewData(QObject):
    def __init__(self, mode, parent=None):
        super().__init__(parent)
        self.samples = copy.deepcopy(SAMPLES[mode])
        for name in ('brainArcs', 'hybArcs', 'devArcs'):
            for arc in self.samples.get(name, []):
                arc['amount'] = float(re.search(r'0deg ([\d.]+)deg', arc['bg'])[1]) / 270
        if mode == 's':
            self.samples['hybridEngines'] = self.samples['hybA'] + self.samples['hybB']
            self.samples['melodicPads'] = copy.deepcopy(self.samples['pads'])
            for pad in self.samples['melodicPads']:
                pad['bg'] = '#E37FB0' if pad['tag'] == 'ROOT' else '#4B2D3C' if pad['tag'] != 'PLAYING' else '#F2F2F2'
                pad['bd'] = pad['bg']
            for note in self.samples['rollNotes']:
                note.update(step=round(note['l'] * .64), length=round(note['w'] * .64),
                            pitch=round(24 - note['t'] * .25), velocity=104, micro=-4,
                            owner='engine' if note['c'] == '#7A6FE0' else 'you', selected=note['sh'] != 'none')
        self._models = {name: SnapshotModel([{'entry': entry} for entry in value], ['entry'], self)
                        for name, value in self.samples.items() if isinstance(value, list)}

    for name in LIST_NAMES:
        locals()[name] = Property(QObject, lambda self, n=name: self._models.get(n), constant=True)

    def rows(self, name):
        return [row['entry'] for row in self._models[name].rows]

    def update_entry(self, name, index, values):
        model = self._models[name]
        entry = dict(model._pending.get(index, {}).get('entry', model.rows[index]['entry']))
        entry.update(values)
        model.queue_update(index, {'entry': entry})
