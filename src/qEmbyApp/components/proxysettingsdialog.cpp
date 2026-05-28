#include "proxysettingsdialog.h"

#include "modernmessagebox.h"
#include "moderncombobox.h"
#include "modernswitch.h"
#include "moderntoast.h"

#include "api/proxymanager.h"
#include "services/manager/servermanager.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>

namespace {

constexpr const char* kTestProbeUrl =
    "http://www.gstatic.com/generate_204";
constexpr int kTestTimeoutMs = 8000;

class ProxySettingsTestProxyFactory : public QNetworkProxyFactory {
public:
    explicit ProxySettingsTestProxyFactory(ProxyConfig cfg)
        : m_config(std::move(cfg)) {}

    QList<QNetworkProxy> queryProxy(
        const QNetworkProxyQuery& query) override {
        if (m_config.mode == ProxyConfig::System) {
            const auto proxies = QNetworkProxyFactory::systemProxyForQuery(query);
            return proxies.isEmpty()
                       ? QList<QNetworkProxy>{
                             QNetworkProxy(QNetworkProxy::NoProxy)}
                       : proxies;
        }
        return {m_config.toQNetworkProxy()};
    }

private:
    ProxyConfig m_config;
};

}  





ProxySettingsDialog* ProxySettingsDialog::createForGlobal(QWidget* parent) {
    auto* dlg = new ProxySettingsDialog(Scope::Global, parent);
    dlg->m_initialConfig = ProxyManager::instance()->globalConfig();
    dlg->m_initialUseGlobal = false;
    dlg->loadInitialValues();
    return dlg;
}

ProxySettingsDialog* ProxySettingsDialog::createForServer(
    ServerManager* sm, const QString& serverId, QWidget* parent) {
    auto* dlg = new ProxySettingsDialog(Scope::PerServer, parent);
    dlg->m_serverId = serverId;
    dlg->m_serverManager = sm;
    dlg->m_isDraft = false;

    if (sm) {
        const auto servers = sm->servers();
        for (const auto& s : servers) {
            if (s.id == serverId) {
                dlg->m_initialConfig = s.proxy;
                dlg->m_initialUseGlobal = s.useGlobalProxy;
                break;
            }
        }
    } else {
        qWarning() << "[ProxySettingsDialog] createForServer: null sm "
                      "| serverId:"
                   << serverId;
    }

    dlg->loadInitialValues();
    return dlg;
}

ProxySettingsDialog* ProxySettingsDialog::createForDraft(
    const ProxyConfig& initial, bool initialUseGlobal, QWidget* parent) {
    auto* dlg = new ProxySettingsDialog(Scope::PerServer, parent);
    dlg->m_isDraft = true;
    dlg->m_initialConfig = initial;
    dlg->m_initialUseGlobal = initialUseGlobal;
    dlg->loadInitialValues();
    return dlg;
}





ProxySettingsDialog::ProxySettingsDialog(Scope scope, QWidget* parent)
    : ModernDialogBase(parent), m_scope(scope) {
    setObjectName(QStringLiteral("ProxySettingsDialog"));
    setTitle(scope == Scope::Global ? tr("Proxy Settings")
                                    : tr("Server Proxy Settings"));
    setModal(true);

    
    
    
    setMinimumWidth(460);

    buildUi();
    wireSignals();
}

void ProxySettingsDialog::buildUi() {
    auto* content = contentLayout();
    content->setSpacing(12);

    
    if (m_scope == Scope::PerServer) {
        auto* useGlobalCard = new QWidget(this);
        useGlobalCard->setObjectName(QStringLiteral("LibAdvancedPanel"));
        useGlobalCard->setAttribute(Qt::WA_StyledBackground, true);
        auto* useGlobalLayout = new QVBoxLayout(useGlobalCard);
        useGlobalLayout->setContentsMargins(14, 12, 14, 12);
        useGlobalLayout->setSpacing(8);

        auto* useGlobalRow = new QHBoxLayout();
        useGlobalRow->setContentsMargins(0, 0, 0, 0);
        useGlobalRow->setSpacing(12);

        auto* titleLbl = new QLabel(tr("Use Global Proxy"), useGlobalCard);
        titleLbl->setObjectName(QStringLiteral("ManageCardTitle"));
        titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        m_useGlobalSwitch = new ModernSwitch(useGlobalCard);
        useGlobalRow->addWidget(titleLbl, 1);
        useGlobalRow->addWidget(m_useGlobalSwitch, 0, Qt::AlignVCenter);
        useGlobalLayout->addLayout(useGlobalRow);

        m_useGlobalHint = new QLabel(
            tr("When enabled, this server uses the global proxy settings. "
               "Disable to configure a proxy specific to this server."),
            useGlobalCard);
        m_useGlobalHint->setObjectName(QStringLiteral("ManageInfoKey"));
        m_useGlobalHint->setWordWrap(true);
        useGlobalLayout->addWidget(m_useGlobalHint);

        content->addWidget(useGlobalCard);
    } else {
        auto* banner = new QLabel(
            tr("These settings apply as the default proxy for all servers, "
               "unless a server overrides them."),
            this);
        banner->setObjectName(QStringLiteral("ProxyDialogHint"));
        banner->setWordWrap(true);
        content->addWidget(banner);
    }

    
    m_formContainer = new QWidget(this);
    m_formContainer->setObjectName(QStringLiteral("LibAdvancedPanel"));
    m_formContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto* formLayout = new QVBoxLayout(m_formContainer);
    formLayout->setContentsMargins(14, 12, 14, 12);
    formLayout->setSpacing(10);

    auto makeFieldLabel = [this](const QString& text) {
        auto* lbl = new QLabel(text, m_formContainer);
        lbl->setObjectName(QStringLiteral("ManageInfoKey"));
        return lbl;
    };

    
    
    auto* modeRow = new QHBoxLayout();
    modeRow->setContentsMargins(0, 0, 0, 0);
    modeRow->setSpacing(10);

    auto* modeLabel = makeFieldLabel(tr("Proxy Mode"));
    m_modeCombo = new ModernComboBox(m_formContainer);
    m_modeCombo->addItem(tr("Do not use proxy"),
                         static_cast<int>(ProxyConfig::None));
    m_modeCombo->addItem(tr("Use system proxy"),
                         static_cast<int>(ProxyConfig::System));
    m_modeCombo->addItem(tr("Custom proxy"),
                         static_cast<int>(ProxyConfig::Custom));
    m_modeCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_typeLabel = makeFieldLabel(tr("Type"));
    m_typeCombo = new ModernComboBox(m_formContainer);
    m_typeCombo->addItem(QStringLiteral("HTTP"),
                         static_cast<int>(ProxyConfig::Http));
    m_typeCombo->addItem(QStringLiteral("SOCKS5"),
                         static_cast<int>(ProxyConfig::Socks5));

    modeRow->addWidget(modeLabel, 0);
    modeRow->addWidget(m_modeCombo, 1);
    modeRow->addSpacing(8);
    modeRow->addWidget(m_typeLabel, 0);
    modeRow->addWidget(m_typeCombo, 0);
    formLayout->addLayout(modeRow);

    
    m_customPanel = new QWidget(m_formContainer);
    auto* customLayout = new QVBoxLayout(m_customPanel);
    customLayout->setContentsMargins(0, 0, 0, 0);
    customLayout->setSpacing(10);

    
    auto* hostPortRow = new QHBoxLayout();
    hostPortRow->setContentsMargins(0, 0, 0, 0);
    hostPortRow->setSpacing(10);

    auto* hostLabel = makeFieldLabel(tr("Host"));
    m_hostEdit = new QLineEdit(m_customPanel);
    m_hostEdit->setObjectName(QStringLiteral("ManageLibInput"));
    m_hostEdit->setPlaceholderText(tr("e.g. 127.0.0.1"));

    auto* portLabel = makeFieldLabel(tr("Port"));
    m_portEdit = new QLineEdit(m_customPanel);
    m_portEdit->setObjectName(QStringLiteral("ManageLibInput"));
    m_portEdit->setPlaceholderText(tr("1 - 65535"));
    m_portEdit->setValidator(new QIntValidator(1, 65535, m_portEdit));
    m_portEdit->setMaxLength(5);
    m_portEdit->setFixedWidth(96);

    hostPortRow->addWidget(hostLabel, 0);
    hostPortRow->addWidget(m_hostEdit, 1);
    hostPortRow->addSpacing(8);
    hostPortRow->addWidget(portLabel, 0);
    hostPortRow->addWidget(m_portEdit, 0);
    customLayout->addLayout(hostPortRow);

    
    auto* userPassRow = new QHBoxLayout();
    userPassRow->setContentsMargins(0, 0, 0, 0);
    userPassRow->setSpacing(10);

    auto* userLabel = makeFieldLabel(tr("Username"));
    m_usernameEdit = new QLineEdit(m_customPanel);
    m_usernameEdit->setObjectName(QStringLiteral("ManageLibInput"));
    m_usernameEdit->setPlaceholderText(tr("Optional"));

    auto* passLabel = makeFieldLabel(tr("Password"));
    m_passwordEdit = new QLineEdit(m_customPanel);
    m_passwordEdit->setObjectName(QStringLiteral("ManageLibInput"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("Optional"));

    userPassRow->addWidget(userLabel, 0);
    userPassRow->addWidget(m_usernameEdit, 1);
    userPassRow->addSpacing(8);
    userPassRow->addWidget(passLabel, 0);
    userPassRow->addWidget(m_passwordEdit, 1);
    customLayout->addLayout(userPassRow);

    
    auto* bypassRow = new QHBoxLayout();
    bypassRow->setContentsMargins(0, 4, 0, 0);
    bypassRow->setSpacing(12);
    auto* bypassLabel = makeFieldLabel(tr("Bypass for local addresses"));
    bypassLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_bypassSwitch = new ModernSwitch(m_customPanel);
    bypassRow->addWidget(bypassLabel, 1);
    bypassRow->addWidget(m_bypassSwitch, 0, Qt::AlignVCenter);
    customLayout->addLayout(bypassRow);

    formLayout->addWidget(m_customPanel);

    content->addWidget(m_formContainer);

    
    
    auto* testRow = new QHBoxLayout();
    testRow->setContentsMargins(0, 0, 0, 0);
    testRow->setSpacing(10);
    m_testBtn = new QPushButton(tr("Test Connection"), this);
    m_testBtn->setObjectName(QStringLiteral("SettingsCardButton"));
    m_testBtn->setCursor(Qt::PointingHandCursor);
    m_testBtn->setFixedHeight(32);

    m_testUrlEdit = new QLineEdit(this);
    m_testUrlEdit->setObjectName(QStringLiteral("ManageLibInput"));
    m_testUrlEdit->setText(QString::fromLatin1(kTestProbeUrl));
    m_testUrlEdit->setPlaceholderText(QString::fromLatin1(kTestProbeUrl));
    m_testUrlEdit->setClearButtonEnabled(true);
    m_testUrlEdit->setToolTip(
        tr("Probe URL used to verify the proxy connection. "
           "Leave empty to use the default."));

    testRow->addWidget(m_testBtn, 0);
    testRow->addWidget(m_testUrlEdit, 1);
    content->addLayout(testRow);

    m_testStatusLabel = new QLabel(this);
    m_testStatusLabel->setObjectName(QStringLiteral("ProxyDialogTestStatus"));
    m_testStatusLabel->setWordWrap(true);
    content->addWidget(m_testStatusLabel);

    
    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 4, 0, 0);
    btnRow->setSpacing(10);
    btnRow->addStretch(1);
    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName(QStringLiteral("dialog-btn-cancel"));
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn = new QPushButton(tr("Save"), this);
    m_saveBtn->setObjectName(QStringLiteral("dialog-btn-primary"));
    m_saveBtn->setCursor(Qt::PointingHandCursor);
    m_saveBtn->setDefault(true);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_saveBtn);
    content->addLayout(btnRow);
}

void ProxySettingsDialog::loadInitialValues() {
    
    applyConfigToUi(m_initialConfig);

    if (m_useGlobalSwitch) {
        
        
        QSignalBlocker blocker(m_useGlobalSwitch);
        m_useGlobalSwitch->setChecked(m_initialUseGlobal);

        
        
        
        if (m_initialUseGlobal) {
            const auto gc = ProxyManager::instance()->globalConfig();
            qInfo() << "[ProxySettingsDialog] initial useGlobal=true; "
                       "form shows GLOBAL proxy"
                    << "| isExplicit:" << gc.isExplicit()
                    << "| global:" << gc.summary();
            applyConfigToUi(gc);
        }
    }

    updateFormEnabled();
}

void ProxySettingsDialog::applyConfigToUi(const ProxyConfig& cfg) {
    
    const int modeIdx = m_modeCombo->findData(static_cast<int>(cfg.mode));
    if (modeIdx >= 0) m_modeCombo->setCurrentIndex(modeIdx);

    
    const int typeIdx = m_typeCombo->findData(static_cast<int>(cfg.type));
    if (typeIdx >= 0) m_typeCombo->setCurrentIndex(typeIdx);

    m_hostEdit->setText(cfg.host);
    if (cfg.port > 0) {
        m_portEdit->setText(QString::number(cfg.port));
    } else {
        m_portEdit->clear();
    }
    m_usernameEdit->setText(cfg.username);
    m_passwordEdit->setText(cfg.password);
    m_bypassSwitch->setChecked(cfg.bypassLocalhost);

    
    updateCustomPanelVisibility();
}

void ProxySettingsDialog::wireSignals() {
    connect(m_modeCombo,
            QOverload<int>::of(&ModernComboBox::currentIndexChanged), this,
            [this](int) { updateCustomPanelVisibility(); });

    if (m_useGlobalSwitch) {
        connect(m_useGlobalSwitch, &ModernSwitch::toggled, this,
                &ProxySettingsDialog::onUseGlobalToggled);
    }

    connect(m_testBtn, &QPushButton::clicked, this,
            &ProxySettingsDialog::onTestClicked);
    connect(m_saveBtn, &QPushButton::clicked, this,
            &ProxySettingsDialog::onSaveClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}





void ProxySettingsDialog::updateCustomPanelVisibility() {
    const auto mode = static_cast<ProxyConfig::Mode>(
        m_modeCombo->currentData().toInt());
    const bool show = (mode == ProxyConfig::Custom);

    
    if (m_typeLabel) m_typeLabel->setVisible(show);
    if (m_typeCombo) m_typeCombo->setVisible(show);

    
    m_customPanel->setVisible(show);

    
    
    
    
    QTimer::singleShot(0, this, [this]() {
        if (auto* lay = layout()) {
            lay->invalidate();
            lay->activate();
        }
        const QSize hint = sizeHint();
        resize(width(), hint.height());
    });
}

void ProxySettingsDialog::updateFormEnabled() {
    
    
    
    m_formContainer->setEnabled(true);
    m_testBtn->setEnabled(true);
    if (m_testUrlEdit) m_testUrlEdit->setEnabled(true);
}

void ProxySettingsDialog::onUseGlobalToggled(bool checked) {
    
    
    
    
    
    if (checked) {
        
        const auto gc = ProxyManager::instance()->globalConfig();
        qInfo() << "[ProxySettingsDialog] use-global toggled on; "
                   "form now represents GLOBAL proxy"
                << "| isExplicit:" << gc.isExplicit()
                << "| global:" << gc.summary();
        applyConfigToUi(gc);
        if (gc.isExplicit()) {
            ModernToast::showMessage(
                tr("Loaded global proxy settings below. You can review or "
                   "modify them before saving."),
                4500);
        } else {
            ModernToast::showMessage(
                tr("Editing global proxy. Fill in the form below and save "
                   "to apply it globally."),
                5000);
        }
    } else {
        
        qInfo() << "[ProxySettingsDialog] use-global toggled off; "
                   "form now represents SERVER-PRIVATE proxy"
                << "| server:" << m_initialConfig.summary();
        applyConfigToUi(m_initialConfig);
    }
    updateFormEnabled();
}

ProxyConfig ProxySettingsDialog::collectFromUi() const {
    ProxyConfig cfg;
    cfg.mode = static_cast<ProxyConfig::Mode>(
        m_modeCombo->currentData().toInt());
    cfg.type = static_cast<ProxyConfig::Type>(
        m_typeCombo->currentData().toInt());
    cfg.host = m_hostEdit->text().trimmed();
    const int parsedPort = m_portEdit->text().trimmed().toInt();
    cfg.port = (parsedPort > 0 && parsedPort <= 65535)
                   ? static_cast<quint16>(parsedPort)
                   : 0;
    cfg.username = m_usernameEdit->text();
    cfg.password = m_passwordEdit->text();
    cfg.bypassLocalhost = m_bypassSwitch->isChecked();
    return cfg;
}

bool ProxySettingsDialog::validateForm(QString* errorMessage) const {
    const auto cfg = collectFromUi();
    if (cfg.mode != ProxyConfig::Custom) {
        return true;
    }
    if (cfg.host.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("Please enter the proxy host.");
        }
        return false;
    }
    if (cfg.port == 0) {
        if (errorMessage) {
            *errorMessage = tr("Please enter a valid port (1 - 65535).");
        }
        return false;
    }
    return true;
}

void ProxySettingsDialog::onSaveClicked() {
    const bool useGlobal =
        m_useGlobalSwitch && m_useGlobalSwitch->isChecked();

    
    
    QString err;
    if (!validateForm(&err)) {
        ModernMessageBox::warning(this, tr("Invalid Proxy"), err);
        return;
    }

    const auto cfg = collectFromUi();

    qInfo() << "[ProxySettingsDialog] save"
            << "| scope:" << (m_scope == Scope::Global ? "global" : "server")
            << "| serverId:" << m_serverId
            << "| isDraft:" << m_isDraft
            << "| useGlobal:" << useGlobal
            << "| form-proxy:" << cfg.summary();

    if (m_scope == Scope::Global) {
        
        persistGlobal(cfg);
        m_resultConfig = cfg;
        m_resultUseGlobal = false;
    } else if (useGlobal) {
        
        
        
        
        
        qInfo() << "[ProxySettingsDialog] PerServer ON: persist form to "
                   "GLOBAL; keep server-private snapshot"
                << "| server-private kept:" << m_initialConfig.summary();
        persistGlobal(cfg);
        if (!m_isDraft) {
            persistServer(m_initialConfig, true);
        }
        m_resultConfig = m_initialConfig;
        m_resultUseGlobal = true;
    } else {
        
        
        
        qInfo() << "[ProxySettingsDialog] PerServer OFF: persist form to "
                   "SERVER-PRIVATE; leave global untouched";
        if (!m_isDraft) {
            persistServer(cfg, false);
        }
        m_resultConfig = cfg;
        m_resultUseGlobal = false;
    }
    accept();
}

void ProxySettingsDialog::persistGlobal(const ProxyConfig& cfg) {
    
    ProxyManager::instance()->setGlobalConfig(cfg);
}

void ProxySettingsDialog::persistServer(const ProxyConfig& cfg,
                                        bool useGlobal) {
    if (m_serverId.isEmpty() || !m_serverManager) {
        qWarning() << "[ProxySettingsDialog] persistServer: missing context"
                   << "| serverId:" << m_serverId
                   << "| serverManager:" << (m_serverManager != nullptr);
        return;
    }
    m_serverManager->updateServerProxy(m_serverId, cfg, useGlobal);
}





void ProxySettingsDialog::onTestClicked() {
    QString err;
    if (!validateForm(&err)) {
        m_testStatusLabel->setText(
            tr("✗ %1").arg(err));
        m_testStatusLabel->setProperty("status", "error");
        m_testStatusLabel->style()->unpolish(m_testStatusLabel);
        m_testStatusLabel->style()->polish(m_testStatusLabel);
        return;
    }

    
    
    QString rawUrl = m_testUrlEdit ? m_testUrlEdit->text().trimmed() : QString();
    if (rawUrl.isEmpty()) {
        rawUrl = QString::fromLatin1(kTestProbeUrl);
    }
    const QUrl probeUrl = QUrl::fromUserInput(rawUrl);
    if (!probeUrl.isValid() ||
        (probeUrl.scheme().compare(QStringLiteral("http"),
                                   Qt::CaseInsensitive) != 0 &&
         probeUrl.scheme().compare(QStringLiteral("https"),
                                   Qt::CaseInsensitive) != 0)) {
        qWarning() << "[ProxySettingsDialog] invalid probe URL"
                   << "| raw:" << rawUrl
                   << "| parsed:" << probeUrl.toString();
        m_testStatusLabel->setText(
            tr("✗ %1").arg(tr("Invalid test URL.")));
        m_testStatusLabel->setProperty("status", "error");
        m_testStatusLabel->style()->unpolish(m_testStatusLabel);
        m_testStatusLabel->style()->polish(m_testStatusLabel);
        return;
    }

    const auto cfg = collectFromUi();

    m_testBtn->setEnabled(false);
    m_testStatusLabel->setText(tr("Testing..."));
    m_testStatusLabel->setProperty("status", "pending");
    m_testStatusLabel->style()->unpolish(m_testStatusLabel);
    m_testStatusLabel->style()->polish(m_testStatusLabel);

    qInfo() << "[ProxySettingsDialog] test start"
            << "| proxy:" << cfg.summary()
            << "| probeUrl:" << probeUrl.toString();

    
    
    auto* nam = new QNetworkAccessManager(this);
    nam->setProxyFactory(new ProxySettingsTestProxyFactory(cfg));

    QNetworkRequest req(probeUrl);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setRawHeader("User-Agent", "qEmby-ProxyTest/1.0");
    auto* timer = new QElapsedTimer();
    timer->start();
    QNetworkReply* reply = nam->head(req);

    QPointer<ProxySettingsDialog> guard(this);

    auto* timeout = new QTimer(this);
    timeout->setSingleShot(true);
    timeout->start(kTestTimeoutMs);
    connect(timeout, &QTimer::timeout, this, [reply]() {
        if (reply && reply->isRunning()) {
            reply->abort();
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, nam, timer,
                                                    timeout, guard]() {
        const qint64 elapsed = timer->elapsed();
        delete timer;
        timeout->stop();
        timeout->deleteLater();

        if (!guard) {
            reply->deleteLater();
            nam->deleteLater();
            return;
        }

        const bool ok = (reply->error() == QNetworkReply::NoError);
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString errStr = reply->errorString();

        reply->deleteLater();
        nam->deleteLater();

        m_testBtn->setEnabled(true);

        if (ok) {
            m_testStatusLabel->setText(
                tr("✓ Success (HTTP %1, %2 ms)")
                    .arg(QString::number(httpStatus),
                         QString::number(elapsed)));
            m_testStatusLabel->setProperty("status", "ok");
            qInfo() << "[ProxySettingsDialog] test ok"
                    << "| http:" << httpStatus
                    << "| elapsed:" << elapsed;
        } else {
            m_testStatusLabel->setText(
                tr("✗ Failed (%1)").arg(errStr));
            m_testStatusLabel->setProperty("status", "error");
            qWarning() << "[ProxySettingsDialog] test failed"
                       << "| http:" << httpStatus
                       << "| err:" << errStr
                       << "| elapsed:" << elapsed;
        }
        m_testStatusLabel->style()->unpolish(m_testStatusLabel);
        m_testStatusLabel->style()->polish(m_testStatusLabel);
    });
}
