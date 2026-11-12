"""LED23 output only: compact pad state to Teensy; animation time belongs there."""
from dataclasses import dataclass
from threading import RLock
import time
from key_leds import key_frame, key_messages

SELECTED, DIM, HELD, ROOT, PLAYHEAD, HIT = 1, 2, 4, 8, 16, 32
ROLE_COLORS = ('#3D8CFF', '#FF6FC8', '#B6F23D', '#F2C9A0')


def rgb7(color):
    return tuple(round(int(color.lstrip('#')[i:i+2], 16)*127/255) for i in (0, 2, 4))


def cc(number, value):
    return dict(type='control_change', channel=15, control=number, value=value)


@dataclass(frozen=True)
class PadState:
    rgb: tuple = (0, 0, 0)
    flags: int = 0
    level: int = 0
    serial: int = 0


def pad_messages(index, state):
    if not 0 <= index < 32 or len(state.rgb) != 3:
        raise ValueError('LED pad/rgb outside grid')
    values = (*state.rgb, state.flags, state.level, state.serial)
    if any(type(v) is not int or not 0 <= v <= 127 for v in values) or state.flags > 63:
        raise ValueError('LED23 value outside protocol')
    return [cc(20+i, v) for i, v in enumerate(state.rgb)] + [
        cc(29, state.flags), cc(30, state.level), cc(31, state.serial),
        dict(type='note_on', channel=15, note=48+index, velocity=1)]


class NoteLights:
    """Observe successful transport output; preserve the transport's public identity."""
    def __init__(self, notes):
        self.held = {}
        self.lock = RLock()
        original = notes.note

        def observe(part, note, velocity):
            result = original(part, note, velocity)
            with self.lock:
                if velocity:
                    self.held[part, note] = velocity
                else:
                    self.held.pop((part, note), None)
            return result
        notes.note = observe

    def snapshot(self):
        with self.lock:
            return dict(self.held)


def synth_frame(keyboard, part, held, physical=None):
    result = []
    physical = physical or {}
    for i in range(32):
        note = keyboard.note(i)
        candidates = [(p, velocity) for (p, n), velocity in held.items() if n == note]
        destination = physical.get(('Pad', i))
        if destination in held:
            candidates = [(destination[0], held[destination])]
        playing = max(candidates, key=lambda x:x[1]) if candidates else None
        flags = (ROOT if note is not None and keyboard.is_root(i) else 0)
        if i == (part-1)*2:
            flags |= SELECTED
        if playing:
            flags |= HELD
        color = ROLE_COLORS[(playing[0] if playing else part)-1]
        result.append(PadState(rgb7(color) if note is not None or playing else (0, 0, 0),
                               flags, playing[1] if playing else 16))
    return tuple(result)


def drum_frame(drum, hits=None, choked=None, serials=None):
    hits, choked = hits or {}, choked or [0]*16
    pads, steps, selected = drum.pad_entries(), drum.steps, drum.selectedPad
    serials = serials or [0]*16
    groups = drum._rack['groups']
    result = []
    for i, pad in enumerate(pads):
        dim = pad['mute'] or pad['dimmed'] or groups[pad['group']]['mute'] or bool(choked[i])
        flags = (SELECTED if i == selected else 0) | (DIM if dim else 0)
        velocity, serial = hits.get(i, (16, serials[i]))
        if i in hits:
            flags |= HIT
        result.append(PadState(rgb7(pad['tint']) if not pad['empty'] else (0, 0, 0), flags, velocity, serial))
    for step in steps:
        flags = (HELD if step['active'] and step['velocity'] else 0) | (PLAYHEAD if step['playhead'] else 0)
        if result[selected].flags & DIM:
            flags |= DIM
        result.append(PadState(rgb7(pads[selected]['tint']), flags,
                               step['velocity'] if flags & HELD else 0))
    return tuple(result)


class LedOutput:
    def __init__(self, surface, now=time.monotonic):
        self.surface, self.now = surface, now
        self.bind_notes()
        self.recovered = False
        self.port = self.mode = self.frame = self.controls = None
        self.last_send = self.last_snapshot = self.last_heartbeat = -float('inf')
        self.host_seen = {1: -float('inf'), 2: -float('inf')}
        self.counter = 0
        self.clock = {}
        self.clocks = {}
        self.clock_seen = {}
        self.key_frame = self.key_controls = None
        self.last_key_heartbeat = -float("inf")
        self.hits = None
        self.pending_hits = {}
        self.serials = [0]*16
        self.choked = [0]*16
        self.epoch = None
        self.drum = None
        if surface.synth.client:
            surface.synth.client.message.connect(lambda m:self.observe(m, 1))

    def bind_notes(self):
        self.note_transport = self.surface.notes
        self.notes = NoteLights(self.note_transport)

    def attach_drum(self, drum):
        self.drum = drum
        if drum.client:
            drum.client.message.connect(lambda m:self.observe(m, 2))

    def observe(self, message, source):
        event = message.get('event')
        if event == 'connection' and message.get('state') != 'ready':
            self.host_seen[source] = -float('inf')
            self.clock_seen[source] = -float('inf')
            if source == 2:
                self.hits = None
                self.pending_hits.clear()
            return
        if event not in ('meters', 'cpu', 'clock', 'drum_meters'):
            return
        if self.now()-self.host_seen[source] >= 1:
            self.recovered = True
        self.host_seen[source] = self.now()
        if event != 'clock':
            return
        clock = message['clock']
        self.clocks[source] = clock
        self.clock_seen[source] = self.now()
        epoch = clock.get('audio_epoch')
        counters = clock.get('led_hits')
        if counters is not None and len(counters) == 16:
            counters = [int(v) for v in counters]
            if epoch == self.epoch and self.hits is not None:
                for i, (old, new) in enumerate(zip(self.hits, counters)):
                    if new > old and self.mode == 2:
                        self.serials[i] = (self.serials[i]+1) % 128
                        self.pending_hits[i] = (int(clock['led_velocity'][i]), self.serials[i])
            else:
                self.pending_hits.clear()
            self.hits = counters
            self.epoch = epoch
            self.choked = list(clock.get('led_choked', [0]*16))
        self.clock = clock

    def update(self, port, mode):
        now = self.now()
        if not port:
            self.port = None
            return
        if now-self.last_send < .034:
            return
        import mido
        if self.surface.notes is not self.note_transport:
            self.bind_notes()
        reset = port is not self.port or mode != self.mode
        if reset:
            self.pending_hits.clear()
            self.serials = [0]*16
        prefs = self.surface.console.preferences
        clock = self.clocks.get(mode, {})
        running = bool(clock.get('running', False))
        transport = int(running) | (2 if mode == 2 and clock.get('armed') else 0) | (4 if mode == 2 and clock.get('queued', -1) >= 0 else 0)
        beat = (int(clock.get('bar', 1))*4+int(clock.get('step', 0))//4) % 128
        controls = (prefs.ledBrightness, int(prefs.ledAnimations), transport, beat)
        if mode == 2:
            frame = drum_frame(self.drum, self.pending_hits, self.choked, self.serials)
        elif mode == 1:
            with self.surface.lock:
                frame = synth_frame(self.surface.keyboard, self.surface.part,
                                    self.notes.snapshot(), dict(self.surface.held))
        else:
            frame = (PadState(),)*32
        snapshot = reset or self.recovered or now-self.last_snapshot >= 1
        messages = [cc(23, 23), cc(24, mode)] if reset else []
        if snapshot or controls != self.controls:
            messages += [cc(25+i, v) for i, v in enumerate(controls)]
        fresh = now-self.host_seen.get(mode, -float('inf')) < 1
        heartbeat = fresh and (reset or now-self.last_heartbeat >= .5)
        if heartbeat:
            messages.append(cc(119, self.counter))
        for i, state in enumerate(frame):
            if snapshot or self.frame is None or state != self.frame[i]:
                messages.extend(pad_messages(i, state))
        console = self.surface.console
        key_fresh = now-self.clock_seen.get(mode, -float('inf')) < 1
        keys = key_frame(console, clock, key_fresh)
        bpm = max(30, min(240, round(float(console.drum.clock['bpm']))))
        key_controls = (bpm % 128, bpm // 128, console.buttons.tap_serial,
                        int(key_fresh and running))
        if reset:
            messages.append(cc(102, 27))
        if snapshot or key_controls != self.key_controls:
            messages += [cc(n, v) for n, v in zip((107, 108, 109, 111), key_controls)]
        key_heartbeat = reset or now-self.last_key_heartbeat >= .5
        if key_heartbeat:
            messages.append(cc(110, self.counter))
        for i, state in enumerate(keys):
            if snapshot or self.key_frame is None or state != self.key_frame[i]:
                messages.extend(key_messages(i, state))
        try:
            for message in messages:
                port.send(mido.Message(**message))
        except (OSError, RuntimeError):
            self.port = None
            raise
        self.port, self.mode, self.frame, self.controls = port, mode, frame, controls
        self.key_frame, self.key_controls = keys, key_controls
        if key_heartbeat:
            self.last_key_heartbeat = now
        self.last_send = now
        self.pending_hits.clear()
        self.recovered = False
        if snapshot:
            self.last_snapshot = now
        if heartbeat:
            self.counter = (self.counter+1) % 128
            self.last_heartbeat = now
