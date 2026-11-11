"""Observable regressions for apps/docs/theory/consola-touch-velocity.md; desktop cannot certify evdev."""
from copy import deepcopy
from types import SimpleNamespace
import pytest
import shiboken6
from PySide6.QtCore import QPoint, QPointF
from PySide6.QtGui import QGuiApplication
from PySide6.QtTest import QSignalSpy, QTest
from local_settings import LocalSettings
from synth_surface import Boton, Pad, Fsr, Fader
from test_surface_controls import rotated
from test_touch import items, targets


def point(item, dx=0, dy=0):
    p=item.mapToScene(QPointF(item.width()/2+dx,item.height()/2+dy))
    return QPoint(round(p.x()),round(p.y()))


def target(window, category):
    return next(t for t in targets(window) if t.isVisible() and t.property('enabled')
                and t.property('hardwareAction')==category)


def test_keys_tabs_press_once_and_pressed_feedback(rotated):
    e,c,w,device=rotated
    c.triggerAction('mode:3');c.navigate('DrumSteps');QTest.qWait(20)
    tab=next(t for t in targets(w) if t.isVisible() and t.parentItem().width()==152 and t.parentItem().height()==44);d=e.drum_state;calls=[];request=d.request
    d.request=lambda cmd,**f:(calls.append((cmd,f)),request(cmd,**f))[1]
    p=point(tab)
    QTest.touchEvent(w,device).press(0,p,w).commit();QTest.qWait(1)
    assert calls[0][0]=='set_pattern' and tab.property('pressed')
    feedback=next(x for x in tab.childItems() if x.objectName()=='pressedFeedback')
    assert feedback.isVisible()
    QTest.touchEvent(w,device).release(0,p,w).commit();QTest.qWait(1)
    assert len(calls)==1 and not feedback.isVisible()
    cap=next(x for x in items(w.contentItem()) if x.objectName()=='keycap0');p=point(cap)
    QTest.touchEvent(w,device).press(0,p,w).commit();QTest.qWait(1)
    assert d.clock['armed']
    QTest.touchEvent(w,device).release(0,p,w).commit();QTest.qWait(1)
    assert d.clock['armed']


def test_menu_acts_before_release(rotated):
    e,c,w,device=rotated;c.triggerAction('mode:3');QTest.qWait(20)
    menu=next(t for t in targets(w) if t.parentItem().objectName()=='topBar'
              and t.property('hardwareAction')=='navigation');p=point(menu)
    QTest.touchEvent(w,device).press(0,p,w).commit();QTest.qWait(1)
    assert c.screenName=='ModeSelect'
    QTest.touchEvent(w,device).release(0,p,w).commit()


@pytest.mark.parametrize('movement',[(18,7),(0,0)])
def test_rack_release_jitter_and_telemetry_preserve_pressed_target(rotated,movement):
    e,c,w,device=rotated;c.triggerAction('mode:3');c.navigate('DrumBrowser');d=e.drum_state
    calls=[];d.client=SimpleNamespace(request=lambda cmd,**f:calls.append((cmd,f)),drain_frame=lambda:None)
    QTest.qWait(20);row=target(w,'list')
    assert row.property('inScrollArea') and not row.property('activateOnPress')
    start,end=point(row),point(row,*movement)
    QTest.touchEvent(w,device).press(0,start,w).commit();QTest.qWait(1)
    assert not calls
    for tick in range(30):
        d._handle(dict(event='clock',clock=dict(d._clock,step=tick%16,pattern_step=str(tick),running=True)))
        d._handle(dict(event='drum_meters',pads=[tick/30]*16,groups=[tick/30]*4,room=0,cue=0))
        e.surface_batcher.drain_frame()
    assert shiboken6.isValid(row) and row.property('pressed')
    QTest.touchEvent(w,device).move(0,end,w).commit();QTest.qWait(1)
    QTest.touchEvent(w,device).release(0,end,w).commit();QTest.qWait(1)
    assert len(calls)==1 and calls[0][0]=='load_rack'
    assert d.loadingText=='LOADING RACK'


def test_rack_drag_scrolls_without_loading(rotated):
    e,c,w,device=rotated;c.triggerAction('mode:3');c.navigate('DrumBrowser');d=e.drum_state
    d._racks=[dict(name=f'TRIBE {i}',path=f'racks/{i}.json') for i in range(20)]
    d._touch();e.surface_batcher.drain_frame();QTest.qWait(20)
    row=[t for t in targets(w) if t.property('hardwareAction')=='list' and e.touch_controller.available(t)][1]
    viewport=next(a for a in e.touch_controller.ancestors(row) if a.property('contentY') is not None)
    p=point(row);moves=[point(row,0,dy) for dy in (-10,-20,-30,-40,-60,-90)];end=moves[-1]
    QTest.touchEvent(w,device).press(0,p,w).commit()
    for dest in moves:
        QTest.touchEvent(w,device).move(0,dest,w).commit();QTest.qWait(20)
    QTest.touchEvent(w,device).release(0,end,w).commit();QTest.qWait(20)
    assert viewport.property('contentY')>0 and d.loadingText==''
    assert QGuiApplication.styleHints().startDragDistance()==25


@pytest.mark.parametrize('mode,command,fields,label',[
    (3,'load_rack',dict(path='racks/tribe.json'),'LOADING RACK'),
    (3,'set_pad_sample',dict(pad=0,layer={}),'LOADING SAMPLE'),
    (2,'load_preset',dict(part=1,name='Slow preset'),'LOADING PRESET')])
@pytest.mark.parametrize('outcome',['success','error','disconnect'])
def test_loading_immediate_survives_telemetry_until_matching_reply(gui,mode,command,fields,label,outcome):
    e,c,w,_=gui;c.triggerAction('mode:'+str(mode));s=e.drum_state if mode==3 else e.synth_state
    s.client=SimpleNamespace(request=lambda *a,**f:1,drain_frame=lambda:None)
    s.request(command,**fields)
    overlay=next(x for x in items(w.contentItem()) if x.objectName()=='loadingFeedback')
    assert s.loadingText==label and overlay.isVisible() and overlay.property('message')==label
    if mode==3:
        s._handle(dict(event='drum_meters',pads=[.5]*16))
        s._handle(dict(event='clock',clock=dict(s._clock,step=4)))
    else:s._handle(dict(event='meters',parts=[]))
    s.flush_frame();s._handle(dict(_cmd='subscribe',_fields={},ok=True));QTest.qWait(20)
    assert s.loadingText==label and overlay.isVisible()
    if outcome=='disconnect':s._handle(dict(event='connection',state='not_responding'))
    elif outcome=='error':s._handle(dict(_cmd=command,_fields=fields,ok=False,error='load failed'))
    else:s._handle(dict(_cmd=command,_fields=fields,ok=True))
    s.flush_frame();QTest.qWait(1)
    assert s.loadingText=='' and not overlay.isVisible()
    if outcome=='error':assert s.error=='load failed'


def test_clock_and_meters_only_notify_their_properties(gui,monkeypatch):
    e,c,w,_=gui;c.triggerAction('mode:3');c.navigate('DrumSteps');d=e.drum_state;e.surface_batcher.drain_frame()
    models=[QSignalSpy(m.dataChanged) for m in (d.padsModel,d.cellsModel,d.lanesModel,d.samplesModel,c.softKeys)]
    changed,clock,meters=QSignalSpy(d.changed),QSignalSpy(d.clockChanged),QSignalSpy(d.metersChanged)
    def unexpected(*a,**kw):pytest.fail('Telemetry rebuilt a structural model')
    for name in ('pad_entries','step_entries','filtered_samples','rack_summary','group_entries'):
        monkeypatch.setattr(d,name,unexpected)
    for i in range(30):
        d._handle(dict(event='clock',clock=dict(d._clock,running=True,step=i%16,pattern_step=str(i))))
        d._handle(dict(event='drum_meters',pads=[i/30]*16,groups=[i/30]*4,room=0,cue=0));d.flush_frame()
    assert changed.count()==0 and clock.count()==meters.count()==30
    assert [s.count() for s in models[:4]]==[0]*4 and models[4].count()==1
    d._handle(dict(event='clock',clock=dict(d._clock,sample_counter='999999')))
    d._handle(dict(event='drum_meters',pads=[29/30]*16,groups=[29/30]*4,room=0,cue=0));d.flush_frame()
    assert clock.count()==meters.count()==30


@pytest.mark.parametrize('mode',[2,3])
@pytest.mark.parametrize('position,velocity',[(0,1),(.5,64),(1,127)])
def test_live_velocity_immediate_persisted_accent_and_fsr(gui,mode,position,velocity):
    e,c,w,_=gui;c.triggerAction('mode:'+str(mode));s=e.synth_surface;b=e.surface_batcher
    notes=e.drum_surface.notes if mode==3 else s.notes
    before=deepcopy(e.drum_state._rack['groups'] if mode==3 else e.synth_state._parts)
    s.receive(Boton('SHIFT',True));s.receive(Fader(3,position));s.receive(Boton('SHIFT',False))
    s.receive(Pad(0,True,.8));assert notes.events[-1][2]==velocity
    s.receive(Pad(0,False,0));s.receive(Boton('SHIFT',True));s.receive(Pad(1,True,.8))
    assert notes.events[-1][2]==127
    s.receive(Fsr(0,True,87/127));assert notes.events[-1][2]==87
    s.receive(Pad(1,False,0));s.receive(Boton('SHIFT',False));b.drain_frame()
    assert c.preferences.padVelocity==LocalSettings().padVelocity==velocity
    assert (e.drum_state._rack['groups'] if mode==3 else e.synth_state._parts)==before
    assert f'M4 VEL {velocity}' in [i.property('text') for i in items(w.contentItem()) if i.isVisible()]
    c.preferences.setPadVelocity(100)
    s.receive(Pad(2,True,.1));assert notes.events[-1][2]==100
    s.receive(Pad(2,False,0))


def test_shift_step_accent_keeps_timing_and_perform_mute(gui):
    e,c,w,_=gui;c.triggerAction('mode:3');c.navigate('DrumPerform');d=e.drum_state;s=e.synth_surface;b=e.surface_batcher
    lane=d.lane(0);lane['steps'][0]=dict(velocity=0,probability=.4,micro=.13);d.put_lane(0,lane);b.drain_frame()
    for _ in range(2):
        s.receive(Boton('SHIFT',True));s.receive(Pad(16,True,.8));s.receive(Pad(16,False,0))
        s.receive(Boton('SHIFT',False));b.drain_frame()
        assert d.lane(0)['steps'][0]==dict(velocity=127,probability=.4,micro=.13)
    s.receive(Pad(16,True,.8));b.drain_frame();assert d.lane(0)['steps'][0]['velocity']==0
    count=len(e.drum_surface.notes.events)
    s.receive(Boton('ENCODER_4',True));s.receive(Boton('SHIFT',True));s.receive(Pad(0,True,.8));b.drain_frame()
    assert len(e.drum_surface.notes.events)==count and d.params['mute']==1


def test_loading_rack_with_530ms_process_reply_and_live_telemetry(app):
    import select
    import subprocess
    import sys
    import time
    from main import ROOT, create_engine
    from test_synth import PipeSocket
    from tools.real_synth_review import until
    process=subprocess.Popen([sys.executable,str(ROOT/'fake_drum_host.py'),'--stdio','--load-delay-ms','530'],
        stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    host=SimpleNamespace(process=process,transports=[])
    engine=None
    try:
        assert select.select([process.stderr],[],[],5)[0]
        assert process.stderr.readline().startswith('FAKE drum host')
        engine,c=create_engine(drum_socket='pipe',drum_transport=PipeSocket(host))
        c.triggerAction('mode:3');c.navigate('DrumBrowser');d=engine.drum_state
        until(lambda:d.status=='ready' and bool(d._racks) and not d.client.pending,d.flush_frame)
        received=[];d.client.message.connect(received.append)
        started=time.perf_counter();d.loadRack(d._racks[0]['path'])
        assert d.loadingText=='LOADING RACK'
        QTest.qWait(100);d.flush_frame()
        assert d.loadingText=='LOADING RACK'
        assert {m.get('event') for m in received}>={'clock','drum_meters'}
        until(lambda:d.loadingText=='' and not d.client.pending,d.flush_frame)
        assert time.perf_counter()-started>=.53 and d.status=='ready' and d.error==''
    finally:
        if engine:
            engine.drum_state.client.stop();engine.drum_surface.close();engine.synth_surface.close()
            shiboken6.delete(engine);app.processEvents()
        process.terminate();process.communicate(timeout=5)


def test_qml_playhead_follows_polymeter_without_rebuilding_steps(gui):
    e,c,w,_=gui;c.triggerAction('mode:3');c.navigate('DrumSteps');d=e.drum_state
    d._rack['patterns'][0]['lanes'][0]['length']=7;d._touch();d.flush_frame();QTest.qWait(20)
    cells=[i for i in items(w.contentItem()) if i.property('playhead') is not None and i.property('padIndex')==0]
    assert len(cells)==16
    model=QSignalSpy(d.lanesModel.dataChanged)
    d._handle(dict(event='clock',clock=dict(d._clock,running=True,step=9,pattern_step='9')))
    d.flush_frame();QTest.qWait(1)
    active=[i for i in cells if i.property('playhead')]
    assert len(active)==1 and active[0].property('entry')['index']==2
    assert model.count()==0
    # Pattern transitions remain structural, to display the new pattern's steps.
    d._rack['patterns'][1]['lanes'][0]['steps'][3]['velocity']=53
    d._handle(dict(event='clock',clock=dict(d._clock,pattern=1)))
    d.flush_frame();QTest.qWait(1)
    assert d.lanesModel.rows[0]['entry']['steps'][3]['velocity']==53 and model.count()==1


def test_qml_meters_change_without_recreating_mixer_controls(gui):
    e,c,w,_=gui;c.triggerAction('mode:3');c.navigate('DrumMixer');d=e.drum_state;QTest.qWait(20)
    panels=[i for i in items(w.contentItem()) if i.property('meter') is not None]
    assert len(panels)==12
    d._handle(dict(event='drum_meters',pads=[.75]*16,groups=[.25]*4,room=.5,cue=0))
    d.flush_frame();QTest.qWait(1)
    assert all(shiboken6.isValid(p) for p in panels)
    assert sorted(p.property('meter') for p in panels)==[.25]*4+[.75]*8


def test_new_rack_choice_is_sent_while_preview_pending_and_old_reply_keeps_busy(gui):
    e,c,w,_=gui;c.triggerAction('mode:3');d=e.drum_state;sent=[]
    d.client=SimpleNamespace(request=lambda cmd,**f:sent.append((cmd,f)),drain_frame=lambda:None)
    d.request('preview',pad=0)
    d.loadRack('racks/tribe1.json');d.loadRack('racks/tribe2.json')
    assert [cmd for cmd,_ in sent]==['preview','load_rack','load_rack']
    d._handle(dict(_cmd='preview',_fields=dict(pad=0),ok=True))
    d._handle(dict(_cmd='load_rack',_fields=dict(path='racks/tribe1.json'),ok=True))
    assert d.loadingText=='LOADING RACK' and d.status=='loading'
    d._handle(dict(_cmd='load_rack',_fields=dict(path='racks/tribe2.json'),ok=True))
    assert d.loadingText==''
