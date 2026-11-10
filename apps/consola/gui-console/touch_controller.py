"""One auxiliary focus path shared by touch, jog and buttons on every screen."""
from PySide6.QtCore import QObject, Property, Signal, Slot, QMetaObject, Qt, QPointF
import shiboken6

HARDWARE_ACTIONS = {'soft-key', 'parameter', 'navigation', 'choice', 'list', 'text', 'detail'}


class TouchController(QObject):
    changed = Signal()

    def __init__(self, console, parent=None):
        super().__init__(parent)
        self.console = console
        self.targets = []
        self.current = None
        self._detail = ''

    detail = Property(str, lambda s:s._detail, notify=changed)

    @Slot(QObject)
    def registerTarget(self, target):
        self.targets.append(target)

    @Slot(QObject)
    def unregisterTarget(self, target):
        self.targets = [t for t in self.targets if shiboken6.isValid(t) and t != target]
        if self.current == target:
            self.current = None

    @staticmethod
    def available(target):
        if not shiboken6.isValid(target) or not target.isVisible() or not target.property('enabled'):
            return False
        point = target.mapToScene(QPointF(target.width()/2, target.height()/2))
        window = target.window()
        if not window or not (0 <= point.x() <= window.width() and 0 <= point.y() <= window.height()):
            return False
        ancestor = target.parentItem()
        while ancestor:
            if ancestor.clip():
                local = ancestor.mapFromScene(point)
                if not ancestor.contains(local):
                    return False
            ancestor = ancestor.parentItem()
        return True

    @staticmethod
    def ancestors(target):
        item = target.parentItem()
        while item:
            yield item
            item = item.parentItem()

    def focus_targets(self):
        targets = [t for t in self.targets if t.isVisible() and t.property('enabled') and
                   (self.available(t) or any(self.scroll_ancestors(t)))]
        modal_names = {'modal','reviewModal','synthSaveWindow'}
        modals = {a for t in targets for a in self.ancestors(t) if a.objectName() in modal_names}
        if modals:
            targets = [t for t in targets if any(a in modals or a.objectName() in ('topBar','softKeys')
                                                for a in self.ancestors(t))]
        return targets

    @staticmethod
    def scroll_ancestors(target):
        ancestor = target.parentItem()
        while ancestor:
            if ancestor.property('contentHeight') is not None and ancestor.property('contentY') is not None:
                yield ancestor
            ancestor = ancestor.parentItem()

    def expose(self, target):
        for viewport in self.scroll_ancestors(target):
            point = viewport.mapFromItem(target, QPointF(target.width()/2,target.height()/2))
            for axis, coordinate, size, content in [('Y',point.y(),viewport.height(),'contentHeight'),
                                                    ('X',point.x(),viewport.width(),'contentWidth')]:
                if coordinate < 0 or coordinate > size:
                    name = 'content'+axis
                    value = viewport.property(name)+coordinate-size/2
                    viewport.setProperty(name,max(0.,min(viewport.property(content)-size,value)))

    @Slot(int)
    def move(self, delta):
        self.targets = [t for t in self.targets if shiboken6.isValid(t)]
        targets = self.focus_targets()
        if not targets:
            return
        if self.current and shiboken6.isValid(self.current):
            self.current.setProperty('hardwareFocused', False)
        start = targets.index(self.current) if self.current in targets else (-1 if delta > 0 else 0)
        self.current = targets[(start+delta)%len(targets)]
        self.expose(self.current)
        self.current.setProperty('hardwareFocused', True)
        self.changed.emit()

    @Slot()
    def activate(self):
        if not self.current or self.current not in self.focus_targets() or not self.available(self.current):
            self.move(1)
        if self.current and self.available(self.current):
            QMetaObject.invokeMethod(self.current, 'activate', Qt.DirectConnection)

    @Slot(str)
    def inspect(self, text):
        self._detail = text
        self.changed.emit()

    @Slot()
    def dismiss(self):
        self._detail = ''
        self.changed.emit()
