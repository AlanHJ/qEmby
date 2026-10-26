// Offline regression driver. This file is not linked into the released app.
#include "mainwindow.h"
#include "components/mpvwidget.h"
#include "managers/playbackmanager.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include "models/media/playerlaunchcontext.h"
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QImage>
#include <QPointer>
#include <QSurfaceFormat>
#include <QTimer>
#include <QUrl>

int main(int argc, char **argv)
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("qEmbyPlaybackProbe"));
    app.setOrganizationName(QStringLiteral("qEmbyCI"));
    if (app.arguments().size() != 3)
        return 2;
    const QString video = app.arguments().at(1);
    const bool independent = app.arguments().at(2) == QStringLiteral("independent");
    ConfigStore::instance()->set(ConfigKeys::UiAnimations, true);
    ConfigStore::instance()->set(ConfigKeys::PlayerIndependentWindow, independent);
    qInfo() << "[PlaybackProbe] Runtime | Qt:" << qVersion()
            << "| platform:" << QGuiApplication::platformName()
            << "| independent:" << independent;
    MainWindow window;
    window.showCurrentWindow();
    QTimer::singleShot(500, &window, &MainWindow::navigateToHome);
    QPointer<MpvWidget> player;
    double position = 0;
    WId originalId = 0;
    QTimer::singleShot(1800, &window, [&]() {
        originalId = window.internalWinId();
        PlayerLaunchContext context;
        context.mediaItem.id = QStringLiteral("offline-playback-probe");
        context.mediaItem.name = QStringLiteral("Offline playback probe");
        context.mediaItem.type = QStringLiteral("Movie");
        PlaybackManager::instance()->startInternalPlayback(
            context.mediaItem.id, context.mediaItem.name,
            QUrl::fromLocalFile(video).toString(), 0, QVariant::fromValue(context));
    });
    QTimer poll;
    poll.setInterval(100);
    QObject::connect(&poll, &QTimer::timeout, &window, [&]() {
        if (!player) {
            for (QWidget *top : QApplication::topLevelWidgets()) {
                if (auto *candidate = top->findChild<MpvWidget *>()) {
                    player = candidate;
                    QObject::connect(candidate, &MpvWidget::positionChanged, &window,
                                     [&](double value) { position = value; });
                    break;
                }
            }
        }
        if (!player || !player->hasPresentedVideo() || position < 1.5)
            return;
        QWidget *host = player->window();
        if (!host || !host->isVisible() || !player->isVisible() ||
            (!independent && window.internalWinId() != originalId)) {
            qCritical() << "[PlaybackProbe] FAIL player host disappeared or was recreated";
            app.exit(3);
            return;
        }
        const QImage frame = player->grabFramebuffer();
        bool varied = false;
        if (!frame.isNull()) {
            const QRgb first = frame.pixel(0, 0);
            for (int y = 0; y < frame.height() && !varied; y += 16)
                for (int x = 0; x < frame.width(); x += 16)
                    if (frame.pixel(x, y) != first) { varied = true; break; }
        }
        if (!varied)
            return;
        qInfo() << "[PlaybackProbe] PASS first-frame, advancing playback, visible host and rendered pixels";
        poll.stop();
        player->stop();
        app.exit(0);
    });
    poll.start();
    QTimer::singleShot(45000, &window, [&]() {
        qCritical() << "[PlaybackProbe] FAIL timeout | player:" << bool(player)
                    << "| position:" << position;
        app.exit(4);
    });
    return app.exec();
}
