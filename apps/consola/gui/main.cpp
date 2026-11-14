#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

#include "SerialController.h"

int main(int argc, char *argv[])
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
    // Enable high-quality rendering (available in Qt 5.4+)
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

    QGuiApplication app(argc, argv);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6 only: Set OpenGL rendering backend for better performance
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
#endif

    // Optimize render loop for embedded systems (works in both Qt5 and Qt6)
    qputenv("QSG_RENDER_LOOP", "basic");  // Stable render loop for embedded

    QQmlApplicationEngine engine;
    auto serialController = new SerialController(&app);
    engine.rootContext()->setContextProperty(QStringLiteral("serialController"), serialController);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt6: Use objectCreationFailed signal
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
#else
    // Qt5: Add import path for PushClone module
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
#endif

    // Load Main.qml directly from resources
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/PushClone/Main.qml")));

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt5: Check if root objects is empty
    if (engine.rootObjects().isEmpty())
        return -1;
#endif

    return app.exec();
}
