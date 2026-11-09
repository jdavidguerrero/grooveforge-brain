from PySide6.QtCore import QAbstractListModel, QModelIndex, Qt, QTimer


class SnapshotModel(QAbstractListModel):
    def __init__(self, rows, roles, parent=None):
        super().__init__(parent)
        self.rows = [dict(row) for row in rows]
        self._roles = {Qt.UserRole + i + 1: name.encode() for i, name in enumerate(roles)}
        self._pending = {}
        self._timer = QTimer(self)
        self._timer.setInterval(16)
        self._timer.timeout.connect(self.flush)
        self._timer.setSingleShot(True)

    def roleNames(self):
        return self._roles

    def rowCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else len(self.rows)

    def data(self, index, role=Qt.DisplayRole):
        if not index.isValid() or not 0 <= index.row() < len(self.rows):
            return None
        name = self._roles.get(role)
        return self.rows[index.row()].get(name.decode()) if name else None

    def queue_update(self, row, values):
        if not 0 <= row < len(self.rows):
            raise IndexError(row)
        unknown = values.keys() - {n.decode() for n in self._roles.values()}
        if unknown:
            raise ValueError(f'Unknown roles: {unknown}')
        self._pending.setdefault(row, {}).update(values)
        if not self._timer.isActive():
            self._timer.start()

    def flush(self):
        self._timer.stop()
        if not self._pending:
            return
        changed = sorted(self._pending)
        roles = set()
        for row, values in self._pending.items():
            self.rows[row].update(values)
            roles.update(k for k, v in self._roles.items() if v.decode() in values)
        self._pending.clear()
        # One bounding range per frame bounds signal traffic under encoder bursts.
        self.dataChanged.emit(self.index(changed[0]), self.index(changed[-1]), sorted(roles))

    def replace(self, rows):
        self.beginResetModel()
        self._pending.clear()
        self._timer.stop()
        self.rows = [dict(row) for row in rows]
        self.endResetModel()
