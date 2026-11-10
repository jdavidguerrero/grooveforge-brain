import copy
from PySide6.QtCore import QObject, Property, Signal, Slot, QTimer
from models import SnapshotModel
from registry import SCREENS, screen_id
from review_data import ReviewData

ROLES = ['KICK', 'DRUMS', 'PERC', 'TOP', 'BASS', 'CHORDS', 'LEAD', 'PAD']
PATTERNS = ['x...x...x...x...', '....x..x....x..x', '..x..x....x..x.x', '..x...x...x...x.',
            'x..x..x.x..x..x.', 'x.....x...x.....', '..x.x...x..x.x..', 'x...............']
LEVELS = [.82, .74, .61, .58, .77, .64, 0, .52]
CELL_ROLES = 'roleName title subtitle cellState steps level row channel'.split()
CHANNEL_ROLES = 'roleName level meter peak eq filterValue echo verb cue selected'.split()
OPTION_ROLES = 'letter title subtitle optionState energy notes hint encoder'.split()
KEY_ROLES = 'label value roleName lit turning enabled position action'.split()


def cells():
    definitions = [
        [('playing', 'Wood Kick 04', 'A1 · 808 body'), ('playing', 'Clap Room', '+ shaker'),
         ('playing', 'Congas Lo', 'skin · tuned A'), ('playing', 'Hat Open 2', 'offbeat'),
         ('playing', 'Sub Model D', 'A1 · round'), ('playing', 'Juno Stabs', 'Am9 · Fmaj7'),
         ('muted', 'Kalimba Arp', 'muted'), ('playing', 'OB Pad Warm', 'Am · slow')],
        [('queued', 'Kick Dusk', 'enters phrase'), ('idle', 'Clap Dry', 'ready'),
         ('idle', 'Bongo Hi', 'ready'), ('idle', 'Hat Tight', 'ready'),
         ('cue', 'Bass Prophet', 'A1 · pluck'), ('idle', 'DX Bells', 'ready'),
         ('idle', 'Kalimba Lead', 'ready'), ('empty', '—', 'no pad')],
        [('option', n, '') for n in ['Kick B · E3', 'Rim Clap · E4', 'Shaker · E2', 'Ride · E4',
                                   'Moog Round · E4', 'Prophet Chord · E3', 'Kalimba B · E4', 'Juno Air · E2']],
        [('loading' if i in (3, 6) else 'option', n, '') for i, n in enumerate(
            ['Kick C · E4', 'Snare Lo · E3', 'Cowbell · E3', 'Generating…', 'DX Bass · E3',
             'Organ · E2', 'Generating…', 'Strings · E3'])]]
    return [dict(roleName=ROLES[i], title=name, subtitle=sub, cellState=state,
                 steps=[int(c == 'x') for c in PATTERNS[i]], level=LEVELS[i], row=r, channel=i)
            for r, defs in enumerate(definitions) for i, (state, name, sub) in enumerate(defs)]


def options():
    patterns = [[(0, 0, 2), (3, 2, 1), (6, 0, 2), (10, 3, 1), (12, 0, 2)],
                [(0, 0, 3), (4, 2, 1), (6, 4, 2), (9, 2, 1), (12, 0, 3), (15, 6, 1)],
                [(0, 0, 4), (8, 0, 4), (6, 3, 1), (14, 2, 1)], []]
    return [dict(letter=chr(65+i), title=n, subtitle=s, optionState=state, energy=e,
                 notes=[dict(s=a, n=b, l=c) for a, b, c in patterns[i]], encoder=f'ENC {i+1} CUE',
                 hint=['PRESS ENC 1 TO CUE', 'IN CUE · ACCEPT ▸ NEXT PHRASE',
                       'PRESS ENC 3 TO CUE', 'GENERATE ▸ ANOTHER'][i])
            for i, (n, s, state, e) in enumerate([
                ('Sub Prophet', 'A1 · pluck · 16 steps', 'option', 3),
                ('Moog Round', 'A1 · legato · 16 steps', 'cue', 4),
                ('DX Bass', 'A1 · FM · 16 steps', 'option', 2),
                ('Waiting for engine', 'pattern · sound · energy', 'loading', 1)])]


def key(label='—', value='', role='', lit=False, action='', turning=False, position=-1):
    return dict(label=label, value=value, roleName=role, lit=lit, turning=turning,
                enabled=label != '—', position=position, action=action)


class ConsoleState(QObject):
    changed = Signal()

    def __init__(self):
        super().__init__()
        from local_settings import LocalSettings
        self.preferences = LocalSettings(self)
        self.preferences.changed.connect(self.refresh_keys)
        self._screen = 5
        self._return_screen = 5
        self._query = "kalim"
        self._keyboard = True
        self._pad_step = 16
        self._pad_note = 0
        self._step_lane = 5
        self._step_column = 10
        self._zoom = 1.0
        self._standalone = ReviewData("s", self)
        self._controller = ReviewData("c", self)
        self._review_selection = 0
        self._live_state = 'normal'
        self._selected = 4
        self._mode = 1
        self._settings = 'Surface'
        self._encoding = 'fill'
        self._cells = SnapshotModel(cells(), CELL_ROLES, self)
        eqs = [[.1, 0, .3], [.2, 0, 0], [.3, .1, -.3], [.2, 0, -1],
               [-1, -.2, .2], [.1, -.2, -.6], [0, 0, -.5], [.2, 0, -.7]]
        self._channels = SnapshotModel([
            dict(roleName=r, level=LEVELS[i], meter=[.82,.7,.6,.55,.74,.62,0,.48][i], peak=0.,
                 eq=eqs[i], filterValue=[0,.12,.22,.35,-.4,0,0,-.15][i],
                 echo=[.05,.2,.35,.1,0,.4,.3,.2][i], verb=[.05,.25,.3,.2,0,.5,.4,.7][i],
                 cue=i == 4, selected=i == 4) for i, r in enumerate(ROLES)], CHANNEL_ROLES, self)
        self._options = SnapshotModel(options(), OPTION_ROLES, self)
        self._softKeys = SnapshotModel([], KEY_ROLES, self)
        self._groups = SnapshotModel([
            dict(title=n, level=l, meter=m, members=rs) for n,l,m,rs in [
                ('RHYTHM',.82,.78,ROLES[:4]), ('BASS',.74,.7,['BASS']),
                ('MUSIC',.68,.58,ROLES[5:]), ('FX',.4,.3,['FX'])]],
            'title level meter members'.split(), self)
        self._connections = SnapshotModel([dict(title=n, status=s) for n,s in
            [('BRAIN','ok'), ('LINK','ok'), ('LIVE','off'), ('NET','ok')]], ['title','status'], self)
        self._hands = SnapshotModel([
            dict(side='LEFT HAND',target='FILTER',distance=12,inRange=False,value='OFF',rangeMin=6,rangeMax=26),
            dict(side='RIGHT HAND',target='NOTES',distance=17,inRange=False,value='OFF',rangeMin=6,rangeMax=28)],
            'side target distance inRange value rangeMin rangeMax'.split(), self)
        self._toasts = SnapshotModel([
            dict(icon='LINK',message='Link: 3 devices',value='124.0',tone='link',opacity=.6),
            dict(icon='KICK',message='KICK enters on the phrase',value='IN 4 BARS',tone='KICK',opacity=1.)],
            'icon message value tone opacity'.split(), self)
        self._property_dirty = False
        self._in_surface_batch = False
        self._frame_timer = QTimer(self)
        self._frame_timer.setInterval(16)
        self._frame_timer.timeout.connect(self.flush_properties)
        self._frame_timer.start()
        self.refresh_keys()

    def _notify(self):
        if self._in_surface_batch:
            self._property_dirty = True
        else:
            self.changed.emit()

    def flush_properties(self):
        if self._property_dirty:
            self._property_dirty = False
            self.changed.emit()

    cells = Property(QObject, lambda s: s._cells, constant=True)
    channels = Property(QObject, lambda s: s._channels, constant=True)
    options = Property(QObject, lambda s: s._options, constant=True)
    softKeys = Property(QObject, lambda s: s._softKeys, constant=True)
    groups = Property(QObject, lambda s: s._groups, constant=True)
    connections = Property(QObject, lambda s: s._connections, constant=True)
    hands = Property(QObject, lambda s: s._hands, constant=True)
    toasts = Property(QObject, lambda s: s._toasts, constant=True)
    standalone = Property(QObject, lambda s: s._standalone, constant=True)
    controller = Property(QObject, lambda s: s._controller, constant=True)
    screenSource = Property(str, lambda s: SCREENS[s._screen-1].qml, notify=changed)
    screenName = Property(str, lambda s: SCREENS[s._screen-1].name, notify=changed)
    screenTitle = Property(str, lambda s: SCREENS[s._screen-1].title, notify=changed)
    screenMode = Property(str, lambda s: 'SYNTH' if s._mode == 2 and s.screenName.startswith('Synth') else
                          ('CONTROLLER' if not s._mode else 'STANDALONE') if SCREENS[s._screen-1].mode == 'SHARED'
                          else SCREENS[s._screen-1].mode, notify=changed)
    query = Property(str, lambda s: s._query, notify=changed)
    keyboardVisible = Property(bool, lambda s: s._keyboard, notify=changed)
    padStep = Property(int, lambda s: s._pad_step, notify=changed)
    padNote = Property(int, lambda s: s._pad_note, notify=changed)
    rollZoom = Property(float, lambda s: s._zoom, notify=changed)
    screen = Property(int, lambda s: s._screen, notify=changed)
    liveState = Property(str, lambda s: s._live_state, notify=changed)
    selectedEncoder = Property(int, lambda s: s._selected, notify=changed)
    drumMode = Property(bool, lambda s: s._mode == 3, notify=changed)
    synthMode = Property(bool, lambda s: s._mode == 2, notify=changed)
    selectedMode = Property(int, lambda s: s._mode, notify=changed)
    settingsSection = Property(str, lambda s: s._settings, notify=changed)
    encoding = Property(str, lambda s: s._encoding, notify=changed)
    roomTitle = Property(str, lambda s: 'Selva Nocturna', constant=True)
    nextTitle = Property(str, lambda s: 'Kalimba Dusk', constant=True)
    keyName = Property(str, lambda s: 'A MIN', constant=True)
    bpm = Property(float, lambda s: 124., constant=True)
    bar = Property(int, lambda s: 37, constant=True)
    phrase = Property(int, lambda s: 5, constant=True)
    section = Property(str, lambda s: 'DROP', constant=True)

    @Slot(int)
    def setScreen(self, value):
        if 1 <= value <= len(SCREENS):
            descriptor = SCREENS[value-1]
            if descriptor.mode == 'SHARED' and SCREENS[self._screen-1].mode != 'SHARED':
                self._return_screen = self._screen
            if descriptor.mode == 'DRUM':
                self._mode = 3
            if descriptor.mode.startswith('CONTROLLER'):
                self._mode = 0
            elif descriptor.mode == 'STANDALONE' and not (self._mode == 2 and descriptor.name.startswith('Synth')):
                self._mode = 1
            self._screen = value
            live_state = descriptor.state if descriptor.state in ('offline', 'empty', 'loading') else 'normal'
            if live_state != self._live_state:
                self.setLiveState(live_state)
            self._connections.queue_update(2, {'status': 'warn' if descriptor.state == 'no-live' else
                                             'ok' if not self._mode or descriptor.name == 'Routing' else 'off'})
            if hasattr(self, 'synth'):
                self.synth.configure(descriptor.name)
            if hasattr(self, "drum"):
                self.drum.configure(descriptor.name)
            self.refresh_keys()
            self._notify()

    @Slot(str)
    def navigate(self, name):
        self.setScreen(screen_id(name))

    @Slot(int)
    def cycleScreen(self, direction):
        self.setScreen((self._screen - 1 + direction) % len(SCREENS) + 1)

    @Slot(str)
    def typeKey(self, value):
        if value == '⌫':
            self._query = self._query[:-1]
        elif value in ('ENTER', 'CLOSE'):
            self._keyboard = False
        elif value == '123':
            self._query += '1'
        else:
            self._query += ' ' if value == 'SPACE' else value.lower()
        self._notify()

    @Slot(int, int)
    def panRoll(self, steps, notes):
        step_view = self.screenName == 'SequencerSteps'
        self._pad_step = max(16 if step_view else 0, min(24 if step_view else 56, self._pad_step + steps))
        self._pad_note = max(0, min(4 if step_view else 17, self._pad_note + notes))
        self._notify()

    @Slot(float)
    def zoomRoll(self, scale):
        self._zoom = max(1., min(4., scale))
        self._notify()

    @Slot(int, int)
    def pressPad(self, column, row):
        if not (0 <= column < 8 and 0 <= row < 4):
            return
        if self.screenName == 'SequencerSteps':
            self._step_lane = min(7, self._pad_note+row)
            self._step_column = min(15, max(0, self._pad_step-16)+column)
            self.edit_step(0, 0)
            return
        if self.screenName != 'SequencerPianoRoll':
            return
        model = self._standalone._models['rollNotes']
        model.flush()
        step, pitch = self._pad_step + column, self._pad_note + [0, 2, 3, 5][row]
        found = next((i for i, r in enumerate(model.rows) if r['entry']['step'] == step and r['entry']['pitch'] == pitch), None)
        for i in range(model.rowCount()):
            self._standalone.update_entry('rollNotes', i, {'selected': i == found, 'sh': 'selected' if i == found else 'none'})
        if found is None:
            model.flush()
            note = dict(step=step, pitch=pitch, length=1, velocity=104, micro=0, owner='you', selected=True,
                        l=step / .64, t=(24-pitch)*4., w=1/.64, h=4., c='#3D8CFF', sh='selected')
            model.replace(model.rows + [{'entry': note}])

    @Slot(int,int)
    def selectStep(self, lane, column):
        self._step_lane = max(0,min(7,lane))
        self._step_column = max(0,min(15,column))
        self.selectEncoder(1)
        if hasattr(self, 'surface'):
            self.surface.touch.inspect(f'Step {self._step_column+1} · lane {self._step_lane+1} · encoder edits')

    def edit_step(self, index, delta):
        model = self._standalone._models['stepLanes']
        entry = copy.deepcopy(model._pending.get(self._step_lane, {}).get('entry', model.rows[self._step_lane]['entry']))
        step = entry['steps'][self._step_column]
        if index == 0:
            step['on'] = not step['on']
        elif index == 1:
            step['h'] = max(1, min(100, step['h']+delta))
        elif index == 2:
            step['prob'] = str(max(1, min(100, int(step['prob'] or 100)+delta)))
        elif index == 3:
            step['mi'] = max(-50, min(50, step['mi']+delta))
        step['c'] = entry['c']
        step['mc'] = '#F2F2F2'
        model.queue_update(self._step_lane, {'entry': entry})
        if index:
            self._softKeys.queue_update(index, {'value': str(step[{1:'h', 2:'prob', 3:'mi'}[index]])})

    def edit_note(self, index, delta):
        model = self._standalone._models['rollNotes']
        selected = next((i for i, r in enumerate(model.rows) if r['entry']['selected']), None)
        if selected is None:
            return
        note = dict(model._pending.get(selected, {}).get('entry', model.rows[selected]['entry']))
        field, low, high = {1: ('pitch', 0, 24), 2: ('step', 0, 63), 3: ('length', 1, 16),
                            4: ('velocity', 1, 127), 5: ('micro', -50, 50)}[index]
        note[field] = max(low, min(high, note[field] + delta))
        note['length'] = min(note['length'], 64-note['step'])
        note.update(l=note['step']/.64, w=note['length']/.64, t=(24-note['pitch'])*4.,
                    owner='you', c='#3D8CFF')
        model.queue_update(selected, {'entry': note})
        self._softKeys.queue_update(index, {'value': str(note[field])})

    def vary_engine(self):
        model = self._standalone._models['rollNotes']
        model.flush()
        occupied = {(step, r['entry']['pitch']) for r in model.rows if r['entry']['owner'] == 'you'
                    for step in range(r['entry']['step'], r['entry']['step'] + r['entry']['length'])}
        for i, row in enumerate(model.rows):
            note = row['entry']
            if note['owner'] == 'engine':
                step = (note['step'] + 1) % (65-note['length'])
                if not any((s, note['pitch']) in occupied for s in range(step, step+note['length'])):
                    self._standalone.update_entry('rollNotes', i, {'step': step, 'l': step/.64})

    @Slot(str)
    def setLiveState(self, value):
        if value not in ('normal','offline','empty','loading'):
            return
        self._live_state = value
        rows = cells()
        for cell in rows:
            if cell['row'] >= 2 and value != 'normal':
                cell.update(cellState='loading' if value == 'loading' else 'empty',
                            title='Paused' if value == 'offline' else 'Generating…' if value == 'loading' else '—',
                            subtitle='')
        self._cells.replace(rows)
        self._notify()

    @Slot(int)
    def selectEncoder(self, value):
        self._selected = value % 8
        self._notify()

    @Slot(int, int)
    def turnEncoder(self, index, delta):
        if not 0 <= index < 8:
            return
        if self._selected != index:
            self._selected = index
            self._property_dirty = True
        if hasattr(self, 'drum') and self.screenName.startswith('Drum'):
            self.drum.turn(index, delta)
            return
        if hasattr(self, 'synth') and self.screenName.startswith('Synth'):
            if self.screenName == 'SynthScale' and hasattr(self, 'surface'):
                self.surface.apply_frame('Encoder', dict(indice=index, delta=delta))
            else:
                self.synth.turn(index, delta)
            return
        if self._screen == 2:
            self._mode = index % 4
            self.refresh_keys()
            self._property_dirty = True
            return
        if self._screen > 9 and SCREENS[self._screen-1].source != 'LiveSession':
            if self.screenName == 'SequencerSteps' and index in (1, 2, 3):
                self.edit_step(index, delta)
            elif self.screenName == 'SequencerPianoRoll' and index in (1, 2, 3, 4, 5):
                self.edit_note(index, delta)
            elif self.screenName == 'SequencerPianoRoll' and index == 7:
                self.zoomRoll(self._zoom + delta * .25)
            else:
                self._review_selection = max(0, self._review_selection + delta)
                self._softKeys.queue_update(index, {'value': str(self._review_selection), 'turning': True})
            return
        if self._screen == 6 and index < 4:
            option = self._options.rows[index]
            energy = self._options._pending.get(index, {}).get('energy', option['energy'])
            self._options.queue_update(index, {'energy': max(1, min(5, energy + delta))})
            return
        old = self._channels._pending.get(index, {}).get('level', self._channels.rows[index]['level'])
        level = max(0., min(1., old + delta * .01))
        self._channels.queue_update(index, {'level': level, 'meter': level * .96})
        self._cells.queue_update(index, {'level': level})
        self._softKeys.queue_update(index, {'turning': True, 'position': level, 'value': str(round(level*100))})

    @Slot(int)
    def pressEncoder(self, index):
        if not 0 <= index < 8:
            return
        self._softKeys.flush()
        if not self._softKeys.rows[index]['enabled']: return
        action = self._softKeys.rows[index]['action']
        self.triggerAction(action or 'demo:'+self._softKeys.rows[index]['label'])

    @Slot(str)
    def triggerAction(self, action):
        if action.startswith('hands:'):
            self.preferences.toggleHand(int(action.split(':')[1]))
        elif action == 'pad-velocity':
            self.preferences.cyclePadVelocity()
        elif action.startswith('demo:') and hasattr(self, 'surface'):
            self.surface.touch.inspect('DEMO · '+action[5:]+' · no connected service')
        elif action.startswith('drum:') and hasattr(self, 'drum'):
            self.drum.action(action[5:])
        elif action.startswith('synth:') and hasattr(self, 'synth'):
            self.synth.action(action[6:])
        elif action.startswith('nav:'):
            self.navigate(action[4:])
        elif action == 'back':
            self.setScreen(self._return_screen if SCREENS[self._screen-1].mode == 'SHARED' else 5 if self._mode else screen_id('Session'))
        elif action == 'close-window':
            self.setScreen(self._return_screen)
        elif action == 'vary-notes':
            self.vary_engine()
        elif action == 'keyboard':
            self._keyboard = not self._keyboard
            self._notify()
        elif action.startswith('review:'):
            self._review_selection = int(action.split(':')[1])
            if self.screenName == 'Motor':
                model = self._controller._models['engCards']
                model.flush()
                if self._review_selection < 4:
                    for i in range(4):
                        if self._controller.rows('engCards')[i]['state'] != 'loading':
                            self._controller.update_entry('engCards', i, {'state': 'cue' if i == self._review_selection else 'option'})
                elif self._review_selection == 4:
                    template = copy.deepcopy(self._controller.rows('engCards')[0])
                    template.update(letter='D', state='option', sound='Operator · Sub D', enc='ENC 4 CUE')
                    self._controller.update_entry('engCards', 3, template)
                elif self._review_selection in (5, 6):
                    for i, row in enumerate(self._controller.rows('engCards')):
                        if row['state'] == 'cue':
                            self._controller.update_entry('engCards', i, {'state': 'room' if self._review_selection == 5 else 'option'})
            self._softKeys.queue_update(self._review_selection, {'lit': True, 'value': 'selected'})
        elif action.startswith('screen:'):
            self.setScreen(int(action.split(':')[1]))
        elif action.startswith('mode:'):
            self._mode = int(action[-1])
            self.setScreen(screen_id("DrumRack") if self._mode == 3 else screen_id("SynthParts") if self._mode == 2 else 5 if self._mode else screen_id("Session"))
        elif action.startswith('settings:'):
            self._settings = action.split(':')[1]
            if self._settings != 'Surface':
                self.navigate('SynthSettings'+self._settings)
                return
            self.refresh_keys()
            self._notify()
        elif action.startswith('cue:'):
            selected = int(action[-1])
            for i in range(4):
                self._options.queue_update(i, {'optionState': 'cue' if i == selected else 'option'})
        elif action == 'accept':
            self._options.flush()
            chosen = next((o for o in self._options.rows if o['optionState'] == 'cue'), None)
            if chosen is None:
                return
            self._cells.queue_update(12, {'cellState':'queued', 'title':chosen['title'], 'subtitle':'enters phrase'})
            self.setScreen(5)
        elif action == 'generate':
            self._options.queue_update(3, {'optionState':'option', 'title':'Juno Sub', 'energy':3,
                                          'notes':copy.deepcopy(options()[0]['notes'])})
        elif action == 'skip':
            self._options.queue_update(1, {'optionState':'option'})
        elif action == 'drop':
            self.setScreen(7)
        elif action == 'encoding':
            self._encoding = 'stripe' if self._encoding == 'fill' else 'fill'
            self._notify()

    def refresh_keys(self):
        descriptor = SCREENS[self._screen-1]
        if descriptor.name.startswith('Drum') and hasattr(self, 'drum'):
            self.update_soft_keys(self.drum.keys())
            return
        if descriptor.name.startswith('Synth') and hasattr(self, 'synth'):
            self.update_soft_keys(self.synth.keys())
            return
        if descriptor.keys:
            collection, name = descriptor.keys.split('.')
            data = self._standalone if collection == 's' else self._controller
            actions = {
                'Search': {0: 'keyboard', 7: 'nav:LiveSession'},
                'SequencerSteps': {0: 'nav:SequencerPianoRoll'},
                'SequencerPianoRoll': {0: 'nav:SequencerSteps', 6: 'vary-notes'},
                'Pads': {1: 'nav:Scale'}, 'NoteMelodic': {1: 'nav:Scale'}, 'Note': {6: 'nav:Repeat', 7: 'nav:Scale'},
                'SessionNoLive': {0: 'nav:Session', 6: 'mode:1', 7: 'screen:4'},
                'Scale': {7: 'close-window'}, 'Tempo': {7: 'close-window'},
                'Repeat': {}, 'Library': {7: 'nav:LiveSession'}, 'Log': {7: 'nav:LiveSession'},
                'Browse': {7: 'nav:Session'},
            }.get(descriptor.name, {})
            keys = [key(r.get('l', '—'), r.get('v', ''), r.get('c', 'PURPLE' if r.get('ai') else ''),
                        r.get('on', False), actions.get(i, f'review:{i}'), r.get('turn', False), r.get('p', -1))
                    for i, r in enumerate(data.rows(name))]
            if descriptor.name == 'NoteMelodic':
                keys[0]['roleName'] = '#E37FB0'
            if descriptor.name == 'Motor':
                keys = [key('Option '+chr(65+i), 'CUE', '#6E9BE8', i == 1, f'review:{i}') for i in range(4)]
                keys += [key('Generate', 'another', 'PURPLE', action='review:4'),
                         key('Accept', 'write clip', lit=True, action='review:5'),
                         key('Skip', action='review:6'), key('Back', action='nav:Session')]
        elif self._screen in (3,5) or descriptor.source == 'LiveSession':
            keys = [key(r, 'MUTE' if i == 6 else str(round(LEVELS[i]*100)), r, i != 6,
                        'screen:6', i == 4, LEVELS[i] if i == 4 else -1) for i,r in enumerate(ROLES)]
        elif self._screen == 2:
            keys = [key('Controller',lit=self._mode==0,action='mode:0'),key('Standalone','enter',lit=self._mode==1,action='mode:1'),key('Synth','play',lit=self._mode==2,action='mode:2'),key('Drum','jam',lit=self._mode==3,action='mode:3')]
            keys += [key() for _ in range(2)] + [key('Settings',action='screen:4'),key('Power',action='screen:1')]
        elif self._screen == 4:
            prefs = self.preferences
            keys = [key('Hands', 'ON' if prefs.handsEnabled else 'OFF', action='hands:0'),
                    key('Left IR', 'ON' if prefs.leftHand else 'OFF', action='hands:1'),
                    key('Right IR', 'ON' if prefs.rightHand else 'OFF', action='hands:2'),
                    key('Pad velocity', str(prefs.padVelocity), action='pad-velocity'),
                    key('Audio', action='nav:SynthSettingsAudio'),
                    key('Connections', action='nav:SynthSettingsConnections'),
                    key('System', action='nav:SynthSettingsSystem'),
                    key('Back', action='nav:'+('DrumRack' if self.drumMode else 'SynthParts' if self.synthMode else 'ModeSelect'))]
        elif self._screen == 6:
            keys = [key('Option '+chr(65+i),'CUE','BASS',i == 1,'cue:'+str(i)) for i in range(4)]
            keys += [key('Generate','another',action='generate'),key('Accept','next phrase',lit=True,action='accept'),
                     key('Skip',action='skip'),key('Back',action='screen:5')]
        elif self._screen == 7:
            keys = [key('Swap','running',lit=True),key('Break'),key('Cut'),key('Length','8 PH'),
                    key('Order','Auto'),key('Hold','step 3'),key('Drop',action='drop'),key('Abort',action='screen:5')]
        elif self._screen == 8:
            keys = [key(r,v,r,False,'screen:6',i==4,.3 if i==4 else -1) for i,(r,v) in enumerate(zip(
                ROLES,['OPEN','HP 46','HP 92','HP 220','LP 2.0k','OPEN','OPEN','LP 8.4k']))]
        elif self._screen == 9:
            keys = [key(n,v,lit=i in (0,4)) for i,(n,v) in enumerate([
                ('L target','Filter'),('L range','6–26 cm'),('L curve','Exp'),('L return','300 ms'),
                ('R target','Notes'),('R scale','A MIN'),('R octave','4'),('Hold','Off')])]
        else:
            keys = [key() for _ in range(8)]
        self.update_soft_keys(keys)

    def update_soft_keys(self, rows):
        # Preserve pressed QML delegates across clock/meter/parameter updates.
        if len(self._softKeys.rows) != len(rows):
            self._softKeys.replace(rows)
        else:
            for i, row in enumerate(rows):
                if self._softKeys.rows[i] != row:
                    self._softKeys.queue_update(i, row)
            self._softKeys.flush()
