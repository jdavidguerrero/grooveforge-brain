"""Registry-wide touch contract plus real pointer/hardware equivalence checks."""
from pathlib import Path
import re
import copy

import pytest
import shiboken6
from PySide6.QtCore import Qt, QPoint, QPointF, QMetaObject
from PySide6.QtQml import QJSValue
from PySide6.QtTest import QTest
from registry import SCREENS
from main import ROOT
from touch_controller import HARDWARE_ACTIONS


def items(parent):
    for child in parent.childItems():
        yield child
        yield from items(child)


def targets(window):
    return [x for x in items(window.contentItem()) if x.property('clickable') is True]


@pytest.mark.parametrize('screen',SCREENS,ids=lambda s:s.name)
def test_every_registered_screen_has_touch_handlers_and_hardware_contract(gui,screen):
    engine,state,window,messages=gui
    state.navigate(screen.name)
    QTest.qWait(20)
    found=targets(window)
    assert found
    for target in found:
        assert target.property('hardwareAction') in HARDWARE_ACTIONS
        callback=target.property('callback')
        assert isinstance(callback,QJSValue) and callback.isCallable(), screen.name
    # Touch, jog and keyboard activate the very same function, never a second handler.
    assert all(t.metaObject().indexOfMethod('activate()') >= 0 for t in found)
    assert not messages


def test_no_unmarked_mouse_click_areas_and_table_documents_every_category():
    for path in [*ROOT.glob('screens/*.qml'),*ROOT.glob('components/*.qml')]:
        if path.stem != 'TouchTarget':
            assert not re.search(r'\bMouseArea\s*\{',path.read_text()),path
    doc=(ROOT.parents[1]/'docs/consola/21-modo-synth.md').read_text()
    assert all(f'| {category} |' in doc for category in HARDWARE_ACTIONS)
    for name in ('PadGrid','SynthScale'):
        directory='components' if name=='PadGrid' else 'screens'
        assert 'fakePad(' not in (ROOT/directory/(name+'.qml')).read_text()


def tap(window,item):
    point=item.mapToScene(QPointF(item.width()/2,item.height()/2))
    QTest.mouseClick(window,Qt.LeftButton,Qt.NoModifier,QPoint(round(point.x()),round(point.y())))
    QTest.qWait(20)


def test_touch_mode_part_knob_focus_scale_and_hardware_equivalence(gui):
    engine,state,window,messages=gui
    state.navigate('ModeSelect')
    QTest.qWait(20)
    mode=[t for t in targets(window) if t.isVisible() and t.property('enabled') and t.property('hardwareAction')=='choice'][2]
    tap(window,mode)
    assert state.synthMode and state.screenName=='SynthParts'
    # Same mode action is available on encoder click 3.
    state.navigate('ModeSelect')
    state.pressEncoder(2)
    assert state.synthMode
    QTest.qWait(20)
    part=[t for t in targets(window) if t.isVisible() and t.property('enabled') and t.property('hardwareAction')=='choice'][0]
    tap(window,part)
    assert engine.synth_state.selectedPart==1 and state.screenName=='SynthFilter'
    QTest.qWait(20)
    synth=engine.synth_state
    synth.selectPage('FILTER')
    before=dict(synth.values)
    knobs=[t for t in targets(window) if t.isVisible() and t.property('enabled') and t.property('hardwareAction')=='parameter']
    tap(window,knobs[1])
    assert synth.focusedCell==1 and state.selectedEncoder==1
    assert synth.values==before
    state.navigate('SynthScale')
    QTest.qWait(20)
    assert engine.synth_surface.notes.events==[]
    # Root, scale and octave are not musical note touch targets.
    buttons=[t for t in targets(window) if t.isVisible() and t.property('enabled') and t.parentItem().property('text')=='C']
    tap(window,buttons[0])
    assert engine.synth_surface.root==0
    assert engine.synth_surface.notes.events==[]
    assert not messages


def test_touch_target_hardware_focus_executes_same_action(gui):
    engine,state,window,messages=gui
    state.navigate('Transition')
    QTest.qWait(20)
    tile=next(t for t in targets(window) if t.isVisible() and t.property('enabled') and t.property('hardwareAction')=='choice')
    tap(window,tile)
    expected=engine.touch_controller.detail
    engine.touch_controller.dismiss()
    engine.touch_controller.current=tile
    engine.touch_controller.activate()
    assert engine.touch_controller.detail==expected
    assert not messages


def test_preset_list_drag_and_touch_selection(gui):
    engine,state,window,messages=gui
    state.triggerAction('mode:2')
    synth=engine.synth_state
    synth.selectPart(1)
    state.navigate('SynthPresets')
    synth._presets = [dict(name=f'Preset {i}', engine='model_d', tags=[], favorite=False) for i in range(40)]
    synth._dirty = True
    synth.flush_frame()
    QTest.qWait(40)
    rows=[t for t in targets(window) if t.isVisible() and t.property('enabled') and t.property('hardwareAction')=='list']
    assert rows
    tap(window,rows[min(1,len(rows)-1)])
    assert synth.presetIndex==min(1,len(rows)-1)
    # Drag belongs to ListView, MouseArea allows stealing.
    QTest.mousePress(window,Qt.LeftButton,Qt.NoModifier,QPoint(400,450))
    QTest.mouseMove(window,QPoint(400,400),30)
    QTest.mouseMove(window,QPoint(400,300),30)
    QTest.mouseMove(window,QPoint(400,200),30)
    QTest.mouseRelease(window,Qt.LeftButton,Qt.NoModifier,QPoint(400,200))
    viewport=next(i for i in items(window.contentItem()) if i.property('contentHeight') is not None and i.property('currentIndex') is not None)
    assert viewport.property('contentY') > 0
    assert synth.presetIndex == 1
    assert not messages


def test_activate_local_detail_rows_without_qml_reference_errors(gui):
    engine,state,window,messages=gui
    for screen in SCREENS:
        state.navigate(screen.name)
        QTest.qWait(10)
        for target in targets(window):
            if (shiboken6.isValid(target) and target.isVisible() and target.property('enabled')
                    and target.property('hardwareAction')=='detail'):
                QMetaObject.invokeMethod(target,'activate')
        assert not messages,screen.name


def test_jog_focus_excludes_controls_behind_modal(gui):
    engine,state,window,messages=gui
    state.navigate('Transition')
    QTest.qWait(20)
    touch=engine.touch_controller
    assert touch.focus_targets()
    for target in touch.focus_targets():
        ancestors=list(touch.ancestors(target))
        assert any(a.objectName() in ('modal','topBar','softKeys') for a in ancestors)
    assert not messages


def test_boolean_touch_toggle_commits_host_and_preserves_two_taps_before_frame(gui):
    engine,state,window,messages=gui
    state.triggerAction('mode:2')
    synth=engine.synth_state
    synth.changeEngine('dx7_6op')
    synth.action('open:'+str(synth.selectedPart))
    synth.selectPage('OP 1')
    QTest.qWait(20)
    target=next(t for t in targets(window) if t.property('hardwareAction')=='parameter'
                and t.isVisible() and t.parentItem().property('encoderIndex')==6)
    tap(window,target)
    assert synth.values['op1.on']==0
    assert synth.fake.values[synth.selectedPart]['op1.on']==0
    synth.focusParameter(6)
    synth.focusParameter(6)
    engine.surface_batcher.drain_frame()
    assert synth.values['op1.on']==0
    assert not messages
