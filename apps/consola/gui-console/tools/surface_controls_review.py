"""Capture the fixed surface controls with deterministic silent preview data."""
import os
os.environ.setdefault('QT_QPA_PLATFORM','offscreen')
os.environ.setdefault('QT_QUICK_BACKEND','software')
os.environ.setdefault('QT_SCALE_FACTOR','1')
import json
from pathlib import Path
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from PySide6.QtCore import qInstallMessageHandler,QtMsgType
from PySide6.QtGui import QGuiApplication
from PySide6.QtTest import QTest
import shiboken6
from main import ROOT,create_engine
from drum_state import DRUM_SCREENS


def main():
    app=QGuiApplication.instance() or QGuiApplication([])
    out=ROOT/'screenshots/surface-controls';out.mkdir(exist_ok=True)
    warnings=[];manifest=[]
    previous=qInstallMessageHandler(lambda kind,ctx,msg:warnings.append(msg) if kind in
        (QtMsgType.QtWarningMsg,QtMsgType.QtCriticalMsg,QtMsgType.QtFatalMsg) else None)
    try:
        with tempfile.TemporaryDirectory(prefix='gf-controls-review-') as directory:
            os.environ['GF_CONSOLE_SETTINGS']=str(Path(directory)/'settings.json')
            for angle,names in [(0,(*DRUM_SCREENS,'Settings','SynthHybrid','ModeSelect')),(90,('DrumRack','Settings'))]:
                e,c=create_engine(frameless=True,rotation={'qml_angle':angle})
                try:
                    w=e.rootObjects()[0]
                    for name in names:
                        c.navigate(name);e.surface_batcher.drain_frame();QTest.qWait(80)
                        image=w.grabWindow();file=f'{name}-{angle}.png'
                        assert not image.isNull() and image.save(str(out/file))
                        assert not warnings,warnings
                        manifest.append(dict(screen=name,file=file,qml_rotation=angle,width=image.width(),height=image.height(),
                            data='deterministic preview; no audio',hands='master/left/right OFF',pad_velocity=100))
                        if angle==0 and name=='Settings': image.save(str(ROOT/'screenshots/04-Settings.png'))
                        if angle==0 and name=='SynthHybrid': image.save(str(ROOT/'screenshots/53-SynthHybrid.png'))
                finally:
                    e.drum_surface.close();e.synth_surface.close();shiboken6.delete(e);app.processEvents()
        (out/'manifest.json').write_text(json.dumps(dict(platform='offscreen/software',physical_pi_verified=False,screens=manifest),indent=2)+'\n')
        figures=''.join(f'<figure><figcaption>{r["screen"]} · QML {r["qml_rotation"]}°</figcaption><a href="{r["file"]}"><img src="{r["file"]}" loading="lazy"></a></figure>' for r in manifest)
        (out/'index.html').write_text('<!doctype html><html lang="en"><meta charset="utf-8"><title>Surface controls correction</title><style>body{background:#101313;color:#eee;font:14px monospace;margin:24px}img{max-width:100%;max-height:900px}figure{margin:24px 0}figcaption{padding:12px;color:#5dcaa5}a{color:#5dcaa5}</style><h1>Surface controls · DRUM / SYNTH</h1><p>Deterministic silent preview · offscreen/software · no physical Pi certification. <a href="manifest.json">Provenance</a></p>'+figures+'</html>\n')
        print(f'{len(manifest)} captures; no QML warnings: {out}')
    finally:
        qInstallMessageHandler(previous)


if __name__=='__main__':main()
