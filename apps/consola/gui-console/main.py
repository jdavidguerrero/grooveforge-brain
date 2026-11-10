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


def create_engine(state=None, frameless=False, rotation=None, perf_mode=False, display_screen=None, cycle_screens=False):
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
    batcher = SurfaceBatcher(state, engine)
    engine.surface_batcher = batcher
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
    return engine, state


def main():
    parser = argparse.ArgumentParser(description='GrooveForge Console visual demo (no motor connection)')
    add_rotation_arguments(parser)
    parser.add_argument('--display-screen', help='QScreen name; select DRM outputs with GUI_KMS_CONFIG')
    parser.add_argument('--frameless', action='store_true')
    parser.add_argument('--screen', type=screen_id, default=5, metavar='NAME', help=', '.join(SCREEN_NAMES))
    parser.add_argument('--list-screens', action='store_true')
    args = parser.parse_args()
    if args.list_screens:
        print('\n'.join(SCREEN_NAMES))
        return 0
    rotation = configure_rotation(args.rotate, args.rotation_method)
    print('Rotation:', rotation)
    app = QGuiApplication(sys.argv[:1])
    engine, state = create_engine(frameless=args.frameless, rotation=rotation, display_screen=args.display_screen)
    if not engine.rootObjects():
        return 1
    state.setScreen(args.screen)
    result = app.exec()
    import shiboken6
    shiboken6.delete(engine)
    return result


if __name__ == '__main__':
    sys.exit(main())
