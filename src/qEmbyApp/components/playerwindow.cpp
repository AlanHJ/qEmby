#include "playerwindow.h"
#include "../views/media/playerview.h"
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVBoxLayout>
#include <QWKWidgets/widgetwindowagent.h>

PlayerWindow::PlayerWindow(QEmbyCore* core, QWidget *parent)
    : QWidget(parent)
{
#if (defined(Q_OS_MACOS) || defined(Q_OS_MAC)) && \
    (QT_VERSION >= QT_VERSION_CHECK(6, 9, 0))
    setWindowFlag(Qt::ExpandedClientAreaHint, true);
    setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
    setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
#endif

    
    m_windowAgent = new QWK::WidgetWindowAgent(this);
    m_windowAgent->setup(this);
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    m_windowAgent->setWindowAttribute("no-system-buttons", false);
#endif
#if defined(Q_OS_MAC)
    m_windowAgent->setSystemButtonAreaCallback([](const QSize &size) {
        return QRect(QPoint(0, 0), QSize(80, size.height()));
    });
#endif

    
    m_playerView = new PlayerView(core, this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_playerView);

    
    QWidget* topHUD = m_playerView->findChild<QWidget*>("playerTopHUD");
    if (topHUD) {
        m_windowAgent->setTitleBar(topHUD);
    }

    
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
    if (auto* minBtn = m_playerView->findChild<QWidget*>("hud-min-btn"))
        m_windowAgent->setSystemButton(QWK::WindowAgentBase::Minimize, minBtn);
    if (auto* maxBtn = m_playerView->findChild<QWidget*>("hud-max-btn"))
        m_windowAgent->setSystemButton(QWK::WindowAgentBase::Maximize, maxBtn);
    if (auto* closeBtn = m_playerView->findChild<QWidget*>("hud-close-btn"))
        m_windowAgent->setSystemButton(QWK::WindowAgentBase::Close, closeBtn);
#endif

    
    if (auto* backBtn = m_playerView->findChild<QWidget*>("hud-back-btn"))
        m_windowAgent->setHitTestVisible(backBtn, true);

    connect(m_playerView, &PlayerView::playerChromeVisibilityChanged, this,
            &PlayerWindow::setMacSystemButtonsVisible);
    connect(m_playerView, &PlayerView::playbackTitleChanged, this,
            [this](const QString &title) { setWindowTitle(title); });

    
    connect(m_playerView, &BaseView::navigateBack, this, [this]() {
        close();
    });
}

void PlayerWindow::playMedia(const QString &mediaId, const QString &title,
                              const QString &streamUrl, long long startPositionTicks,
                              const QVariant& sourceInfoVar)
{
    setWindowTitle(title);
    m_playerView->playMedia(mediaId, title, streamUrl, startPositionTicks, sourceInfoVar);
}

void PlayerWindow::setMacSystemButtonsVisible(bool visible)
{
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    if (m_windowAgent) {
        m_windowAgent->setWindowAttribute("no-system-buttons", !visible);
    }
#else
    Q_UNUSED(visible);
#endif
}
