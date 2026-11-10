"""Presentation state: metadata stays host-owned; render commits stay frame-owned."""
from copy import deepcopy
from PySide6.QtCore import QObject, Property, Signal, Slot
from models import SnapshotModel
from fm_algorithms import algorithm
from synth_catalog import ENGINE_LABELS

SYNTH_SCREENS = {
    'SynthParts': ('juno','FILTER',0), 'SynthPartsStates': ('juno','FILTER',0),
    'SynthFilter': ('juno','FILTER',0), 'SynthNotResponding': ('juno','FILTER',0),
    'SynthLoading': ('juno','FILTER',0), 'SynthOsc': ('arp2600','OSC',0),
    'SynthEnv': ('model_d','ENV',0), 'SynthEnvBank2': ('model_d','ENV',1),
    'SynthLfo': ('juno','LFO',0), 'SynthFm': ('dx7','OPS',0), 'SynthFmFocus': ('dx7','OP 2',0),
    'SynthDx7': ('dx7_6op','OPS',0), 'SynthDx7Bank2': ('dx7_6op','OPS',1),
    'SynthDx7Focus': ('dx7_6op','OP 2',1), 'SynthGroove': ('juno','GROOVE',0),
    'SynthFx': ('dx7','FX',1), 'SynthMacros': ('model_d','MACROS',0),
    'SynthHybrid': ('hybrid','HYBRID',0), 'SynthHybridA': ('hybrid','A FILTER',0),
    'SynthHybridB': ('hybrid','B FILTER',0), 'SynthPresets': ('model_d','FILTER',0),
    'SynthSaveAs': ('model_d','FILTER',0), 'SynthSettingsAudio': ('juno','FILTER',0),
    'SynthSettingsConnections': ('juno','FILTER',0), 'SynthSettingsSystem': ('juno','FILTER',0),
}


def relative_value(meta, value, delta, fine=False):
    discrete = meta['kind'] in ('enum','bool')
    step = max(1,meta['step']) if discrete else meta['step'] * (.1 if fine else 1)
    value = max(meta['min'], min(meta['max'], value + delta*step))
    return int(round(value)) if discrete else round(value,8)


def formatted(meta, value):
    if meta['kind'] == 'bool':
        return 'ON' if value else 'OFF'
    if meta['kind'] == 'enum':
        options = meta.get('options',[])
        return str(options[int(value)]) if 0 <= int(value) < len(options) else str(value)
    return f'{value:+g}' if meta['kind'] == 'bipolar' else f'{value:g}'


class SynthState(QObject):
    changed = Signal()
    wakeRequested = Signal()

    def __init__(self, console, client=None, parent=None):
        super().__init__(parent)
        self.console = console
        self.client = client
        self.preview = client is None
        self.fake = None
        if self.preview:
            from fake_synth_host import FakeSynth
            self.fake = FakeSynth()
        self._parts = []
        self._metadata = {}
        self._values = {}
        self._meters = []
        self._cpu = {}
        self._hello = {}
        self._engines = []
        self._status = 'preview' if self.preview else 'connecting'
        self._loading = False
        self._error = ''
        self._part = 3
        self._page = 'FILTER'
        self._bank = 0
        self._focus = 0
        self._fine = False
        self._screen = ''
        self._variant = ''
        self._name = 'Moog Round Dark'
        self._presets = []
        self._preset_index = 0
        self._engine_filter = ''
        self._tag_filter = ''
        self._dirty = True
        self._outbound = {}
        self._serial = 0
        self._preserve_selection = False
        self._last_presentation = None
        self.in_surface_batch = False
        self.partsModel = SnapshotModel([],['entry'],self)
        self.cellsModel = SnapshotModel([],['entry'],self)
        self.presetsModel = SnapshotModel([],['entry'],self)
        if client:
            client.message.connect(self._handle)
            client.wakeRequested.connect(self.wakeRequested)
        else:
            for cmd, fields in [('hello',dict(client='preview')),('get_state',{}),('list_presets',{})]:
                self.request(cmd,**fields)
        self.flush_frame()

    parts = Property(QObject,lambda s:s.partsModel,constant=True)
    cells = Property(QObject,lambda s:s.cellsModel,constant=True)
    presets = Property(QObject,lambda s:s.presetsModel,constant=True)
    values = Property('QVariantMap',lambda s:dict(s._values.get(s._part,{})),notify=changed)
    selectedPart = Property(int,lambda s:s._part,notify=changed)
    pageName = Property(str,lambda s:s._page,notify=changed)
    bankIndex = Property(int,lambda s:s._bank,notify=changed)
    bankCount = Property(int,lambda s:len(s.current_page.get('banks',[])),notify=changed)
    pages = Property('QVariantList',lambda s:[p['name'] for p in s.current_pages],notify=changed)
    engine = Property(str,lambda s:s.current_part.get('engine',''),notify=changed)
    engineLabel = Property(str,lambda s:ENGINE_LABELS.get(s.current_part.get('engine',''),s.current_part.get('engine','')),notify=changed)
    engineOptions = Property('QVariantList',lambda s:s._engines,notify=changed)
    partInfo = Property('QVariantMap',lambda s:s.current_part,notify=changed)
    status = Property(str,lambda s:s._variant or ('loading' if s._loading else s._status),notify=changed)
    error = Property(str,lambda s:s._error,notify=changed)
    fine = Property(bool,lambda s:s._fine,notify=changed)
    focusedCell = Property(int,lambda s:s._focus,notify=changed)
    operatorCount = Property(int,lambda s:6 if s.engine == 'dx7_6op' else 4,notify=changed)
    selectedOperator = Property(int,lambda s:int(s._page[3:]) if s._page.startswith('OP ') and s._page[3:].isdigit() else 0,notify=changed)
    algorithm = Property('QVariantMap',lambda s:deepcopy(algorithm(s.operatorCount,s._values.get(s._part,{}).get('fm.algorithm',1))),notify=changed)
    saveName = Property(str,lambda s:s._name,notify=changed)
    presetIndex = Property(int,lambda s:s._preset_index,notify=changed)
    cpu = Property('QVariantMap',lambda s:s._cpu,notify=changed)
    hostInfo = Property('QVariantMap',lambda s:s._hello,notify=changed)
    demo = Property(bool,lambda s:s.preview,constant=True)
    engineFilter = Property(str,lambda s:s._engine_filter or 'ALL ENGINES',notify=changed)
    tagFilter = Property(str,lambda s:s._tag_filter or 'ALL',notify=changed)

    @property
    def current_part(self):
        return next((p for p in self._parts if p['part'] == self._part),{})

    @property
    def current_pages(self):
        return self._metadata.get(self.current_part.get('engine'),[])

    @property
    def current_page(self):
        return next((p for p in self.current_pages if p['name'] == self._page),{})

    @property
    def bank(self):
        banks = self.current_page.get('banks',[])
        return banks[min(self._bank,len(banks)-1)] if banks else []

    def request(self, cmd, **fields):
        if self.client:
            return self.client.request(cmd,**fields)
        self._serial += 1
        response, events = self.fake.request(dict(id=self._serial,cmd=cmd,**fields))
        response.update(_cmd=cmd,_fields=fields)
        self._handle(response)
        for event in events:
            self._handle(event)
        return self._serial

    def _fetch_part(self, part):
        if part['engine'] not in self._metadata:
            self.request('list_params',engine=part['engine'])
        self.request('get_params',part=part['part'])

    def _handle(self, msg):
        event, cmd, fields = msg.get('event'),msg.get('_cmd'),msg.get('_fields',{})
        if event == 'connection':
            self._status = msg['state']
            if self._status == 'ready':
                self._error = ''
        elif event == 'meters':
            self._meters = msg['parts']
        elif event == 'cpu':
            self._cpu = {k:v for k,v in msg.items() if k != 'event'}
        elif event == 'param':
            self._values.setdefault(msg['part'],{})[msg['param']] = msg['value']
        elif event == 'state':
            p = msg['part'] if isinstance(msg.get('part'),dict) else {k:v for k,v in msg.items() if k != 'event'}
            old = next((o for o in self._parts if o['part'] == p['part']),{})
            changed_engine = old.get('engine') != p['engine']
            self._parts = [p if o['part'] == p['part'] else o for o in self._parts]
            if changed_engine:
                # Values of the previous engine must not stand in for the new one while get_params is in flight.
                self._values.pop(p['part'],None)
                self._fetch_part(p)
        elif event == 'protocol_error':
            self._error = msg['error']
        elif 'ok' in msg:
            if cmd == 'load_preset':
                self._loading = False
            if not msg['ok']:
                self._error = msg.get('error','Host error')
                if cmd == 'set_param' and self._status == 'ready':
                    self.request('get_params',part=fields['part'])
            elif cmd == 'hello':
                self._metadata.clear()
                self._hello = {k:v for k,v in msg.items() if not k.startswith('_') and k not in ('id','ok')}
                self._engines = msg['engines']
            elif cmd == 'get_state':
                self._parts = msg['parts']
                for p in self._parts:
                    self._fetch_part(p)
                self.request('list_presets',role=self.current_part.get('role',''))
            elif cmd == 'set_engine':
                # The host resets the part to the engine defaults even when the engine name did not change,
                # so no state event may follow: refetch here instead of relying on it.
                self._values.pop(fields['part'],None)
                self._fetch_part({'part':fields['part'],'engine':fields['engine']})
            elif cmd == 'list_params':
                self._metadata[fields['engine']] = msg['pages']
            elif cmd == 'get_params':
                if 'page' in fields:
                    self._values.setdefault(fields['part'],{}).update(msg['values'])
                else:
                    self._values[fields['part']] = dict(msg['values'])
            elif cmd == 'set_param':
                self._values.setdefault(fields['part'],{})[fields['param']] = msg.get('value',fields['value'])
            elif cmd == 'list_presets':
                self._presets = msg['presets']
                self._preset_index = min(self._preset_index,max(0,len(self._presets)-1))
            elif cmd in ('set_engine','load_preset','save_preset','set_level','set_mute'):
                p = msg.get('part')
                if isinstance(p,dict):
                    self._parts = [p if o['part'] == p['part'] else o for o in self._parts]
                if cmd in ('set_engine','load_preset'):
                    self.request('get_state')
                if cmd == 'save_preset':
                    self.request('list_presets',role=self.current_part.get('role',''))
                    self.openScreen('SynthPresets')
        self._dirty = True
        self.wakeRequested.emit()

    def configure(self, name):
        if name not in SYNTH_SCREENS:
            self._screen = ''
            return
        self._screen = name
        engine, page, bank = SYNTH_SCREENS[name]
        if self.preview and not self._preserve_selection:
            self._part = 4 if engine == 'hybrid' else 2 if engine in ('dx7','dx7_6op') else 1 if engine == 'model_d' else 3
            if self.current_part.get('engine') != engine:
                self.request('set_engine',part=self._part,engine=engine)
            self._variant = {'SynthNotResponding':'not_responding','SynthLoading':'loading'}.get(name,'')
            self._meters = self.fake.meters()['parts']
            self._cpu = dict(mean_pct=21.,max_pct=24.,xruns=0)
        else:
            self._variant = ''
        self._preserve_selection = False
        self._page, self._bank = page, bank
        self._focus = 0
        if name in ('SynthPresets','SynthSaveAs'):
            self.filterPresets(self._engine_filter,self._tag_filter)
        self._dirty = True
        self.flush_frame()

    @Slot(int)
    def selectPart(self, part):
        if part == self._part:
            return
        if 1 <= part <= 4:
            self._part = part
            self._bank = 0
            self._page = self.current_pages[0]['name'] if self.current_pages else ''
            self.filterPresets('','')
            self._dirty = True
            self.flush_frame()

    @Slot(str)
    def selectPage(self, page):
        if page in self.pages:
            if page == 'HYBRID' and self._screen != 'SynthHybrid':
                self.openScreen('SynthHybrid')
                return
            self._page, self._bank = page, 0
            self._dirty = True
            self.flush_frame()

    @Slot(int)
    def nextPage(self, delta):
        pages = self.pages
        if pages:
            index = pages.index(self._page) if self._page in pages else 0
            self.selectPage(pages[(index+delta)%len(pages)])

    @Slot(int)
    def chooseBank(self, bank):
        self._bank = max(0,min(self.bankCount-1,bank))
        self._dirty = True
        self.flush_frame()

    @Slot()
    def nextBank(self):
        self._bank = (self._bank+1)%max(1,self.bankCount)
        self._dirty = True
        self.flush_frame()

    @Slot(bool)
    def setFine(self, value):
        self._fine = value
        self._dirty = True
        self.wakeRequested.emit()

    @Slot(int,int)
    def turn(self, index, delta):
        if self.status not in ('ready','preview'):
            return
        if self._screen.startswith('SynthSettings') or self._screen == 'SynthSaveAs':
            return
        if self._screen in ('SynthParts','SynthPartsStates'):
            if index < 4:
                self.selectPart(index+1)
            elif index == 5 and self._engines:
                current = self._engines.index(self.engine)
                self.changeEngine(self._engines[(current+delta)%len(self._engines)])
            elif index == 7:
                self.request('set_level',part=self._part,db=self.current_part['level_db']+delta*.5)
            return
        if self._screen == 'SynthPresets':
            if index == 0:
                self._preset_index = max(0,min(len(self._presets)-1,self._preset_index+delta))
            elif index == 1:
                opts = ['']+self._engines
                self.filterPresets(opts[(opts.index(self._engine_filter)+delta)%len(opts)],self._tag_filter)
            elif index == 2:
                opts = ['','SUB','SUS','PLK','AIR','PAD','MID']
                self.filterPresets(self._engine_filter,opts[(opts.index(self._tag_filter)+delta)%len(opts)])
            self._dirty = True
            return
        if index < len(self.bank):
            meta = self.bank[index]
            vals = self._values.setdefault(self._part,{})
            value = relative_value(meta,vals.get(meta['id'],meta['default']),delta,self._fine)
            vals[meta['id']] = value
            self._focus = index
            self._outbound[(self._part,meta['id'])] = value
            self._dirty = True
            self.wakeRequested.emit()

    @Slot(str)
    def changeEngine(self, name):
        if name in self._engines and self.status in ('ready','preview'):
            self._outbound = {k:v for k,v in self._outbound.items() if k[0] != self._part}
            self.request('set_engine',part=self._part,engine=name)

    @Slot(int)
    def focusOperator(self, op):
        if 1 <= op <= self.operatorCount:
            self.selectPage(f'OP {op}')

    @Slot(int)
    def chooseAlgorithm(self, number):
        p = next((p for pg in self.current_pages for b in pg['banks'] for p in b if p['id'] == 'fm.algorithm'),None)
        if p and self.status in ('ready','preview'):
            self.request('set_param',part=self._part,param=p['id'],value=max(p['min'],min(p['max'],number)))

    @Slot(str,str)
    def filterPresets(self, engine, tag):
        self._engine_filter,self._tag_filter = engine,tag
        fields = dict(role=self.current_part.get('role',''))
        if engine:
            fields['engine'] = engine
        if tag:
            fields['tag'] = tag
        self.request('list_presets',**fields)

    @Slot(int)
    def selectPreset(self, index):
        if 0 <= index < len(self._presets):
            self._preset_index = index
            self._dirty = True
            self.flush_frame()

    @Slot()
    def loadPreset(self):
        if self._presets and self.status in ('ready','preview'):
            self._outbound = {k:v for k,v in self._outbound.items() if k[0] != self._part}
            self._loading = True
            self._error = ''
            self.request('load_preset',part=self._part,name=self._presets[self._preset_index]['name'])
            self._dirty = True
            self.wakeRequested.emit()

    @Slot(str)
    def typeKey(self, key):
        if key == 'SAVE':
            self.savePreset()
        elif key == 'CANCEL':
            self.openScreen('SynthPresets')
        elif key == '⌫':
            self._name = self._name[:-1]
        elif len(self._name) < 24:
            self._name += ' ' if key == 'SPACE' else key
        self._dirty = True
        self.flush_frame()

    @Slot()
    def savePreset(self):
        if self._name.strip() and self.status in ('ready','preview'):
            self._send_edits()
            self.request('save_preset',part=self._part,name=self._name.strip())

    @Slot(str)
    def openScreen(self, name):
        self._preserve_selection = True
        self.console.navigate(name)

    @Slot()
    def retry(self):
        self._variant = ''
        if self.client:
            self.client.reconnect()
        else:
            self._status = 'preview'
        self._dirty = True
        self.flush_frame()

    def keys(self):
        from fake_data import key
        nav = lambda label, action, value='': key(label,value,action='synth:'+action)
        if self._screen.startswith('SynthSettings'):
            return [key(n,action='nav:'+dest,lit=self._screen==dest) for n,dest in [
                ('Audio','SynthSettingsAudio'),('Connections','SynthSettingsConnections'),('Surface','Settings'),('System','SynthSettingsSystem')]] + [key(),key(),nav('Retry','retry'),key('Back',action='nav:SynthParts')]
        if self._screen in ('SynthParts','SynthPartsStates'):
            keys = [key(p['role'],ENGINE_LABELS.get(p['engine'],p['engine']),p['role'],p['part']==self._part,'synth:open:'+str(p['part'])) for p in self._parts]
            keys += [key() for _ in range(4-len(keys))]
            keys += [key('BRAIN','OFFLINE',action='nav:SynthSettingsConnections'),key('Engine',self.engineLabel),key('Mute','ON' if self.current_part.get('mute') else 'OFF',action='synth:mute'),key('Level',f"{self.current_part.get('level_db',0):g} dB")]
            return keys
        if self._screen == 'SynthHybrid':
            return [nav('Edit A','half-a'),key(),key(),key('Morph',str(self.values.get('hybrid.morph',0))),
                    key('Layer',formatted(self.bank[4],self.values.get('hybrid.layer',0)) if len(self.bank)>4 else ''),
                    key(),key('Presets',action='synth:nav:SynthPresets'),nav('Edit B','half-b')]
        if self._screen == 'SynthSaveAs':
            return [key() for _ in range(5)]+[nav('Save','save'),key(),key('Cancel',action='synth:nav:SynthPresets')]
        if self._screen == 'SynthPresets':
            return [nav('Scroll · load','load',str(self._preset_index+1)+' / '+str(len(self._presets))),key('Engine',self.engineFilter),key('Tag',self.tagFilter),key(),key(),key('Save as',action='synth:nav:SynthSaveAs'),key(),key('Back',action='nav:SynthParts')]
        pages = self.pages
        index = pages.index(self._page) if self._page in pages else 0
        prev = pages[(index-1)%len(pages)] if pages else ''
        nxt = pages[(index+1)%len(pages)] if pages else ''
        keys = [nav('Page ◂','prev',prev),nav('Page ▸','next',nxt),nav('Bank','bank',f'{self._bank+1} / {max(1,self.bankCount)}'),nav('Fine','fine','SHIFT' if self._fine else 'Hold SHIFT'),key(),key(),key('Presets',action='synth:nav:SynthPresets'),key('Back',action='nav:SynthParts')]
        if self.engine in ('dx7','dx7_6op'):
            keys[4] = nav('Op ◂','op-prev')
            keys[5] = nav('Op ▸','op-next')
        if self.engine == 'hybrid':
            keys[4] = nav('Half','half','A │ B')
            keys[7] = key('Back',action='synth:nav:SynthHybrid')
        if self._page == 'FX':
            keys[4] = nav('FX ◂','fx-prev')
            keys[5] = nav('FX ▸','fx-next')
        if self.status == 'not_responding':
            keys = [key() for _ in range(6)]+[nav('Retry','retry'),key('Back',action='nav:SynthParts')]
        return keys

    def action(self, value):
        if value.startswith('nav:'):
            self.openScreen(value[4:])
        elif value in ('half-a','half-b'):
            self.openScreen('SynthHybrid'+value[-1].upper())
        elif value.startswith('open:'):
            self.selectPart(int(value.split(':')[1]))
            self.openScreen('SynthHybrid' if self.engine == 'hybrid' else 'SynthFilter')
        elif value == 'prev': self.nextPage(-1)
        elif value == 'next': self.nextPage(1)
        elif value == 'bank': self.nextBank()
        elif value == 'fine': self.setFine(not self._fine)
        elif value == 'retry': self.retry()
        elif value == 'save': self.savePreset()
        elif value == 'load': self.loadPreset()
        elif value == 'mute': self.request('set_mute',part=self._part,mute=not self.current_part.get('mute',False))
        elif value in ('op-prev','op-next'):
            op = self.selectedOperator or 1
            self.focusOperator((op-1+(-1 if value == 'op-prev' else 1))%self.operatorCount+1)
        elif value in ('fx-prev','fx-next'):
            self.chooseBank((self._bank+(-1 if value=='fx-prev' else 1))%max(1,self.bankCount))
        elif value == 'half':
            self.selectPage(('B' if self._page.startswith('A ') else 'A')+' FILTER')

    def _send_edits(self):
        outgoing, self._outbound = self._outbound, {}
        for (part,param),value in outgoing.items():
            self._values.setdefault(part,{})[param] = value
            self.request('set_param',part=part,param=param,value=value)
    @staticmethod
    def _update_rows(model, rows):
        if len(model.rows) != len(rows):
            model.replace(rows)
        else:
            for i,row in enumerate(rows):
                if model.rows[i] != row:
                    model.queue_update(i,row)
            model.flush()

    def flush_frame(self):
        if self.in_surface_batch:
            return
        if self.client:
            self.client.drain_frame()
        self._send_edits()
        if not self._dirty:
            return
        self._dirty = False
        if self.current_pages and self._page not in self.pages:
            self._page,self._bank = self.pages[0],0
        self._bank = min(self._bank,max(0,self.bankCount-1))
        rows = []
        for i,p in enumerate(self._parts):
            meter = self._meters[i] if i < len(self._meters) else dict(peak_db=-60,rms_db=-60)
            visual = 'selected' if p['part'] == self._part else 'idle'
            if self._status == 'not_responding':
                visual = 'offline'
            elif self._loading and p['part'] == self._part:
                visual = 'loading'
            if self.preview and self._screen == 'SynthPartsStates':
                visual = ['loading','idle','empty','offline'][i]
            rows.append(dict(entry=dict(p, label=ENGINE_LABELS.get(p['engine'],p['engine']),
                                        selected=p['part']==self._part,visual=visual,**meter)))
        self._update_rows(self.partsModel,rows)
        cells = []
        for i in range(8):
            if i < len(self.bank):
                meta = self.bank[i]
                value = self._values.get(self._part,{}).get(meta['id'],meta['default'])
                amount = (value-meta['min'])/(meta['max']-meta['min']) if meta['max'] != meta['min'] else 0
                cells.append(dict(entry=dict(meta,value=value,text=formatted(meta,value),amount=amount,empty=False)))
            else:
                cells.append(dict(entry=dict(empty=True,id='',name='—',text='—',unit='',kind='empty',amount=0)))
        self._update_rows(self.cellsModel,cells)
        self._update_rows(self.presetsModel,[dict(entry=dict(p,selected=i==self._preset_index,loaded=p['name']==self.current_part.get('preset'))) for i,p in enumerate(self._presets)])
        if self._screen:
            self._update_rows(self.console._softKeys,self.keys())
        snapshot = (self._part,self._page,self._bank,self._focus,self._fine,self.status,self._error,
                    self._name,self._preset_index,self._engine_filter,self._tag_filter,self._parts,
                    self.values,self.current_pages,self._engines,self._cpu,self._hello)
        if snapshot != self._last_presentation:
            self._last_presentation = deepcopy(snapshot)
            self.changed.emit()
