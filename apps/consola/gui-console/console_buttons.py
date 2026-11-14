"""Console v1 intentions on the GUI frame; audio timing stays in the host."""
from collections import deque
from time import monotonic
from PySide6.QtCore import QObject, Property, Signal


class ConsoleButtons(QObject):
    changed = Signal()
    intentRequested = Signal(object)

    def __init__(self, console, parent=None):
        super().__init__(parent)
        self.console = console
        self.shift = False
        self._armed = ''
        self._boundary = 'phrase'
        self._taps = deque(maxlen=5)
        self.tap_serial = 0
        self._menu_index = 0

    armed = Property(str, lambda s: s._armed, notify=changed)
    boundary = Property(str, lambda s: s._boundary, notify=changed)
    menuIndex = Property(int, lambda s: s._menu_index, notify=changed)
    sceneColor = Property(str, lambda s: '#E9AD50' if s.console.drumMode else
                          '#3D8CFF' if s.console.synthMode else '#5DCAA5', notify=changed)

    def notice(self, text):
        self.console.surface.touch.inspect(text)

    def pending(self, action, **fields):
        # Explicit integration seam: no núcleo/service currently consumes these intentions.
        self.intentRequested.emit(dict(action=action, pending=True, **fields))
        label = ('SCENE '+str(fields['scene'])) if action == 'launch_scene' else action.upper().replace('_', ' ')
        self.notice(label+' · PENDING · no execution service connected')

    def tap(self, now=None):
        now = monotonic() if now is None else now
        if self._taps and (now-self._taps[-1] > 2 or now <= self._taps[-1]):
            self._taps.clear()
        if self._taps and now-self._taps[-1] < .3:
            return
        self._taps.append(now)
        self.tap_serial = (self.tap_serial+1) % 128
        self.changed.emit()
        if len(self._taps) < 2:
            self.notice('TAP · tap again to set tempo')
            return
        bpm = max(60., min(200., 60*(len(self._taps)-1)/(self._taps[-1]-self._taps[0])))
        drum = self.console.drum
        if drum.clock['running']:
            self.notice('TAP · stop transport to edit BPM (host v1)')
            return
        drum.setBpm(bpm)
        self.notice(f'TAP · {bpm:.1f} BPM'+(' · PREVIEW' if drum.demo else ' · HOST'))

    def section(self, name):
        self._armed = '' if self._armed == name else name
        self._boundary = 'bar' if self.shift else 'phrase'
        self.intentRequested.emit(dict(action='section_arm' if self._armed else 'section_cancel',
                                       section=name, boundary=self._boundary, pending=True))
        self.changed.emit()
        self.notice((name+' ARMED · NEXT '+self._boundary.upper()+' · recipe pending')
                    if self._armed else name+' CANCELLED')

    def execute_section(self):
        """Reserved executor hook; no recipes or scheduling until the section spec exists."""
        if self._armed:
            self.pending('section_execute', section=self._armed, boundary=self._boundary)

    def page(self, delta):
        c = self.console
        if c.screenName == 'Settings' or c.screenName.startswith('SynthSettings'):
            names = ['Settings', 'SynthSettingsAudio', 'SynthSettingsConnections', 'SynthSettingsSystem']
            c.navigate(names[(names.index(c.screenName)+delta) % len(names)])
        elif c.synthMode and c.screenName.startswith('Synth'):
            c.synth.nextPage(delta)
        elif c.drumMode and c.screenName.startswith('Drum'):
            d = c.drum
            if c.screenName in ('DrumSteps', 'DrumProbability'):
                d._step_page = max(0, min(1, d.stepPage+delta))
            elif c.screenName == 'DrumMixer':
                d._page = (d._page+delta) % 3
            elif d._pages:
                d._page = (d._page+delta) % len(d._pages)
            d._touch()
        else:
            names = ['Session', 'Note', 'Device', 'Mix'] if not c._mode else ['LiveSession', 'LiveArrange', 'Mixer']
            if c.screenName in ('SequencerSteps', 'SequencerPianoRoll'):
                names = ['SequencerSteps', 'SequencerPianoRoll']
            c.navigate(names[(names.index(c.screenName)+delta) % len(names)] if c.screenName in names else names[0])

    def handle(self, kind, fields):
        c = self.console
        if kind == 'Jog' and c.screenName == 'ModeSelect' and not self.shift:
            self._menu_index = (self._menu_index+fields['delta']) % 7
            self.changed.emit()
            return True
        if kind != 'Boton':
            return False
        name, down = fields['nombre'], fields['presionado']
        if name == 'SHIFT':
            self.shift = down
            self.changed.emit()
            return False  # Fine mode/hold state must reach the existing bridges too.
        if not down:
            return False
        if name == 'MENU':
            self._menu_index = (4 if c.screenName.startswith('Sequencer') else
                                5 if c.screenName == 'Mix' else max(0, min(3, c.selectedMode)))
            c.navigate('ModeSelect')
            self.changed.emit()
        elif name == 'BACK':
            if c.surface.touch.detail:
                c.surface.touch.dismiss()
            elif c.drumMode and c.drum.tempoVisible:
                c.drum.action('tempo-close')
            else:
                c.goBack()
        elif name in ('BROWSE', 'SEARCH'):
            c.navigate('DrumBrowser' if c.drumMode else 'SynthPresets' if c.synthMode else 'Browse' if not c._mode else 'Library')
        elif name == 'JOG' and c.screenName == 'ModeSelect' and not self.shift:
            c.triggerAction('nav:Settings' if self._menu_index == 6 else 'mode:'+str(self._menu_index))
        elif name in ('LEFT', 'RIGHT', 'PAGE_LEFT', 'PAGE_RIGHT'):
            delta = -1 if name in ('LEFT', 'PAGE_LEFT') else 1
            if self.shift and name in ('LEFT', 'RIGHT'):
                c.surface.touch.move(delta)
            elif name.startswith('PAGE') and not (c.synthMode or c.drumMode):
                self.pending('launch_scene', scene=3 if delta < 0 else 4)
            else:
                self.page(delta)
        elif name in ('OCT_PLUS', 'OCT_MINUS'):
            if c.synthMode:
                return False  # Preserve immediate transpose before the next MIDI pad hit.
            if c.drumMode:
                self.pending('drum_octave', delta=1 if name == 'OCT_PLUS' else -1)
            else:
                self.pending('launch_scene', scene=1 if name == 'OCT_PLUS' else 2)
        elif name == 'TAP':
            self.tap()
        elif name in ('BUILD', 'DROP', 'BREAK'):
            self.section(name)
        elif name == 'CUE' and c.drumMode and c.screenName.startswith('Drum'):
            c.drum.action('preview' if c.screenName.startswith('DrumBrowser') else 'audition')
        elif name in ('CUE', 'FILL'):
            self.pending(name.lower())
        else:
            return False
        return True
