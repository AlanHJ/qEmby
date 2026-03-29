#include "playerwindow.h"
#include "../views/media/playerview.h"
#include <QVBoxLayout>
#include <QWKWidgets/widgetwindowagent.h>

PlayerWindow::PlayerWindow(QEmbyCore* core, QWidget *parent)
    : QWidget(parent)
{
    
    auto *agent = new QWK::WidgetWindowAgent(this);
    agent->setup(this);

    
    m_playerView = new PlayerView(core, this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_playerView);

    
    QWidget* topHUD = m_playerView->findChild<QWidget*>("playerTopHUD");
    if (topHUD) {
        agent->setTitleBar(topHUD);
    }

    
    if (auto* minBtn = m_playerView->findChild<QWidget*>("hud-min-btn"))
        agent->setSystemButton(QWK::WindowAgentBase::Minimize, minBtn);
    if (auto* maxBtn = m_playerView->findChild<QWidget*>("hud-max-btn"))
        agent->setSystemButton(QWK::WindowAgentBase::Maximize, maxBtn);
    if (auto* closeBtn = m_playerView->findChild<QWidget*>("hud-close-btn"))
        agent->setSystemButton(QWK::WindowAgentBase::Close, closeBtn);

    
    if (auto* backBtn = m_playerView->findChild<QWidget*>("hud-back-btn"))
        agent->setHitTestVisible(backBtn, true);

    
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
