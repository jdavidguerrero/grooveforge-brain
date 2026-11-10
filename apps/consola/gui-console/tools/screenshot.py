"""Render the registry deterministically, plus an offline index and manifest."""
import argparse
import html
import json
import os
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
os.environ.setdefault('QT_QUICK_BACKEND', 'software')
os.environ.setdefault('QT_SCALE_FACTOR', '1')
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PySide6.QtCore import QtMsgType, qInstallMessageHandler
from PySide6.QtGui import QGuiApplication
from PySide6.QtTest import QTest
from main import ROOT, create_engine
from registry import SCREENS, screen_id


def capture(names=None, synth_only=False):
    app = QGuiApplication.instance() or QGuiApplication([])
    messages = []
    def collect(kind, context, message):
        if kind in (QtMsgType.QtWarningMsg, QtMsgType.QtCriticalMsg, QtMsgType.QtFatalMsg):
            messages.append(message)
    previous = qInstallMessageHandler(collect)
    engine, state = create_engine(frameless=True)
    try:
        if not engine.rootObjects():
            raise RuntimeError('Main.qml failed to load: ' + '\n'.join(messages))
        window = engine.rootObjects()[0]
        destination = ROOT/'screenshots'
        destination.mkdir(exist_ok=True)
        ids = [screen_id(n) for n in names] if names else [i for i,s in enumerate(SCREENS,1) if not synth_only or s.name.startswith('Synth')]
        manifest = []
        for i in ids:
            descriptor = SCREENS[i-1]
            state.setScreen(i)
            QTest.qWait(400)
            app.processEvents()
            if messages:
                raise RuntimeError(descriptor.name + ': ' + '\n'.join(messages))
            image = window.grabWindow()
            path = destination/f'{i:02d}-{descriptor.name}.png'
            if (image.width(), image.height()) != (1280, 720) or not image.save(str(path)):
                raise RuntimeError(f'Capture failed: {path}, {image.size()}')
            manifest.append(dict(name=descriptor.name, file=path.name, mode=descriptor.mode, design=descriptor.design, width=1280, height=720))
            print(path)
        if not names:
            manifest_name='synth-manifest.json' if synth_only else 'manifest.json'
            gallery_name='synth-index.html' if synth_only else 'index.html'
            (destination/manifest_name).write_text(json.dumps(manifest, indent=2)+'\n')
            figures = '\n'.join(f'<figure id="{r["name"]}"><figcaption>{r["file"]} · {r["mode"]}</figcaption><a href="{r["file"]}"><img loading="lazy" width="1280" height="720" src="{r["file"]}" alt="{html.escape(r["name"])}"></a></figure>' for r in manifest)
            (destination/gallery_name).write_text('<!doctype html><html lang="en"><meta charset="utf-8"><title>GrooveForge Console review</title><style>body{margin:24px;background:#09090b;color:#f2f2f2;font:14px monospace}img{width:100%;height:auto;display:block}figure{margin:24px 0;max-width:1280px}figcaption{padding:12px 0;color:#5dcaa5}a{color:#5dcaa5}</style><h1>GrooveForge Console · Synth review</h1><p>Deterministic fake snapshots · 1280×720 · <a href="'+manifest_name+'">Manifest</a></p>'+figures+'</html>\n')
        window.close()
    finally:
        import shiboken6
        shiboken6.delete(engine)
        app.processEvents()
        qInstallMessageHandler(previous)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--screen', action='append', help='Capture only this registry name; repeatable')
    parser.add_argument('--synth',action='store_true',help='Capture the 25 synth views and a separate synth gallery')
    args=parser.parse_args()
    capture(args.screen,args.synth)
