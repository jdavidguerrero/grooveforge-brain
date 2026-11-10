import argparse
from pathlib import Path
import sys
import os

from PySide6.QtCore import QUrl, Qt
from PySide6.QtGui import QGuiApplication, QFontDatabase, QFont
from PySide6.QtQuick import QQuickWindow
from PySide6.QtQml import QQmlApplicationEngine
from fake_data import ConsoleState

ROOT = Path(__file__).resolve().parent
from registry import SCREEN_NAMES, screen_id
from rotation import add_rotation_arguments, configure_rotation
from surface_batch import SurfaceBatcher


def create_engine(state=None, frameless=False, rotation=None, perf_mode=False, display_screen=None, cycle_screens=False, synth_socket=None, synth_transport=None, fake_surface=False):
    sans = QFontDatabase.systemFont(QFontDatabase.GeneralFont)
    mono = QFontDatabase.systemFont(QFontDatabase.FixedFont)
    families = QFontDatabase.families()
    sans_name = next((name for name in [sans.family(), 'Helvetica Neue', 'DejaVu Sans', 'Liberation Sans'] if name in families), families[0])
    mono_name = next((name for name in [mono.family(), 'Menlo', 'DejaVu Sans Mono', 'Liberation Mono'] if name in families),
                     next((name for name in families if QFontDatabase.isFixedPitch(name)), sans_name))
    QGuiApplication.setFont(QFont(sans_name))
    engine = QQmlApplicationEngine()
    state = state or ConsoleState()
    state.setParent(engine)
    engine.console_state = state
    engine.rootContext().setContextProperty('console', state)
    engine.rootContext().setContextProperty('frameless', frameless)
    rotation = rotation or configure_rotation(0, 'auto')
    engine.rootContext().setContextProperty('qmlRotation', rotation['qml_angle'])
    engine.rootContext().setContextProperty('perfMode', perf_mode)
    engine.rootContext().setContextProperty('cycleScreens', cycle_screens)
    engine.rootContext().setContextProperty('eglfsMode', os.environ.get('QT_QPA_PLATFORM') == 'eglfs')
    from synth_state import SynthState
    from synth_client import SynthClient
    client = SynthClient(synth_socket, engine, transport=synth_transport) if synth_socket else None
    synth = SynthState(state, client, engine)
    state.synth = synth
    engine.synth_state = synth
    engine.rootContext().setContextProperty('synth', synth)
    batcher = SurfaceBatcher(state, engine)
    engine.surface_batcher = batcher
    from touch_controller import TouchController
    from synth_surface import SynthSurface
    touch = TouchController(state, engine)
    surface = SynthSurface(state, synth, batcher, touch, parent=engine)
    engine.touch_controller, engine.synth_surface = touch, surface
    state.surface = surface
    engine.rootContext().setContextProperty('touch', touch)
    engine.rootContext().setContextProperty('surface', surface)
    engine.rootContext().setContextProperty('fakeSurface', fake_surface)
    engine.rootContext().setContextProperty('surfaceBatch', batcher)
    engine.rootContext().setContextProperty('fallbackFonts', {'sans': sans_name, 'mono': mono_name})
    fonts = {'display':'BigShouldersDisplay.ttf', 'mono':'IBMPlexMono-Regular.ttf', 'monoMedium':'IBMPlexMono-Medium.ttf',
             'monoSemiBold':'IBMPlexMono-SemiBold.ttf', 'monoBold':'IBMPlexMono-Bold.ttf', 'archivo':'Archivo.ttf',
             'os':'GeneralSans.ttf', 'osMono':'JetBrainsMono.ttf'}
    engine.rootContext().setContextProperty('localFonts', {
        key: QUrl.fromLocalFile(str(ROOT/'fonts'/name)) for key,name in fonts.items()
        if (ROOT/'fonts'/name).is_file()})
    engine.load(QUrl.fromLocalFile(str(ROOT/'Main.qml')))
    if engine.rootObjects():
        window = engine.rootObjects()[0]
        if display_screen:
            screens = QGuiApplication.screens()
            match = next((screen for screen in screens if screen.name() == display_screen), None)
            if match is None:
                raise ValueError(f'Unknown display {display_screen!r}; available: {[s.name() for s in screens]}')
            window.setScreen(match)
        window.afterAnimating.connect(batcher.drain_frame)
        batcher.wakeRequested.connect(window.update, Qt.QueuedConnection)
        synth.wakeRequested.connect(window.update, Qt.QueuedConnection)
        if client:
            client.start()
    return engine, state


def main():
    parser = argparse.ArgumentParser(description='GrooveForge Console visual demo (no motor connection)')
    add_rotation_arguments(parser)
    parser.add_argument('--display-screen', help='QScreen name; select DRM outputs with GUI_KMS_CONFIG')
    parser.add_argument('--frameless', action='store_true')
    parser.add_argument('--screen', type=screen_id, default=5, metavar='NAME', help=', '.join(SCREEN_NAMES))
    parser.add_argument('--synth-socket', help='Real/fake contract-14 Unix socket (default: XDG_RUNTIME_DIR/gf-synth.sock)')
    parser.add_argument('--preview', action='store_true', help='Deterministic offline design review')
    parser.add_argument('--fake-synth', action='store_true', help='Launch a separate fake host process; no audio')
    parser.add_argument('--synth-mode', action='store_true', help='Enter SYNTH performance mode')
    parser.add_argument('--surface-port', help='MIDI input/output substring, e.g. GF Consola')
    parser.add_argument('--fake-surface', action='store_true', help='Desktop pad/encoder simulation, no surface input')
    parser.add_argument('--host-midi-port', default='gf-synth-host', help='Host MIDI output substring')
    parser.add_argument('--note-transport', choices=['auto', 'midi', 'socket'], default='auto')
    parser.add_argument('--external-midi-port', action='append', default=[], help='External input substring; auto opens external keyboards, none disables discovery')
    parser.add_argument('--surface-cdc', help='Explicit GF Consola CDC device for HP/IN')
    parser.add_argument('--room-node', default='@DEFAULT_AUDIO_SINK@', help='PipeWire room sink node for MASTER')
    parser.add_argument('--list-screens', action='store_true')
    args = parser.parse_args()
    if args.list_screens:
        print('\n'.join(SCREEN_NAMES))
        return 0
    rotation = configure_rotation(args.rotate, args.rotation_method)
    print('Rotation:', rotation)
    app = QGuiApplication(sys.argv[:1])
    import subprocess
    import tempfile
    from fake_synth_host import socket_path
    fake_process = None
    fake_dir = None
    synth_socket = None if args.preview else args.synth_socket or socket_path()
    if args.fake_synth:
        fake_dir = tempfile.TemporaryDirectory(prefix='gf-synth-')
        synth_socket = str(Path(fake_dir.name)/'host.sock')
        fake_process = subprocess.Popen([sys.executable, str(ROOT/'fake_synth_host.py'), '--socket', synth_socket], stdout=subprocess.PIPE, text=True)
        ready = fake_process.stdout.readline()
        if not ready:
            raise RuntimeError('Fake host failed to start')
        print(ready.strip())
    engine, state = create_engine(synth_socket=synth_socket, frameless=args.frameless, rotation=rotation, display_screen=args.display_screen, fake_surface=args.fake_surface)
    if not engine.rootObjects():
        return 1
    if args.synth_mode:
        state._mode = 2
    state.setScreen(screen_id('SynthParts') if args.synth_mode and args.screen == 5 else args.screen)
    if args.surface_port or args.fake_surface:
        from synth_surface import open_note_output, RoomControls
        try:
            if not args.preview:
                engine.synth_surface.notes, route = open_note_output(args.note_transport, args.host_midi_port,
                                                                      synth_socket, args.fake_surface)
                engine.synth_surface._status = ('FAKE SURFACE · ' if args.fake_surface else '')+route
                engine.synth_surface.changed.emit()
                print('Note route:', route)
            if args.surface_port:
                engine.synth_surface.room = RoomControls(args.surface_cdc, args.room_node)
                engine.synth_surface.open_ports(args.surface_port, args.external_midi_port)
        except (OSError, ValueError, ImportError, RuntimeError) as exc:
            engine.synth_surface.close()
            engine.synth_surface._failure(str(exc))
            print(f'Surface unavailable: {exc}', file=sys.stderr)
    try:
        return app.exec()
    finally:
        engine.synth_surface.close()
        if engine.synth_state.client:
            engine.synth_state.client.stop()
        import shiboken6
        shiboken6.delete(engine)
        if fake_process:
            fake_process.terminate()
            fake_process.wait(timeout=5)
        if fake_dir:
            fake_dir.cleanup()


if __name__ == '__main__':
    sys.exit(main())
