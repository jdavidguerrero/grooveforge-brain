import copy
import json
from pathlib import Path

import pytest
from PySide6.QtCore import QObject, QPoint, QPointF, Qt, QUrl
from PySide6.QtGui import QFont, QFontDatabase, QFontInfo
from PySide6.QtQml import QQmlComponent
from PySide6.QtQuick import QQuickItem
from PySide6.QtTest import QTest, QSignalSpy

from fake_data import ConsoleState
from main import ROOT
from registry import SCREENS, SCREEN_NAMES, screen_id
from models import SnapshotModel


def visual_items(parent):
    for child in parent.childItems():
        yield child
        yield from visual_items(child)


@pytest.mark.parametrize('screen,descriptor', list(enumerate(SCREENS, 1)), ids=SCREEN_NAMES)
def test_all_chrome_and_design_columns(gui, screen, descriptor):
    _, state, window, messages = gui
    state.setScreen(screen)
    QTest.qWait(30)
    found = {o.objectName(): o for o in visual_items(window.contentItem()) if o.objectName()}
    for name, y, height in [('topBar', 0, 56), ('content', 56, 568), ('softKeys', 624, 96)]:
        obj = found[name]
        assert (obj.y(), obj.height(), obj.width()) == (y, height, 1280)
        assert obj.isVisible() == (screen != 1)
    for i in range(8):
        obj = found[f'keycap{i}']
        outer = 26 / .12109
        inner = (1280-2*outer)/6
        x = 4 + (0 if i == 0 else outer+(i-1)*inner)
        width = (outer if i in (0,7) else inner)-8
        assert (obj.x(), obj.y(), obj.width(), obj.height()) == pytest.approx((x,8,width,80))
    if descriptor.columns:
        for i in range(8):
            name = f'cell0_{i}' if descriptor.source == 'LiveSession' or descriptor.name == 'LiveSession' else f'strip{i}' if descriptor.name == 'Mixer' else f'column{i}'
            obj = found[name]
            assert obj.width() == 152
            assert obj.mapToScene(QPointF(0, 0)).x() == 8+i*160
    if descriptor.name in ('Scale', 'Tempo', 'Repeat'):
        obj = found['reviewModal']
        expected = (8, 248, 1264, 368) if descriptor.name == 'Repeat' else (160, 88, 960, 504)
        assert (obj.mapToScene(QPointF()).x(), obj.mapToScene(QPointF()).y(), obj.width(), obj.height()) == expected
    assert not messages


@pytest.mark.parametrize('file,family,weight,style', [
    ('BigShouldersDisplay.ttf', 'Big Shoulders', 800, None),
    ('Archivo.ttf', 'Archivo', 600, None),
    ('JetBrainsMono.ttf', 'JetBrains Mono', 500, None),
    ('IBMPlexMono-Regular.ttf', 'IBM Plex Mono', 400, 'Regular'),
    ('IBMPlexMono-Medium.ttf', 'IBM Plex Mono', 500, 'Medium'),
    ('IBMPlexMono-SemiBold.ttf', 'IBM Plex Mono', 600, 'SemiBold'),
    ('IBMPlexMono-Bold.ttf', 'IBM Plex Mono', 700, 'Bold'),
    ('GeneralSans.ttf', 'General Sans', 600, None),
])
def test_fontloader_and_real_weights(gui, file, family, weight, style):
    engine, _, _, messages = gui
    path = ROOT/'fonts'/file
    if file == 'GeneralSans.ttf' and not path.exists():
        pytest.skip('General Sans is a local, gitignored Fontshare download')
    assert path.exists(), file
    loader_name = {'BigShouldersDisplay.ttf':'display', 'Archivo.ttf':'archivo',
                   'JetBrainsMono.ttf':'osMono', 'GeneralSans.ttf':'os',
                   'IBMPlexMono-Regular.ttf':'mono', 'IBMPlexMono-Medium.ttf':'monoMedium',
                   'IBMPlexMono-SemiBold.ttf':'monoSemiBold', 'IBMPlexMono-Bold.ttf':'monoBold'}[file]+'Loader'
    component = QQmlComponent(engine)
    source = f'import QtQuick\nimport "theme"\nQtObject {{ property int loadStatus: Theme.{loader_name}.status; property string family: Theme.{loader_name}.name }}'
    component.setData(source.encode(), QUrl.fromLocalFile(str(ROOT/'FontProbe.qml')))
    probe = component.create()
    assert probe is not None, [e.toString() for e in component.errors()]
    probe.setParent(engine)
    assert probe.property('loadStatus') == 1
    name = probe.property('family')
    assert family in name
    font = QFont(name)
    font.setWeight(QFont.Weight(weight))
    actual = QFontInfo(font)
    assert actual.family() == name
    if style:
        assert actual.styleName() == style and actual.weight() == weight
    assert not messages



def test_general_sans_missing_falls_back_without_warning(gui):
    engine, _, _, messages = gui
    fonts = dict(engine.rootContext().contextProperty('localFonts'))
    fonts.pop('os', None)
    engine.rootContext().setContextProperty('localFonts', fonts)
    QTest.qWait(30)
    assert not messages


def test_registry_mode_select_cli_and_cycle(gui):
    _, state, window, messages = gui
    assert len(set(SCREEN_NAMES)) == len(SCREENS)
    for i, descriptor in enumerate(SCREENS, 1):
        assert screen_id(descriptor.name.lower()) == i
        assert (ROOT/'screens'/descriptor.qml).is_file()
        state.navigate(descriptor.name)
        assert state.screenName == descriptor.name
        assert state.softKeys.rowCount() == 8
    with pytest.raises(ValueError):
        screen_id('missing')
    state.setScreen(2)
    state.pressEncoder(0)
    assert state.screenName == 'Session' and state.screenMode == 'CONTROLLER'
    state.setScreen(2)
    state.pressEncoder(1)
    assert state.screenName == 'LiveSession' and state.screenMode == 'STANDALONE'
    window.requestActivate()
    QTest.qWait(20)
    QTest.keyClick(window, Qt.Key_Tab)
    assert state.screen == 6
    QTest.keyClick(window, Qt.Key_Backtab)
    assert state.screen == 5
    state.navigate('Note')
    state.pressEncoder(7)
    assert state.screenName == 'Scale' and state.screenMode == 'CONTROLLER'
    state.pressEncoder(7)
    assert state.screenName == 'Note'
    state.navigate('Pads')
    state.pressEncoder(1)
    assert state.screenName == 'Scale' and state.screenMode == 'STANDALONE'
    assert not messages


def test_all_fake_snapshots_have_transportable_roles_and_design_content(app):
    state = ConsoleState()
    for collection in (state.standalone, state.controller):
        for name, model in collection._models.items():
            assert isinstance(model, SnapshotModel)
            assert list(model.roleNames().values()) == [b'entry']
            json.dumps(model.rows)
            assert model.rowCount() > 0, name
    assert len(state.standalone.rows('pads')) == 32
    assert len(state.standalone.rows('lanes')) == 8
    assert len(state.standalone.rows('stepLanes')) == 8
    assert all(len(lane['steps']) == 16 for lane in state.standalone.rows('stepLanes'))
    assert len(state.controller.rows('scenes')) == 6
    assert all(len(scene['clips']) == 8 for scene in state.controller.rows('scenes'))
    assert len(state.controller.rows('steps14')) == 14
    assert [round(r['amount'], 2) for r in state.standalone.rows('brainArcs')] == [.62,.35,.5,.66,.08,.4,.2,.3]
    for descriptor in SCREENS:
        if descriptor.keys:
            collection, name = descriptor.keys.split('.')
            data = state.standalone if collection == 's' else state.controller
            assert len(data.rows(name)) == 8
    assert state.standalone.rows('sets')[0]['n'] == 'Saturday · video'
    assert state.controller.rows('items')[0]['n'] == 'Organic House Kit'


def test_note_editing_engine_protection_and_batching(app):
    state = ConsoleState()
    state.navigate('SequencerPianoRoll')
    model = state.standalone._models['rollNotes']
    selected = next(i for i, row in enumerate(model.rows) if row['entry']['selected'])
    spy = QSignalSpy(model.dataChanged)
    for _ in range(1000):
        state.turnEncoder(4, 1)
    assert spy.count() == 0
    model.flush()
    assert spy.count() == 1
    assert model.rows[selected]['entry']['velocity'] == 127
    own = copy.deepcopy([r for r in model.rows if r['entry']['owner'] == 'you'])
    engine = copy.deepcopy([r for r in model.rows if r['entry']['owner'] == 'engine'])
    state.vary_engine()
    model.flush()
    assert [r for r in model.rows if r['entry']['owner'] == 'you'] == own
    assert [r for r in model.rows if r['entry']['owner'] == 'engine'] != engine
    for field in range(1, 6):
        for _ in range(100):
            state.turnEncoder(field, -1)
        model.flush()
    for row in model.rows:
        note = row['entry']
        assert 0 <= note['step'] < 64 and 1 <= note['length'] <= 64-note['step']
        assert 0 <= note['pitch'] <= 24 and 1 <= note['velocity'] <= 127
    state.panRoll(1000, 1000)
    assert (state.padStep, state.padNote) == (56, 17)
    state.pressPad(7, 3)
    assert len([r for r in model.rows if r['entry']['selected']]) == 1
    last = next(r['entry'] for r in model.rows if r['entry']['selected'])
    assert (last['step'], last['pitch'], last['owner']) == (63, 22, 'you')


def test_touch_navigates_without_drawing_notes(gui):
    _, state, window, messages = gui
    state.navigate('SequencerPianoRoll')
    QTest.qWait(30)
    model = state.standalone._models['rollNotes']
    before = copy.deepcopy(model.rows)
    QTest.mouseClick(window, Qt.LeftButton, Qt.NoModifier, QPoint(500, 350))
    QTest.mousePress(window, Qt.LeftButton, Qt.NoModifier, QPoint(800, 350))
    QTest.mouseMove(window, QPoint(500, 400), 30)
    QTest.mouseRelease(window, Qt.LeftButton, Qt.NoModifier, QPoint(500, 400))
    state.zoomRoll(2)
    QTest.qWait(30)
    assert model.rows == before
    viewport = next(i for i in visual_items(window.contentItem()) if i.objectName() == 'rollViewport')
    assert viewport.property('contentWidth') == viewport.width()*2
    QTest.keyClick(window, Qt.Key_Right, Qt.ShiftModifier)
    assert state.padStep == 17
    assert not messages


def test_keyboard_changes_query_and_closes(gui):
    _, state, _, messages = gui
    state.navigate('Search')
    state.typeKey('B')
    assert state.query == 'kalimb'
    state.typeKey('⌫')
    assert state.query == 'kalim'
    state.typeKey('SPACE')
    assert state.query == 'kalim '
    state.typeKey('ENTER')
    assert not state.keyboardVisible
    assert not messages


def test_step_encoders_and_pad_window_are_local_batched_edits(app):
    state = ConsoleState()
    state.navigate('SequencerSteps')
    model = state.standalone._models['stepLanes']
    before = copy.deepcopy(model.rows[5]['entry']['steps'][10])
    spy = QSignalSpy(model.dataChanged)
    for _ in range(1000):
        state.turnEncoder(1, 1)
    assert spy.count() == 0
    model.flush()
    assert spy.count() == 1 and model.rows[5]['entry']['steps'][10]['h'] == 100
    state.pressPad(1, 2)
    model.flush()
    assert model.rows[2]['entry']['steps'][1]['on']
    assert state.channels.rows[5]['level'] == .64


def test_motor_soft_actions_never_change_live_state(app):
    state = ConsoleState()
    state.navigate('Motor')
    cells = copy.deepcopy(state.cells.rows)
    state.pressEncoder(0)
    state.controller._models['engCards'].flush()
    assert state.controller.rows('engCards')[0]['state'] == 'cue'
    state.pressEncoder(5)
    state.controller._models['engCards'].flush()
    assert state.controller.rows('engCards')[0]['state'] == 'room'
    assert state.cells.rows == cells


def test_label_uses_explicit_variable_weight_without_font_binding_loops(gui):
    engine, _, _, messages = gui
    component = QQmlComponent(engine)
    component.setData(b'''import QtQuick
import "theme"
import "components"
QtObject {
    property Label displayLabel: Label { face: Theme.displayFont; weight: Font.ExtraBold }
    property Label osLabel: Label { face: Theme.osFont; weight: Font.Medium }
    property real displayWeight: displayLabel.font.variableAxes.wght
    property real osWeight: osLabel.font.variableAxes.wght
}''', QUrl.fromLocalFile(str(ROOT/'WeightProbe.qml')))
    probe = component.create()
    assert probe is not None, [e.toString() for e in component.errors()]
    probe.setParent(engine)
    assert probe.property('displayWeight') == 800
    assert probe.property('osWeight') == 500
    assert not messages
