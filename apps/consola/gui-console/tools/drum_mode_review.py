"""Capture the DRUM design registry with deterministic, explicitly silent preview data."""
import os
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
os.environ.setdefault('QT_SCALE_FACTOR','1')
import hashlib
import html
import json
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from PySide6.QtCore import qInstallMessageHandler,QtMsgType
from PySide6.QtGui import QGuiApplication
from PySide6.QtTest import QTest
import shiboken6
from main import ROOT,create_engine
from registry import SCREENS,screen_id
from drum_state import DRUM_SCREENS


def main():
    app=QGuiApplication.instance() or QGuiApplication([])
    messages=[]
    previous=qInstallMessageHandler(lambda kind,context,message:messages.append(message) if kind in (QtMsgType.QtWarningMsg,QtMsgType.QtCriticalMsg,QtMsgType.QtFatalMsg) else None)
    engine,state=create_engine(frameless=True)
    destination=ROOT/'screenshots/modo-drum';destination.mkdir(parents=True,exist_ok=True)
    design=ROOT.parents[1]/'docs/consola/design/claude-design-drum/Console Drum Screens.html'
    manifest=[]
    try:
        assert engine.rootObjects(),messages
        window=engine.rootObjects()[0]
        for name in (*DRUM_SCREENS,'ModeSelect'):
            descriptor=SCREENS[screen_id(name)-1]
            state.setScreen(screen_id(name))
            QTest.qWait(200);app.processEvents()
            if messages: raise RuntimeError(name+': '+'\n'.join(messages))
            image=window.grabWindow();file=f'{descriptor.design}-{name}.png'
            assert image.width()==1280 and image.height()==720 and image.save(str(destination/file))
            manifest.append(dict(name=name,design=descriptor.design,file=file,width=1280,height=720,
                                 snapshot='deterministic fake · no audio'))
            print(destination/file)
        source=dict(path=str(design.relative_to(ROOT.parents[2])),sha256=hashlib.sha256(design.read_bytes()).hexdigest(),
                    extraction='Parsed JSON script[type="__bundler/template"] and Layout notes for QML; no font assets extracted')
        (destination/'manifest.json').write_text(json.dumps(dict(source=source,screens=manifest),indent=2)+'\n')
        figures='\n'.join(f'<figure><figcaption>{r["design"]} · {r["name"]}</figcaption><a href="{r["file"]}"><img width="1280" height="720" loading="lazy" src="{r["file"]}" alt="{html.escape(r["name"])}"></a></figure>' for r in manifest)
        (destination/'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8"><title>DRUM review</title><style>body{background:#0a0a0a;color:#f2f2f2;font:14px monospace;margin:24px}figure{margin:24px 0}img{width:100%;max-width:1280px;height:auto}figcaption{color:#5dcaa5;padding:12px}a{color:#5dcaa5}</style><h1>Console DRUM · designs 46–55</h1><p>Deterministic preview. No audio or hardware certification. <a href="manifest.json">Manifest</a></p>'+figures+'</html>\n')
    finally:
        engine.drum_surface.close();engine.synth_surface.close();shiboken6.delete(engine);app.processEvents();qInstallMessageHandler(previous)


if __name__=='__main__':main()
