import os
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
os.environ.setdefault('QT_QUICK_BACKEND', 'software')
os.environ.setdefault('QT_SCALE_FACTOR', '1')
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PySide6.QtGui import QGuiApplication
from PySide6.QtTest import QTest
from main import ROOT, SCREEN_NAMES, create_engine


def capture():
    app = QGuiApplication.instance() or QGuiApplication([])
    engine, state = create_engine(frameless=True)
    if not engine.rootObjects():
        raise RuntimeError('Main.qml failed to load')
    window = engine.rootObjects()[0]
    destination = ROOT/'screenshots'
    destination.mkdir(exist_ok=True)
    for i, name in enumerate(SCREEN_NAMES, 1):
        state.setScreen(i)
        QTest.qWait(400)
        app.processEvents()
        image = window.grabWindow()
        path = destination/f'{i:02d}-{name}.png'
        if image.size().width() != 1280 or image.size().height() != 720 or not image.save(str(path)):
            raise RuntimeError(f'Capture failed: {path}, {image.size()}')
        print(path)
    window.close()
    import shiboken6
    shiboken6.delete(engine)
    app.processEvents()


if __name__ == '__main__':
    capture()
