"""Hardware-free phase-3 seam: FIFO → one GUI-thread transaction per render frame.

Accepts superficie.py dataclasses by class name/fields, or equivalent dictionaries.
Mappings are visual demo mappings, not a motor/IPC command specification.
"""
from collections import deque
from dataclasses import dataclass
from threading import Lock
from time import perf_counter_ns

from PySide6.QtCore import QObject, Property, Signal, Slot
from models import SnapshotModel


@dataclass(frozen=True)
class QueuedEvent:
    sequence: int
    enqueued_ns: int
    kind: str
    fields: dict


class SurfaceBatcher(QObject):
    batchChanged = Signal()
    wakeRequested = Signal()

    def __init__(self, state, parent=None):
        super().__init__(parent)
        self.state = state
        self._queue = deque()
        self._lock = Lock()
        self.enqueued = self.applied = self.batch_count = self.max_depth = 0
        self.kind_counts = {}
        self._serial = 0
        self.last_batch = ()
        self.on_batch = None
        self.models = state.findChildren(SnapshotModel)
        for model in self.models:
            model.frame_managed = True
            model._timer.stop()
        state._frame_timer.stop()

    serial = Property(int, lambda self: self._serial, notify=batchChanged)

    def enqueue(self, event, timestamp_ns=None):
        kind = event['type'] if isinstance(event, dict) else type(event).__name__
        fields = {k: v for k, v in event.items() if k != 'type'} if isinstance(event, dict) else vars(event).copy()
        if kind not in ('Pad', 'Fsr', 'Boton', 'Encoder', 'Jog', 'Fader', 'Expresion',
                        'Perilla', 'Mano', 'ManoPresencia', 'SelectorPad'):
            raise ValueError(f'Unsupported surface event: {kind}')
        with self._lock:
            was_empty = not self._queue
            self.enqueued += 1
            queued = QueuedEvent(self.enqueued, timestamp_ns or perf_counter_ns(), kind, fields)
            self._queue.append(queued)
            self.max_depth = max(self.max_depth, len(self._queue))
        if was_empty:
            self.wakeRequested.emit()
        return queued.sequence

    @property
    def pending(self):
        with self._lock:
            return len(self._queue)

    @staticmethod
    def value(model, row, role):
        return model._pending.get(row, {}).get(role, model.rows[row][role])

    def apply(self, event):
        s, f, kind = self.state, event.fields, event.kind
        index = f.get('indice', 0)
        drum_surface = getattr(self, 'drum_surface', None)
        if drum_surface and drum_surface.apply_frame(kind, f):
            return
        surface = getattr(self, 'synth_surface', None)
        if surface and surface.apply_frame(kind, f):
            return
        if kind == 'Encoder' and s.screenName.startswith('Synth') and hasattr(s, 'synth'):
            s._selected = index
            s._property_dirty = True
            s.synth.turn(index, f['delta'])
        elif kind == 'Boton' and s.screenName.startswith('Synth') and hasattr(s, 'synth') and f.get('nombre') == 'SHIFT':
            s.synth.setFine(f['presionado'])
        elif kind in ('Encoder', 'Jog'):
            index = index if kind == 'Encoder' else 0
            old = self.value(s.channels, index, 'level')
            level = max(0., min(1., old + f['delta'] * .01))
            s.channels.queue_update(index, {'level': level, 'meter': level * .96})
            s.cells.queue_update(index, {'level': level})
        elif kind in ('Fader', 'Expresion', 'Perilla'):
            if kind == 'Perilla':
                index = ('MASTER', 'CUE', 'IN').index(f['nombre'])
            level = max(0., min(1., f['valor']))
            s.groups.queue_update(index, {'level': level, 'meter': level})
            # Also changes LIVE content, so all control classes affect the visible test.
            s.cells.queue_update(index, {'level': level})
        elif kind in ('Pad', 'Fsr'):
            row = index if kind == 'Pad' else index + 4
            level = f['velocidad'] if f['presionado'] else 0.
            s.cells.queue_update(row, {'level': level})
            s.channels.queue_update(row % 8, {'meter': level})
        elif kind in ('Mano', 'ManoPresencia'):
            if kind == 'Mano':
                value = max(0., min(1., f['valor']))
                s.hands.queue_update(index, {'distance': round(4 + 26 * (1-value)), 'value': f'{value:.2f}'})
                s.cells.queue_update(index + 6, {'level': value})
                s.channels.queue_update(index + 6, {'meter': value})
            else:
                s.hands.queue_update(index, {'inRange': f['presente']})
                s.channels.queue_update(index + 6, {'peak': float(f['presente'])})
        elif kind == 'Boton':
            # No motor/navigation actions inferred before the núcleo specification.
            s.channels.queue_update(0, {'cue': f['presionado']})

    @Slot()
    def drain_frame(self):
        synth = getattr(self.state, "synth", None)
        if synth and synth.client:
            synth.client.drain_frame()
        drum = getattr(self.state, "drum", None)
        if drum and drum.client:
            drum.client.drain_frame()
        with self._lock:
            batch, self._queue = self._queue, deque()
        if synth:
            synth.in_surface_batch = True
        self.state._in_surface_batch = True
        try:
            for event in batch:
                self.apply(event)
                self.applied += 1
                self.kind_counts[event.kind] = self.kind_counts.get(event.kind, 0) + 1
        finally:
            self.state._in_surface_batch = False
            if synth:
                synth.in_surface_batch = False
        for model in self.models:
            model.flush()
        self.state.flush_properties()
        if hasattr(self.state, "synth"):
            self.state.synth.flush_frame()
        if drum:
            drum.flush_frame()
        self.last_batch = tuple(batch)
        if batch:
            self._serial = batch[-1].sequence
            self.batch_count += 1
            self.batchChanged.emit()
            if self.on_batch:
                self.on_batch(self.last_batch)
