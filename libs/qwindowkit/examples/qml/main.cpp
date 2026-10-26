// Copyright (C) 2023-2024 Stdware Collections (https:
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#include <QtGui/QGuiApplication>
#include <QtQml/QQmlApplicationEngine>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickWindow>

#include <QWKQuick/qwkquickglobal.h>

#ifdef Q_OS_WIN

extern "C" {
    Q_DECL_EXPORT unsigned long NvOptimusEnablement = 0x00000001;
    Q_DECL_EXPORT int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

int main(int argc, char *argv[]) {
    qputenv("QT_WIN_DEBUG_CONSOLE", "attach"); 
    qputenv("QSG_INFO", "1");
    qputenv("QSG_NO_VSYNC", "1");

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
#else
    qputenv("QT_QUICK_CONTROLS_STYLE", "Default");
#endif

#ifdef Q_OS_WINDOWS
    qputenv("QSG_RHI_BACKEND", "d3d11"); 
    qputenv("QT_QPA_DISABLE_REDIRECTION_SURFACE", "1");
#endif
    

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QGuiApplication application(argc, argv);
    
    QQuickWindow::setDefaultAlphaBuffer(true);
    QQmlApplicationEngine engine;
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    const bool curveRenderingAvailable = true;
#else
    const bool curveRenderingAvailable = false;
#endif
    engine.rootContext()->setContextProperty(QStringLiteral("$curveRenderingAvailable"), QVariant(curveRenderingAvailable));
    QWK::registerTypes(&engine);
    engine.load(QUrl(QStringLiteral("qrc:///main.qml")));
    return application.exec();
}