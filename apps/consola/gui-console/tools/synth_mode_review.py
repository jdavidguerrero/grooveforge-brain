"""Reproducible desktop SYNTH screenshots; fake host data, no audio/hardware."""
import json
import os
from pathlib import Path
import sys
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from PySide6.QtGui import QGuiApplication
from PySide6.QtCore import qInstallMessageHandler, QtMsgType
from PySide6.QtTest import QTest
import shiboken6
from main import create_engine, ROOT


def capture():
    app=QGuiApplication.instance() or QGuiApplication([])
    warnings=[]
    previous=qInstallMessageHandler(lambda kind,ctx,msg:warnings.append(msg) if kind in (QtMsgType.QtWarningMsg,QtMsgType.QtCriticalMsg,QtMsgType.QtFatalMsg) else None)
    engine,state=create_engine(fake_surface=True)
    folder=ROOT/'screenshots'/'modo-synth'
    folder.mkdir(exist_ok=True)
    manifest=[]
    try:
        window=engine.rootObjects()[0]
        state.triggerAction('mode:2')
        for screen in ['ModeSelect','SynthParts','SynthFilter','SynthScale','SynthPresets','SynthSettingsAudio']:
            if screen=='SynthFilter':
                engine.synth_state.selectPart(1)
            state.navigate(screen)
            if screen=='SynthFilter':
                engine.synth_state.selectPage('FILTER')
            QTest.qWait(100)
            assert not warnings,warnings
            image=window.grabWindow()
            assert image.save(str(folder/(screen+'.png')))
            manifest.append(dict(screen=screen,file=screen+'.png',width=image.width(),height=image.height(),
                                 mode=state.screenMode,source='offline fake host; no audio or physical surface'))
        (folder/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        (folder/'index.html').write_text('<!doctype html><meta charset="utf-8"><title>SYNTH mode review</title><style>body{background:#08090b;color:#eee;font:16px monospace;margin:24px}img{max-width:100%;height:auto}figure{margin:24px 0}</style><h1>SYNTH · desktop preview</h1><p>Fake host data · no audio/hardware certification · 1280×720</p>'+''.join(f'<figure><figcaption>{m["screen"]}</figcaption><img src="{m["file"]}" alt="{m["screen"]}"></figure>' for m in manifest))
        print(folder/'index.html')
    finally:
        engine.synth_surface.close()
        shiboken6.delete(engine)
        app.processEvents()
        qInstallMessageHandler(previous)


if __name__=='__main__':
    capture()
