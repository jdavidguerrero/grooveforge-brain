"""Capture the touch contract with synthetic input, rotated QML and no audio/hardware."""
import json
import os
from pathlib import Path
import sys
from types import SimpleNamespace
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
os.environ.setdefault('QT_SCALE_FACTOR','1')
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from PySide6.QtCore import QPointF, QPoint, qInstallMessageHandler, QtMsgType
from PySide6.QtGui import QGuiApplication
from PySide6.QtTest import QTest
import shiboken6
from main import ROOT, create_engine
from synth_surface import Boton, Fader


def descendants(item):
    for child in item.childItems():
        yield child
        yield from descendants(child)


def capture():
    destination=ROOT/'screenshots/touch'
    os.environ['GF_CONSOLE_SETTINGS']=str(destination/'capture.settings.json')
    app=QGuiApplication.instance() or QGuiApplication([])
    warnings=[]
    previous=qInstallMessageHandler(lambda kind,ctx,msg:warnings.append(msg) if kind in
        (QtMsgType.QtWarningMsg,QtMsgType.QtCriticalMsg,QtMsgType.QtFatalMsg) else None)
    e,c=create_engine(rotation={'qml_angle':90})
    w=e.rootObjects()[0];device=QTest.createTouchDevice();manifest=[]
    def shot(name,condition):
        QTest.qWait(30)
        image=w.grabWindow()
        assert image.size().width()==720 and image.size().height()==1280
        assert image.save(str(destination/name))
        manifest.append(dict(file=name,condition=condition,width=720,height=1280))
    try:
        c.triggerAction('mode:3');c.navigate('DrumBrowser');d=e.drum_state
        d._racks=[dict(name='TRIBE 1',path='racks/tribe1.json'),dict(name='TRIBE 2',path='racks/tribe2.json')]
        d._touch();e.surface_batcher.drain_frame();QTest.qWait(30)
        d.client=SimpleNamespace(request=lambda cmd,**f:1,drain_frame=lambda:None)
        row=next(t for t in descendants(w.contentItem()) if t.property('hardwareAction')=='list' and e.touch_controller.available(t))
        p=row.mapToScene(QPointF(row.width()/2,row.height()/2));p=QPoint(round(p.x()),round(p.y()))
        QTest.touchEvent(w,device).press(0,p,w).commit()
        shot('rack-pressed-90.png','Finger down on TRIBE 1; no load until release')
        QTest.touchEvent(w,device).release(0,p,w).commit()
        assert d.loadingText=='LOADING RACK'
        shot('rack-loading-90.png','Deferred host reply; LOADING RACK published on release')
        d._handle(dict(_cmd='load_rack',_fields=dict(path='racks/tribe1.json'),ok=True))
        c.navigate('DrumPerform')
        e.synth_surface.receive(Boton('SHIFT',True));e.synth_surface.receive(Fader(3,.5))
        e.synth_surface.receive(Boton('SHIFT',False));e.surface_batcher.drain_frame()
        shot('drum-velocity-90.png','SHIFT+fader4 sets global M4 VEL 64; FSR remains measured')
        c.triggerAction('mode:2')
        shot('synth-velocity-90.png','SYNTH shares the persisted global M4 VEL 64')
        assert not warnings,warnings
        (destination/'captures.json').write_text(json.dumps(dict(qpa='offscreen',backend='software',
            rotation=90,input='QTest synthetic touch/MIDI dataclasses; preview, no hardware/audio',
            captures=manifest),indent=2)+'\n')
    finally:
        e.drum_surface.close();e.synth_surface.close();shiboken6.delete(e);app.processEvents()
        qInstallMessageHandler(previous)
        (destination/'capture.settings.json').unlink(missing_ok=True)


if __name__=='__main__':capture()
