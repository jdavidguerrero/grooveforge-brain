"""Contract round trips run against a separate process, including recovery and UI."""
import json
import os
from pathlib import Path
import select
import socket
import subprocess
import sys
import tempfile
import time
from copy import deepcopy

import pytest
from PySide6.QtCore import QUrl, Qt, QObject, Signal, QTimer, QSocketNotifier
from PySide6.QtQml import QQmlComponent
from PySide6.QtNetwork import QLocalSocket
from PySide6.QtTest import QTest, QSignalSpy
import shiboken6

from main import ROOT, create_engine
from synth_client import SynthClient, COMMANDS
from synth_catalog import ENGINES
from synth_state import relative_value, SYNTH_SCREENS
from fm_algorithms import DX7_ALGORITHMS, FM4_ALGORITHMS


class HostProcess:
    def __init__(self, delay=0, mode="stdio"):
        self.directory = tempfile.TemporaryDirectory(prefix='gf-synth-test-',dir='/tmp')
        self.path = str(Path(self.directory.name)/'s.sock')
        self.delay = delay
        self.mode = mode
        self.process = None
        self.transports = []
        self.start()

    def start(self):
        self.process = subprocess.Popen([sys.executable,str(ROOT/'fake_synth_host.py'),'--socket',self.path,
                                         '--delay-ms',str(self.delay)]+(['--stdio'] if self.mode=='stdio' else []), stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        stream = self.process.stderr if self.mode=="stdio" else self.process.stdout
        ready,_,_ = select.select([stream],[],[],5)
        assert ready, 'fake host did not start'
        line = stream.readline()
        if not line.startswith('FAKE synth host'):
            error=self.process.stderr.read()
            self.close()
            if 'Operation not permitted' in error:
                pytest.skip('Sandbox prohibits Unix socket bind; pipe protocol harness is tested separately')
            pytest.fail(error)

    def stop(self):
        for transport in self.transports:
            transport.abort()
        if self.process and self.process.poll() is None:
            self.process.terminate()
            self.process.communicate(timeout=5)
        self.process = None

    def close(self):
        self.stop()
        self.directory.cleanup()


class PipeSocket(QObject):
    """Qt transport double over real process pipes; no network calls."""
    connected=Signal()
    readyRead=Signal()
    disconnected=Signal()
    errorOccurred=Signal(object)

    def __init__(self, host):
        super().__init__()
        self.host=host
        host.transports.append(self)
        self.notifier=None
        self.buffer=bytearray()
        self._connected=False
        self.fd=None

    def connectToServer(self, _):
        if not self.host.process or self.host.process.poll() is not None:
            self.errorOccurred.emit(None)
            return
        self.buffer.clear()
        self.fd=self.host.process.stdout.fileno()
        os.set_blocking(self.fd,False)
        self.notifier=QSocketNotifier(self.fd,QSocketNotifier.Read,self)
        self.notifier.activated.connect(self._ready)
        self._connected=True
        QTimer.singleShot(0,self.connected.emit)

    def _ready(self, *_):
        try:
            data=os.read(self.fd,65536)
        except (BlockingIOError,OSError):
            return
        if not data:
            self.abort()
        else:
            self.buffer.extend(data)
            self.readyRead.emit()

    def state(self):
        return QLocalSocket.ConnectedState if self._connected else QLocalSocket.UnconnectedState

    def write(self, data):
        self.host.process.stdin.buffer.write(data)
        self.host.process.stdin.buffer.flush()

    def readAll(self):
        data=bytes(self.buffer)
        self.buffer.clear()
        return data

    def abort(self):
        if self.notifier:
            self.notifier.setEnabled(False)
            self.notifier.deleteLater()
            self.notifier=None
        was=self._connected
        self._connected=False
        if was:
            self.disconnected.emit()


@pytest.fixture(params=["stdio","unix"])
def host(request):
    h = HostProcess(mode=request.param)
    try:
        yield h
    finally:
        h.close()


@pytest.fixture
def unix_host():
    h=HostProcess(mode='unix')
    try:
        yield h
    finally:
        h.close()


def until(predicate, drain=lambda:None, timeout=3000):
    deadline=time.monotonic()+timeout/1000
    while time.monotonic()<deadline:
        QTest.qWait(5)
        drain()
        if predicate():
            return
    assert predicate(), 'condition did not become true'


def connected(app, host):
    client=SynthClient(host.path,transport=PipeSocket(host) if host.mode=="stdio" else None)
    messages=[]
    client.message.connect(messages.append)
    client.start()
    until(lambda:any(m.get('_cmd')=='get_state' and m.get('ok') for m in messages),client.drain_frame)
    return client,messages


def rpc(client, messages, cmd, /, **fields):
    rid=client.request(cmd,**fields)
    until(lambda:any(m.get('id')==rid for m in messages),client.drain_frame)
    return next(m for m in messages if m.get('id')==rid)


def test_every_command_round_trip_and_events(app,host):
    client,messages=connected(app,host)
    try:
        seen=set()
        def call(cmd,**fields):
            seen.add(cmd)
            response=rpc(client,messages,cmd,**fields)
            assert response['ok'],response
            return response
        hello=call('hello',client='pytest')
        assert hello['sample_rate']==48000 and hello['block']==128 and set(hello['engines'])==set(ENGINES)
        assert len(call('get_state')['parts'])==4
        call('subscribe',topics=['meters','state','cpu'])
        call('set_engine',part=1,engine='model_d')
        pages=call('list_params',engine='model_d')['pages']
        assert len(next(p for p in pages if p['name']=='ENV')['banks'])==2
        params=call('get_params',part=1,page='FILTER')['values']
        assert set(params)=={p['id'] for pg in pages if pg['name']=='FILTER' for bank in pg['banks'] for p in bank}
        cutoff=next(p for pg in pages for bank in pg['banks'] for p in bank if p['id']=='cutoff')
        applied=call('set_param',part=1,param='cutoff',value=999999)
        assert applied['value']==cutoff['max']
        assert call('get_params',part=1)['values']['cutoff']==cutoff['max']
        presets=call('list_presets',role='BASS',tag='SUB')['presets']
        assert presets and all('BASS' in p['tags'] and 'SUB' in p['tags'] for p in presets)
        state=call('load_preset',part=1,name='Moog Round')['part']
        assert state['preset']=='Moog Round' and not state['modified']
        call('set_param',part=1,param='cutoff',value=3210)
        call('save_preset',part=1,name='Round Trip User')
        call('set_param',part=1,param='cutoff',value=500)
        call('load_preset',part=1,name='Round Trip User')
        assert call('get_params',part=1)['values']['cutoff']==3210
        assert call('set_level',part=1,db=-12)['part']['level_db']==-12
        assert call('set_mute',part=1,mute=True)['part']['mute']
        call('note',part=1,note=60,velocity=100)
        call('note',part=1,note=60,velocity=0)
        until(lambda:{'meters','cpu','state','param'} <= {m.get('event') for m in messages},client.drain_frame)
        assert seen==COMMANDS
        meters=next(m for m in messages if m.get('event')=='meters')
        assert len(meters['parts'])==4 and set(meters['mix'])=={'peak_db','rms_db'}
        assert any(m.get('event')=='param' and m.get('value')==cutoff['max'] for m in messages)
        assert not rpc(client,messages,'save_preset',part=1,name='Round Trip User')['ok']
        assert not rpc(client,messages,'set_engine',part=1,engine='missing')['ok']
        assert not rpc(client,messages,'set_param',part=1,param='missing',value=10)['ok']
        assert not rpc(client,messages,'note',part=5,note=60,velocity=100)['ok']
        call('subscribe',topics=[])
        QTest.qWait(600)
        client.drain_frame()
        assert client.state=='ready'
        call('hello',client='still connected without event subscriptions')
    finally:
        client.stop()


@pytest.mark.parametrize('engine',ENGINES)
def test_remote_metadata_builds_parameter_banks_for_every_engine(app,host,engine):
    gui,state=create_engine(synth_socket=host.path,synth_transport=PipeSocket(host) if host.mode=='stdio' else None)
    client=gui.synth_state.client
    messages=[]
    client.message.connect(messages.append)
    try:
        synth=gui.synth_state
        state.navigate('SynthFilter')
        until(lambda:len(synth._parts)==4 and bool(synth.current_pages),synth.flush_frame)
        synth.changeEngine(engine)
        until(lambda:synth.engine==engine and bool(synth.current_pages) and set(synth.values)=={p['id'] for pg in synth.current_pages for bank in pg['banks'] for p in bank} and not client.pending,synth.flush_frame)
        pages=rpc(client,messages,'list_params',engine=engine)['pages']
        assert synth.pages==[p['name'] for p in pages]
        for page in pages:
            synth.selectPage(page['name'])
            assert synth.bankCount==len(page['banks'])
            for index,bank in enumerate(page['banks']):
                synth.chooseBank(index)
                rows=[r['entry'] for r in synth.cellsModel.rows]
                assert [p['id'] for p in rows if not p['empty']]==[p['id'] for p in bank]
                assert all(row['empty'] for row in rows[len(bank):])
                for row,meta in zip(rows,bank):
                    assert all(row[k]==meta[k] for k in meta)
        synth.selectPage('OPS' if engine in ('dx7','dx7_6op') else 'FILTER' if engine!='hybrid' else 'LAYER')
        meta=synth.bank[0]
        before=synth.values[meta['id']]
        synth.turn(0,1)
        expected=relative_value(meta,before,1)
        until(lambda:rpc(client,messages,'get_params',part=synth.selectedPart)['values'][meta['id']]==expected,synth.flush_frame)
        assert synth.values[meta['id']]==expected
    finally:
        gui.synth_state.client.stop()
        shiboken6.delete(gui)
        app.processEvents()


@pytest.mark.parametrize('mode',['stdio','unix'])
def test_timeout_then_reconnect_resynchronizes(app,mode):
    host=HostProcess(delay=700,mode=mode)
    client=SynthClient(host.path,transport=PipeSocket(host) if mode=='stdio' else None)
    messages=[]
    client.message.connect(messages.append)
    started=time.monotonic()
    try:
        client.start()
        until(lambda:client.state=='not_responding',client.drain_frame,2000)
        assert .45<=time.monotonic()-started<1.2
        assert any(m.get('error')=='timeout' for m in messages)
        host.stop()
        host.delay=0
        host.start()
        until(lambda:any(m.get('_cmd')=='get_state' and m.get('ok') for m in messages),client.drain_frame,5000)
        assert client.state=='ready'
        host.stop()
        until(lambda:client.state=='not_responding',client.drain_frame)
        assert client.retry.isActive()
        assert client.initial_backoff<=client.backoff<=5000
        host.start()
        until(lambda:client.state=='ready' and not client.pending,client.drain_frame,5000)
    finally:
        client.stop()
        host.close()


def test_client_framing_fragmented_batched_unicode_and_invalid_lines(app,unix_host):
    host=unix_host
    # A raw peer exercises the fake's JSON-lines framing independently of Qt.
    with socket.socket(socket.AF_UNIX) as peer:
        peer.settimeout(2)
        peer.connect(host.path)
        raw='{"id":71,"cmd":"hello","client":"música"}\n'.encode()
        split=raw.index('ú'.encode())+1
        peer.sendall(raw[:split]);peer.sendall(raw[split:]+b'{"id":72,"cmd":"get_state"}\n{bad}\n')
        stream=peer.makefile('rb')
        responses=[json.loads(stream.readline()) for _ in range(3)]
        assert responses[0]['id']==71 and responses[0]['ok']
        assert responses[1]['id']==72 and len(responses[1]['parts'])==4
        assert not responses[2]['ok']


def test_pipe_framing_fragmentation_and_frame_delivery(app):
    client=SynthClient('/tmp/unused-synth-test.sock')
    messages=[]
    client.message.connect(messages.append)
    # Feed exactly the same read seam as QLocalSocket.readyRead, without mutating UI.
    class Bytes:
        def __init__(self): self.data=b''
        def readAll(self): return self.data
    original=client.socket
    source=Bytes()
    client.socket=source
    client.pending[1]=(time.monotonic()+1,'hello',{'client':'test'})
    source.data=b'{"id":1,"ok":true,"version":"m\xc3'
    client._read()
    assert not messages
    source.data=b'\xbAsica"}\n{"event":"cpu","mean_pct":21,"max_pct":24,"xruns":0}\n{bad}\n'
    client._read()
    assert not messages
    assert client.drain_frame()==3
    assert messages[0]['version']=='música'
    assert messages[1]['event']=='cpu' and messages[2]['event']=='protocol_error'
    client.socket=original


def test_multiclient_changes_broadcast_without_touching_other_parts(app,unix_host):
    host=unix_host
    first,a=connected(app,host)
    second,b=connected(app,host)
    try:
        rpc(first,a,'subscribe',topics=['state'])
        rpc(second,b,'subscribe',topics=['state'])
        before=deepcopy(rpc(second,b,'get_params',part=2)['values'])
        rpc(first,a,'set_param',part=1,param='cutoff',value=740)
        until(lambda:any(m.get('event')=='param' and m.get('value')==740 for m in b),second.drain_frame)
        assert rpc(second,b,'get_params',part=2)['values']==before
        rpc(second,b,'subscribe',topics=[])
        b.clear()
        rpc(first,a,'set_mute',part=1,mute=True)
        QTest.qWait(50);second.drain_frame()
        assert not any(m.get('event')=='state' for m in b)
    finally:
        first.stop();second.stop()


def test_algorithm_tables_and_known_dx7_topologies():
    assert len(DX7_ALGORITHMS)==32 and len(FM4_ALGORITHMS)==8
    for count,table in [(6,DX7_ALGORITHMS),(4,FM4_ALGORITHMS)]:
        assert [a['number'] for a in table]==list(range(1,len(table)+1))
        for alg in table:
            ops=set(range(1,count+1))
            assert alg['carriers'] and set(alg['carriers'])<=ops
            assert alg['feedback'] in ops and alg['feedback_source'] in ops
            assert len({tuple(e) for e in alg['edges']})==len(alg['edges'])
            assert all(src in ops and dest in ops and src>dest for src,dest in alg['edges'])
            assert set(alg['carriers'])==ops-{src for src,_ in alg['edges']}
    assert DX7_ALGORITHMS[0]['edges']==[[6,5],[5,4],[4,3],[2,1]]
    assert DX7_ALGORITHMS[3]['feedback']==6 and DX7_ALGORITHMS[3]['feedback_source']==4
    assert DX7_ALGORITHMS[5]['feedback_source']==5
    assert DX7_ALGORITHMS[31]['carriers']==[1,2,3,4,5,6] and not DX7_ALGORITHMS[31]['edges']
    assert len({json.dumps(a['edges']) for a in DX7_ALGORITHMS})==22  # feedback position differentiates otherwise identical routes


@pytest.mark.parametrize('kind,step,value,delta,fine,expected',[
    ('cont',10,100,1,False,110),('cont',10,100,1,True,101),
    ('bipolar',1,-10,-3,True,-10.3),('enum',1,0,1,True,1),
    ('bool',1,0,-1,True,0),('bool',1,0,100,True,1),('cont',10,100,100,False,1000),
])
def test_relative_encoder_semantics(kind,step,value,delta,fine,expected):
    minimum=-1000 if kind=='bipolar' else 0
    maximum=1 if kind=='bool' else 4 if kind=='enum' else 1000
    assert relative_value(dict(kind=kind,step=step,min=minimum,max=maximum),value,delta,fine)==pytest.approx(expected)


def test_surface_batch_coalesces_synth_edits_and_shift(gui):
    engine,state,window,messages=gui
    state.navigate('SynthFilter')
    synth=engine.synth_state
    batch=engine.surface_batcher
    meta=synth.bank[0]
    before=synth.values[meta['id']]
    spy=QSignalSpy(synth.changed)
    for _ in range(10):
        batch.enqueue(dict(type='Encoder',indice=0,delta=1))
    assert synth.values[meta['id']]==before and spy.count()==0
    batch.drain_frame()
    assert synth.values[meta['id']]==before+10*meta['step'] and spy.count()==1
    batch.enqueue(dict(type='Boton',nombre='SHIFT',presionado=True))
    batch.enqueue(dict(type='Encoder',indice=0,delta=1))
    batch.enqueue(dict(type='Boton',nombre='SHIFT',presionado=False))
    batch.drain_frame()
    assert synth.values[meta['id']]==before+10.1*meta['step']
    state.selectEncoder(0)
    window.requestActivate()
    QTest.qWait(20)
    QTest.keyPress(window,Qt.Key_Shift)
    QTest.keyClick(window,Qt.Key_Up)
    QTest.keyRelease(window,Qt.Key_Shift)
    synth.flush_frame()
    assert synth.values[meta['id']]==before+10.2*meta['step']


def test_loading_lock_error_save_keyboard_and_navigation(gui):
    engine,state,window,messages=gui
    synth=engine.synth_state
    state.navigate('SynthNotResponding')
    before=synth.values
    synth.turn(0,100)
    assert synth.values==before
    synth.retry()
    assert synth.status=='preview'
    state.navigate('SynthLoading')
    synth.turn(0,100)
    assert synth.values==before
    state.navigate('SynthParts')
    state.triggerAction('synth:open:1')
    assert synth.selectedPart==1 and synth.engine=='model_d'
    state.navigate('SynthSaveAs')
    synth.typeKey('X');synth.typeKey('⌫');synth.typeKey('SPACE');synth.typeKey('7')
    assert synth.saveName.endswith(' 7')
    synth.savePreset()
    assert state.screenName=='SynthPresets'
    assert any(p['name']==synth.saveName for p in synth._presets)
    state.navigate('SynthSaveAs')
    synth.savePreset()
    assert synth.error=='preset already exists'
    assert state.screenName=='SynthSaveAs'


def test_qml_renders_all_32_algorithms_and_six_operator_focus(gui):
    engine,state,window,messages=gui
    synth=engine.synth_state
    state.navigate('SynthDx7')
    for i in range(1,33):
        synth.chooseAlgorithm(i)
        synth.flush_frame()
        QTest.qWait(5)
        assert synth.algorithm['number']==i
        assert not window.grabWindow().isNull()
    for op in range(1,7):
        synth.focusOperator(op)
        synth.chooseBank(1)
        assert [r['entry']['name'] for r in synth.cellsModel.rows]==['R1','R2','R3','R4','L1','L2','L3','L4']
        synth.turn(0,-10)
        synth.flush_frame()
        QTest.qWait(5)
    assert not messages


def test_opaque_host_metadata_and_partial_banks_are_authoritative(gui):
    engine,state,window,messages=gui
    state.navigate('SynthFilter')
    synth=engine.synth_state
    pages=[dict(name='CUSTOM',banks=[[
        dict(id='opaque:42',name='Host control',unit='V',min=-2,max=3,default=.5,step=.25,kind='bipolar'),
        dict(id='opaque:mode',name='Mode',unit='',min=0,max=2,default=1,step=1,kind='enum',options=['A','B','C'])
    ],[dict(id='opaque:on',name='Enabled',unit='',min=0,max=1,default=0,step=1,kind='bool')]])]
    synth._handle(dict(ok=True,_cmd='list_params',_fields=dict(engine=synth.engine),pages=pages))
    synth.selectPage('CUSTOM')
    assert synth.pageName=='CUSTOM' and synth.bankCount==2
    assert synth.cellsModel.rows[0]['entry']['text']=='+0.5'
    calls=[]
    synth.request=lambda cmd,**fields: calls.append((cmd,fields))
    synth.turn(0,2)
    synth.turn(1,1)
    synth.flush_frame()
    assert calls==[('set_param',dict(part=synth.selectedPart,param='opaque:42',value=1.)),
                   ('set_param',dict(part=synth.selectedPart,param='opaque:mode',value=2))]
    synth.nextBank()
    assert synth.cellsModel.rows[0]['entry']['kind']=='bool'
    assert all(row['entry']['empty'] for row in synth.cellsModel.rows[1:])
    QTest.qWait(20)
    assert not messages


def test_hybrid_navigation_preserves_part_and_settings_do_not_edit(gui):
    engine,state,_,_=gui
    synth=engine.synth_state
    state.navigate('SynthParts')
    synth.selectPart(1)
    synth.changeEngine('hybrid')
    state.triggerAction('synth:open:1')
    assert synth.selectedPart==1 and state.screenName=='SynthHybrid'
    state.pressEncoder(0)
    assert synth.selectedPart==1 and synth.pageName=='a.FILTER'
    state.pressEncoder(4)
    assert synth.pageName=='b.FILTER'
    state.pressEncoder(7)
    assert synth.selectedPart==1 and state.screenName=='SynthHybrid'
    state.pressEncoder(7)
    assert synth.selectedPart==1 and synth.pageName=='b.FILTER'
    for screen in ('SynthSettingsAudio','SynthSettingsConnections','SynthSettingsSystem'):
        state.navigate(screen)
        before=deepcopy(synth.values)
        for i in range(8):
            synth.turn(i,1)
        synth.flush_frame()
        assert synth.values==before


def test_encoder_burst_sends_one_absolute_value_per_frame_and_save_orders_edits(gui):
    engine,state,_,_=gui
    synth=engine.synth_state
    state.navigate('SynthFilter')
    actual=synth.fake.request
    calls=[]
    def traced(req):
        calls.append(deepcopy(req))
        return actual(req)
    synth.fake.request=traced
    meta=synth.bank[0]
    expected=relative_value(meta,synth.values[meta['id']],1000)
    for _ in range(1000):
        synth.turn(0,1)
    assert not calls
    synth.flush_frame()
    writes=[r for r in calls if r['cmd']=='set_param']
    assert len(writes)==1 and writes[0]['value']==expected
    calls.clear()
    synth.turn(0,-1)
    synth._name='Saved after encoder'
    synth.savePreset()
    assert [r['cmd'] for r in calls][:2]==['set_param','save_preset']
    assert synth.fake.saved['Saved after encoder']['cutoff']==expected-meta['step']


def test_meters_do_not_reset_parameter_or_preset_models(gui):
    engine,state,_,_=gui
    synth=engine.synth_state
    state.navigate('SynthFilter')
    cells=QSignalSpy(synth.cellsModel.modelReset)
    presets=QSignalSpy(synth.presetsModel.modelReset)
    notify=QSignalSpy(synth.changed)
    for tick in range(30):
        synth._handle(synth.fake.meters(tick))
        synth.flush_frame()
    assert cells.count()==presets.count()==notify.count()==0


def test_process_loading_state_locks_controls_until_reply_and_recovers_errors(app):
    host=HostProcess()
    gui,state=create_engine(synth_socket=host.path,synth_transport=PipeSocket(host))
    synth=gui.synth_state
    try:
        state.navigate('SynthPresets')
        until(lambda:bool(synth._presets) and bool(synth.current_pages),synth.flush_frame)
        before=deepcopy(synth.values)
        synth.loadPreset()
        assert synth.status=='loading'
        synth.turn(0,100)
        assert synth.values==before
        until(lambda:synth.status=='ready',synth.flush_frame)
        previous=synth.partInfo['preset']
        synth.request('load_preset',part=synth.selectedPart,name='This preset does not exist')
        until(lambda:synth.error=='preset not found',synth.flush_frame)
        assert synth.partInfo['preset']==previous
        assert synth.status=='ready'
    finally:
        synth.client.stop()
        shiboken6.delete(gui)
        app.processEvents()
        host.close()


def test_parts_selection_also_commits_once_in_a_surface_frame(gui):
    engine,state,_,_=gui
    state.navigate('SynthParts')
    synth=engine.synth_state
    spy=QSignalSpy(synth.changed)
    model=QSignalSpy(synth.partsModel.dataChanged)
    for index in (0,1,3,0,1):
        engine.surface_batcher.enqueue(dict(type='Encoder',indice=index,delta=1))
    assert synth.selectedPart==3
    engine.surface_batcher.drain_frame()
    assert synth.selectedPart==2 and spy.count()==1 and model.count()==1


def test_fake_host_never_unlinks_a_path_it_failed_to_bind(tmp_path):
    from fake_synth_host import FakeSynthServer
    existing=tmp_path/'already-owned.sock'
    existing.write_bytes(b'owned by another service')
    with pytest.raises(OSError):
        FakeSynthServer(existing)
    assert existing.read_bytes()==b'owned by another service'
