import argparse
from pathlib import Path
import sys

from PySide6.QtCore import QUrl
from PySide6.QtGui import QGuiApplication, QFontDatabase, QFont
from PySide6.QtQuick import QQuickWindow
from PySide6.QtQml import QQmlApplicationEngine
from fake_data import ConsoleState

ROOT = Path(__file__).resolve().parent
from registry import SCREEN_NAMES, screen_id


def create_engine(state=None, frameless=False):
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
    engine.rootContext().setContextProperty('fallbackFonts', {'sans': sans_name, 'mono': mono_name})
    fonts = {'display':'BigShouldersDisplay.ttf', 'mono':'IBMPlexMono-Regular.ttf', 'monoMedium':'IBMPlexMono-Medium.ttf',
             'monoSemiBold':'IBMPlexMono-SemiBold.ttf', 'monoBold':'IBMPlexMono-Bold.ttf', 'archivo':'Archivo.ttf',
             'os':'GeneralSans.ttf', 'osMono':'JetBrainsMono.ttf'}
    engine.rootContext().setContextProperty('localFonts', {
        key: QUrl.fromLocalFile(str(ROOT/'fonts'/name)) for key,name in fonts.items()
        if (ROOT/'fonts'/name).is_file()})
    engine.load(QUrl.fromLocalFile(str(ROOT/'Main.qml')))
    return engine, state


def main():
    parser = argparse.ArgumentParser(description='GrooveForge Console fake-data desktop demo')
    parser.add_argument('--frameless', action='store_true')
    parser.add_argument('--screen', type=screen_id, default=5, metavar='NAME', help=', '.join(SCREEN_NAMES))
    parser.add_argument('--list-screens', action='store_true')
    args = parser.parse_args()
    if args.list_screens:
        print('\n'.join(SCREEN_NAMES))
        return 0
    app = QGuiApplication(sys.argv[:1])
    engine, state = create_engine(frameless=args.frameless)
    if not engine.rootObjects():
        return 1
    state.setScreen(args.screen)
    result = app.exec()
    import shiboken6
    shiboken6.delete(engine)
    return result


if __name__ == '__main__':
    sys.exit(main())
