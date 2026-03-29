#include "adduserdialog.h"

#include "collapsiblesection.h"
#include "moderncombobox.h"
#include "modernswitch.h"
#include "moderntoast.h"
#include "../utils/layoututils.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QWidget *createScrollablePage(QVBoxLayout *&innerLayout, QWidget *parent) {
    auto *scroll = new QScrollArea(parent);
    scroll->setObjectName("LibCreateScrollArea");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto *container = new QWidget(scroll);
    container->setAttribute(Qt::WA_StyledBackground, true);

    innerLayout = new QVBoxLayout(container);
    innerLayout->setContentsMargins(0, 0, 12, 0);
    innerLayout->setSpacing(8);
    innerLayout->setAlignment(Qt::AlignTop);

    scroll->setWidget(container);
    return scroll;
}

QWidget *createPanelCard(QWidget *parent, QVBoxLayout *&bodyLayout) {
    auto *card = new QWidget(parent);
    card->setObjectName("LibAdvancedPanel");
    card->setAttribute(Qt::WA_StyledBackground, true);

    bodyLayout = new QVBoxLayout(card);
    bodyLayout->setContentsMargins(12, 10, 12, 10);
    bodyLayout->setSpacing(10);
    return card;
}

CollapsibleSection *createSectionCard(const QString &title, QWidget *parent,
                                      QVBoxLayout *&bodyLayout) {
    auto *section = new CollapsibleSection(title, parent);
    section->setExpanded(true);

    bodyLayout = section->contentLayout();
    bodyLayout->setSpacing(10);
    return section;
}

QLabel *createFieldLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("ManageInfoKey");
    label->setWordWrap(true);
    return label;
}

QLabel *createDescriptionLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("ManageInfoKey");
    label->setWordWrap(true);
    return label;
}

QWidget *createSwitchRow(const QString &label, ModernSwitch *&sw,
                         QWidget *parent) {
    auto *rowWidget = new QWidget(parent);

    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 6, 0);
    rowLayout->setSpacing(12);

    auto *lbl = new QLabel(label, rowWidget);
    lbl->setObjectName("ManageInfoKey");
    lbl->setWordWrap(true);
    rowLayout->addWidget(lbl, 1);

    sw = new ModernSwitch(rowWidget);
    rowLayout->addWidget(sw, 0, Qt::AlignVCenter);
    return rowWidget;
}

} 

AddUserDialog::AddUserDialog(QEmbyCore *core, QWidget *parent)
    : ModernDialogBase(parent), m_core(core) {
    setTitle(tr("Add User"));
    setMinimumWidth(500);
    setMaximumHeight(720);
    resize(520, 580);

    setupUi();
    updateCopyModeState();
    updatePrimaryActionState();

    m_pendingTask = loadInitialData();
}

void AddUserDialog::setupUi() {
    contentLayout()->setContentsMargins(20, 10, 0, 20);
    contentLayout()->setSpacing(12);

    QVBoxLayout *scrollLayout = nullptr;
    QWidget *scrollPage = createScrollablePage(scrollLayout, this);

    QVBoxLayout *basicBody = nullptr;
    auto *basicCard = createPanelCard(scrollPage, basicBody);
    QWidget *basicContent = basicBody->parentWidget();

    auto *nameTitle = new QLabel(tr("Name"), basicContent);
    nameTitle->setObjectName("ManageCardTitle");
    basicBody->addWidget(nameTitle);

    basicBody->addWidget(createDescriptionLabel(
        tr("This will be the name displayed for the user on the server."),
        basicContent));

    m_editUserName = new QLineEdit(basicContent);
    m_editUserName->setObjectName("ManageLibInput");
    m_editUserName->setPlaceholderText(tr("Enter user name"));
    m_editUserName->setClearButtonEnabled(true);
    basicBody->addWidget(m_editUserName);

    auto *copyRow = new QHBoxLayout();
    copyRow->setContentsMargins(0, 0, 0, 0);
    copyRow->setSpacing(12);
    copyRow->addWidget(createFieldLabel(tr("Copy from user"), basicContent));
    copyRow->addStretch(1);
    m_comboCopyFromUser = LayoutUtils::createStyledCombo(basicContent);
    m_comboCopyFromUser->setMinimumWidth(220);
    m_comboCopyFromUser->addItem(tr("Do not copy"), QString());
    copyRow->addWidget(m_comboCopyFromUser);
    basicBody->addLayout(copyRow);

    basicBody->addWidget(createDescriptionLabel(
        tr("Optionally copy settings from an existing user."),
        basicContent));

    basicBody->addWidget(createSwitchRow(tr("Allow user to manage server"),
                                         m_swAdmin, basicContent));
    scrollLayout->addWidget(basicCard);

    QVBoxLayout *copyOptionsBody = nullptr;
    m_copyOptionsSection =
        createSectionCard(tr("Copy Options"), scrollPage, copyOptionsBody);
    QWidget *copyOptionsContent = copyOptionsBody->parentWidget();

    m_chkCopyUserPolicy = new QCheckBox(tr("User policy"), copyOptionsContent);
    m_chkCopyUserPolicy->setObjectName("ManageLibCheckBox");
    m_chkCopyUserPolicy->setChecked(true);
    copyOptionsBody->addWidget(m_chkCopyUserPolicy);

    m_chkCopyUserConfiguration =
        new QCheckBox(tr("User configuration"), copyOptionsContent);
    m_chkCopyUserConfiguration->setObjectName("ManageLibCheckBox");
    m_chkCopyUserConfiguration->setChecked(true);
    copyOptionsBody->addWidget(m_chkCopyUserConfiguration);

    copyOptionsBody->addWidget(createDescriptionLabel(
        tr("Choose which settings to copy from the selected user."),
        copyOptionsContent));
    scrollLayout->addWidget(m_copyOptionsSection);

    QVBoxLayout *libraryAccessBody = nullptr;
    m_libraryAccessSection =
        createSectionCard(tr("Library Access"), scrollPage, libraryAccessBody);
    QWidget *libraryAccessContent = libraryAccessBody->parentWidget();

    libraryAccessBody->addWidget(createSwitchRow(
        tr("Allow access to all libraries"), m_swAllFolders,
        libraryAccessContent));
    m_swAllFolders->setChecked(true);

    m_folderListContainer = new QWidget(libraryAccessContent);
    m_folderListContainer->setObjectName("LibAdvancedPanel");
    m_folderListContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto *folderOuterLayout = new QVBoxLayout(m_folderListContainer);
    folderOuterLayout->setContentsMargins(12, 10, 12, 10);
    folderOuterLayout->setSpacing(8);
    m_folderListLayout = new QVBoxLayout();
    m_folderListLayout->setContentsMargins(0, 0, 0, 0);
    m_folderListLayout->setSpacing(8);
    folderOuterLayout->addLayout(m_folderListLayout);
    m_folderListContainer->hide();
    libraryAccessBody->addWidget(m_folderListContainer);

    libraryAccessBody->addWidget(createDescriptionLabel(
        tr("Choose which libraries this user can access."),
        libraryAccessContent));
    scrollLayout->addWidget(m_libraryAccessSection);

    QVBoxLayout *channelAccessBody = nullptr;
    m_channelAccessSection =
        createSectionCard(tr("Channel Access"), scrollPage, channelAccessBody);
    QWidget *channelAccessContent = channelAccessBody->parentWidget();

    channelAccessBody->addWidget(createSwitchRow(
        tr("Allow access to all channels"), m_swAllChannels,
        channelAccessContent));
    m_swAllChannels->setChecked(true);

    m_channelListContainer = new QWidget(channelAccessContent);
    m_channelListContainer->setObjectName("LibAdvancedPanel");
    m_channelListContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto *channelOuterLayout = new QVBoxLayout(m_channelListContainer);
    channelOuterLayout->setContentsMargins(12, 10, 12, 10);
    channelOuterLayout->setSpacing(8);
    m_channelListLayout = new QVBoxLayout();
    m_channelListLayout->setContentsMargins(0, 0, 0, 0);
    m_channelListLayout->setSpacing(8);
    channelOuterLayout->addLayout(m_channelListLayout);
    m_channelListContainer->hide();
    channelAccessBody->addWidget(m_channelListContainer);

    channelAccessBody->addWidget(createDescriptionLabel(
        tr("Choose which channels this user can access."),
        channelAccessContent));
    scrollLayout->addWidget(m_channelAccessSection);

    scrollLayout->addStretch(1);
    contentLayout()->addWidget(scrollPage, 1);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 12, 0);
    bottomRow->setSpacing(8);
    bottomRow->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setObjectName("SettingsCardButton");
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    bottomRow->addWidget(m_btnCancel);

    m_btnCreate = new QPushButton(tr("Create"), this);
    m_btnCreate->setObjectName("SettingsCardButton");
    m_btnCreate->setCursor(Qt::PointingHandCursor);
    bottomRow->addWidget(m_btnCreate);

    contentLayout()->addLayout(bottomRow);

    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnCreate, &QPushButton::clicked, this,
            [this]() { m_pendingTask = createUser(); });
    connect(m_editUserName, &QLineEdit::textChanged, this,
            [this]() { updatePrimaryActionState(); });
    connect(m_comboCopyFromUser, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateCopyModeState(); });
    connect(m_swAllFolders, &ModernSwitch::toggled, this,
            [this](bool checked) {
                m_folderListContainer->setVisible(!checked);
            });
    connect(m_swAllChannels, &ModernSwitch::toggled, this,
            [this](bool checked) {
                m_channelListContainer->setVisible(!checked);
            });
}

void AddUserDialog::updateCopyModeState() {
    const bool hasCopySource =
        m_comboCopyFromUser &&
        !m_comboCopyFromUser->currentData().toString().trimmed().isEmpty();

    if (m_copyOptionsSection) {
        m_copyOptionsSection->setVisible(hasCopySource);
    }
    if (m_libraryAccessSection) {
        m_libraryAccessSection->setVisible(!hasCopySource);
    }
    if (m_channelAccessSection) {
        m_channelAccessSection->setVisible(!hasCopySource && m_hasChannelOptions);
    }
    if (m_folderListContainer && m_swAllFolders) {
        m_folderListContainer->setVisible(!hasCopySource &&
                                         !m_swAllFolders->isChecked());
    }
    if (m_channelListContainer && m_swAllChannels) {
        m_channelListContainer->setVisible(!hasCopySource &&
                                          !m_swAllChannels->isChecked());
    }
}

void AddUserDialog::updatePrimaryActionState() {
    if (!m_btnCreate) {
        return;
    }

    const bool hasName =
        m_editUserName && !m_editUserName->text().trimmed().isEmpty();
    m_btnCreate->setEnabled(m_isDataLoaded && !m_isSubmitting && hasName);
}

void AddUserDialog::populateCopyUserOptions(const QList<UserInfo> &users) {
    if (!m_comboCopyFromUser) {
        return;
    }

    m_comboCopyFromUser->clear();
    m_comboCopyFromUser->addItem(tr("Do not copy"), QString());
    for (const UserInfo &user : users) {
        m_comboCopyFromUser->addItem(user.name, user.id);
    }
}

void AddUserDialog::populateSelectableFolders(
    const QList<SelectableMediaFolderInfo> &folders) {
    while (QLayoutItem *child = m_folderListLayout->takeAt(0)) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    m_folderCheckboxes.clear();
    m_fixedEnabledFolderIds.clear();

    for (const SelectableMediaFolderInfo &folder : folders) {
        if (!folder.isUserAccessConfigurable) {
            if (!folder.id.isEmpty() &&
                !m_fixedEnabledFolderIds.contains(folder.id)) {
                m_fixedEnabledFolderIds.append(folder.id);
            }
            continue;
        }

        auto *cb = new QCheckBox(folder.name, m_folderListContainer);
        cb->setObjectName("ManageLibCheckBox");
        cb->setProperty("folderId", folder.id);
        cb->setChecked(true);
        m_folderListLayout->addWidget(cb);
        m_folderCheckboxes.append(cb);
    }
}

void AddUserDialog::populateChannels(const QList<AdminChannelInfo> &channels) {
    while (QLayoutItem *child = m_channelListLayout->takeAt(0)) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }

    m_channelCheckboxes.clear();
    m_hasChannelOptions = !channels.isEmpty();

    for (const AdminChannelInfo &channel : channels) {
        auto *cb = new QCheckBox(channel.name, m_channelListContainer);
        cb->setObjectName("ManageLibCheckBox");
        cb->setProperty("channelId", channel.id);
        cb->setChecked(true);
        m_channelListLayout->addWidget(cb);
        m_channelCheckboxes.append(cb);
    }
}

QCoro::Task<void> AddUserDialog::loadInitialData() {
    QPointer<AddUserDialog> safeThis(this);

    QList<UserInfo> users;
    try {
        users = co_await m_core->adminService()->getUsers();
        if (!safeThis) {
            co_return;
        }
        populateCopyUserOptions(users);
    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }
        qDebug() << "[AddUserDialog] Failed to load users for copy options"
                 << "| error=" << e.what();
    }

    try {
        const QList<SelectableMediaFolderInfo> folders =
            co_await m_core->adminService()->getSelectableMediaFolders(false);
        if (!safeThis) {
            co_return;
        }
        populateSelectableFolders(folders);
    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }
        qDebug() << "[AddUserDialog] Failed to load selectable folders"
                 << "| error=" << e.what();
    }

    try {
        const QList<AdminChannelInfo> channels =
            co_await m_core->adminService()->getChannels();
        if (!safeThis) {
            co_return;
        }
        populateChannels(channels);
    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }
        qDebug() << "[AddUserDialog] Failed to load channels"
                 << "| error=" << e.what();
    }

    if (!safeThis) {
        co_return;
    }

    m_isDataLoaded = true;
    updateCopyModeState();
    updatePrimaryActionState();
    m_editUserName->setFocus();
}

QCoro::Task<void> AddUserDialog::createUser() {
    QPointer<AddUserDialog> safeThis(this);

    const QString trimmedName = m_editUserName->text().trimmed();
    if (trimmedName.isEmpty()) {
        m_editUserName->setFocus();
        ModernToast::showMessage(tr("User name cannot be empty"), 2000);
        co_return;
    }

    const QString copyFromUserId =
        m_comboCopyFromUser->currentData().toString().trimmed();
    QStringList userCopyOptions;
    if (!copyFromUserId.isEmpty()) {
        if (m_chkCopyUserPolicy->isChecked()) {
            userCopyOptions.append(QStringLiteral("UserPolicy"));
        }
        if (m_chkCopyUserConfiguration->isChecked()) {
            userCopyOptions.append(QStringLiteral("UserConfiguration"));
        }
    }

    m_isSubmitting = true;
    updatePrimaryActionState();

    QString createdUserId;
    QString createdUserName = trimmedName;

    try {
        qDebug() << "[AddUserDialog] Creating user"
                 << "| name=" << trimmedName
                 << "| copyFromUserId=" << copyFromUserId
                 << "| copyOptions=" << userCopyOptions;

        QJsonObject createdUser = co_await m_core->adminService()->createUser(
            trimmedName, copyFromUserId, userCopyOptions);
        if (!safeThis) {
            co_return;
        }

        createdUserId = createdUser["Id"].toString();
        const QString returnedName = createdUser["Name"].toString().trimmed();
        if (!returnedName.isEmpty()) {
            createdUserName = returnedName;
        }

        QJsonObject policy = createdUser["Policy"].toObject();
        policy["IsAdministrator"] = m_swAdmin->isChecked();

        if (copyFromUserId.isEmpty()) {
            const bool isAdmin = m_swAdmin->isChecked();
            policy["EnableSubtitleDownloading"] = isAdmin;
            policy["EnableSubtitleManagement"] = isAdmin;
            policy["EnableContentDeletion"] = isAdmin;

            policy["EnableAllFolders"] = m_swAllFolders->isChecked();
            QJsonArray enabledFolders;
            if (!m_swAllFolders->isChecked()) {
                QStringList addedFolderIds;
                for (const QString &folderId : m_fixedEnabledFolderIds) {
                    if (!folderId.isEmpty() && !addedFolderIds.contains(folderId)) {
                        enabledFolders.append(folderId);
                        addedFolderIds.append(folderId);
                    }
                }
                for (QCheckBox *cb : m_folderCheckboxes) {
                    const QString folderId = cb->property("folderId").toString();
                    if (cb->isChecked() && !folderId.isEmpty() &&
                        !addedFolderIds.contains(folderId)) {
                        enabledFolders.append(folderId);
                        addedFolderIds.append(folderId);
                    }
                }
            }
            policy["EnabledFolders"] = enabledFolders;

            policy["EnableAllChannels"] = m_swAllChannels->isChecked();
            QJsonArray enabledChannels;
            if (!m_swAllChannels->isChecked()) {
                QStringList addedChannelIds;
                for (QCheckBox *cb : m_channelCheckboxes) {
                    const QString channelId =
                        cb->property("channelId").toString();
                    if (cb->isChecked() && !channelId.isEmpty() &&
                        !addedChannelIds.contains(channelId)) {
                        enabledChannels.append(channelId);
                        addedChannelIds.append(channelId);
                    }
                }
            }
            policy["EnabledChannels"] = enabledChannels;
        }

        if (!createdUserId.isEmpty()) {
            co_await m_core->adminService()->setUserPolicy(createdUserId, policy);
            if (!safeThis) {
                co_return;
            }
        }

        ModernToast::showMessage(
            tr("User \"%1\" created").arg(createdUserName), 2000);
        emit userCreated();
        accept();
        co_return;
    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }

        if (!createdUserId.isEmpty()) {
            ModernToast::showMessage(
                tr("User \"%1\" created, but failed to apply permissions: %2")
                    .arg(createdUserName, QString::fromUtf8(e.what())),
                3500);
            emit userCreated();
            accept();
            co_return;
        }

        ModernToast::showMessage(
            tr("Failed to create user: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (safeThis) {
        m_isSubmitting = false;
        updatePrimaryActionState();
    }
}
