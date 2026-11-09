import copy
from PySide6.QtCore import QObject, Property, Signal, Slot, QTimer
from models import SnapshotModel

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
        self._screen = 5
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
            dict(side='LEFT HAND',target='FILTER',distance=12,inRange=True,value='LP 1.8k',rangeMin=6,rangeMax=26),
            dict(side='RIGHT HAND',target='NOTES',distance=17,inRange=True,value='C4',rangeMin=6,rangeMax=28)],
            'side target distance inRange value rangeMin rangeMax'.split(), self)
        self._toasts = SnapshotModel([
            dict(icon='LINK',message='Link: 3 devices',value='124.0',tone='link',opacity=.6),
            dict(icon='KICK',message='KICK enters on the phrase',value='IN 4 BARS',tone='KICK',opacity=1.)],
            'icon message value tone opacity'.split(), self)
        self._property_dirty = False
        self._frame_timer = QTimer(self)
        self._frame_timer.setInterval(16)
        self._frame_timer.timeout.connect(self.flush_properties)
        self._frame_timer.start()
        self.refresh_keys()

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
    screen = Property(int, lambda s: s._screen, notify=changed)
    liveState = Property(str, lambda s: s._live_state, notify=changed)
    selectedEncoder = Property(int, lambda s: s._selected, notify=changed)
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
        if 1 <= value <= 9:
            self._screen = value
            self.refresh_keys()
            self.changed.emit()

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
        self._connections.queue_update(0, {'status': 'warn' if value == 'offline' else 'ok'})
        self.changed.emit()

    @Slot(int)
    def selectEncoder(self, value):
        self._selected = value % 8
        self.changed.emit()

    @Slot(int, int)
    def turnEncoder(self, index, delta):
        if not 0 <= index < 8:
            return
        if self._selected != index:
            self._selected = index
            self._property_dirty = True
        if self._screen == 2:
            self._mode = index % 2
            self._property_dirty = True
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
        action = self._softKeys.rows[index]['action']
        self.triggerAction(action)

    @Slot(str)
    def triggerAction(self, action):
        if action.startswith('screen:'):
            self.setScreen(int(action.split(':')[1]))
        elif action.startswith('mode:'):
            self._mode = int(action[-1])
            self.setScreen(5 if self._mode else 2)
        elif action.startswith('settings:'):
            self._settings = action.split(':')[1]
            self.changed.emit()
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
            self.changed.emit()

    def refresh_keys(self):
        if self._screen in (3,5):
            keys = [key(r, 'MUTE' if i == 6 else str(round(LEVELS[i]*100)), r, i != 6,
                        'screen:6', i == 4, LEVELS[i] if i == 4 else -1) for i,r in enumerate(ROLES)]
        elif self._screen == 2:
            keys = [key('Controller',action='mode:0'),key('Standalone','enter',lit=True,action='mode:1')]
            keys += [key() for _ in range(4)] + [key('Settings',action='screen:4'),key('Power',action='screen:1')]
        elif self._screen == 4:
            keys = [key(n,lit=n == self._settings,action='settings:'+n) for n in ['Audio','Connections','Surface','System']]
            keys += [key(),key(),key('Calibrate','FSR 2',action='settings:Surface'),key('Back',action='screen:2')]
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
        self._softKeys.replace(keys)
