"""Reproducible silent desktop captures for the console v1 button map."""
import os
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
os.environ.setdefault('QT_SCALE_FACTOR','1')
from pathlib import Path
import sys
import tempfile
import json
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from PySide6.QtGui import QGuiApplication
from PySide6.QtCore import qInstallMessageHandler,QtMsgType
from PySide6.QtTest import QTest
import shiboken6
from main import create_engine,ROOT


def main():
    app=QGuiApplication.instance() or QGuiApplication([])
    out=ROOT/'screenshots/botones';out.mkdir(parents=True,exist_ok=True)
    (ROOT/'.pytest_cache').mkdir(exist_ok=True)
    warnings=[];captures=[]
    previous=qInstallMessageHandler(lambda kind,ctx,msg:warnings.append(msg) if kind in
        (QtMsgType.QtWarningMsg,QtMsgType.QtCriticalMsg,QtMsgType.QtFatalMsg) else None)
    try:
        with tempfile.TemporaryDirectory(dir=ROOT/'.pytest_cache',prefix='buttons-') as temporary:
            os.environ['GF_CONSOLE_SETTINGS']=str(Path(temporary)/'settings.json')
            e,c=create_engine()
            try:
                w=e.rootObjects()[0]
                for mode,screen,file in [(1,'ModeSelect','mode-selector.png'),
                                         (2,'SynthFilter','soft-keys-synth.png'),
                                         (3,'DrumRack','soft-keys-drum.png')]:
                    c.triggerAction('mode:'+str(mode))
                    if screen=='ModeSelect':
                        e.console_buttons.handle('Boton',dict(nombre='MENU',presionado=True))
                    else:c.navigate(screen)
                    e.surface_batcher.drain_frame();QTest.qWait(100)
                    snapshot=w.grabWindow()
                    assert not snapshot.isNull() and snapshot.save(str(out/file))
                    captures.append(dict(file=file,screen=screen,width=snapshot.width(),height=snapshot.height()))
                e.console_buttons.section('BUILD');QTest.qWait(80)
                assert w.grabWindow().save(str(out/'section-armed.png'))
                captures.append(dict(file='section-armed.png',screen=c.screenName,width=w.width(),height=w.height()))
                assert not warnings,warnings
            finally:
                e.drum_surface.close();e.synth_surface.close();shiboken6.delete(e);app.processEvents()
    finally:qInstallMessageHandler(previous)
    (out/'manifest.json').write_text(json.dumps(dict(platform='offscreen/software',audio=False,
        physical_pi_verified=False,qml_rotation=0,captures=captures),indent=2)+'\n')
    print(out)


if __name__=='__main__':main()
