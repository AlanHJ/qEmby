#include "useragentsettingsdialog.h"

#include "modernmessagebox.h"
#include "modernswitch.h"
#include "moderntoast.h"
#include "api/useragentmanager.h"
#include "services/manager/servermanager.h"

#include <QHBoxLayout>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

UserAgentSettingsDialog *
UserAgentSettingsDialog::createForGlobal(QWidget *parent) {
    auto *dialog = new UserAgentSettingsDialog(Scope::Global, parent);
    dialog->m_initialConfig = UserAgentManager::instance()->globalConfig();
    dialog->loadInitialValues();
    return dialog;
}

UserAgentSettingsDialog *UserAgentSettingsDialog::createForServer(
    ServerManager *serverManager, const QString &serverId, QWidget *parent) {
    auto *dialog = new UserAgentSettingsDialog(Scope::PerServer, parent);
    dialog->m_serverManager = serverManager;
    dialog->m_serverId = serverId;
    if (serverManager) {
        for (const ServerProfile &profile : serverManager->servers()) {
            if (profile.id == serverId) {
                dialog->m_initialConfig = profile.userAgent;
                dialog->m_initialUseGlobal = profile.useGlobalUserAgent;
                break;
            }
        }
    }
    dialog->loadInitialValues();
    return dialog;
}

UserAgentSettingsDialog *UserAgentSettingsDialog::createForDraft(
    const UserAgentConfig &initial, bool initialUseGlobal, QWidget *parent) {
    auto *dialog = new UserAgentSettingsDialog(Scope::PerServer, parent);
    dialog->m_isDraft = true;
    dialog->m_initialConfig = initial;
    dialog->m_initialUseGlobal = initialUseGlobal;
    dialog->loadInitialValues();
    return dialog;
}

UserAgentSettingsDialog::UserAgentSettingsDialog(Scope scope, QWidget *parent)
    : ModernDialogBase(parent), m_scope(scope) {
    setObjectName(QStringLiteral("UserAgentSettingsDialog"));
    setTitle(scope == Scope::Global ? tr("User-Agent Settings")
                                    : tr("Server User-Agent Settings"));
    setModal(true);
    setMinimumWidth(500);
    buildUi();
}

void UserAgentSettingsDialog::buildUi() {
    auto *content = contentLayout();
    content->setSpacing(12);

    if (m_scope == Scope::PerServer) {
        auto *scopeCard = new QWidget(this);
        scopeCard->setObjectName(QStringLiteral("LibAdvancedPanel"));
        scopeCard->setAttribute(Qt::WA_StyledBackground, true);
        auto *scopeLayout = new QVBoxLayout(scopeCard);
        scopeLayout->setContentsMargins(14, 12, 14, 12);
        scopeLayout->setSpacing(8);
        auto *scopeRow = new QHBoxLayout();
        auto *scopeTitle = new QLabel(tr("Use Global User-Agent"), scopeCard);
        scopeTitle->setObjectName(QStringLiteral("ManageCardTitle"));
        m_useGlobalSwitch = new ModernSwitch(scopeCard);
        scopeRow->addWidget(scopeTitle, 1);
        scopeRow->addWidget(m_useGlobalSwitch, 0, Qt::AlignVCenter);
        scopeLayout->addLayout(scopeRow);
        m_scopeHint = new QLabel(
            tr("When enabled, this server uses the global User-Agent. Disable "
               "to configure a User-Agent specific to this server."),
            scopeCard);
        m_scopeHint->setObjectName(QStringLiteral("ManageInfoKey"));
        m_scopeHint->setWordWrap(true);
        scopeLayout->addWidget(m_scopeHint);
        content->addWidget(scopeCard);
    } else {
        auto *hint = new QLabel(
            tr("These settings apply as the default User-Agent for all "
               "requests, unless a server overrides them."),
            this);
        hint->setObjectName(QStringLiteral("ProxyDialogHint"));
        hint->setWordWrap(true);
        content->addWidget(hint);
    }

    auto *formCard = new QWidget(this);
    formCard->setObjectName(QStringLiteral("LibAdvancedPanel"));
    formCard->setAttribute(Qt::WA_StyledBackground, true);
    auto *formLayout = new QVBoxLayout(formCard);
    formLayout->setContentsMargins(14, 12, 14, 12);
    formLayout->setSpacing(10);

    auto *enabledRow = new QHBoxLayout();
    auto *enabledTitle = new QLabel(tr("Enable Custom User-Agent"), formCard);
    enabledTitle->setObjectName(QStringLiteral("ManageCardTitle"));
    m_enabledSwitch = new ModernSwitch(formCard);
    enabledRow->addWidget(enabledTitle, 1);
    enabledRow->addWidget(m_enabledSwitch, 0, Qt::AlignVCenter);
    formLayout->addLayout(enabledRow);

    auto *valueLabel = new QLabel(tr("User-Agent"), formCard);
    valueLabel->setObjectName(QStringLiteral("ManageInfoKey"));
    formLayout->addWidget(valueLabel);
    m_valueEdit = new QLineEdit(formCard);
    m_valueEdit->setObjectName(QStringLiteral("ManageLibInput"));
    m_valueEdit->setPlaceholderText(
        tr("Enter the User-Agent sent with network requests"));
    m_valueEdit->setClearButtonEnabled(true);
    formLayout->addWidget(m_valueEdit);
    content->addWidget(formCard);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setObjectName(QStringLiteral("dialog-btn-cancel"));
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setObjectName(QStringLiteral("dialog-btn-primary"));
    m_saveButton->setCursor(Qt::PointingHandCursor);
    m_saveButton->setDefault(true);
    buttonRow->addWidget(m_cancelButton);
    buttonRow->addWidget(m_saveButton);
    content->addLayout(buttonRow);

    connect(m_enabledSwitch, &ModernSwitch::toggled, m_valueEdit,
            &QWidget::setEnabled);
    if (m_useGlobalSwitch) {
        connect(m_useGlobalSwitch, &ModernSwitch::toggled, this,
                &UserAgentSettingsDialog::onUseGlobalToggled);
    }
    connect(m_saveButton, &QPushButton::clicked, this,
            &UserAgentSettingsDialog::onSaveClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void UserAgentSettingsDialog::loadInitialValues() {
    if (m_useGlobalSwitch) {
        QSignalBlocker blocker(m_useGlobalSwitch);
        m_useGlobalSwitch->setChecked(m_initialUseGlobal);
    }
    applyConfigToUi(m_initialUseGlobal && m_scope == Scope::PerServer
                        ? UserAgentManager::instance()->globalConfig()
                        : m_initialConfig);
}

void UserAgentSettingsDialog::applyConfigToUi(
    const UserAgentConfig &config) {
    m_enabledSwitch->setChecked(config.enabled);
    m_valueEdit->setText(config.value);
    m_valueEdit->setEnabled(config.enabled);
}

UserAgentConfig UserAgentSettingsDialog::collectFromUi() const {
    UserAgentConfig config;
    config.enabled = m_enabledSwitch->isChecked();
    config.value = m_valueEdit->text().trimmed();
    return config;
}

void UserAgentSettingsDialog::onUseGlobalToggled(bool checked) {
    applyConfigToUi(checked ? UserAgentManager::instance()->globalConfig()
                            : m_initialConfig);
    ModernToast::showMessage(
        checked ? tr("Editing the global User-Agent settings below.")
                : tr("Editing this server's User-Agent settings below."),
        2500);
}

void UserAgentSettingsDialog::onSaveClicked() {
    const UserAgentConfig config = collectFromUi();
    if (config.enabled && config.value.isEmpty()) {
        ModernMessageBox::warning(
            this, tr("Invalid User-Agent"),
            tr("Please enter a User-Agent value or disable the setting."));
        return;
    }
    if (config.value.contains(QLatin1Char('\r')) ||
        config.value.contains(QLatin1Char('\n'))) {
        ModernMessageBox::warning(
            this, tr("Invalid User-Agent"),
            tr("User-Agent cannot contain line breaks."));
        return;
    }

    const bool useGlobal =
        m_useGlobalSwitch && m_useGlobalSwitch->isChecked();
    if (m_scope == Scope::Global) {
        UserAgentManager::instance()->setGlobalConfig(config);
        m_resultConfig = config;
        m_resultUseGlobal = false;
    } else if (useGlobal) {
        if (!m_isDraft && (!m_serverManager || m_serverId.isEmpty())) {
            qWarning() << "[UserAgentSettingsDialog] missing server context";
            return;
        }
        UserAgentManager::instance()->setGlobalConfig(config);
        if (!m_isDraft) {
            m_serverManager->updateServerUserAgent(m_serverId, m_initialConfig,
                                                   true);
        }
        m_resultConfig = m_initialConfig;
        m_resultUseGlobal = true;
    } else {
        if (!m_isDraft && (!m_serverManager || m_serverId.isEmpty())) {
            qWarning() << "[UserAgentSettingsDialog] missing server context";
            return;
        }
        if (!m_isDraft) {
            m_serverManager->updateServerUserAgent(m_serverId, config, false);
        }
        m_resultConfig = config;
        m_resultUseGlobal = false;
    }
    qInfo() << "[UserAgentSettingsDialog] saved"
            << "| scope:"
            << (m_scope == Scope::Global ? "global" : "server")
            << "| useGlobal:" << useGlobal
            << "| isDraft:" << m_isDraft
            << "| enabled:" << config.enabled
            << "| hasValue:" << !config.value.isEmpty();
    accept();
}
