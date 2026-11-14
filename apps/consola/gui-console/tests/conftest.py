import os
os.environ['QT_QPA_PLATFORM'] = 'offscreen'
os.environ['QT_QUICK_BACKEND'] = 'software'
os.environ['QT_SCALE_FACTOR'] = '1'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pytest
from PySide6.QtGui import QGuiApplication, QFont
from PySide6.QtCore import qInstallMessageHandler, QtMsgType
from PySide6.QtTest import QTest
import shiboken6
from main import create_engine


@pytest.fixture(scope='session')
def app():
    app = QGuiApplication.instance() or QGuiApplication([])
    app.setFont(QFont('Helvetica Neue'))
    return app


@pytest.fixture
def gui(app):
    messages = []
    def collect(kind, context, message):
        if kind in (QtMsgType.QtWarningMsg, QtMsgType.QtCriticalMsg, QtMsgType.QtFatalMsg):
            messages.append(message)
    previous = qInstallMessageHandler(collect)
    engine, state = create_engine()
    assert engine.rootObjects(), '\n'.join(messages)
    QTest.qWait(40)
    yield engine, state, engine.rootObjects()[0], messages
    shiboken6.delete(engine)
    app.processEvents()
    qInstallMessageHandler(previous)
    assert messages == []


@pytest.fixture(autouse=True)
def isolated_preferences(tmp_path, monkeypatch):
    monkeypatch.setenv('GF_CONSOLE_SETTINGS', str(tmp_path/'settings.json'))
