"""Console v1 wiring → decoded MIDI → frame seam → visible state/host intentions."""
from copy import deepcopy
import mido
import pytest
from PySide6.QtQuick import QQuickItem
from PySide6.QtTest import QTest
from synth_surface import decodificar

NOTES = dict(JOG=88, OCT_PLUS=89, OCT_MINUS=90, PAGE_RIGHT=91, PAGE_LEFT=92,
             BACK=102, MENU=103, LEFT=104, RIGHT=105, BROWSE=106, SHIFT=100,
             CUE=112, FILL=113, TAP=114, BUILD=115, DROP=116, BREAK=117)


def button(e, name, down=True):
    e.synth_surface.receive(decodificar(mido.Message('note_on' if down else 'note_off',
                              channel=0, note=NOTES[name], velocity=127 if down else 0)))
    e.surface_batcher.drain_frame()


@pytest.mark.parametrize('mode', [0, 1, 2, 3])
def test_menu_back_browse_and_mode_jog(gui, mode):
    e,c,w,_ = gui
    c.triggerAction('mode:'+str(mode)); original = c.screenName
    button(e,'MENU')
    assert c.screenName == 'ModeSelect'
    # MENU opens the selector without replacing the current musical mode.
    assert c.selectedMode == mode
    button(e,'BACK')
    assert c.screenName == original
    button(e,'BROWSE')
    assert c.screenName == ('Browse','Library','SynthPresets','DrumBrowser')[mode]
    button(e,'BACK')
    assert c.screenName == original
    button(e,'MENU')
    e.surface_batcher.enqueue(dict(type='Jog',delta=2-e.console_buttons.menuIndex))
    e.surface_batcher.drain_frame()
    button(e,'JOG')
    assert c.synthMode and c.screenName == 'SynthParts'
    button(e,'MENU')
    e.surface_batcher.enqueue(dict(type='Jog',delta=6-e.console_buttons.menuIndex))
    e.surface_batcher.drain_frame(); button(e,'JOG')
    assert c.screenName == 'Settings'
    button(e,'RIGHT'); assert c.screenName == 'SynthSettingsAudio'
    button(e,'LEFT'); assert c.screenName == 'Settings'


@pytest.mark.parametrize('choice,target', [(0,'Session'),(1,'LiveSession'),(2,'SynthParts'),
                                            (3,'DrumRack'),(4,'SequencerSteps'),(5,'Mix')])
def test_mode_selector_soft_keys(gui,choice,target):
    e,c,_,_ = gui
    button(e,'MENU'); c.pressEncoder(choice)
    assert c.screenName == target


@pytest.mark.parametrize('mode', [0, 1])
def test_live_scene_column_emits_four_distinct_intentions(gui,mode):
    e,c,_,_ = gui
    c.triggerAction('mode:'+str(mode)); intents=[]
    e.console_buttons.intentRequested.connect(intents.append)
    for name in ('OCT_PLUS','OCT_MINUS','PAGE_LEFT','PAGE_RIGHT'):
        button(e,name)
        assert 'PENDING' in e.touch_controller.detail
    assert [i['scene'] for i in intents] == [1,2,3,4]
    assert all(i['action']=='launch_scene' and i['pending'] for i in intents)


def test_synth_scene_column_transposes_and_pages(gui):
    e,c,_,_ = gui
    c.triggerAction('mode:2'); c.navigate('SynthFilter')
    intents=[]; e.console_buttons.intentRequested.connect(intents.append)
    s=e.synth_surface; octave=s.octave; page=e.synth_state.pageName
    button(e,'OCT_PLUS'); assert s.octave == octave+1
    button(e,'OCT_MINUS'); assert s.octave == octave
    button(e,'PAGE_RIGHT'); assert e.synth_state.pageName != page
    button(e,'PAGE_LEFT'); assert e.synth_state.pageName == page
    button(e,'RIGHT'); assert e.synth_state.pageName != page
    button(e,'LEFT'); assert e.synth_state.pageName == page
    assert not intents


def test_drum_octave_is_explicit_pending_and_step_page_is_directional(gui):
    e,c,_,_ = gui
    c.triggerAction('mode:3'); c.navigate('DrumSteps'); d=e.drum_state
    intents=[]; e.console_buttons.intentRequested.connect(intents.append)
    original=deepcopy(d._rack)
    button(e,'OCT_PLUS'); button(e,'OCT_MINUS')
    assert [i['delta'] for i in intents] == [1,-1]
    assert all(i['action']=='drum_octave' and i['pending'] for i in intents)
    assert d._rack == original
    intents.clear()
    button(e,'PAGE_RIGHT'); assert d.stepPage == 1
    button(e,'PAGE_RIGHT'); assert d.stepPage == 1
    button(e,'PAGE_LEFT'); assert d.stepPage == 0
    assert not intents


def test_tap_tempo_uses_host_transport_without_stopping_playback(gui, monkeypatch):
    e,c,_,_ = gui
    c.triggerAction('mode:3'); d=e.drum_state; sent=[]
    request=d.request
    def capture(cmd,**f):
        sent.append((cmd,f)); return request(cmd,**f)
    monkeypatch.setattr(d,'request',capture)
    times=iter([10.,10.5,11.,11.5,12.,12.5,15.,15.75])
    monkeypatch.setattr('console_buttons.monotonic',lambda:next(times))
    button(e,'TAP'); assert not sent
    button(e,'TAP'); button(e,'TAP'); button(e,'TAP')
    assert sent[-1] == ('transport',dict(action='stop',bpm=120.))
    d.play(); before=len(sent)
    button(e,'TAP'); button(e,'TAP')
    assert len(sent)==before and d.clock['running']
    assert 'stop transport' in e.touch_controller.detail
    d.stop(); before=len(sent)
    button(e,'TAP'); assert len(sent)==before  # timeout resets the estimator
    button(e,'TAP'); assert sent[-1] == ('transport',dict(action='stop',bpm=80.))


@pytest.mark.parametrize('mode',[0,1,2,3])
@pytest.mark.parametrize('name',['BUILD','DROP','BREAK'])
def test_sections_arm_cancel_and_shift_next_bar_without_music(gui,mode,name):
    e,c,w,_ = gui
    c.triggerAction('mode:'+str(mode)); b=e.console_buttons
    rack=deepcopy(e.drum_state._rack); intents=[]
    b.intentRequested.connect(intents.append)
    button(e,name); QTest.qWait(10)
    assert b.armed==name and b.boundary=='phrase'
    indicator=w.findChild(QQuickItem,'sectionArmed')
    assert indicator.isVisible()
    button(e,name,False); assert b.armed==name
    button(e,name); assert b.armed==''
    button(e,'SHIFT'); button(e,name)
    assert b.armed==name and b.boundary=='bar'
    button(e,'SHIFT',False)
    other='DROP' if name!='DROP' else 'BUILD'
    button(e,other); assert b.armed==other
    b.execute_section()
    assert intents[-1]==dict(action='section_execute',pending=True,section=other,boundary='phrase')
    assert e.drum_state._rack==rack
    assert not e.drum_state.clock['running']


def test_cue_and_fill_are_visible_pending_hooks_except_real_drum_preview(gui,monkeypatch):
    e,c,_,_ = gui; c.triggerAction('mode:2')
    button(e,'CUE'); assert 'CUE · PENDING' in e.touch_controller.detail
    button(e,'FILL'); assert 'FILL · PENDING' in e.touch_controller.detail
    c.triggerAction('mode:3'); calls=[]
    monkeypatch.setattr(e.drum_state,'request',lambda cmd,**f:calls.append((cmd,f)))
    button(e,'CUE'); assert calls[-1] == ('preview',dict(pad=e.drum_state.selectedPad))
