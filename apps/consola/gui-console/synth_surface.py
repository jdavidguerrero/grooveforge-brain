"""SYNTH surface adapter: immediate music, frame-batched presentation/control.

No Qt presentation object is accessed by a MIDI callback. Snapshot publication
and held-note routing share a short lock; note output never waits for a frame.
"""
from dataclasses import replace
from pathlib import Path
from threading import RLock, Thread, Event
from collections import Counter
import json
import socket
import sys
import subprocess

from PySide6.QtCore import QObject, Property, Signal, Slot, QTimer
from scale_keyboard import ScaleKeyboard, ROOTS, SCALES

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'tools' / 'consola'))
from superficie import decodificar, Pad, Fsr, Boton, Encoder, Jog


class PreviewNotes:
    def __init__(self):
        self.events = []

    def note(self, part, note, velocity):
        self.events.append((part, note, velocity))

    def close(self):
        pass


class MidiNotes:
    def __init__(self, name):
        import mido
        matches = [n for n in mido.get_output_names() if name in n]
        if len(matches) != 1:
            raise ValueError(f'Host MIDI output {name!r}: expected one match, found {matches}')
        self.port = mido.open_output(matches[0])

    def note(self, part, note, velocity):
        import mido
        self.port.send(mido.Message('note_on' if velocity else 'note_off', channel=part-1,
                                    note=note, velocity=velocity))

    def control(self, part, msg):
        self.port.send(msg.copy(channel=part-1))

    def close(self):
        self.port.close()


class SocketNotes:
    """Independent contract-14 connection, ACKs drained continuously off Qt.

    Never replay stale notes after reconnect. On failure, stop accepting music;
    restarting the surface is explicit so releases cannot address a new session.
    """
    def __init__(self, path, transport=None):
        self.sock = transport or socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(.05)
        if transport is None:
            self.sock.connect(path)
        self.lock = RLock()
        self.serial = 0
        self.error = ''
        self.stop = Event()
        self.reader = Thread(target=self._read, daemon=True, name='synth-note-acks')
        self.reader.start()

    def _read(self):
        buffer = b''
        while not self.stop.is_set():
            try:
                data = self.sock.recv(4096)
                if not data:
                    self.error = 'Note socket disconnected'
                    return
                buffer += data
                while b'\n' in buffer:
                    line, buffer = buffer.split(b'\n', 1)
                    reply = json.loads(line)
                    if not reply.get('ok', False):
                        self.error = reply.get('error', 'Note rejected')
                if len(buffer) > 65536:
                    raise ValueError('Oversize note reply')
            except socket.timeout:
                continue
            except (OSError, ValueError, RuntimeError) as exc:
                if not self.stop.is_set():
                    self.error = str(exc)
                return

    def note(self, part, note, velocity):
        with self.lock:
            if self.error:
                raise OSError(self.error)
            self.serial += 1
            payload = dict(id=self.serial, cmd='note', part=part, note=note, velocity=velocity)
            self.sock.sendall((json.dumps(payload)+'\n').encode())

    def close(self):
        self.stop.set()
        try:
            self.sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        self.reader.join(timeout=.2)
        self.sock.close()


def open_note_output(transport, midi_port, path, fake_surface=False):
    """Prefer the graph MIDI path when exposed; make fallback visible to the caller."""
    if transport == 'socket' or fake_surface:
        return SocketNotes(path), 'SOCKET NOTES'
    try:
        return MidiNotes(midi_port), 'MIDI NOTES'
    except (OSError, ValueError, ImportError, RuntimeError) as exc:
        if transport == 'midi':
            raise
        return SocketNotes(path), f'SOCKET NOTES · MIDI unavailable: {exc}'


class RoomControls:
    """Slow CDC/PipeWire writes coalesced in a worker, never in MIDI/Qt callbacks."""
    def __init__(self, cdc=None, room_node='@DEFAULT_AUDIO_SINK@'):
        self.cdc = cdc
        self.room_node = room_node
        self.pending = {}
        self.lock = RLock()
        self.wake = Event()
        self.stopped = False
        self.error = ''
        self.thread = Thread(target=self._run, daemon=True, name='synth-room-controls')
        self.thread.start()

    def submit(self, name, value):
        with self.lock:
            self.pending[name] = value
        self.wake.set()

    def _run(self):
        serial = None
        try:
            while True:
                self.wake.wait()
                self.wake.clear()
                with self.lock:
                    batch, self.pending = self.pending, {}
                for name, value in batch.items():
                    try:
                        if name == 'MASTER':
                            subprocess.run(['wpctl', 'set-volume', self.room_node, f'{value:.4f}'],
                                           check=True, capture_output=True, timeout=.5)
                        else:
                            if not self.cdc:
                                raise OSError('CUE/IN require --surface-cdc')
                            if serial is None:
                                import serial as serial_module
                                serial = serial_module.Serial(self.cdc, 115200, timeout=.1, write_timeout=.1)
                            serial.write(pot_command(name, value).encode())
                            serial.read_until(b"\n", size=1024)
                    except (OSError, ImportError, subprocess.SubprocessError) as exc:
                        self.error = str(exc)
                with self.lock:
                    if self.stopped and not self.pending:
                        return
        finally:
            if serial:
                serial.close()

    def close(self):
        self.stopped = True
        self.wake.set()
        self.thread.join(timeout=1)


def pot_command(name, value):
    value = max(0., min(1., value))
    if name == 'CUE':
        return f'@GF HP {round(value*127)}\n'
    if name == 'IN':
        return f'@GF IN {round(value*127)*15//127}\n'
    raise ValueError('MASTER belongs to the PipeWire room node, not @GF')


def led_messages(index, rgb):
    """Proposed receive protocol; does not overlap channel-1 surface input."""
    return [dict(type='control_change', channel=15, control=20+i, value=v) for i, v in enumerate(rgb)] + [
        dict(type='note_on', channel=15, note=48+index, velocity=1 if any(rgb) else 0)]


def external_input_names(names, patterns, surface_name, own_ports=()):
    """Resolve explicit substrings, or every external ALSA input with 'auto'."""
    selected = []
    excluded = ('gf consola', 'gf superficie', 'midi through', 'gf-synth-host',
                'gf-synth-gui', 'gui-console', 'rtmidi input', 'rtmidi output',
                'rtmidiin client', 'rtmidiout client')
    for pattern in patterns:
        if pattern.lower() == 'none':
            continue
        if pattern.lower() == 'auto':
            matches = [n for n in names if n != surface_name and n not in own_ports and
                       not any(token in n.lower() for token in excluded)]
        else:
            matches = [n for n in names if pattern in n and n != surface_name]
            if len(matches) != 1:
                raise ValueError(f'External MIDI {pattern!r}: select one distinct input, found {matches}')
        for name in matches:
            if name not in selected:
                selected.append(name)
    return selected


class SynthSurface(QObject):
    changed = Signal()
    failed = Signal(str)

    def __init__(self, console, synth, batcher, touch, notes=None, room=None, parent=None):
        super().__init__(parent)
        self.console, self.synth, self.batcher, self.touch = console, synth, batcher, touch
        self.notes = notes or PreviewNotes()
        self.room = room
        self.keyboard = ScaleKeyboard()
        self.lock = RLock()
        self.held = {}
        self.refs = Counter()
        self.part = synth.selectedPart
        self.active = False
        self.shift = False
        self.route_shift = False
        self.route_screen = console.screenName
        self.presence = [True, True]
        self.ports = []
        self.input_names = []
        self.led_port = None
        self.last_leds = None
        self._status = 'FAKE SURFACE' if notes is None else 'SURFACE READY'
        self.failed.connect(self._failure)
        console.changed.connect(self.publish)
        synth.changed.connect(self.publish)
        batcher.synth_surface = self
        self.publish()
        self.health = QTimer(self)
        self.health.setInterval(500)
        self.health.timeout.connect(self.check_health)
        self.health.start()

    @Slot()
    def check_health(self):
        error = getattr(self.notes, 'error', '') or (self.room.error if self.room else '')
        if self.input_names:
            try:
                import mido
                available = mido.get_input_names()
                missing = [n for n in self.input_names if n not in available]
                if missing:
                    for port in self.ports:
                        port.close()
                    self.ports.clear()
                    self.input_names.clear()
                    self.panic()
                    error = 'MIDI input disconnected; restart surface: '+', '.join(missing)
            except (OSError, RuntimeError) as exc:
                error = str(exc)
        if error and self._status != 'SURFACE ERROR: '+error:
            self._failure(error)

    status = Property(str, lambda s:s._status, notify=changed)
    root = Property(int, lambda s:s.keyboard.root, notify=changed)
    scale = Property(str, lambda s:s.keyboard.scale, notify=changed)
    octave = Property(int, lambda s:s.keyboard.octave, notify=changed)
    roots = Property('QVariantList', lambda s:list(ROOTS), constant=True)
    scales = Property('QVariantList', lambda s:list(SCALES), constant=True)
    layout = Property('QVariantList', lambda s:[dict(note=s.keyboard.note(i), root=s.keyboard.is_root(i)) for i in range(32)], notify=changed)

    @Slot(str)
    def _failure(self, message):
        self._status = 'SURFACE ERROR: '+message
        self.changed.emit()

    @Slot()
    def publish(self):
        with self.lock:
            active = self.console.synthMode
            if self.active and not active:
                self.panic()
            self.active, self.part = active, self.synth.selectedPart
            self.route_screen = self.console.screenName
        self.update_leds()

    @Slot(int)
    def setRoot(self, value):
        if 0 <= value < 12:
            with self.lock:
                self.keyboard = replace(self.keyboard, root=value)
            self.changed.emit()
            self.update_leds()

    @Slot(str)
    def setScale(self, value):
        if value in SCALES:
            with self.lock:
                self.keyboard = replace(self.keyboard, scale=value)
            self.changed.emit()
            self.update_leds()

    @Slot(int)
    def transpose(self, delta):
        with self.lock:
            self.keyboard = replace(self.keyboard, octave=max(0, min(7, self.keyboard.octave+delta)))
        self.changed.emit()
        self.update_leds()

    def _note(self, source, pressed, velocity, note):
        # Save the note-on destination even if selection changes before release.
        with self.lock:
            if pressed:
                if source in self.held or not self.active or note is None:
                    return
                destination = (self.part, note)
                if not self.refs[destination]:
                    self.notes.note(*destination, max(1, min(127, round(velocity*127))))
                self.refs[destination] += 1
                self.held[source] = destination
            elif source in self.held:
                destination = self.held.pop(source)
                self.refs[destination] -= 1
                if not self.refs[destination]:
                    del self.refs[destination]
                    self.notes.note(*destination, 0)

    def receive(self, event):
        try:
            musical_applied = False
            if isinstance(event, Boton):
                with self.lock:
                    if event.nombre == 'SHIFT':
                        self.route_shift = event.presionado
                    elif self.active and event.presionado:
                        if event.nombre in ('OCT_MINUS', 'OCT_PLUS'):
                            delta = -1 if event.nombre == 'OCT_MINUS' else 1
                            self.keyboard = replace(self.keyboard, octave=max(0,min(7,self.keyboard.octave+delta)))
                            musical_applied = True
                        elif self.route_screen == 'SynthParts' and event.nombre.startswith('ENCODER_'):
                            index = int(event.nombre.split('_')[1])-1
                            if index < 4:
                                self.part = index+1
            elif isinstance(event, Jog) and self.active and self.route_screen == 'SynthParts' and not self.route_shift:
                with self.lock:
                    self.part = (self.part-1+event.delta)%4+1
            if isinstance(event, (Pad, Fsr)):
                with self.lock:
                    note = self.keyboard.note(event.indice) if isinstance(event, Pad) else (36, 38, 42, 46)[event.indice]
                    self._note((type(event).__name__, event.indice), event.presionado, event.velocidad, note)
            # Feedback and every non-musical action commit through the frame seam.
            if musical_applied:
                self.batcher.enqueue(dict(type='Boton', nombre=event.nombre, presionado=event.presionado,
                                          musical_applied=True))
            else:
                self.batcher.enqueue(event)
        except (OSError, ValueError, RuntimeError) as exc:
            self.failed.emit(str(exc))

    def external(self, port, msg):
        try:
            if msg.type in ('note_on', 'note_off'):
                self._note(('external', port, msg.channel, msg.note), msg.type == 'note_on' and msg.velocity > 0,
                           msg.velocity/127, msg.note)
            elif self.active and msg.type in ('pitchwheel', 'control_change') and hasattr(self.notes, 'control'):
                if msg.type == 'pitchwheel' or msg.control == 64:
                    # Sustain belongs to the part at pedal-down until pedal-up.
                    with self.lock:
                        if msg.type == 'control_change':
                            key = ('sustain', port, msg.channel)
                            part = self.held.get(key, (self.part, None))[0]
                            if msg.value >= 64:
                                self.held[key] = (part, None)
                            else:
                                self.held.pop(key, None)
                        else:
                            part = self.part
                        self.notes.control(part, msg)
        except (OSError, ValueError, RuntimeError) as exc:
            self.failed.emit(str(exc))

    def apply_frame(self, kind, fields):
        s, c = self.synth, self.console
        index = fields.get('indice', 0)
        if kind == 'Boton':
            name, down = fields['nombre'], fields['presionado']
            if name == 'SHIFT':
                self.shift = down
                s.setFine(down)
            elif down:
                if name.startswith('ENCODER_'):
                    c.pressEncoder(int(name.split('_')[1])-1)
                elif name == 'JOG':
                    if self.shift or not c.synthMode:
                        self.touch.activate()
                    elif c.screenName == 'SynthPresets':
                        s.loadPreset()
                    elif c.screenName == 'SynthParts':
                        s.action('open:'+str(s.selectedPart))
                    else:
                        s.nextBank()
                elif name in ('LEFT', 'RIGHT'):
                    self.touch.move(-1 if name == 'LEFT' else 1)
                elif name in ('PAGE_LEFT', 'PAGE_RIGHT'):
                    s.nextPage(-1 if name == 'PAGE_LEFT' else 1)
                elif name in ('OCT_MINUS', 'OCT_PLUS'):
                    if fields.get('musical_applied'):
                        self.changed.emit()
                        self.update_leds()
                    else:
                        self.transpose(-1 if name == 'OCT_MINUS' else 1)
                elif name == 'MENU':
                    c.navigate('SynthSettingsAudio' if c.synthMode else 'Settings')
                elif name == 'SEARCH':
                    c.navigate('SynthPresets' if c.synthMode else 'Search')
                elif name == 'BACK':
                    c.navigate('SynthParts' if c.synthMode else 'ModeSelect')
                elif name == 'STOP':
                    self.panic()
            return True
        if kind == 'Jog':
            delta = fields['delta']
            if self.shift or not c.synthMode or not c.screenName.startswith('Synth'):
                self.touch.move(delta)
            elif c.screenName == 'SynthPresets':
                s.selectPreset(max(0, min(len(s._presets)-1, s.presetIndex+delta)))
            elif c.screenName == 'SynthParts':
                s.selectPart((s.selectedPart-1+delta)%4+1)
            else:
                s.nextPage(delta)
            return True
        if not c.synthMode:
            if kind == 'Encoder' and c.screenName == 'ModeSelect':
                c.turnEncoder(index, fields['delta'])
                return True
            return False
        if kind == 'Encoder':
            if not c.screenName.startswith('Synth'):
                c.turnEncoder(index, fields['delta'])
                return True
            c.selectEncoder(index)
            if c.screenName == 'SynthScale':
                if index == 0: self.setRoot((self.root+fields['delta'])%12)
                elif index == 1:
                    names = list(SCALES)
                    self.setScale(names[(names.index(self.scale)+fields['delta'])%len(names)])
                elif index == 2: self.transpose(fields['delta'])
            else:
                s.turn(index, fields['delta'])
        elif kind == 'Fader':
            s.request('set_level', part=index+1, db=-60+72*max(0., min(1., fields['valor'])))
        elif kind in ('Mano', 'Expresion'):
            if kind == 'Expresion' or self.presence[index]:
                target = 'cutoff' if index == 0 else 'move'
                ids = (target,) if target != 'cutoff' or s.engine != 'hybrid' else ('a.cutoff','b.cutoff')
                metas = [p for page in s.current_pages for bank in page['banks'] for p in bank if p['id'] in ids]
                for meta in metas:
                    value = max(0., min(1., fields['valor']))
                    s._outbound[(s.selectedPart, meta['id'])] = meta['min']+value*(meta['max']-meta['min'])
                    s._dirty = True
        elif kind == 'ManoPresencia':
            self.presence[index] = fields['presente']
        elif kind == 'Perilla':
            if self.room:
                self.room.submit(fields['nombre'], max(0., min(1., fields['valor'])))
            else:
                self._status = 'PREVIEW · '+fields['nombre']
                self.changed.emit()
        elif kind == 'SelectorPad':
            self._status = f'INPUT PAD {fields["db"]} dB (physical)'
            self.changed.emit()
        return True

    def open_ports(self, surface_port, external_ports=(), led=True):
        import mido
        names = mido.get_input_names()
        matches = [n for n in names if surface_port in n]
        if len(matches) != 1:
            raise ValueError(f'Surface {surface_port!r}: expected one match, found {matches}')
        surface_name = matches[0]
        def callback(msg):
            event = decodificar(msg)
            if event is not None:
                self.receive(event)
        self.ports.append(mido.open_input(surface_name, callback=callback))
        self.input_names.append(surface_name)
        own_ports = (getattr(getattr(self.notes,'port',None),'name',None),)
        for name in external_input_names(names,external_ports,surface_name,own_ports):
            self.ports.append(mido.open_input(name, callback=lambda msg, n=name:self.external(n, msg)))
            self.input_names.append(name)
        if led:
            outputs = [n for n in mido.get_output_names() if surface_port in n]
            if len(outputs) != 1:
                raise ValueError(f'Surface LED output {surface_port!r}: found {outputs}')
            self.led_port = mido.open_output(outputs[0])
            self.update_leds()
        self._status = 'SURFACE READY · '+('SOCKET NOTES' if isinstance(self.notes,SocketNotes) else 'MIDI NOTES')
        self.changed.emit()

    def update_leds(self):
        if not self.led_port:
            return
        # Theme.qml roleColors, reduced from RGB8 to the proposed MIDI RGB7.
        colors = tuple(tuple(round(int(color[i:i+2],16)*127/255) for i in (0,2,4))
                       for color in ('3D8CFF','FF6FC8','B6F23D','F2C9A0'))
        frame = tuple((0,0,0) if self.keyboard.note(i) is None else (127,127,127) if self.keyboard.is_root(i) else colors[self.part-1] for i in range(32)) if self.active else ((0, 0, 0),)*32
        if frame == self.last_leds:
            return
        try:
            import mido
            # RGB latches + note trigger, proposed in doc21; no sleep/animations.
            for index, rgb in enumerate(frame):
                if self.last_leds is None or self.last_leds[index] != rgb:
                    for msg in led_messages(index, rgb):
                        self.led_port.send(mido.Message(**msg))
            self.last_leds = frame
        except (OSError, RuntimeError) as exc:
            self.failed.emit(str(exc))

    @Slot()
    def panic(self):
        with self.lock:
            for part, note in list(self.refs):
                try:
                    self.notes.note(part, note, 0)
                except (OSError, RuntimeError) as exc:
                    self.failed.emit(str(exc))
            if hasattr(self.notes, 'control'):
                import mido
                for part in range(1, 5):
                    try:
                        self.notes.control(part, mido.Message('control_change', control=64, value=0))
                    except (OSError, RuntimeError) as exc:
                        self.failed.emit(str(exc))
            self.held.clear()
            self.refs.clear()

    @Slot(int, bool)
    def fakePad(self, index, down):
        self.receive(Pad(index, down, .8 if down else 0))

    @Slot(int, int)
    def fakeEncoder(self, index, delta):
        self.receive(Encoder(index, delta))

    def close(self):
        self.health.stop()
        for port in self.ports:
            port.close()
        self.ports.clear()
        self.input_names.clear()
        self.panic()
        self.active = False
        self.update_leds()
        if self.led_port:
            self.led_port.close()
            self.led_port = None
        self.notes.close()
        if self.room:
            self.room.close()
