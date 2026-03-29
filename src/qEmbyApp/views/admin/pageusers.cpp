#include "pageusers.h"
#include "../../components/adminusercard.h"
#include "../../components/adduserdialog.h"
#include "../../components/flowlayout.h"
#include "../../components/moderntoast.h"
#include "../../components/usereditdialog.h"
#include "../../components/modernmessagebox.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/manager/servermanager.h>

#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QTransform>
#include <QVBoxLayout>
#include <QVariantAnimation>





PageUsers::PageUsers(QEmbyCore *core, QWidget *parent)
    : ManagePageBase(core, tr("Users"), parent) {
    m_mainLayout->setContentsMargins(16, 30, 0, 0);

    
    auto *titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 12, 0);

    m_btnRefresh = new QPushButton(this);
    m_btnRefresh->setObjectName("ManageRefreshBtn");
    m_btnRefresh->setCursor(Qt::PointingHandCursor);
    m_btnRefresh->setFixedSize(32, 32);
    m_btnRefresh->setToolTip(tr("Refresh"));
    titleRow->addWidget(m_btnRefresh);

    titleRow->addSpacing(6);

    m_btnAddUser = new QPushButton(this);
    m_btnAddUser->setObjectName("ManageLibCreateBtn");
    m_btnAddUser->setCursor(Qt::PointingHandCursor);
    m_btnAddUser->setFixedSize(32, 32);
    m_btnAddUser->setToolTip(tr("Add user"));
    titleRow->addWidget(m_btnAddUser);

    titleRow->addStretch(2);

    m_statsLabel = new QLabel(this);
    m_statsLabel->setObjectName("ManageInfoValue");
    m_statsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statsLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    titleRow->addWidget(m_statsLabel);

    m_mainLayout->addLayout(titleRow);

    auto *scrollContent = new QWidget(this);
    scrollContent->setAttribute(Qt::WA_StyledBackground, true);
    auto *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 12, 40);
    scrollLayout->setSpacing(16);

    m_cardsContainer = new QWidget(scrollContent);
    m_userCardsLayout = new FlowLayout(m_cardsContainer, 0, 12, 12);
    scrollLayout->addWidget(m_cardsContainer, 0, Qt::AlignTop);

    
    m_emptyStateContainer = new QWidget(scrollContent);
    m_emptyStateContainer->setSizePolicy(QSizePolicy::Preferred,
                                         QSizePolicy::Expanding);
    auto *emptyLayout = new QVBoxLayout(m_emptyStateContainer);
    emptyLayout->setContentsMargins(0, 0, 0, 0);
    emptyLayout->setSpacing(0);
    emptyLayout->addStretch(1);
    m_emptyLabel = new QLabel(tr("No users found"), m_emptyStateContainer);
    m_emptyLabel->setObjectName("ManageEmptyLabel");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->hide();
    emptyLayout->addWidget(m_emptyLabel);
    emptyLayout->addStretch(1);
    m_emptyStateContainer->hide();
    scrollLayout->addWidget(m_emptyStateContainer, 1);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("SettingsScrollArea");
    m_scrollArea->setWidget(scrollContent);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->viewport()->setAutoFillBackground(false);

    m_mainLayout->addWidget(m_scrollArea, 1);

    
    connect(m_btnRefresh, &QPushButton::clicked, this,
            [this]() { m_pendingTask = loadData(true); });
    connect(m_btnAddUser, &QPushButton::clicked, this, [this]() {
        auto *dialog = new AddUserDialog(m_core, this);
        connect(dialog, &AddUserDialog::userCreated, this,
                [this]() { m_pendingTask = loadData(false); });
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->open();
    });
}

void PageUsers::showEvent(QShowEvent *event) {
    ManagePageBase::showEvent(event);
    if (!m_loaded) {
        m_pendingTask = loadData();
        m_loaded = true;
    }
}

void PageUsers::clearUserCards() {
    while (QLayoutItem *item = m_userCardsLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

void PageUsers::updateContentState(int userCount) {
    const bool hasUsers = userCount > 0;
    m_cardsContainer->setVisible(hasUsers);
    m_emptyStateContainer->setVisible(!hasUsers);
    m_emptyLabel->setVisible(!hasUsers);
    m_emptyLabel->setText(tr("No users found"));

    if (hasUsers) {
        m_statsLabel->setText(tr("%n user(s)", "", userCount));
    } else {
        m_statsLabel->setText(tr("No users"));
    }
}

QCoro::Task<void> PageUsers::loadData(bool isManual) {
    QPointer<PageUsers> safeThis(this);

    if (m_isLoading && !isManual) co_return;
    m_isLoading = true;
    m_btnRefresh->setEnabled(false);

    if (isManual) {
        QIcon originalIcon = m_btnRefresh->icon();
        QSize iconSz = m_btnRefresh->iconSize();
        QPixmap basePx = originalIcon.pixmap(iconSz);

        auto *rotAnim = new QVariantAnimation(this);
        rotAnim->setStartValue(0.0);
        rotAnim->setEndValue(360.0);
        rotAnim->setDuration(800);
        rotAnim->setEasingCurve(QEasingCurve::InOutCubic);

        connect(rotAnim, &QVariantAnimation::valueChanged, this,
                [this, basePx, iconSz](const QVariant &val) {
                    qreal angle = val.toReal();
                    QTransform t;
                    t.translate(iconSz.width() / 2.0, iconSz.height() / 2.0);
                    t.rotate(angle);
                    t.translate(-iconSz.width() / 2.0, -iconSz.height() / 2.0);
                    QPixmap rotated =
                        basePx.transformed(t, Qt::SmoothTransformation);
                    int xOff = (rotated.width() - iconSz.width()) / 2;
                    int yOff = (rotated.height() - iconSz.height()) / 2;
                    QPixmap cropped =
                        rotated.copy(xOff, yOff, iconSz.width(), iconSz.height());
                    m_btnRefresh->setIcon(QIcon(cropped));
                });
        connect(rotAnim, &QVariantAnimation::finished, this,
                [this, originalIcon]() { m_btnRefresh->setIcon(originalIcon); });
        rotAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    try {
        auto users = co_await m_core->adminService()->getUsers();
        if (!safeThis) co_return;

        qDebug() << "[PageUsers] Loaded users"
                 << "| count=" << users.size();

        
        const QString currentUserId =
            m_core->serverManager()->activeProfile().userId;

        clearUserCards();

        updateContentState(users.size());
        if (users.isEmpty()) {
            qDebug() << "[PageUsers] No users available";
        } else {
            qDebug() << "[PageUsers] Rendering user cards"
                     << "| count=" << users.size()
                     << "| currentUserId=" << currentUserId;

            for (const UserInfo &user : users) {
                auto *card =
                    new AdminUserCard(user, currentUserId, m_cardsContainer);
                connect(card, &AdminUserCard::clicked, this,
                        [this](const QString &userId) { onCardClicked(userId); });
                connect(card, &AdminUserCard::toggleRequested, this,
                        [this](QString userId, QString userName, bool enable) {
                            m_pendingTask =
                                onToggleUserEnabled(userId, userName, enable);
                        });
                connect(card, &AdminUserCard::deleteRequested, this,
                        [this](QString userId, QString userName) {
                            m_pendingTask = onDeleteUser(userId, userName);
                        });
                m_userCardsLayout->addWidget(card);
            }
        }

    } catch (const std::exception &e) {
        if (!safeThis) co_return;
        ModernToast::showMessage(
            tr("Failed to load users: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (!safeThis) co_return;
    m_isLoading = false;
    m_btnRefresh->setEnabled(true);
}

QCoro::Task<void> PageUsers::onToggleUserEnabled(QString userId,
                                                   QString userName,
                                                   bool enable) {
    QPointer<PageUsers> safeThis(this);
    const bool isDisabled = !enable;

    qDebug() << "[PageUsers] Setting user policy"
             << "| userId=" << userId
             << "| userName=" << userName
             << "| isDisabled=" << isDisabled;

    bool needRefresh = false;
    try {
        QJsonObject policy;
        policy["IsDisabled"] = isDisabled;
        co_await m_core->adminService()->setUserPolicy(userId, policy);
        if (!safeThis) co_return;

        if (isDisabled) {
            ModernToast::showMessage(
                tr("User \"%1\" disabled").arg(userName), 2000);
        } else {
            ModernToast::showMessage(
                tr("User \"%1\" enabled").arg(userName), 2000);
        }

        needRefresh = true;
    } catch (const std::exception &e) {
        if (!safeThis) co_return;
        ModernToast::showMessage(
            tr("Failed to update user: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
        needRefresh = true;
    }

    if (needRefresh && safeThis) {
        co_await loadData(false);
    }
}

QCoro::Task<void> PageUsers::onDeleteUser(QString userId, QString userName) {
    QPointer<PageUsers> safeThis(this);

    bool confirmed = ModernMessageBox::question(
        this,
        tr("Delete User"),
        tr("Are you sure you want to delete user \"%1\"? This action cannot be undone.")
            .arg(userName),
        tr("Delete"),
        tr("Cancel"),
        ModernMessageBox::Danger,
        ModernMessageBox::Warning);

    if (!confirmed) co_return;

    qDebug() << "[PageUsers] Deleting user"
             << "| userId=" << userId
             << "| userName=" << userName;

    try {
        co_await m_core->adminService()->deleteUser(userId);
        if (!safeThis) co_return;

        ModernToast::showMessage(
            tr("User \"%1\" deleted").arg(userName), 2000);
        co_await loadData(false);
    } catch (const std::exception &e) {
        if (!safeThis) co_return;
        ModernToast::showMessage(
            tr("Failed to delete user: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }
}

void PageUsers::onCardClicked(const QString &userId) {
    qDebug() << "[PageUsers] Opening user editor"
             << "| userId=" << userId;
    auto *dialog = new UserEditDialog(m_core, userId, this);
    connect(dialog, &UserEditDialog::userUpdated, this,
            [this]() { m_pendingTask = loadData(false); });
    connect(dialog, &UserEditDialog::userDeleted, this,
            [this]() { m_pendingTask = loadData(false); });
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}
