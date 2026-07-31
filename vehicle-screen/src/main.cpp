#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QUrl>
#include <QString>
#include <iostream>
#include "backend.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;

    Backend backend;
    engine.rootContext()->setContextProperty("backend", &backend);

    QUrl url(QStringLiteral("qrc:/qml/Main.qml"));
    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        std::cerr << "Failed to load QML file" << std::endl;
        return -1;
    }

    auto root = engine.rootObjects().first();
    std::cout << "Root object loaded: " << root->metaObject()->className() << std::endl;

    QQuickWindow *window = qobject_cast<QQuickWindow *>(root);
    if (window) {
        window->show();
        window->raise();
        window->requestActivate();
        std::cout << "Window shown with size: " << window->width() << "x" << window->height() << std::endl;
    }

    std::cout << "Application started successfully." << std::endl;
    return app.exec();
}