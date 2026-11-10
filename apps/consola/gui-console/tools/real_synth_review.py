"""Qt review against the compiled C++ host, using inherited Unix sockets in sandboxes.

Run: python3 tools/real_synth_review.py /path/to/gf-synth-host --screenshots DIR
The application still uses SynthClient + QLocalSocket; only listener setup is injected.
"""
import argparse
from contextlib import contextmanager
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
os.environ.setdefault('QT_QUICK_BACKEND', 'software')
os.environ.setdefault('QT_SCALE_FACTOR', '1')
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from PySide6.QtCore import QTimer, QIODevice, qInstallMessageHandler, QtMsgType
from PySide6.QtGui import QGuiApplication
from PySide6.QtNetwork import QLocalSocket
from PySide6.QtTest import QTest
import shiboken6
from main import create_engine


class InheritedSocket(QLocalSocket):
    def __init__(self, fd):
        super().__init__()
        self.fd = fd

    def connectToServer(self, _):
        fd, self.fd = self.fd, None
        if fd is None or not self.setSocketDescriptor(fd, QLocalSocket.ConnectedState, QIODevice.ReadWrite):
            raise RuntimeError('Cannot attach inherited Unix control socket')
        QTimer.singleShot(0, self.connected.emit)


def until(predicate, drain, timeout=5000):
    deadline = time.monotonic() + timeout/1000
    while time.monotonic() < deadline:
        QTest.qWait(5)
        drain()
        if predicate():
            return
    raise AssertionError('Real synth host did not reach the expected state')


@contextmanager
def review(binary):
    app = QGuiApplication.instance() or QGuiApplication([])
    with tempfile.TemporaryDirectory(prefix='gf-six-review-', dir='/tmp') as directory:
        parent, child = socket.socketpair()
        with open(Path(directory)/'host.log', 'w+') as log:
            process = subprocess.Popen([str(binary), '--backend', 'offline', '--no-state',
                '--presets', directory, '--control-fd', str(child.fileno())],
                pass_fds=(child.fileno(),), stdout=log, stderr=log)
            child.close()
            transport = InheritedSocket(parent.detach())
            messages = []
            previous = qInstallMessageHandler(lambda kind, context, message: messages.append(message)
                if kind in (QtMsgType.QtWarningMsg, QtMsgType.QtCriticalMsg, QtMsgType.QtFatalMsg) else None)
            engine = None
            try:
                engine, state = create_engine(synth_socket='inherited-real-host', synth_transport=transport)
                synth = engine.synth_state
                until(lambda: len(synth._parts)==4 and bool(synth.values) and not synth.client.pending, synth.flush_frame)
                yield engine, state, engine.rootObjects()[0], messages
                assert not messages, '\n'.join(messages)
            finally:
                if engine:
                    engine.synth_state.client.stop()
                    shiboken6.delete(engine)
                    app.processEvents()
                qInstallMessageHandler(previous)
                process.terminate()
                process.wait(timeout=5)
                transport.deleteLater()


def select_engine(synth, engine):
    synth.changeEngine(engine)
    until(lambda: synth.engine==engine and synth.current_pages and
        set(synth.values)=={p['id'] for pg in synth.current_pages for bank in pg['banks'] for p in bank}
        and not synth.client.pending, synth.flush_frame)


def capture(binary, directory):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    with review(binary) as (engine, state, window, messages):
        synth = engine.synth_state
        state.navigate('SynthDx7')
        select_engine(synth, 'dx7_6op')
        synth.selectPage('OPS')
        synth.chooseAlgorithm(5)
        until(lambda: synth.algorithm['number']==5 and not synth.client.pending, synth.flush_frame)
        for name, focused in [('dx7-6op-overview.png', False), ('dx7-6op-operator-focus.png', True)]:
            if focused:
                synth.focusOperator(6)
                for key, value in [('op6.left_depth',65),('op6.right_depth',45),('op6.left_curve',1),('op6.right_curve',3),('op6.level',82)]:
                    synth.request('set_param',part=synth.selectedPart,param=key,value=value)
                until(lambda: not synth.client.pending, synth.flush_frame)
                synth.chooseBank(1)
            QTest.qWait(100)
            image = window.grabWindow()
            assert (image.width(), image.height())==(1280,720) and image.save(str(directory/name))
            print(directory/name)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary',type=Path)
    parser.add_argument('--screenshots',type=Path,required=True)
    args=parser.parse_args()
    capture(args.binary,args.screenshots)
