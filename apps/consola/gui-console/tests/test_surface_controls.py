"""Rendered Pi-rotation soft keys, MIDI clicks and local surface preferences."""
from copy import deepcopy
import json

import mido
import pytest
import shiboken6
from PySide6.QtCore import QPoint, QPointF, Qt, qInstallMessageHandler, QtMsgType
from PySide6.QtTest import QTest
from main import create_engine
from registry import SCREENS
from local_settings import LocalSettings
from synth_surface import decodificar
from test_touch import items


@pytest.fixture
def rotated(app):
    warnings=[]
    previous=qInstallMessageHandler(lambda kind,ctx,msg: warnings.append(msg) if kind in
        (QtMsgType.QtWarningMsg,QtMsgType.QtCriticalMsg,QtMsgType.QtFatalMsg) else None)
    engine,state=create_engine(rotation={'qml_angle':90})
    window=engine.rootObjects()[0]
    device=QTest.createTouchDevice()
    yield engine,state,window,device
    engine.drum_surface.close();engine.synth_surface.close()
    shiboken6.delete(engine);app.processEvents()
    qInstallMessageHandler(previous)
    assert not warnings


def wire(engine, **message):
    event=decodificar(mido.Message(channel=0,**message))
    assert event is not None
    engine.synth_surface.receive(event)


def prepare(e,c,name):
    c.navigate('ModeSelect')
    e.surface_batcher.drain_frame();QTest.qWait(10)
    # Load each design's own engine and state, then exercise the live bridge.
    c._mode=1
    c.navigate(name)
    if name.startswith('Synth'): c._mode=2
    e.drum_state._pad=0
    e.drum_state._focus=7
    e.drum_state._layer_index=0
    e.drum_state._fine=False
    e.drum_state._hold=''
    c.refresh_keys()
    c._notify()
    e.surface_batcher.drain_frame()
    QTest.qWait(10)


def assert_result(action,e,c,before,calls):
    d,s=e.drum_state,e.synth_state
    op=action.removeprefix('drum:').removeprefix('synth:')
    commands=[cmd for cmd,_ in calls]
    if op.startswith('hands:'): assert c.preferences._hands[int(op[-1])]!=before['hands'][int(op[-1])]
    elif op=='pad-velocity': assert c.preferences.padVelocity!=before['velocity']
    elif op.startswith('nav:'): assert c.screenName==op[4:]
    elif op.startswith('open:'): assert s.selectedPart==int(op[-1]) and c.screenName in ('SynthFilter','SynthHybrid')
    elif op in ('play','stop'): assert d.clock['running']==(op=='play') and 'transport' in commands
    elif op=='record': assert d.clock['armed']!=before['armed']
    elif op in ('VEL','PROB','MICRO'): assert d.layer==op
    elif op in ('lane-bank','step-page','mixer-bank'):
        attr={'lane-bank':'laneBank','step-page':'stepPage','mixer-bank':'mixerBank'}[op]
        assert getattr(d,attr)==1-before[attr]
    elif op=='bank':
        assert (d.page if action.startswith('drum:') else s._bank)==(before['bank']+1)%before['banks']
    elif op=='layer-next': assert d.selectedLayer==(before['layer']+1)%max(1,len(d.padLayers))
    elif op=='fine': assert (d.fine if action.startswith('drum:') else s._fine)!=before['fine']
    elif op in ('mute','solo') and action.startswith('drum:'): assert d.holdMode==op
    elif op=='mute': assert s.current_part['mute']!=before['mute']
    elif op in ('pan','send','level'): assert d.page=={'pan':1,'send':2,'level':0}[op]
    elif op=='groups': assert d.params['group']==(before['group']+1)%4
    elif op.startswith('text:'): assert d.textTarget==op[5:]
    elif op in ('save-rack','save-pattern'): assert d.saveKind==op[5:].upper()
    elif op in ('save','save-new'):
        assert any(cmd in ('save_rack','save_pattern','save_preset') for cmd in commands)
    elif op=='revert': assert 'set_pad_param' in commands
    elif op in ('audition','preview','cue-stop'): assert 'preview' in commands
    elif op=='import': assert 'import_folder' in commands
    elif op=='load': assert any(cmd in ('set_pad_sample','load_preset') for cmd in commands)
    elif op in ('render','more','variations'):
        assert ('render_rack' if op=='render' else 'generate_variations') in commands
        if op=='variations': assert c.screenName=='DrumVariations'
    elif op=='keep': assert 'set_pad_sample' in commands
    elif op=='discard': assert d._variants[d.variantIndex]['state']=='discarded'
    elif op in ('count-focus','spread-focus'): assert d.focusedCell==(0 if op=='count-focus' else 1)
    elif op in ('prev','next'): assert s._page==before[op]
    elif op in ('op-prev','op-next'): assert s.selectedOperator==before[op]
    elif op in ('fx-prev','fx-next'): assert s._bank==before[op]
    elif op=='half': assert s._page.startswith(before['half'])
    elif op in ('half-a','half-b'): assert c.screenName=='SynthHybrid'+op[-1].upper()
    elif op=='morph-focus': assert s._page=='LAYER' and s.focusedCell==0
    elif op=='engine-next': assert s.engine!=before['engine'] and 'set_engine' in commands
    elif op=='filter-engine': assert s.engineFilter!=before['engineFilter']
    elif op=='filter-tag': assert s.tagFilter!=before['tagFilter']
    elif op=='retry': assert s.status in ('preview','ready')
    elif op=='root-next': assert e.synth_surface.root==(before['root']+1)%12
    elif op=='scale-next': assert e.synth_surface.scale!=before['scale']
    elif op in ('oct-minus','oct-plus'): assert e.synth_surface.octave==before['octave']+(-1 if op=='oct-minus' else 1)
    else: pytest.fail('Unverified labeled action: '+action)


PERFORMANCE=[s.name for s in SCREENS if s.name.startswith(('Drum','Synth'))]+['Settings']


@pytest.mark.parametrize('name',PERFORMANCE)
@pytest.mark.parametrize('route',['touch','midi'])
def test_every_labeled_performance_key_has_documented_effect(rotated,name,route):
    e,c,w,device=rotated
    for index in range(8):
        prepare(e,c,name)
        d,s=e.drum_state,e.synth_state
        row=deepcopy(c.softKeys.rows[index])
        if row['label'] in ('','—'): continue
        action=row['action'];assert action,(name,row)
        calls=[]
        originals=[d.request,s.request]
        def spy(original):
            def request(cmd,**fields):
                calls.append((cmd,deepcopy(fields)))
                return original(cmd,**fields)
            return request
        d.request,s.request=map(spy,originals)
        pages=s.pages;pi=pages.index(s._page) if s._page in pages else 0
        op=s.selectedOperator or 1
        before=dict(hands=c.preferences._hands,velocity=c.preferences.padVelocity,armed=d.clock['armed'],laneBank=d.laneBank,stepPage=d.stepPage,mixerBank=d.mixerBank,
            bank=d.page if name.startswith('Drum') else s._bank,banks=len(d._pages) if name.startswith('Drum') else max(1,s.bankCount),
            layer=d.selectedLayer,fine=d.fine if name.startswith('Drum') else s._fine,group=d.params['group'],
            mute=s.current_part.get('mute',False),engine=s.engine,engineFilter=s.engineFilter,tagFilter=s.tagFilter,
            prev=pages[(pi-1)%len(pages)] if pages else '',next=pages[(pi+1)%len(pages)] if pages else '',
            half='a.' if s._page.startswith('b.') else 'b.',root=e.synth_surface.root,scale=e.synth_surface.scale,octave=e.synth_surface.octave)
        before.update({'op-prev':(op-2)%max(1,s.operatorCount)+1,'op-next':op%max(1,s.operatorCount)+1,
                       'fx-prev':(s._bank-1)%max(1,s.bankCount),'fx-next':(s._bank+1)%max(1,s.bankCount)})
        try:
            if route=='midi':
                wire(e,type='note_on',note=80+index,velocity=127)
                e.surface_batcher.drain_frame()
            else:
                cap=next(i for i in items(w.contentItem()) if i.objectName()=='keycap'+str(index))
                p=cap.mapToScene(QPointF(cap.width()/2,cap.height()/2));p=QPoint(round(p.x()),round(p.y()))
                assert 0<=p.x()<w.width() and 0<=p.y()<w.height()
                QTest.touchEvent(w,device).press(0,p,w).commit()
                # Host telemetry arrives while the finger is DOWN (the Pi failure).
                if name.startswith('Drum'):
                    d._handle(dict(event='clock',clock=dict(d._clock,step=1)))
                else:
                    s._dirty=True
                e.surface_batcher.drain_frame();QTest.qWait(10)
                QTest.touchEvent(w,device).release(0,p,w).commit()
                QTest.qWait(10)
            e.surface_batcher.drain_frame()
            if row['enabled']: assert_result(action,e,c,before,calls)
            else: assert calls==[]
        finally:
            d.request,s.request=originals
        if route=='midi':
            wire(e,type='note_off',note=80+index,velocity=0)
            e.surface_batcher.drain_frame()


@pytest.mark.parametrize('mode',[1,2,3])
@pytest.mark.parametrize('hand',[0,1])
def test_hands_default_off_individual_master_and_queued_filter(gui,mode,hand):
    e,c,w,_=gui
    c.triggerAction('mode:'+str(mode))
    if mode==2: e.synth_state.action('open:1')
    b=e.surface_batcher;p=c.preferences
    def sensor(value):
        wire(e,type='note_on',note=108+hand,velocity=127)
        wire(e,type='control_change',control=33+hand,value=value)
    def snapshot():
        return deepcopy((e.synth_state.values,e.drum_state.params,c.hands.rows,c.channels.rows,c.cells.rows))
    b.drain_frame();before=snapshot()
    sensor(0);b.drain_frame();assert snapshot()==before
    assert not p.handsEnabled and not p.leftHand and not p.rightHand
    p.toggleHand(0);b.drain_frame();before=snapshot()
    sensor(0);b.drain_frame();assert snapshot()==before  # individual still OFF
    p.toggleHand(hand+1)
    sensor(0 if hand==0 else 127);b.drain_frame();assert snapshot()!=before
    # Disable after enqueue as well as at ingress; no deferred macro/halo changes.
    sensor(127);p.toggleHand(hand+1);b.drain_frame();before=snapshot()
    sensor(127);b.drain_frame();assert snapshot()==before
    p.toggleHand(hand+1);p.toggleHand(0);b.drain_frame();before=snapshot()
    sensor(127);b.drain_frame();assert snapshot()==before
    assert not c.hands.rows[hand]['inRange']


def test_local_preferences_survive_restart_and_invalid_file_defaults(app,tmp_path):
    p=LocalSettings()
    for i in range(3):p.toggleHand(i)
    p.setPadVelocity(64)
    restored=LocalSettings()
    assert restored.hand_allowed(0) and restored.hand_allowed(1) and restored.padVelocity==64
    assert json.loads(p.path.read_text())['hands_enabled'] is True
    p.path.write_text('{broken')
    restored=LocalSettings()
    assert not restored.handsEnabled and restored.padVelocity==100 and restored.error


@pytest.mark.parametrize('velocity',[1,64,100,127])
def test_m4_fixed_velocity_is_immediate_and_fsr_velocity_stays_real(gui,velocity):
    e,c,w,_=gui;c.triggerAction('mode:3');c.preferences.setPadVelocity(velocity)
    wire(e,type='note_on',note=48,velocity=100)
    wire(e,type='note_on',note=37,velocity=87)
    assert e.drum_surface.notes.events[-2:]==[(10,0,velocity),(10,1,87)]
    assert e.surface_batcher.pending==2  # sends happened before GUI frame


@pytest.mark.parametrize('route',['touch','midi'])
def test_legacy_screens_share_the_same_soft_key_dispatch(rotated,route):
    e,c,w,device=rotated
    seen=[]
    c.triggerAction=seen.append
    for screen in SCREENS:
        if screen.name.startswith(('Synth','Drum')) or screen.name=='Boot': continue
        c.navigate(screen.name);e.surface_batcher.drain_frame();QTest.qWait(10)
        for index,row in enumerate(deepcopy(c.softKeys.rows)):
            if not row['enabled']:continue
            seen.clear()
            if route=='midi':
                wire(e,type='note_on',note=80+index,velocity=127)
                e.surface_batcher.drain_frame()
            else:
                cap=next(i for i in items(w.contentItem()) if i.objectName()=='keycap'+str(index))
                p=cap.mapToScene(QPointF(cap.width()/2,cap.height()/2));p=QPoint(round(p.x()),round(p.y()))
                QTest.touchEvent(w,device).press(0,p,w).commit();QTest.qWait(1)
                c.refresh_keys()  # presentation updates must preserve the pressed item
                QTest.touchEvent(w,device).release(0,p,w).commit();QTest.qWait(1)
            assert seen==[row['action'] or 'demo:'+row['label']],(screen.name,index)
            if route=='midi':
                wire(e,type='note_off',note=80+index,velocity=0);e.surface_batcher.drain_frame()


def test_settings_tiles_and_velocity_choices_are_live_touch_targets(rotated):
    e,c,w,device=rotated;c.navigate('Settings');QTest.qWait(10)
    def tap(item):
        p=item.mapToScene(QPointF(item.width()/2,item.height()/2));p=QPoint(round(p.x()),round(p.y()))
        QTest.touchEvent(w,device).press(0,p,w).commit();QTest.qWait(1)
        QTest.touchEvent(w,device).release(0,p,w).commit();QTest.qWait(10)
    for i in range(3):
        target=next(t for t in items(w.contentItem()) if t.objectName()=='handsToggle'+str(i))
        tap(target)
    assert c.preferences.hand_allowed(0) and c.preferences.hand_allowed(1)
    for value in (64,127,100):
        target=next(t for t in items(w.contentItem()) if t.property('clickable') is True
                    and t.parentItem().property('modelData')==value)
        tap(target);assert c.preferences.padVelocity==value


def test_preferences_write_failure_is_visible_but_runtime_control_still_works(app,tmp_path):
    p=LocalSettings();p.path=tmp_path/'directory';p.path.mkdir()
    p.toggleHand(0)
    assert p.handsEnabled and p.error.startswith('Settings not saved:')


def test_variations_plain_click_is_soft_key_and_shift_click_auditions(gui):
    e,c,w,_=gui;c.navigate('DrumVariations');e.surface_batcher.drain_frame()
    d=e.drum_state
    assert len(d._variants)==8
    calls=[];original=d.request
    d.request=lambda cmd,**f:(calls.append((cmd,f)),original(cmd,**f))[1]
    wire(e,type='note_on',note=100,velocity=127)  # SHIFT
    wire(e,type='note_on',note=87,velocity=127)   # eighth candidate
    e.surface_batcher.drain_frame()
    assert c.screenName=='DrumVariations' and d.variantIndex==7
    assert calls[-1]==('preview',dict(layer=d._variants[7]['layer']))
    wire(e,type='note_off',note=87,velocity=0)
    wire(e,type='note_off',note=100,velocity=0)
    wire(e,type='note_on',note=87,velocity=127)   # BACK soft key
    e.surface_batcher.drain_frame()
    assert c.screenName=='DrumRack'


def test_pad_layer_key_cycles_layers_and_step_velocity_is_independent(gui):
    e,c,w,_=gui;c.navigate('DrumPad');d=e.drum_state
    d.pad_info()['layers']*=2
    e.surface_batcher.drain_frame()
    wire(e,type='note_on',note=81,velocity=127)
    e.surface_batcher.drain_frame();assert d.selectedLayer==1
    c.navigate('DrumSteps');c.preferences.setPadVelocity(64)
    lane=d.lane(d.selectedPad);lane['steps'][0]['velocity']=0;d.put_lane(d.selectedPad,lane)
    e.surface_batcher.drain_frame()
    wire(e,type='note_on',note=64,velocity=100)
    e.surface_batcher.drain_frame()
    assert d.lane(d.selectedPad)['steps'][0]['velocity']==100


@pytest.mark.parametrize('mode',[2,3])
def test_changing_velocity_or_other_hand_keeps_enabled_hand_presence(gui,mode):
    e,c,w,_=gui;c.triggerAction('mode:'+str(mode));p=c.preferences
    for i in range(3):p.toggleHand(i)
    for hand in range(2):wire(e,type='note_on',note=108+hand,velocity=127)
    e.surface_batcher.drain_frame()
    def presence():return e.synth_surface.presence if mode==2 else e.drum_surface._presence
    assert presence()==[True,True]
    p.setPadVelocity(64);assert presence()==[True,True]
    p.toggleHand(2);assert presence()==[True,False]
    p.toggleHand(2);assert presence()==[True,False]  # needs fresh presence
