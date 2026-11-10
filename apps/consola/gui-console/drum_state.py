"""DRUM presentation and intentions. Audio state is confirmed by the host; navigation is local."""
from copy import deepcopy
import math
from PySide6.QtCore import QObject, Property, Signal, Slot
from models import SnapshotModel
from fake_drum_host import FakeDrum, metadata
from synth_state import relative_value, formatted

GROUP_COLORS = ('#FF4560', '#FF9A2E', '#F5E04A', '#E8F0FF')
DRUM_SCREENS = ('DrumRack','DrumRackStates','DrumRackEmpty','DrumPad','DrumBrowser',
                'DrumBrowserEmpty','DrumImport','DrumSteps','DrumProbability','DrumPerform',
                'DrumMixer','DrumSave','DrumVariations')


def key(label='', value='', action='', lit=False, role='', enabled=True):
    return dict(label=label,value=str(value),action='drum:'+action if action else '',lit=lit,
                roleName=role,turning=False,position=-1.,enabled=enabled and bool(action))


class DrumState(QObject):
    changed = Signal()
    wakeRequested = Signal()

    def __init__(self,console,client=None,parent=None):
        super().__init__(parent)
        self.console,self.client=console,client
        self.preview=client is None
        self.fake=FakeDrum() if self.preview else None
        self._rack=metadata()['empty_rack']
        self._clock=dict(running=False,armed=False,pattern=0,queued=-1,bpm=122.,swing=0.,bar=1,step=0)
        self._pages=[]
        self._status='preview' if self.preview else 'connecting'
        self._loading=False
        self._tempo=False
        self._error=''
        self._screen=''
        self._pad,self._page,self._focus,self._step=0,0,0,0
        self._lane_bank,self._step_page,self._mixer_bank=0,0,0
        self._layer='VEL'
        self._layer_index=0
        self._fine=False
        self._samples,self._racks,self._variants=[],[],[]
        self._catalog_generation=0
        self._sample_index,self._variant_index=0,0
        self._query,self._category,self._source='','ALL','ALL'
        self._folder,self._licence='','pending'
        self._save_name='My rack'
        self._save_kind='RACK'
        self._text_target=''
        self._text_selected=True
        self._job={}
        self._ram=0
        self._limit=256*1024**2
        self._data_dir='/data/drum'
        self._meters={}
        self._hits=[[] for _ in range(4)]
        self._outbound={}
        self._desired={}
        self._serial=0
        self._dirty=True
        self._modified=False
        self._count,self._spread=8,.35
        self._hold=''
        self._quantize=16
        self._last_models={}
        self._edit_original=None
        self._keep_pending=None
        self.padsModel=SnapshotModel([],['entry'],self)
        self.cellsModel=SnapshotModel([],['entry'],self)
        self.lanesModel=SnapshotModel([],['entry'],self)
        self.samplesModel=SnapshotModel([],['entry'],self)
        if client:
            client.message.connect(self._handle)
            client.wakeRequested.connect(self.wakeRequested)
        else:
            for cmd, fields in [('get_drum_state',{}),('list_pad_params',{}),('list_racks',{}),('list_drum_samples',{})]:
                self.request(cmd,**fields)
        self.flush_frame()

    pads = Property(QObject,lambda s:s.padsModel,constant=True)
    cells = Property(QObject,lambda s:s.cellsModel,constant=True)
    lanes = Property(QObject,lambda s:s.lanesModel,constant=True)
    sounds = Property(QObject,lambda s:s.samplesModel,constant=True)
    rack = Property('QVariantMap',lambda s:s.rack_summary(),notify=changed)
    clock = Property('QVariantMap',lambda s:dict(s._clock),notify=changed)
    selectedPad = Property(int,lambda s:s._pad,notify=changed)
    selectedStep = Property(int,lambda s:s._step,notify=changed)
    selectedLayer = Property(int,lambda s:min(s._layer_index,max(0,len(s.pad_info()["layers"])-1)),notify=changed)
    focusedCell = Property(int,lambda s:s._focus,notify=changed)
    page = Property(int,lambda s:s._page,notify=changed)
    pageName = Property(str,lambda s:s._pages[s._page]['name'] if s._pages else '',notify=changed)
    layer = Property(str,lambda s:s._layer,notify=changed)
    stepPage = Property(int,lambda s:s._step_page,notify=changed)
    laneBank = Property(int,lambda s:s._lane_bank,notify=changed)
    mixerBank = Property(int,lambda s:s._mixer_bank,notify=changed)
    status = Property(str,lambda s:"loading" if s._loading else s._status,notify=changed)
    tempoVisible = Property(bool,lambda s:s._tempo,notify=changed)
    error = Property(str,lambda s:s._error,notify=changed)
    demo = Property(bool,lambda s:s.preview,constant=True)
    padInfo = Property('QVariantMap',lambda s:s.pad_info(),notify=changed)
    params = Property('QVariantMap',lambda s:dict(s.pad_info().get('params',{})),notify=changed)
    waveform = Property('QVariantList',lambda s:s.active_layer().get('waveform',[]),notify=changed)
    padLayers = Property('QVariantList',lambda s:deepcopy(s.pad_info().get('layers',[])),notify=changed)
    groups = Property('QVariantList',lambda s:s.group_entries(),notify=changed)
    strips = Property('QVariantList',lambda s:s.pad_entries()[s._mixer_bank*8:s._mixer_bank*8+8],notify=changed)
    steps = Property('QVariantList',lambda s:s.step_entries(s._pad),notify=changed)
    sources = Property('QVariantList',lambda s:deepcopy(s._rack['sources']),notify=changed)
    racks = Property('QVariantList',lambda s:deepcopy(s._racks),notify=changed)
    variants = Property('QVariantList',lambda s:s.variant_entries(),notify=changed)
    variantIndex = Property(int,lambda s:s._variant_index,notify=changed)
    ramBytes = Property(float,lambda s:float(s._ram),notify=changed)
    ramText = Property(str,lambda s:f'{s._ram/1024**2:.1f} / {s._limit/1024**2:g} MiB',notify=changed)
    ramRatio = Property(float,lambda s:min(1.,s._ram/s._limit),notify=changed)
    dataDir = Property(str,lambda s:s._data_dir,notify=changed)
    soundCount = Property(int,lambda s:len(s.filtered_samples()),notify=changed)
    sampleIndex = Property(int,lambda s:s._sample_index,notify=changed)
    query = Property(str,lambda s:s._query,notify=changed)
    category = Property(str,lambda s:s._category,notify=changed)
    source = Property(str,lambda s:s._source,notify=changed)
    folder = Property(str,lambda s:s._folder,notify=changed)
    licence = Property(str,lambda s:s._licence,notify=changed)
    saveName = Property(str,lambda s:s._save_name,notify=changed)
    saveKind = Property(str,lambda s:s._save_kind,notify=changed)
    textTarget = Property(str,lambda s:s._text_target,notify=changed)
    modified = Property(bool,lambda s:s._modified,notify=changed)
    job = Property('QVariantMap',lambda s:deepcopy(s._job),notify=changed)
    enginePad = Property(bool,lambda s:isinstance(s.pad_info().get('recipe'),dict) and s.source_for(s.pad_info()).get('kind')=='ENGINE',notify=changed)
    tint = Property(str,lambda s:GROUP_COLORS[int(s.pad_info()['params'].get('group',0))],notify=changed)
    fsr = Property('QVariantList',lambda s:s.fsr_entries(),notify=changed)
    count = Property(int,lambda s:s._count,notify=changed)
    spread = Property(float,lambda s:s._spread,notify=changed)
    quantize = Property(int,lambda s:s._quantize,notify=changed)
    fine = Property(bool,lambda s:s._fine,notify=changed)
    holdMode = Property(str,lambda s:s._hold,notify=changed)

    def rack_summary(self):
        result={k:deepcopy(self._rack[k]) for k in ('id','name','root','mode','sources')}
        result['mode']={'menor':'minor','mayor':'major','dorico':'dorian','frigio':'phrygian','lidia_dominante':'lydian dominant'}.get(result['mode'],result['mode'])
        return result

    def pad_info(self):
        return self._rack['pads'][self._pad]

    def source_for(self,pad):
        return next((s for s in self._rack['sources'] if s['id']==pad['source_id']),{})

    def active_layer(self):
        layers=self.pad_info().get('layers',[])
        return layers[min(self._layer_index,len(layers)-1)] if layers else {}

    def _touch(self):
        self._dirty=True
        self.wakeRequested.emit()

    def request(self,cmd,**fields):
        if self.client:
            return self.client.request(cmd,**fields)
        self._serial+=1
        result,events=self.fake.request(dict(id=self._serial,cmd=cmd,**fields))
        result.update(_cmd=cmd,_fields=fields)
        self._handle(result)
        for event in events: self._handle(event)
        return self._serial

    def refresh_samples(self):
        self._catalog_generation+=1
        self.request("list_drum_samples",offset=0,generation=self._catalog_generation)

    def _handle(self,msg):
        event,cmd=msg.get('event'),msg.get('_cmd')
        if event=='connection':
            self._status=msg['state']
            if self._status=='ready':self._catalog_generation=0
        elif event in ('drum_error','protocol_error'): self._error=msg['error']
        elif event=='clock':
            if msg['clock'].get('record_overruns',0)>self._clock.get('record_overruns',0):
                self._error='Recording stopped: event queue full'
            self._clock=msg['clock']
        elif event=='drum':
            if 'rack' in msg: self._rack=deepcopy(msg['rack'])
            elif msg.get('kind')=='pad_param': self._rack['pads'][msg['pad']]['params'][msg['param']]=msg['value']
            elif msg.get('kind')=='steps': self._rack['patterns'][msg['pattern']]['lanes'][msg['lane']]=deepcopy(msg['data'])
            elif msg.get('kind')=='group': self._rack['groups'][msg['group']]=deepcopy(msg['data'])
        elif event=='drum_meters': self._meters=msg
        elif event=='recorded':
            self._rack['patterns'][msg['pattern']]['lanes'][msg['lane']]['steps'][msg['step']]=msg['hit']
            self._modified=True
        elif event=='drum_job':
            self._job={**self._job,**msg}
            if 'variants' in msg:
                self._variants=msg['variants'];self._variant_index=0
            if msg.get('status')=='done':
                self.request('list_racks');self.refresh_samples()
            if msg.get('status')=='error': self._error=msg.get('error','Worker failed')
        elif msg.get('ok') is False:
            if cmd=='load_rack': self._loading=False
            if cmd=='set_pad_sample': self._keep_pending=None
            self._error=msg.get('error','DRUM command failed')
            if 'stale audio epoch' in self._error: self.request('get_drum_state')
        elif msg.get('ok'):
            self._error=''
            if 'clock' in msg: self._clock=msg['clock']
            if 'ram_bytes' in msg: self._ram=msg['ram_bytes']
            if cmd=='get_drum_state':
                self._rack=deepcopy(msg['rack']);self._clock=msg['clock'];self._ram=msg['ram_bytes']
                self._limit=msg['ram_limit'];self._data_dir=msg['data_dir']
            elif cmd=='list_pad_params': self._pages=msg['pages']
            elif cmd=='list_drum_samples':
                generation=msg.get('_fields',{}).get('generation',0)
                if generation!=self._catalog_generation:return
                if not msg.get('_fields',{}).get('offset',0): self._samples=[]
                self._samples.extend(msg['samples'])
                if msg.get('has_more'): self.request('list_drum_samples',offset=msg['next_offset'],generation=generation)
            elif cmd=='list_racks': self._racks=msg['racks']
            elif cmd=='set_pad_param':
                f=msg['_fields'];self._rack['pads'][f['pad']]['params'][f['param']]=msg['value']
                k=('param',f['pad'],f['param'])
                if k in self._desired and math.isclose(self._desired[k],msg['value'],abs_tol=1e-6): self._desired.pop(k,None)
                self._modified=True
            elif cmd in ('set_steps','set_group','set_pad_sample'):
                self._modified=True
                if cmd=='set_pad_sample' and self._keep_pending:
                    index,digest=self._keep_pending
                    if msg['_fields']['layer']['hash']==digest and index<len(self._variants):
                        self._variants[index]['state']='kept'
                    self._keep_pending=None
            elif cmd=='load_rack':
                self._loading=False;self._modified=False;self.refresh_samples()
            elif cmd in ('save_rack','save_pattern'):
                self._modified=False;self.request('list_racks')
            elif cmd in ('import_folder','generate_variations','render_rack'):
                self._job=dict(status='running',job_id=msg['job_id'],done=0,total=self._count if cmd=='generate_variations' else 0)
        self._touch()

    def configure(self,name):
        self._screen=name
        self._tempo=False
        self._hold=""
        self._text_target=""
        if not name.startswith("Drum"):
            self._touch();return
        self._page=1 if name=='DrumPad' else 0
        self._layer='PROB' if name=='DrumProbability' else 'VEL'
        if self.preview:
            if name=='DrumRackStates':
                self._rack=deepcopy(self.fake.rack);self._error='RAM LIMIT · rejected rack; previous rack remains active'
            elif name in ('DrumRackEmpty','DrumBrowserEmpty'):
                self._rack=metadata()['empty_rack'];self._samples=[];self._ram=0;self._error=''
            else:
                self._rack=deepcopy(self.fake.rack);self._error='';self._ram=18*1024**2
                result,_=self.fake.request(dict(cmd='list_drum_samples'));self._samples=result['samples']
            if name=='DrumVariations' and self.enginePad and not self._variants:
                self.request('generate_variations',pad=self._pad,count=8,spread=self._spread)
            if name=='DrumImport':
                self._job=dict(status='preview',done=128,total=205,current='wood_tom_07.wav',imported=126,
                               skipped=[dict(file='broken.wav',reason='unsupported format')],ram_bytes=18*1024**2)
                self._folder='/media/usb/My samples';self._licence='pending'
        if name=='DrumPad': self._edit_original=deepcopy(self.pad_info()['params'])
        self._touch()

    def set_param(self,pad,param,value):
        meta=next((d for p in self._pages for b in p['banks'] for d in b if d['id']==param),None)
        if meta is None: return
        value=max(meta['min'],min(meta['max'],value))
        if meta['kind']=='enum': value=round(value)
        if param=='start': value=min(value,self._rack['pads'][pad]['params']['end']-.001)
        if param=='end': value=max(value,self._rack['pads'][pad]['params']['start']+.001)
        k=('param',pad,param);self._desired[k]=value
        self._outbound[k]=('set_pad_param',dict(pad=pad,param=param,value=value))
        self._touch()

    @Slot(int)
    def selectPad(self,pad):
        self._pad=max(0,min(15,pad));self._layer_index=0;self._touch()

    @Slot(int)
    def openPad(self,pad):
        self.selectPad(pad);self.console.navigate('DrumPad')

    @Slot(int)
    def focusParameter(self,index):
        self._focus=max(0,min(7,index));self.console.selectEncoder(self._focus);self._touch()

    @Slot(int)
    def focusHandle(self,index):
        self._page=1;self.focusParameter(index)

    @Slot(int)
    def selectLayer(self,index):
        self._layer_index=max(0,min(3,index));self._touch()

    @Slot(str)
    def setLayer(self,layer):
        if layer in ('VEL','PROB','MICRO'): self._layer=layer;self._touch()

    @Slot(str,float)
    def setParam(self,param,value):
        self.set_param(self._pad,param,value)

    @Slot(int)
    def selectPattern(self,index):
        self.request('set_pattern',pattern=max(0,min(7,index)))

    @Slot(int,int)
    def selectStep(self,pad,step):
        self.selectPad(pad);self._step=max(0,min(31,step));self._touch()

    def lane(self,pad):
        p=int(self._clock['pattern']);k=('steps',p,pad)
        return deepcopy(self._outbound[k][1] if k in self._outbound else self._rack['patterns'][p]['lanes'][pad])

    def put_lane(self,pad,lane):
        p=int(self._clock['pattern']);lane={k:lane[k] for k in ('length','steps')}
        self._outbound[('steps',p,pad)]=('set_steps',dict(pattern=p,lane=pad,**lane));self._touch()

    @Slot(int,int)
    def toggleStep(self,pad,step):
        if not 0<=step<32: return
        self.selectStep(pad,step);lane=self.lane(pad)
        if step>=lane['length']: return
        lane['steps'][step]['velocity']=0 if lane['steps'][step]['velocity'] else 100
        self.put_lane(pad,lane)

    @Slot(int,str)
    def togglePad(self,pad,param):
        value=self._rack['pads'][pad]['params'].get(param,0)
        self.set_param(pad,param,0 if value else 1)

    @Slot(int)
    def regroup(self,pad):
        self.set_param(pad,'group',(int(self._rack['pads'][pad]['params']['group'])+1)%4)

    @Slot(int)
    def selectSound(self,index):
        self._sample_index=max(0,min(max(0,len(self.filtered_samples())-1),index));self._touch()

    @Slot()
    def auditionSound(self):
        samples=self.filtered_samples()
        if samples:
            self.request('preview',layer=samples[self._sample_index]['layer'])

    @Slot()
    def loadSound(self):
        samples=self.filtered_samples()
        if samples:
            sample=samples[self._sample_index]
            self.request('set_pad_sample',pad=self._pad,layer=sample['layer'],name=sample['name'],
                         source=sample['source'],recipe=sample.get('recipe'))

    @Slot(str)
    def setCategory(self,value):
        self._category=value;self._sample_index=0;self._touch()

    @Slot(str)
    def setSource(self,value):
        self._source=value;self._sample_index=0;self._touch()

    @Slot(str)
    def loadRack(self,path):
        self._loading=True;self._touch()
        self.request('load_rack',path=path)

    @Slot(str)
    def editText(self,target):
        if target in ('folder','licence','save','query'):
            self._text_target=target;self._text_selected=True;self._touch()

    @Slot(str)
    def typeKey(self,key):
        name={'folder':'_folder','licence':'_licence','save':'_save_name','query':'_query'}.get(self._text_target)
        if not name: return
        if key=='DONE': self._text_target='';self._touch();return
        current='' if self._text_selected else getattr(self,name)
        self._text_selected=False
        if key=='⌫': current=current[:-1]
        else: current+=(' ' if key=='SPACE' else key)
        setattr(self,name,current[:4096 if name=='_folder' else 96]);self._touch()

    @Slot(str)
    def setFolder(self,path):
        self._folder=path;self._touch()

    @Slot(str)
    def setLicence(self,note):
        self._licence=note;self._touch()

    @Slot()
    def importFolder(self):
        if not self._folder: self._error='Choose a local folder first';self._touch();return
        self.request('import_folder',path=self._folder,licence_note=self._licence,trim=True)

    @Slot()
    def renderRack(self):
        self.request('render_rack',root=self._rack['root'],mode=self._rack['mode'],genre='organic',layers=1)

    @Slot(int)
    def auditionVariant(self,index):
        if not 0<=index<len(self._variants): return
        self._variant_index=index;self.request('preview',layer=self._variants[index]['layer'])
        for i,v in enumerate(self._variants):
            if v.get('state') not in ('kept','discarded'): v['state']='cue' if i==index else 'ready'
        self._touch()

    @Slot()
    def keepVariant(self):
        if not self._variants: return
        v=self._variants[self._variant_index]
        self._keep_pending=(self._variant_index,v['layer']['hash'])
        self.request('set_pad_sample',pad=self._pad,layer=v['layer'],name=v['name'],
                     source=self.source_for(self.pad_info()),recipe=v['recipe'])
        self._touch()

    @Slot(int,float)
    def setGroupLevel(self,group,value):
        k=('group',group);self._outbound[k]=('set_group',dict(group=group,level=max(0.,min(2.,value))));self._touch()

    @Slot(float)
    def setBpm(self,value):
        if self._clock['running']:
            self._error='Stop transport to edit BPM';self._touch();return
        self.request('transport',action='stop',bpm=max(60.,min(200.,value)))

    @Slot()
    def play(self): self.request('transport',action='play')
    @Slot()
    def stop(self): self.request('transport',action='stop')
    @Slot()
    def record(self): self.request('record_arm',armed=not self._clock['armed'],quantize=self._quantize)

    def record_hit(self,pad,velocity,fsr=None):
        if fsr is not None:
            self._hits[fsr]=(self._hits[fsr]+[velocity])[-10:]
        self._pad=pad;self._touch()

    def turn(self,index,delta):
        if self._tempo:
            if index==0:self.setBpm(self._clock["bpm"]+delta*(.1 if self._fine else 1))
            return
        if self._text_target: return
        if self._screen in ('DrumSteps','DrumProbability'):
            if index==0: self.selectPad((self._pad+delta)%16)
            elif index==1: self._step=max(0,min(31,self._step+delta));self._step_page=self._step//16
            elif index==2:
                lane=self.lane(self._pad);step=lane['steps'][self._step]
                field={'VEL':'velocity','PROB':'probability','MICRO':'micro'}[self._layer]
                low,high,amount=(0,127,1) if field=='velocity' else (0.,1.,.01) if field=='probability' else (-.5,.5,.01)
                step[field]=max(low,min(high,step[field]+delta*amount));self.put_lane(self._pad,lane)
            elif index==3:
                lane=self.lane(self._pad);lane['length']=max(1,min(32,lane['length']+delta));self.put_lane(self._pad,lane)
            elif index==4: self.request('transport',action='play' if self._clock['running'] else 'stop',swing=max(0.,min(.5,self._clock['swing']+delta*.01)))
            elif index==5: self.selectPattern((self._clock['pattern']+delta)%8)
            elif index==6: self._quantize=32 if delta>0 else 16
        elif self._screen=='DrumMixer':
            pad=index+self._mixer_bank*8;params=self._rack['pads'][pad]['params'];param='pan' if self._page==1 else 'send' if self._page==2 else 'level'
            self.set_param(pad,param,params[param]+delta*.01)
        elif self._screen in ('DrumBrowser','DrumBrowserEmpty'):
            if index==0:
                cats=['ALL','kick','snare','clap','hat','skin','wood','metal','perc'];self.setCategory(cats[(cats.index(self._category)+delta)%len(cats)])
            else: self.selectSound(self._sample_index+delta)
        elif self._screen=='DrumVariations':
            if index==0: self._count=max(1,min(8,self._count+delta))
            elif index==1: self._spread=max(0.,min(1.,self._spread+delta*.01))
        elif self._screen=='DrumPerform':
            if index==5: self.selectPattern((self._clock['pattern']+delta)%8)
        elif self._pages:
            desc=self._pages[self._page]['banks'][0][index];param=desc['id'];k=('param',self._pad,param)
            value=self._desired.get(k,self.pad_info()['params'].get(param,desc['default']))
            self.set_param(self._pad,param,relative_value(desc,value,delta,self._fine))
        self._touch()

    @Slot(int,int)
    def jog(self,delta,unused=0):
        if self._screen in ('DrumBrowser','DrumBrowserEmpty'): self.selectSound(self._sample_index+delta)
        elif self._screen in ('DrumSteps','DrumProbability'): self._lane_bank=1 if delta>0 else 0;self._touch()
        else: self.selectPad((self._pad+delta)%16)

    @Slot(str)
    def action(self,name):
        if name.startswith('nav:'): self._tempo=False;self.console.navigate(name[4:]);return
        if name=='tempo': self._tempo=not self._tempo
        elif name=='tempo-close': self._tempo=False
        elif name=='bank': self._page=(self._page+1)%len(self._pages) if self._pages else 0
        elif name=='step-page': self._step_page=1-self._step_page
        elif name=='lane-bank': self._lane_bank=1-self._lane_bank
        elif name=='mixer-bank': self._mixer_bank=1-self._mixer_bank
        elif name=='play': self.play()
        elif name=='stop': self.stop()
        elif name=='record': self.record()
        elif name=='audition':
            if self._screen=='DrumVariations' and self._variants: self.auditionVariant(self._variant_index)
            else: self.request('preview',pad=self._pad)
        elif name=='cue-stop': self.request('preview',stop=True)
        elif name=='load': self.loadSound()
        elif name=='preview': self.auditionSound()
        elif name=='fine': self._fine=not self._fine
        elif name=='count-focus': self.focusParameter(0)
        elif name=='spread-focus': self.focusParameter(1)
        elif name in ('VEL','PROB','MICRO'): self.setLayer(name)
        elif name=='mute' or name=='solo': self._hold='' if self._hold==name else name
        elif name=='pan': self._page=1
        elif name=='send': self._page=2
        elif name=='groups': self.regroup(self._pad)
        elif name=='import': self.importFolder()
        elif name=='render': self.renderRack()
        elif name=='variations':
            if self.enginePad:
                self.request('generate_variations',pad=self._pad,count=self._count,spread=self._spread)
                self.console.navigate('DrumVariations')
        elif name=='keep': self.keepVariant()
        elif name=='discard' and self._variants: self._variants[self._variant_index]['state']='discarded'
        elif name=='more' and self.enginePad: self.request('generate_variations',pad=self._pad,count=self._count,spread=self._spread)
        elif name=='save-rack': self._save_kind='RACK'
        elif name=='save-pattern': self._save_kind='PATTERN'
        elif name in ('save','save-new'):
            save_name=self._save_name if name=='save' else self._save_name+' copy'
            self.request('save_pattern' if self._save_kind=='PATTERN' else 'save_rack',name=save_name,overwrite=name=='save')
        elif name=='revert' and self._edit_original:
            for param,value in self._edit_original.items(): self.set_param(self._pad,param,value)
        elif name.startswith('text:'): self.editText(name[5:])
        self._touch()

    def filtered_samples(self):
        if self._screen=='DrumBrowserEmpty' and self.preview: return []
        return [s for s in self._samples if (self._category=='ALL' or s['category']==self._category) and
                (self._source=='ALL' or s['source'].get('id')==self._source) and self._query.lower() in s['name'].lower()]

    def pad_entries(self):
        result=[]
        solo=any(p['params'].get('solo',0) for p in self._rack['pads'])
        for i,pad in enumerate(self._rack['pads']):
            p=pad['params'];layers=pad['layers'];source=self.source_for(pad);g=int(p.get('group',0))
            result.append(dict(index=i,name=pad['name'],category=pad['category'],tint=GROUP_COLORS[g],
                               group=g,level=p.get('level',.5),pan=p.get('pan',0),send=p.get('send',0),
                               mute=bool(p.get('mute',0)),solo=bool(p.get('solo',0)),dimmed=solo and not bool(p.get('solo',0)),selected=i==self._pad,
                               waveform=layers[0].get('waveform',[]) if layers else [],empty=not layers,
                               source=source.get('kind','—'),licence=source.get('licence_note',''),
                               meter=self._meters.get('pads',[0]*16)[i],choke=p.get('choke',0),
                               state=('FILE MISSING' if i==3 else 'LOADING' if i==8 else 'NOT IN RAM' if i in (13,14) else '') if self.preview and self._screen=='DrumRackStates' else ''))
        return result

    def step_entries(self,pad):
        lane=self._rack['patterns'][int(self._clock['pattern'])]['lanes'][pad]
        result=[]
        for i in range(self._step_page*16,self._step_page*16+16):
            s=lane['steps'][i]
            result.append(dict(index=i,**s,selected=pad==self._pad and i==self._step,
                               active=i<lane['length'],playhead=self._clock['running'] and i==int(self._clock.get('pattern_step',self._clock['step']))%lane['length'],
                               amount=s['velocity']/127 if self._layer=='VEL' else s['probability'] if self._layer=='PROB' else abs(s['micro'])*2,
                               text=str(s['velocity']) if self._layer=='VEL' else f'{s["probability"]:.0%}' if self._layer=='PROB' else f'{s["micro"]:+.2f}'))
        return result

    def group_entries(self):
        entries=[]
        for i,g in enumerate(self._rack['groups']):
            entries.append(dict(**g,index=i,tint=GROUP_COLORS[i],meter=self._meters.get('groups',[0]*4)[i],
                                members=[dict(index=k,name=p['name']) for k,p in enumerate(self._rack['pads']) if p['layers'] and int(p['params']['group'])==i]))
        return entries

    def variant_entries(self):
        result=[]
        for i in range(8):
            v=dict(**self._variants[i],index=i,selected=i==self._variant_index) if i<len(self._variants) else dict(index=i,name='—',state='empty',layer={},selected=False)
            adjust=v.get('render',{}).get('ajuste',{})
            v['traits']=[dict(name=name,amount=max(0.,min(1.,float(adjust.get(field,1))/2)),
                              text=f'{adjust[field]:.2f}' if field in adjust else '—')
                         for name,field in (('DECAY','decay'),('ATTACK','ataque'),('BRIGHT','brillo'))]
            result.append(v)
        return result

    def fsr_entries(self):
        defaults=(0,1,10,4)
        cats=('kick','snare','hat','perc')
        return [dict(index=i,pad=next((k for k,p in enumerate(self._rack['pads']) if p['category']==cat),defaults[i]),
                     name=cat.upper(),hits=list(self._hits[i]),velocity=self._hits[i][-1] if self._hits[i] else 0,
                     tint=GROUP_COLORS[i]) for i,cat in enumerate(cats)]

    def keys(self):
        nav=lambda label,screen,lit=False:key(label,action='nav:'+screen,lit=lit)
        screen=self._screen
        if screen=='DrumPad':
            return [key('Bank',str(self._page+1)+'/4','bank',True),key('Layer',str(self._layer_index+1),'audition'),key('Fine','ON' if self._fine else 'OFF','fine'),key('Audition','Cue','audition'),key('Stop cue','','cue-stop'),key(),key('Revert','','revert'),nav('Back','DrumRack')]
        if screen in ('DrumBrowser','DrumBrowserEmpty'):
            return [key('Load','→ Pad '+str(self._pad+1),'load',True,enabled=bool(self.filtered_samples())),key('Audition','CUE ONLY','preview',enabled=bool(self.filtered_samples())),key('Stop cue','','cue-stop'),key('Search','','text:query'),nav('Import','DrumImport'),key('Engine kit','Render','render'),nav('Rack','DrumRack'),nav('Back','DrumRack')]
        if screen=='DrumImport':
            return [key('Folder','Local','text:folder'),key('Licence','Edit','text:licence'),key('Import','Background','import',True),key('Engine kit','Render','render'),key(),key(),nav('Background','DrumPerform'),nav('Back','DrumBrowser')]
        if screen in ('DrumSteps','DrumProbability'):
            return [key('Record','ARMED' if self._clock['armed'] else 'OFF','record',self._clock['armed']),key('Velocity','','VEL',self._layer=='VEL'),key('Probability','','PROB',self._layer=='PROB'),key('Micro','','MICRO',self._layer=='MICRO'),key('Lanes',str(self._lane_bank+1)+'/2','lane-bank'),key('Steps',str(self._step_page+1)+'/2','step-page'),key('Stop' if self._clock['running'] else 'Play','','stop' if self._clock['running'] else 'play',True),nav('Back','DrumRack')]
        if screen=='DrumPerform':
            return [nav('Pads','DrumRack'),nav('Steps','DrumSteps'),key('Record','','record',self._clock['armed']),key('Mute','Hold + pad','mute',self._hold=='mute'),key('Solo','Hold + pad','solo',self._hold=='solo'),key('Stop','','stop'),key('Play','','play',self._clock['running']),nav('Back','DrumRack')]
        if screen=='DrumMixer':
            return [key('Bank',str(self._mixer_bank+1)+'/2','mixer-bank',True),key('Groups','Selected pad','groups'),key('Send','Reserved','send'),key('Pan','','pan'),key('Mute','+ pad','mute'),key('Solo','+ pad','solo'),key('Level','','bank'),nav('Back','DrumRack')]
        if screen=='DrumSave':
            return [key('Rack','','save-rack',self._save_kind=='RACK'),key('Pattern',chr(65+self._clock['pattern']),'save-pattern',self._save_kind=='PATTERN'),key('Name','','text:save'),key(),key(),key('Save','Replace','save',True,'#E8B84A'),key('Save as new','','save-new',False,'#5DCAA5'),nav('Cancel','DrumRack')]
        if screen=='DrumVariations':
            return [key('Count',self._count,'count-focus'),key('Spread',f'{self._spread:.0%}','spread-focus'),key('Audition','Cue','audition'),key('Render more','','more',True,'PURPLE',self.enginePad),key('Keep → pad',self._pad+1,'keep',True,enabled=bool(self._variants)),key(),key('Discard','','discard',enabled=bool(self._variants)),nav('Back','DrumRack')]
        return [nav('Pads','DrumRack',True),nav('Steps','DrumSteps'),nav('Perform','DrumPerform'),nav('Browse','DrumBrowser'),nav('Mixer','DrumMixer'),key('Variations','Engine pads' if not self.enginePad else 'Render','variations',False,'PURPLE',self.enginePad),nav('Save','DrumSave'),nav('Back','ModeSelect')]

    def _model(self,name,model,entries):
        if self._last_models.get(name)!=entries:
            if len(model.rows)==len(entries):
                for i,entry in enumerate(entries):
                    if model.rows[i]['entry']!=entry: model.queue_update(i,dict(entry=entry))
                model.flush()
            else: model.replace([dict(entry=e) for e in entries])
            self._last_models[name]=deepcopy(entries)

    @Slot()
    def flush_frame(self):
        if self.client: self.client.drain_frame()
        outgoing,self._outbound=self._outbound,{}
        for cmd,fields in outgoing.values(): self.request(cmd,**fields)
        if not self._dirty: return
        self._dirty=False
        pads=self.pad_entries();self._model('pads',self.padsModel,pads)
        cells=[]
        if self._pages:
            for d in self._pages[self._page]['banks'][0]:
                value=self.pad_info()['params'].get(d['id'],d['default']);span=d['max']-d['min']
                cells.append(dict(**d,value=value,text=formatted(d,value),amount=(value-d['min'])/span if span else 0,empty=False))
        self._model('cells',self.cellsModel,cells)
        lanes=[]
        for i in range(self._lane_bank*8,self._lane_bank*8+8):
            lane=self._rack['patterns'][int(self._clock['pattern'])]['lanes'][i]
            lanes.append(dict(**pads[i],length=lane['length'],steps=self.step_entries(i)))
        self._model('lanes',self.lanesModel,lanes)
        sounds=[dict(**s,index=i,selected=i==self._sample_index) for i,s in enumerate(self.filtered_samples())]
        self._sample_index=max(0,min(self._sample_index,len(sounds)-1))
        self._model('sounds',self.samplesModel,sounds)
        if self.console.screenName.startswith('Drum'): self.console.refresh_keys()
        self.changed.emit()
