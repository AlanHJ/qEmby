#include "danmakuserverlistitemwidget.h"

#include "../managers/thememanager.h"
#include "modernswitch.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QStringList>
#include <QVBoxLayout>

namespace {

QColor activeRemoveIconColor()
{
    return ThemeManager::instance()->isDarkMode()
               ? QColor(QStringLiteral("#FCA5A5"))
               : QColor(QStringLiteral("#DC2626"));
}

QColor inactiveRemoveIconColor()
{
    return ThemeManager::instance()->isDarkMode()
               ? QColor(QStringLiteral("#64748B"))
               : QColor(QStringLiteral("#94A3B8"));
}

} 

DanmakuServerListItemWidget::DanmakuServerListItemWidget(QWidget *parent)
    : QFrame(parent)
{
    constexpr int actionColumnWidth = 74;

    setObjectName("DanmakuServerListRow");
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto *outerLayout = new QHBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 8, 0);
    outerLayout->setSpacing(0);

    m_cardFrame = new QFrame(this);
    m_cardFrame->setObjectName("DanmakuServerCard");
    m_cardFrame->setProperty("selected", false);
    m_cardFrame->setProperty("currentServer", false);
    m_cardFrame->setProperty("serverEnabled", true);
    outerLayout->addWidget(m_cardFrame);

    auto *layout = new QHBoxLayout(m_cardFrame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    auto *textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    layout->addLayout(textLayout, 1);

    auto *actionContainer = new QWidget(m_cardFrame);
    actionContainer->setFixedWidth(actionColumnWidth);
    actionContainer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);

    auto *actionLayout = new QHBoxLayout(actionContainer);
    actionLayout->setContentsMargins(0, 0, 2, 0);
    actionLayout->setSpacing(4);
    actionLayout->addStretch(1);
    layout->addWidget(actionContainer, 0, Qt::AlignVCenter);

    m_removeButton = new QPushButton(actionContainer);
    m_removeButton->setObjectName("DanmakuServerRemoveButton");
    m_removeButton->setCursor(Qt::PointingHandCursor);
    m_removeButton->setToolTip(tr("Remove Server"));
    m_removeButton->setFixedSize(20, 20);
    m_removeButton->setIconSize(QSize(10, 10));
    m_removeButton->setFocusPolicy(Qt::NoFocus);
    m_enabledSwitch = new ModernSwitch(actionContainer);
    m_enabledSwitch->setFixedSize(m_enabledSwitch->sizeHint());
    m_enabledSwitch->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    actionLayout->addWidget(m_removeButton, 0, Qt::AlignVCenter | Qt::AlignRight);
    actionLayout->addWidget(m_enabledSwitch, 0, Qt::AlignVCenter | Qt::AlignRight);

    m_nameLabel = new QLabel(m_cardFrame);
    m_nameLabel->setObjectName("DanmakuServerCardTitle");
    m_nameLabel->setWordWrap(true);
    m_nameLabel->installEventFilter(this);
    textLayout->addWidget(m_nameLabel);

    m_baseUrlLabel = new QLabel(m_cardFrame);
    m_baseUrlLabel->setObjectName("DanmakuServerCardLine");
    m_baseUrlLabel->setWordWrap(true);
    m_baseUrlLabel->installEventFilter(this);
    textLayout->addWidget(m_baseUrlLabel);

    m_credentialsLabel = new QLabel(m_cardFrame);
    m_credentialsLabel->setObjectName("DanmakuServerCardMeta");
    m_credentialsLabel->setWordWrap(true);
    m_credentialsLabel->installEventFilter(this);
    textLayout->addWidget(m_credentialsLabel);

    m_cardFrame->installEventFilter(this);
    refreshRemoveIcon();

    connect(m_enabledSwitch, &ModernSwitch::toggled, this,
            &DanmakuServerListItemWidget::enabledChanged);
    connect(m_removeButton, &QPushButton::clicked, this,
            &DanmakuServerListItemWidget::removeRequested);
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this](ThemeManager::Theme) { refreshRemoveIcon(); });
}

void DanmakuServerListItemWidget::setServer(
    const DanmakuServerDefinition &server)
{
    m_server = server;

    const QString displayName = server.name.trimmed().isEmpty()
                                    ? server.baseUrl.trimmed()
                                    : server.name.trimmed();
    {
        const QSignalBlocker blocker(m_enabledSwitch);
        m_enabledSwitch->setChecked(server.enabled);
    }

    m_cardFrame->setProperty("serverEnabled", server.enabled);
    m_nameLabel->setText(displayName);
    m_baseUrlLabel->setText(server.baseUrl.trimmed());
    refreshCredentialsLabel();
    refreshSelectionState();
}

void DanmakuServerListItemWidget::setSelected(bool selected)
{
    if (m_cardFrame->property("selected").toBool() == selected) {
        return;
    }

    m_cardFrame->setProperty("selected", selected);
    refreshSelectionState();
}

void DanmakuServerListItemWidget::setCurrentServer(bool currentServer)
{
    if (m_cardFrame->property("currentServer").toBool() == currentServer) {
        return;
    }

    m_cardFrame->setProperty("currentServer", currentServer);
    refreshSelectionState();
}

void DanmakuServerListItemWidget::setRemovable(bool removable)
{
    m_removable = removable;
    m_removeButton->setVisible(removable);
    m_removeButton->setEnabled(removable);
    m_removeButton->setProperty("removable", removable);
    m_removeButton->setToolTip(removable ? tr("Remove Server")
                                         : QString());
    m_removeButton->style()->unpolish(m_removeButton);
    m_removeButton->style()->polish(m_removeButton);
    refreshRemoveIcon();
    updateGeometry();
}

bool DanmakuServerListItemWidget::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == m_cardFrame || watched == m_nameLabel ||
         watched == m_baseUrlLabel || watched == m_credentialsLabel) &&
        event && event->type() == QEvent::MouseButtonPress) {
        emit clicked();
    }

    return QFrame::eventFilter(watched, event);
}

QString DanmakuServerListItemWidget::maskedSecret(const QString &secret)
{
    const QString trimmed = secret.trimmed();
    if (trimmed.isEmpty()) {
        return QString();
    }
    if (trimmed.size() <= 4) {
        return QStringLiteral("****");
    }
    return QStringLiteral("%1****%2")
        .arg(trimmed.left(2), trimmed.right(2));
}

void DanmakuServerListItemWidget::refreshCredentialsLabel()
{
    QStringList parts;
    if (!m_server.appId.trimmed().isEmpty()) {
        parts.append(tr("App ID: %1").arg(m_server.appId.trimmed()));
    }
    if (!m_server.appSecret.trimmed().isEmpty()) {
        parts.append(tr("App Secret: %1")
                         .arg(maskedSecret(m_server.appSecret)));
    }

    const QString text = parts.join(QStringLiteral("    "));
    m_credentialsLabel->setVisible(!text.isEmpty());
    m_credentialsLabel->setText(text);
    updateGeometry();
}

void DanmakuServerListItemWidget::refreshRemoveIcon()
{
    m_removeButton->setIcon(ThemeManager::getAdaptiveIcon(
        QStringLiteral(":/svg/light/delete.svg"),
        m_removable ? activeRemoveIconColor() : inactiveRemoveIconColor()));
}

void DanmakuServerListItemWidget::refreshSelectionState()
{
    m_cardFrame->style()->unpolish(m_cardFrame);
    m_cardFrame->style()->polish(m_cardFrame);
    m_cardFrame->update();
}
