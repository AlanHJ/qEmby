#include "adminusercard.h"
#include "../utils/layoututils.h"
#include "elidedlabel.h"
#include "modernswitch.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimeZone>
#include <QVBoxLayout>

namespace
{

QString formatIsoDateTime(QString isoDate)
{
    if (isoDate.isEmpty())
    {
        return {};
    }

    QDateTime dateTime = QDateTime::fromString(isoDate, Qt::ISODate);
    if (!dateTime.isValid())
    {
        dateTime = QDateTime::fromString(isoDate, Qt::ISODateWithMs);
    }
    if (!dateTime.isValid())
    {
        return isoDate;
    }

    dateTime.setTimeZone(QTimeZone::UTC);
    return dateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString displayNameForUser(const UserInfo &user)
{
    const QString trimmedName = user.name.trimmed();
    if (!trimmedName.isEmpty())
    {
        return trimmedName;
    }
    return user.id;
}

QString pageUsersText(const char *sourceText)
{
    return QCoreApplication::translate("PageUsers", sourceText);
}

void setPassiveClickThrough(QWidget *widget)
{
    if (!widget)
    {
        return;
    }
    widget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
}

} 

AdminUserCard::AdminUserCard(UserInfo userInfo, QString currentUserId, QWidget *parent)
    : QFrame(parent), m_user(userInfo), m_currentUserId(currentUserId), m_displayName(displayNameForUser(m_user))
{
    static constexpr int CardWidth = 268;
    static constexpr int CardHeight = 196;

    setObjectName("ManageUserCard");
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setToolTip(m_displayName);
    setFixedSize(CardWidth, CardHeight);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto *cardLayout = new QVBoxLayout(this);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(8);

    auto *topRow = new QHBoxLayout();
    topRow->setContentsMargins(0, 0, 0, 0);
    topRow->setSpacing(10);

    auto *avatarLabel = new QLabel(this);
    avatarLabel->setObjectName("ManageUserAvatar");
    avatarLabel->setFixedSize(48, 48);
    avatarLabel->setAlignment(Qt::AlignCenter);
    avatarLabel->setText(m_displayName.isEmpty() ? QStringLiteral("?") : m_displayName.left(1).toUpper());
    setPassiveClickThrough(avatarLabel);
    topRow->addWidget(avatarLabel, 0, Qt::AlignTop);

    auto *titleColumn = new QVBoxLayout();
    titleColumn->setContentsMargins(0, 0, 0, 0);
    titleColumn->setSpacing(4);

    auto *nameLabel = new ElidedLabel(this);
    nameLabel->setObjectName("ManageCardTitle");
    nameLabel->setFullText(m_displayName);
    setPassiveClickThrough(nameLabel);
    titleColumn->addWidget(nameLabel);

    auto *badgeRow = new QHBoxLayout();
    badgeRow->setContentsMargins(0, 0, 0, 0);
    badgeRow->setSpacing(6);

    if (m_user.isAdministrator)
    {
        auto *adminBadge = new QLabel(pageUsersText("Admin"), this);
        adminBadge->setObjectName("ManageUserBadgeAdmin");
        setPassiveClickThrough(adminBadge);
        badgeRow->addWidget(adminBadge);
    }

    if (m_user.isDisabled)
    {
        auto *disabledBadge = new QLabel(pageUsersText("Disabled"), this);
        disabledBadge->setObjectName("ManageUserBadgeDisabled");
        setPassiveClickThrough(disabledBadge);
        badgeRow->addWidget(disabledBadge);
    }

    badgeRow->addStretch(1);
    titleColumn->addLayout(badgeRow);
    topRow->addLayout(titleColumn, 1);

    cardLayout->addLayout(topRow);

    auto *separator = LayoutUtils::createSeparator(this);
    setPassiveClickThrough(separator);
    cardLayout->addWidget(separator);

    const QString lastLoginText = formatIsoDateTime(m_user.lastLoginDate);
    auto *lastLoginLabel = new QLabel(
        pageUsersText("Last login: %1").arg(lastLoginText.isEmpty() ? QStringLiteral("-") : lastLoginText), this);
    lastLoginLabel->setObjectName("ManageSessionClient");
    lastLoginLabel->setWordWrap(true);
    setPassiveClickThrough(lastLoginLabel);
    cardLayout->addWidget(lastLoginLabel);

    const QString lastActivityText = formatIsoDateTime(m_user.lastActivityDate);
    auto *lastActivityLabel = new QLabel(
        pageUsersText("Last active: %1").arg(lastActivityText.isEmpty() ? QStringLiteral("-") : lastActivityText),
        this);
    lastActivityLabel->setObjectName("ManageSessionTime");
    lastActivityLabel->setWordWrap(true);
    setPassiveClickThrough(lastActivityLabel);
    cardLayout->addWidget(lastActivityLabel);

    cardLayout->addStretch(1);

    auto *footerRow = new QHBoxLayout();
    footerRow->setContentsMargins(0, 0, 0, 0);
    footerRow->setSpacing(8);
    footerRow->addStretch(1);

    auto *switchWidget = new ModernSwitch(this);
    switchWidget->setChecked(!m_user.isDisabled);
    switchWidget->setToolTip(m_user.isDisabled ? pageUsersText("Enable user") : pageUsersText("Disable user"));
    if (m_user.id == m_currentUserId)
    {
        switchWidget->setEnabled(false);
        switchWidget->setToolTip(pageUsersText("Cannot disable yourself"));
    }
    connect(switchWidget, &ModernSwitch::toggled, this,
            [this, switchWidget](bool checked)
            {
                switchWidget->setToolTip(checked ? pageUsersText("Disable user") : pageUsersText("Enable user"));
                emit toggleRequested(m_user.id, m_displayName, checked);
            });
    footerRow->addWidget(switchWidget, 0, Qt::AlignVCenter);

    auto *deleteBtn = new QPushButton(this);
    deleteBtn->setObjectName("CollCardDeleteBtn");
    deleteBtn->setFixedSize(24, 24);
    deleteBtn->setCursor(Qt::PointingHandCursor);
    deleteBtn->setToolTip(pageUsersText("Delete user"));
    if (m_user.id == m_currentUserId)
    {
        deleteBtn->setEnabled(false);
        deleteBtn->setToolTip(pageUsersText("Cannot delete yourself"));
    }
    connect(deleteBtn, &QPushButton::clicked, this, [this]() { emit deleteRequested(m_user.id, m_displayName); });
    footerRow->addWidget(deleteBtn, 0, Qt::AlignVCenter);

    cardLayout->addLayout(footerRow);
}

void AdminUserCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        emit clicked(m_user.id);
        event->accept();
        return;
    }

    QFrame::mouseReleaseEvent(event);
}
