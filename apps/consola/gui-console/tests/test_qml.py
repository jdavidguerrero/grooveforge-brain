import pytest
from PySide6.QtCore import QObject, QUrl, Qt, QPointF
from PySide6.QtQml import QQmlComponent
from PySide6.QtQuick import QQuickItem
from PySide6.QtTest import QTest
import shiboken6
from main import ROOT, SCREEN_NAMES
from registry import SCREENS


@pytest.mark.parametrize('screen,name', list(enumerate(SCREEN_NAMES,1)))
def test_every_screen_ready_and_warning_free(gui, screen, name):
    engine,state,window,messages = gui
    state.setScreen(screen)
    component = QQmlComponent(engine, QUrl.fromLocalFile(str(ROOT/'screens'/SCREENS[screen-1].qml)))
    assert component.status() == QQmlComponent.Ready, [e.toString() for e in component.errors()]
    QTest.qWait(40)
    assert not messages
    assert window.findChild(QObject,'screenLoader').property('loadStatus') == 1
    image = window.grabWindow()
    assert (image.width(),image.height()) == (1280,720)
    assert image.pixelColor(640,360).isValid()


def test_geometry_contract(gui):
    engine,state,window,messages = gui
    assert (window.width(),window.height()) == (1280,720)
    for name,y,h in [('topBar',0,56),('content',56,568),('softKeys',624,96)]:
        obj=window.findChild(QQuickItem,name)
        assert (obj.y(),obj.height(),obj.width()) == (y,h,1280)
    for screen in [5,8]:
        state.setScreen(screen)
        QTest.qWait(30)
        items = window.contentItem().childItems()
        def visual_items(parent):
            for child in parent.childItems():
                yield child
                yield from visual_items(child)
        found={i.objectName():i for i in visual_items(window.contentItem()) if i.objectName()}
        for i in range(8):
            cap=found[f'keycap{i}']
            assert (cap.x(),cap.y(),cap.width(),cap.height()) == (8+i*160,8,152,80)
            item=found[f'cell0_{i}' if screen == 5 else f'strip{i}']
            assert (item.x(),item.width()) == (8+i*160,152)
    state.setScreen(7)
    QTest.qWait(30)
    modal=window.findChild(QQuickItem,'modal')
    assert (modal.x(),modal.width(),modal.height()) == (160,960,488)
    assert modal.mapToScene(QPointF(0,0)).y() == 96


@pytest.mark.parametrize('value',['offline','empty','loading','normal'])
def test_live_variants_and_encoding(gui,value):
    _,state,window,messages=gui
    state.setScreen(5)
    state.setLiveState(value)
    state.triggerAction('encoding')
    QTest.qWait(40)
    assert state.encoding == 'stripe'
    assert not window.grabWindow().isNull()
    assert not messages


def test_desktop_keyboard_exploration(gui):
    _,state,window,messages=gui
    window.requestActivate()
    QTest.qWait(30)
    for i in range(1,10):
        QTest.keyClick(window,Qt.Key_0+i)
        assert state.screen == i
    QTest.keyClick(window,Qt.Key_5)
    QTest.keyClick(window,Qt.Key_Right)
    assert state.selectedEncoder == 5
    QTest.keyClick(window,Qt.Key_Up)
    QTest.qWait(30)
    assert state.channels.rows[5]['level'] == pytest.approx(.65)
    QTest.keyClick(window,Qt.Key_Return)
    assert state.screen == 6
    QTest.keyClick(window,Qt.Key_Escape)
    assert state.screen == 5
    QTest.keyClick(window,Qt.Key_O)
    assert state.liveState == 'offline'
    assert not messages


def test_visual_components_receive_model_data(gui):
    _,state,window,messages=gui
    state.setScreen(5)
    QTest.qWait(30)
    def named_items(parent):
        found={}
        for child in parent.childItems():
            found[child.objectName()]=child
            found.update(named_items(child))
        return found
    found=named_items(window.contentItem())
    for index,row in enumerate(state.cells.rows):
        cell=found[f'cell{index//8}_{index%8}']
        for role in ['roleName','title','cellState','level']:
            assert cell.property(role) == row[role]
    state.setScreen(6)
    QTest.qWait(30)
    found=named_items(window.contentItem())
    for i,option in enumerate(state.options.rows):
        item=found[f'optionCard{i}']
        for role in ['letter','title','optionState','energy']:
            assert item.property(role) == option[role]
    assert not messages
