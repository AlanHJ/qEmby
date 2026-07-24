#include "pagegeneral.h"
#include "../../components/elidedlabel.h"
#include "../../components/moderncombobox.h"
#include "../../components/modernmessagebox.h"
#include "../../components/modernswitch.h"
#include "../../components/moderntoast.h"
#include "../../components/proxysettingsdialog.h"
#include "../../components/settingscard.h"
#include "../../components/settingssubpanel.h"
#include "../../managers/logmanager.h"
#include "api/proxymanager.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include "models/profile/proxyconfig.h"
#include "qembycore.h"
#include "services/manager/servermanager.h"
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QPushButton>
#include <QUrl>

PageGeneral::PageGeneral(QEmbyCore *core, QWidget *parent)
    : SettingsPageBase(core, tr("General"), parent) {
  
  auto *langCombo = new ModernComboBox(this);
  langCombo->addItem(tr("System"), "system");
  langCombo->addItem(tr("简体中文"), "zh_CN");
  langCombo->addItem(tr("English"), "en_US");
  langCombo->addItem(tr("French"), "fr_FR");
  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/language.svg", tr("Language"),
                       tr("App interface language (requires restart)"),
                       langCombo, ConfigKeys::Language, this));

  
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/remember-server.svg", tr("Remember Last Server"),
      tr("Automatically connect to the last used server on startup"),
      new ModernSwitch(this), ConfigKeys::RememberServer, this));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/tray.svg", tr("Close to Tray"),
      tr("Minimize to system tray instead of exiting the application"),
      new ModernSwitch(this), ConfigKeys::CloseToTray, this));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/appearance-window-state.svg", tr("Single Application Mode"),
      tr("Allow only one qEmby instance and activate it when launched again "
         "(requires restart)"),
      new ModernSwitch(this), ConfigKeys::SingleApplication, this, false));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/refresh.svg", tr("Check for Updates"),
      tr("Automatically check GitHub for a new qEmby version on startup"),
      new ModernSwitch(this), ConfigKeys::CheckForUpdates, this, true));

  
  
  
  auto *proxyBtn = new QPushButton(tr("Configure..."), this);
  proxyBtn->setObjectName("SettingsCardButton");
  proxyBtn->setCursor(Qt::PointingHandCursor);
  proxyBtn->setFixedHeight(30);

  auto *proxyCard = new SettingsCard(
      ":/svg/dark/proxy.svg", tr("Network Proxy"),
      tr("Proxy for the current server (global or server-specific)"),
      proxyBtn, QString(), this);
  m_mainLayout->addWidget(proxyCard);

  auto proxyStateText = [](const ProxyConfig &cfg) -> QString {
    switch (cfg.mode) {
    case ProxyConfig::None:
      return PageGeneral::tr("no proxy");
    case ProxyConfig::System:
      return PageGeneral::tr("system proxy");
    case ProxyConfig::Custom: {
      const QString typeLabel =
          (cfg.type == ProxyConfig::Socks5) ? QStringLiteral("SOCKS5")
                                            : QStringLiteral("HTTP");
      if (cfg.host.isEmpty() || cfg.port == 0) {
        return PageGeneral::tr("custom (incomplete)");
      }
      return PageGeneral::tr("%1 %2:%3")
          .arg(typeLabel, cfg.host, QString::number(cfg.port));
    }
    }
    return QString();
  };

  auto buildProxySummaryDesc = [this, proxyStateText]() -> QString {
    if (m_core && m_core->serverManager()) {
      const ServerProfile profile = m_core->serverManager()->activeProfile();
      if (profile.isValid()) {
        if (profile.useGlobalProxy) {
          return PageGeneral::tr(
                     "Proxy for the current server — using global: %1")
              .arg(proxyStateText(ProxyManager::instance()->globalConfig()));
        }
        return PageGeneral::tr(
                   "Proxy for the current server — currently: %1")
            .arg(proxyStateText(profile.proxy));
      }
    }

    return PageGeneral::tr("Default proxy for all servers — currently: %1")
        .arg(proxyStateText(ProxyManager::instance()->globalConfig()));
  };

  
  
  
  auto refreshProxyDesc = [proxyCard, buildProxySummaryDesc]() {
    const auto labels =
        proxyCard->findChildren<ElidedLabel *>("SettingsCardDesc");
    for (ElidedLabel *lbl : labels) {
      lbl->setFullText(buildProxySummaryDesc());
    }
  };
  refreshProxyDesc();

  
  connect(ProxyManager::instance(), &ProxyManager::proxyChanged, proxyCard,
          refreshProxyDesc);
  if (m_core && m_core->serverManager()) {
    connect(m_core->serverManager(), &ServerManager::activeServerChanged,
            proxyCard,
            [refreshProxyDesc](const ServerProfile &) { refreshProxyDesc(); });
  }

  connect(proxyBtn, &QPushButton::clicked, this, [this, refreshProxyDesc]() {
    ProxySettingsDialog *dlg = nullptr;
    if (m_core && m_core->serverManager()) {
      const ServerProfile profile = m_core->serverManager()->activeProfile();
      if (profile.isValid()) {
        dlg = ProxySettingsDialog::createForServer(m_core->serverManager(),
                                                   profile.id, this);
      }
    }
    if (!dlg) {
      dlg = ProxySettingsDialog::createForGlobal(this);
    }
    dlg->exec();
    refreshProxyDesc();
    dlg->deleteLater();
  });

  
  auto *logSwitch = new ModernSwitch(this);
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/log.svg", tr("Enable Logging"),
      tr("Record debug information to a log file for troubleshooting"),
      logSwitch, ConfigKeys::LogEnable, this));

  
  auto *logPanel = new SettingsSubPanel(":/svg/dark/log-path.svg", this);

  
  auto *logPathLabel = new QLabel(
      tr("Log path: %1")
          .arg(QDir::toNativeSeparators(LogManager::instance()->logFilePath())),
      this);
  logPathLabel->setObjectName("SettingsCardDesc");
  logPathLabel->setWordWrap(true);
  logPanel->contentLayout()->addWidget(logPathLabel, 1);

  m_mainLayout->addWidget(logPanel);

  
  auto *logActionsPanel = new SettingsSubPanel(":/svg/dark/log-tools.svg", this);

  
  auto *logSizeLabel =
      new QLabel(tr("Size: %1").arg(LogManager::instance()->logFileSize()),
                 this);
  logSizeLabel->setObjectName("SettingsCardDesc");
  logActionsPanel->contentLayout()->addWidget(logSizeLabel, 1);

  
  auto *openLogBtn = new QPushButton(tr("Open Log"), this);
  openLogBtn->setObjectName("SettingsCardButton");
  openLogBtn->setCursor(Qt::PointingHandCursor);
  openLogBtn->setFixedHeight(30);
  logActionsPanel->contentLayout()->addWidget(openLogBtn, 0,
                                              Qt::AlignVCenter);

  
  auto *clearLogBtn = new QPushButton(tr("Clear Log"), this);
  clearLogBtn->setObjectName("SettingsCardButton");
  clearLogBtn->setCursor(Qt::PointingHandCursor);
  clearLogBtn->setFixedHeight(30);
  logActionsPanel->contentLayout()->addWidget(clearLogBtn, 0,
                                              Qt::AlignVCenter);

  
  auto *openDirBtn = new QPushButton(tr("Open Location"), this);
  openDirBtn->setObjectName("SettingsCardButton");
  openDirBtn->setCursor(Qt::PointingHandCursor);
  openDirBtn->setFixedHeight(30);
  logActionsPanel->contentLayout()->addWidget(openDirBtn, 0,
                                              Qt::AlignVCenter);

  m_mainLayout->addWidget(logActionsPanel);

  
  if (ConfigStore::instance()->get<bool>(ConfigKeys::LogEnable, false)) {
    logPanel->initExpanded();
    logActionsPanel->initExpanded();
  }

  
  connect(logSwitch, &ModernSwitch::toggled, logPanel,
          &SettingsSubPanel::setExpanded);
  connect(logSwitch, &ModernSwitch::toggled, logActionsPanel,
          &SettingsSubPanel::setExpanded);

  
  connect(logSwitch, &ModernSwitch::toggled, this,
          [logSizeLabel](bool checked) {
            if (checked) {
              LogManager::instance()->enable();
            } else {
              LogManager::instance()->disable();
            }
            logSizeLabel->setText(QObject::tr("Size: %1").arg(
                LogManager::instance()->logFileSize()));
          });

  
  connect(openLogBtn, &QPushButton::clicked, this, []() {
    QString logPath = LogManager::instance()->logFilePath();
    if (QFileInfo::exists(logPath)) {
      QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
    }
  });

  
  connect(clearLogBtn, &QPushButton::clicked, this, [this, logSizeLabel]() {
    const bool confirmed = ModernMessageBox::question(
        this, tr("Clear Log"),
        tr("Are you sure you want to clear all log files?\n\n"
           "This will remove the current log and rotated backups. This action "
           "cannot be undone."),
        tr("Clear"), tr("Cancel"), ModernMessageBox::Danger,
        ModernMessageBox::Warning);
    if (!confirmed) {
      return;
    }

    LogManager::instance()->clearLog();
    logSizeLabel->setText(
        QObject::tr("Size: %1").arg(LogManager::instance()->logFileSize()));
    ModernToast::showMessage(tr("Log files cleared"), 1500);
  });

  
  connect(openDirBtn, &QPushButton::clicked, this, []() {
    QString logDir =
        QFileInfo(LogManager::instance()->logFilePath()).absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(logDir));
  });

  m_mainLayout->addStretch();
}
