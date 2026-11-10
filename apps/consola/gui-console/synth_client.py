"""Qt-native contract-14 client; delivery and state changes commit once per frame."""
from collections import deque
import json
import time

from PySide6.QtCore import QObject, QTimer, Signal, Slot, Property
from PySide6.QtNetwork import QLocalSocket
from fake_synth_host import socket_path

COMMANDS = frozenset(('hello','get_state','list_params','get_params','set_param','set_engine',
                     'list_presets','load_preset','save_preset','set_level','set_mute','note','subscribe'))


class SynthClient(QObject):
    wakeRequested = Signal()
    message = Signal(object)
    stateChanged = Signal()

    def __init__(self, path=None, parent=None, timeout_ms=500, backoff_ms=100, transport=None):
        super().__init__(parent)
        self.path = path or socket_path()
        self.timeout_ms = timeout_ms
        self.initial_backoff = backoff_ms
        self.backoff = backoff_ms
        self.pending = {}
        self._queue = deque()
        self._state = 'disconnected'
        self._serial = 0
        self._running = False
        self._watch_events = True
        self._buffer = bytearray()
        self.socket = transport or QLocalSocket(self)
        self.socket.connected.connect(self._connected)
        self.socket.readyRead.connect(self._read)
        self.socket.disconnected.connect(self._lost)
        self.socket.errorOccurred.connect(self._lost)
        self.health = QTimer(self)
        self.health.setSingleShot(True)
        self.health.setInterval(timeout_ms)
        self.health.timeout.connect(self._unresponsive)
        self.retry = QTimer(self)
        self.retry.setSingleShot(True)
        self.retry.timeout.connect(self._connect)
        self.deadline = QTimer(self)
        self.deadline.setSingleShot(True)
        self.deadline.timeout.connect(self._expire)

    state = Property(str,lambda s:s._state,notify=stateChanged)

    def _enqueue(self, item):
        empty = not self._queue
        self._queue.append(item)
        if empty:
            self.wakeRequested.emit()

    def _status(self, value):
        self._enqueue(dict(event='connection',state=value))

    @Slot()
    def start(self):
        self._running = True
        self._connect()

    @Slot()
    def stop(self):
        self._running = False
        self.retry.stop()
        self.deadline.stop()
        self.health.stop()
        self.socket.abort()
        self.pending.clear()
        self._status('disconnected')

    @Slot()
    def reconnect(self):
        self.retry.stop()
        self.socket.abort()
        self.backoff = self.initial_backoff
        self.start()

    def _connect(self):
        if self._running:
            self._status('connecting')
            self.health.start()
            self.socket.connectToServer(self.path)

    def _connected(self):
        self.backoff = self.initial_backoff
        self._watch_events = True
        self._buffer.clear()
        self._status('ready')
        self.health.start()
        self.request('hello',client='grooveforge-console')
        self.request('subscribe',topics=['meters','state','cpu'])
        self.request('get_state')

    def _lost(self, *_):
        if not self._running:
            return
        self._buffer.clear()
        self.health.stop()
        self._status('not_responding')
        self.deadline.stop()
        for rid, (_,cmd,fields) in list(self.pending.items()):
            self._enqueue(dict(id=rid,ok=False,error='connection lost',_cmd=cmd,_fields=fields))
        self.pending.clear()
        if not self.retry.isActive():
            self.retry.start(self.backoff)
            self.backoff = min(5000,self.backoff*2)

    def request(self, cmd, **fields):
        if cmd not in COMMANDS:
            raise ValueError(f'Unsupported synth command: {cmd}')
        self._serial += 1
        rid = self._serial
        if self.socket.state() != QLocalSocket.ConnectedState:
            self._enqueue(dict(id=rid,ok=False,error='not connected',_cmd=cmd,_fields=fields))
            return rid
        payload = (json.dumps(dict(id=rid,cmd=cmd,**fields),ensure_ascii=False,allow_nan=False)+'\n').encode()
        self.pending[rid] = (time.monotonic()+self.timeout_ms/1000,cmd,fields)
        self.socket.write(payload)
        self._arm_deadline()
        return rid

    def _arm_deadline(self):
        if self.pending:
            delay = max(1,int((min(p[0] for p in self.pending.values())-time.monotonic())*1000)+1)
            self.deadline.start(delay)
        else:
            self.deadline.stop()

    def _unresponsive(self):
        if self._running:
            for rid, (_,cmd,fields) in list(self.pending.items()):
                self._enqueue(dict(id=rid,ok=False,error='timeout',_cmd=cmd,_fields=fields))
            self.pending.clear()
            self._status('not_responding')
            self.socket.abort()
            self._lost()

    def _expire(self):
        now = time.monotonic()
        expired = [rid for rid,p in self.pending.items() if p[0] <= now]
        for rid in expired:
            _,cmd,fields = self.pending.pop(rid)
            self._enqueue(dict(id=rid,ok=False,error='timeout',_cmd=cmd,_fields=fields))
        if expired:
            self._status('not_responding')
            # A new session plus get_state avoids applying late replies after a timeout.
            self.socket.abort()
            self._lost()
        self._arm_deadline()

    def _read(self):
        self._buffer.extend(bytes(self.socket.readAll()))
        if len(self._buffer) > 2*1024*1024:
            self.socket.abort()
            self._lost()
            return
        while b'\n' in self._buffer:
            line,_,rest = self._buffer.partition(b'\n')
            self._buffer = bytearray(rest)
            try:
                msg = json.loads(line)
                if not isinstance(msg,dict):
                    raise ValueError('object required')
                if 'id' in msg:
                    pending = self.pending.pop(msg['id'],None)
                    if pending is None:
                        continue
                    _,msg['_cmd'],msg['_fields'] = pending
                    if msg['_cmd']=='subscribe' and msg.get('ok'):
                        self._watch_events = 'meters' in msg['_fields']['topics']
                        if not self._watch_events:
                            self.health.stop()
                elif msg.get('event') not in ('meters','state','param','cpu'):
                    continue
                if self._watch_events:
                    self.health.start()
                self._enqueue(msg)
            except (ValueError,TypeError,UnicodeError):
                self._enqueue(dict(event='protocol_error',error='Invalid host JSON'))
        self._arm_deadline()

    @Slot()
    def drain_frame(self):
        batch, self._queue = self._queue, deque()
        status = self._state
        for msg in batch:
            if msg.get('event') == 'connection':
                status = msg['state']
            self.message.emit(msg)
        if status != self._state:
            self._state = status
            self.stateChanged.emit()
        return len(batch)
